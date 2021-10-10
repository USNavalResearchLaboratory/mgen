#include "mgen.h"
#include "mgenTransport.h"
#include "mgenGlobals.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#ifdef WIN32
#include <IpHlpApi.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif // UNIX

class Mgen;

MgenSinkTransport* MgenSinkTransport::Create(Mgen& theMgen, Protocol theProtocol)
{
    return static_cast<MgenSinkTransport*>(new MgenAppSinkTransport(theMgen,theProtocol));
}

MgenAppSinkTransport::MgenAppSinkTransport(Mgen&                theMgen,
                                           Protocol             theProtocol,
                                           UINT16               thePort,
                                           const ProtoAddress&  theDstAddress)
  : MgenSinkTransport(theMgen,theProtocol,thePort,theDstAddress)
{

    ProtoDispatcher* theDispatcher = static_cast<ProtoDispatcher*>(&theMgen.GetSocketNotifier());
    ProtoChannel::SetNotifier(static_cast<ProtoChannel::Notifier*>(theDispatcher));
    ProtoChannel::SetListener(this,&MgenAppSinkTransport::OnEvent);
    path[0] = '\0';
}

MgenAppSinkTransport::MgenAppSinkTransport(Mgen& theMgen, Protocol theProtocol)
  : MgenSinkTransport(theMgen,theProtocol)
{

    ProtoDispatcher* theDispatcher = static_cast<ProtoDispatcher*>(&theMgen.GetSocketNotifier());
    ProtoChannel::SetNotifier(static_cast<ProtoChannel::Notifier*>(theDispatcher));
    ProtoChannel::SetListener(this,&MgenAppSinkTransport::OnEvent);
}

MgenAppSinkTransport::~MgenAppSinkTransport()
{

}
void MgenAppSinkTransport::OnEvent(ProtoChannel& theChannel, ProtoChannel::NotifyFlag theFlag)
{  
    switch (theFlag)
    {
        case ProtoChannel::NOTIFY_INPUT:
        {
            OnInputReady();
            break;
        }
        case ProtoChannel::NOTIFY_OUTPUT:
            if (OnOutputReady()) 
                SendPendingMessage();   

            break;
        case ProtoChannel::NOTIFY_NONE:
        default:
            DMSG(0,"Mgen::OnEvent() Invalid notification received.\n");
            break;
    }
    
} // end MgenAppSinkTransport::OnEvent()

/**
 *  "Source" sinks should be opened with the output open method...
 */
bool MgenAppSinkTransport::Open(ProtoAddress::Type addrType, bool bindOnOpen)
{
  // ljt probably could just as easily make this one method and be better for clarity
  if (is_source)
    return Open();
  
  Close();
  StopInputNotification(); // the channel open method starts input notification

#ifdef WIN32
#ifdef _WIN32_WCE
    DMSG(0, "MgenAppSinkTransport::Open() \"sink\" option not support under WinCE\n");
    return false;
#else // else _WIN32_WCE
    if (('\0' == path[0]) || 0 == strcmp(path, "STDOUT"))
    {
      descriptor = (ProtoDispatcher::Descriptor)GetStdHandle(STD_OUTPUT_HANDLE);
        // (TBD) set stdout for overlapped I/O ???
    }
    else
    {
    //  descriptor = (ProtoDispatcher::Descriptor)CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, 
	//						   NULL, CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);

      descriptor = (ProtoDispatcher::Descriptor)CreateFile(path, FILE_APPEND_DATA, FILE_SHARE_READ, 
							   NULL, CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);
	}
    if ((ProtoDispatcher::Descriptor)INVALID_HANDLE_VALUE == descriptor)
    {
        DMSG(0, "MgenAppSinkTransport::Open() error opening file\n");
        return false;
    }
    // For non-block I/O on Win32 we use overlapped I/O
    if (NULL == (output_handle = CreateEvent(NULL, TRUE, FALSE, NULL)))
    {
        DMSG(0, "MgenAppSinkTransport::Open() error creating overlapped I/O event\n");
        CloseHandle((HANDLE)descriptor);  // ljt shouldn't this be output-handle?
        return false;
    } 
	memset(&write_overlapped,0,sizeof(write_overlapped));
	write_overlapped.hEvent = output_handle;

#endif  // if/else _WIN32_WCE   
#else   // if/else WIN32
    if (('\0' == path[0]) || 0 == strcmp(path, "STDOUT"))
    {
        descriptor = fileno(stdout);
        if (sink_non_blocking)
        {
            // Make the stdout non-blocking and aynchronous
            int flags = O_NONBLOCK;
#ifdef FASYNC
            flags |= FASYNC;
#endif // FASYNC
            if(-1 == fcntl(descriptor, F_SETFL, fcntl(descriptor, F_GETFL, 0) | flags))
            {
                descriptor = -1;
                DMSG(0, "MgenAppSinkTransport::Open() fcntl(stdout, F_SETFL(O_NONBLOCK)) error: %s",
                        GetErrorString());
            }
        }
    }
    else
    {
        int flags = O_CREAT | O_WRONLY;
        if (sink_non_blocking) flags |= O_NONBLOCK;
        descriptor = open(path, flags);
    }
    if (descriptor < 0)
    {
        DMSG(0, "MgenAppSinkTransport::Open() open(%s) error: %s\n", path, GetErrorString());
        descriptor = ProtoDispatcher::INVALID_DESCRIPTOR;
        return false;
    }  
#endif // if/else WIN32
    msg_length = msg_index = 0;
    // we need to _end_ with this
	StartOutputNotification();
	StopInputNotification();  // why do I have to do this??
	return UpdateNotification(); // because protoChannel::Open starts input nots
    // ljt ??return ProtoChannel::Open();
  
} // end MgenAppSinkTransport::Open


MessageStatus MgenAppSinkTransport::SendMessage(MgenMsg& theMsg, const ProtoAddress& dstAddr)
{
    
    UINT32 txChecksum = 0;
    unsigned int len = 0;
    theMsg.SetFlag(MgenMsg::LAST_BUFFER);

    UINT32 txBuffer[MAX_SIZE/4 + 1];
    len = theMsg.Pack(txBuffer,theMsg.GetMsgLen(),mgen.GetChecksumEnable(), txChecksum);
    if (0 == len) return MSG_SEND_FAILED; // no room
    if (mgen.GetChecksumEnable() && theMsg.FlagIsSet(MgenMsg::CHECKSUM))
        theMsg.WriteChecksum(txChecksum,(unsigned char*)txBuffer, (UINT32)len);

    if (msg_index >= msg_length)
    {
        // Copy txBuffer to msg_buffer
        len = (len < MAX_SIZE) ? len : MAX_SIZE;
        memcpy(msg_buffer, txBuffer, len);
        msg_length = len;
        msg_index = 0;
        if (!OnOutputReady())  // and rename this function
	  {
	    return MSG_SEND_BLOCKED;
	  }
    }
    else
    {
        PLOG(PL_WARN, "MgenAppSinkTransport::SendMessage() message sink buffer overflow\n");
	return MSG_SEND_BLOCKED;
    }
    struct timeval currentTime;
    ProtoSystemTime(currentTime);
    LogEvent(SEND_EVENT,&theMsg,currentTime,txBuffer); 
    return MSG_SEND_OK;
    
} // end MgenAppSinkTransport::SendMessage

bool MgenAppSinkTransport::OnOutputReady()
{

    unsigned int nbytes = msg_length - msg_index;

    if (!Write(((char*)msg_buffer)+msg_index, &nbytes))
      {
        DMSG(0, "MgenAppSinkTransport::OnOutputReady() error writing to output!\n");
        return false;
      }
    msg_index += nbytes;
    if (msg_index < msg_length)
    {
        StartOutputNotification();  
    }
    return true;
    
}  // MgenAppSinkTransport::OnOutputReady()

bool MgenAppSinkTransport::Write(char* buffer, unsigned int* nbytes)
{
    unsigned int len = *nbytes;
#ifdef WIN32
    DWORD put = 0;
#ifndef _WIN32_WCE
    while (put < len)
    {
        DWORD written;
	//	OVERLAPPED osWrite = {0};
      //  osWrite.hEvent = write_event;
//        if (!WriteFile((HANDLE)descriptor, buffer+put, len - put, &written, &osWrite))
		if (!WriteFile((HANDLE)descriptor,buffer+put,len-put,&written,&write_overlapped))  
		{
            if (ERROR_IO_PENDING != GetLastError())
            {
                // I/O error
                DMSG(0, "MgenAppSinkTransportSendMgenMessage() WriteFile() error: %d\n",
                        GetLastError()); 
                *nbytes = put;
                return false;
            }
            else
            {            
                // (TBD perhaps wait some small amount of time instead of ZERO?
                DWORD result = WaitForSingleObject(output_handle, 0);
                switch (result)
                {
                    case WAIT_OBJECT_0:
                   //     if (GetOverlappedResult((HANDLE)descriptor, &osWrite, &written, FALSE))
						if (GetOverlappedResult((HANDLE)descriptor,&write_overlapped,&written,FALSE))                   
						{
                            put += written;  
                        }
                        else
                        {
                            DMSG(0, "MgenAppSinkTransportSendMgenMessage() GetOverlappedResult() error: %d\n",
                                    GetLastError());
                            *nbytes = put;
                            return false;    
                        }                        
                        break; 
                    default:
                        // I/O timed out or abandoned
                        DMSG(0, "MgenAppSinkTransportSendMgenMessage() WaitForSingleObject() error: %d\n",
                                GetLastError());
                        *nbytes = put;
                        return false;  
                }
            }
        }
        else
        {
            // WriteFile completed immediately
            put += written;    
        }   
    } 
#endif // !_WIN32_WCE
#else // !WIN32
    unsigned int put = 0;
    while (put < (ssize_t)len)
    {
        ssize_t result = write(descriptor, buffer+put, len - put);
        if (result < 0)
        {
            switch (errno)
            {
                case EINTR:
                    continue;     // interrupted, try again
                case EAGAIN:
                    *nbytes = put;
                    return true; // blocked, can't write message now
                default:
                    DMSG(0, "MgenAppSinkTransportSendMgenMessage() write() error: %s\n", GetErrorString());
                    *nbytes = put;
                    return false; 
            }
        }
        else
        {
            put += result;    
        }           
    }
#endif // if/else WIN32
    *nbytes = put;
    return true;
}  // end MgenAppSinkTransport::Write()

bool MgenAppSinkTransport::Open()
{
  // AddrType not used by sink open method
  // ljt make these a common function
  if (!is_source)
    Open(ProtoAddress::IPv4,true);

  Close();

#ifdef WIN32
#ifdef _WIN32_WCE
    DMSG(0, "MgenAppSinkTransport::Open() \"source\" option not supported under WinCE\n");
    return false;
#else // else _WIN32_WC#
    if (('\0' == path[0]) || 0 == strcmp(path, "STDOUT"))
    {
      descriptor = (ProtoDispatcher::Descriptor)GetStdHandle(STD_INPUT_HANDLE);
        // (TBD) set stdout for overlapped I/O ???
    }
    else
    {
	     descriptor = (ProtoDispatcher::Descriptor)CreateFile(path, GENERIC_READ,  FILE_SHARE_READ, 
                                NULL, OPEN_EXISTING, NULL, NULL);
	
	}
    if ((ProtoDispatcher::Descriptor)INVALID_HANDLE_VALUE == descriptor)
    {
        DMSG(0, "MgenAppSinkTransport::Open() error opening file %d\n",GetLastError());
        return false;
    }

	//while (OnInputReady()); // ljt do we need to get dispatcher to trigger initial read??


#endif  // if/else _WIN32_WCE    
#else   // if/else !WIN32
    if (('\0' == path[0]) || 0 == strcmp(path, "STDIN"))
    {
        descriptor = fileno(stdin);
        // Make the stdin non-blocking
        if(-1 == fcntl(descriptor, F_SETFL, fcntl(descriptor, F_GETFL, 0) | O_NONBLOCK))
        {
            descriptor = -1;
            DMSG(0, "MgenAppSinkTransport::Open() fcntl(stdout, F_SETFL(O_NONBLOCK)) error: %s",
                    GetErrorString());
        }
    }
    else
    {
      descriptor = open(path, O_RDONLY | O_NONBLOCK);
    }
    if (descriptor < 0)
    {
        DMSG(0, "MgenAppSinkTransport::Open() open(%s) error: %s\n", path, GetErrorString());
        descriptor = ProtoDispatcher::INVALID_DESCRIPTOR;
        return false;
    }  
#endif // if/else WIN32
    msg_length = msg_index = 0;

    // we need to _end_ with this
    return ProtoChannel::Open();


} // end MgenAppSinkTransport::Open()

bool MgenAppSinkTransport::OnInputReady()
{
	UINT32 count;
    if (msg_length)
    {
      if (Read(((char*)msg_buffer)+msg_index, msg_length - msg_index, count))
        {
            msg_index += count;
            if (msg_index == msg_length)
            {
                // We get src addr from the socket for udp/tcp
                ProtoAddress srcAddr; 
                srcAddr.Reset(mgen.GetDefaultSocketType());
                srcAddr.SetPort(srcPort);
                HandleMgenMessage(GetMsgBuffer(),GetMsgLength(),srcAddr);
 
                SetMsgLength(0);
                SetMsgIndex(0);
#ifndef WIN32
				StartInputNotification();
#endif // WIN32
				return true;
            }   
        }
      else
      {
#ifndef WIN32
		  StopInputNotification();
#endif // WIN32
          return false;
      }
    }
    else
    {
        // Reading first two bytes of MGEN message to get
        // MGEN "messageSize" field
        if (Read(((char*)msg_buffer)+msg_index, 2 - msg_index, count))
        {
            msg_index += count;
            if (2 == msg_index)
            {
                UINT16 tmpLen;
                memcpy(&tmpLen,msg_buffer,2);
                msg_length = ntohs(tmpLen);

                if ((msg_length < MIN_SIZE) || (msg_length > MAX_SIZE))
                {
                    DMSG(0, "MgenAppSinkTransport::OnInputReady() invalid MGEN message length received: %lu count>%d\n",
                         msg_length,count);
                    msg_length = msg_index = 0;
					return false;  
                }
                OnInputReady();
            }
        }
        else
        {	
#ifndef WIN32
			DMSG(1,"Nothing to read! Stopping input notification\n");
			StopInputNotification();
#endif // WIN32
			return false;
        }     
    }
    return true;
} // end MgenAppSinkTransport::OnInputReadySynch()

bool MgenAppSinkTransport::Read(char* buffer, UINT32 nBytes, UINT32& bytesRead)
{
#ifdef WIN32
    DWORD want = nBytes;
	UINT32 got_pending = 0;
    char read_buffer[BUFFER_MAX];

	if (got_pending < want) 
	{
		BOOL bResult;
		DWORD bytes_read;

		unsigned int len = want - got_pending;
		if (len > BUFFER_MAX) len = BUFFER_MAX;
		bResult = ReadFile(descriptor,read_buffer,len,&bytes_read,NULL);
		// check for EOF
		if (bResult && bytes_read == 0)
		{
			switch(GetLastError())
			{
				case ERROR_IO_PENDING:
					msg_index = 0;
					break;
				case ERROR_BROKEN_PIPE:
					if (0 == got_pending)
					{
						  return false;
					}
					break;
				default:
					PLOG(PL_ERROR, "ProtoPipe::Recv() ReadFile(%d) error: %s\n", GetLastError(), ::GetErrorString());
					if (0 == got_pending) return false;
				break;
			}
			PLOG(PL_ERROR,"EOF Detected\n");
			return false;
		} 

		memcpy(buffer+got_pending, read_buffer, bytes_read);
		got_pending += bytes_read;
	}
    bytesRead = got_pending;
	return true;

#else // ELSE !WIN32
    ssize_t result = read(descriptor, buffer, nBytes);
    if (result <= 0)
    {
        switch (errno)
        {
            case EINTR:
            case EAGAIN:
                bytesRead = 0;
                return true;
            default:
	            DMSG(0, "MgenAppSinkTransport::Read() read error: %s errno>%d\n", strerror(errno),errno);
                return false;   
        }
    }
    else
    {
        bytesRead = (UINT32)result;
        return true;   
    }
#endif // endif WIN32
}  // end MgenAppSinkTransport::ReadSynch()

