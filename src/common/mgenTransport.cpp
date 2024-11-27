#include "mgen.h"
#include "mgenTransport.h"
#include "mgenMsg.h"
#include "protoSocket.h"
#include "protoChannel.h"
#include "mgenGlobals.h"

#ifndef _WIN32_WCE  // JPH 6/8/06
#include <errno.h>  // for EAGAIN
#endif // !_WIN32_WCE

#ifdef WIN32  // for sink io
#define ENOBUFS WSAENOBUFS
#include <IpHlpApi.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif // UNIX

MgenTransportList::MgenTransportList()
  :  head(NULL), tail(NULL)
{
}

MgenTransportList::~MgenTransportList()
{
  Destroy();   
}

void MgenTransportList::Destroy()
{
    MgenTransport* next = head;
    while (next)
    {
        MgenTransport* current = next;
        next = next->next;
        if (current->IsOpen()) current->Close(); 
        delete current;
        
    }
    head = tail = NULL;   
    
}  // end MgenTransportList::Destroy()

void MgenTransportList::Prepend(MgenTransport* mgenTransport)
{
    mgenTransport->prev = NULL;
    if ((mgenTransport->next = head))
      head->prev = mgenTransport;
    else
      tail = mgenTransport;
    head = mgenTransport;
}  // end MgenTransportList::Prepend()

void MgenTransportList::Append(MgenTransport* mgenTransport)
{
    mgenTransport->next = NULL;
    if (NULL != (mgenTransport->prev = tail))
      tail->next = mgenTransport;
    else
      head = mgenTransport;
    tail = mgenTransport;
}  // end MgenTransportList::Append()

void MgenTransportList::Remove(MgenTransport* mgenTransport)
{
    if (mgenTransport->prev)
      mgenTransport->prev->next = mgenTransport->next;
    else
      head = mgenTransport->next;
    if (mgenTransport->next)
      mgenTransport->next->prev = mgenTransport->prev;
    else
      tail = mgenTransport->prev;
}  // end MgenTransportList::Remove()


///////////////////////////////////////////////////////////
// MgenTransport::MgenTransport() implementation

MgenTransport::MgenTransport(Mgen& theMgen,
                             Protocol theProtocol)
  : prev(NULL), next(NULL), 
    srcPort(0), dstPort(0),    
    protocol(theProtocol),
    reference_count(0),
    mgen(theMgen),
    pending_head(NULL),
    pending_tail(NULL),
    pending_current(NULL)
{
}

MgenTransport::MgenTransport(Mgen& theMgen,
                             Protocol theProtocol,
                             UINT16 thePort)
  : prev(NULL), next(NULL),
    srcPort(thePort),dstPort(0),    
    protocol(theProtocol),
    reference_count(0),
    mgen(theMgen),
    pending_head(NULL),
    pending_tail(NULL),
    pending_current(NULL)
{
}

MgenTransport::MgenTransport(Mgen& theMgen,
                             Protocol theProtocol,
                             UINT16 thePort,
                             const ProtoAddress& theAddress)
  : prev(NULL), next(NULL), 
    srcPort(thePort),dstPort(0),
    protocol(theProtocol),
    reference_count(0),
    mgen(theMgen),
    pending_head(NULL),
    pending_tail(NULL),
    pending_current(NULL)
{
  dstAddress = theAddress;
}

MgenTransport::~MgenTransport()
{

}
void MgenTransport::AppendFlow(MgenFlow* const theFlow)
{

    if (pending_head == NULL)
    {
      pending_current = pending_head = pending_tail = theFlow;
      theFlow->AppendPendingNext(NULL);
    }
    else
    {
        if (!theFlow->GetPendingNext() && pending_tail != theFlow)
        {
            pending_tail->AppendPendingNext(theFlow);
            theFlow->AppendPendingPrev(pending_tail);
            pending_tail = theFlow;
            theFlow->AppendPendingNext(NULL);
        }
    }
            
    return;
    
} // end MgenTransport::AppendFlow()

void MgenTransport::RemoveFlow(MgenFlow* theFlow)
{
    // ljt rewrite?
    MgenFlow *next,*prev;
    prev = NULL;
    for (next = pending_head; next != NULL; prev = next, next = next->GetPendingNext())
    {
        if (next == theFlow)
        {
            if (prev == NULL)
            {
                pending_head = next->GetPendingNext();
                if (pending_head)
                  pending_head->AppendPendingPrev(NULL);
                
                if (pending_current == theFlow)
                    pending_current = pending_head;                 
            }
            else
            {   
                prev->AppendPendingNext(next->GetPendingNext());

                if (next->GetPendingNext())
                  next->GetPendingNext()->AppendPendingPrev(next->GetPendingPrev());

                if (pending_current == theFlow)
                    pending_current = prev->GetPendingNext(); 

                if (pending_tail == theFlow)
                {
                    pending_tail = prev;
                    pending_current = pending_head;
                }
            }

            theFlow->AppendPendingNext(NULL);
            theFlow->AppendPendingPrev(NULL);
            return;
        }
    }
} // end MgenTransport::RemoveFlow()

void MgenTransport::PrintList()
{
    DMSG(0,"List ");
    MgenFlow* next = pending_head;

    while (next)
    {
        DMSG(0,"%d ",next->GetFlowId());
        next = next->GetPendingNext();
    }

    if (pending_current)
      DMSG(0,"Current: %d\n",pending_current->GetFlowId());

}    


bool MgenTransport::SendPendingMessage()
{
    // break out of while loop after N iterations to resume asynch i/o
    unsigned int breakOut = 0;
    unsigned int pending_message_limit = 10000;
    
    // Send pending messages until we hit congestion
    // or clear the queue...
    while (IsOpen() && !IsTransmitting() && pending_current)
    {
      breakOut++;
      int msgLimit = pending_current->GetMessageLimit();
      bool sendMore = (msgLimit < 0) || (pending_current->GetMessagesSent() < msgLimit);
      
      if ((pending_current->GetPending() > 0) || 
          (pending_current->UnlimitedRate() && sendMore))
      {
          if (!pending_current->SendMessage()) 
              return false;
      }
      
      // Restart flow timer if we're below the queue limit
      if ((pending_current->QueueLimit() > 0 &&
           (pending_current->GetPending() < pending_current->QueueLimit()))
          ||
          (pending_current->QueueLimit() < 0 &&
           !(pending_current->GetPending() > 0)) // ljt don't think we need 
          // this anymore if we're 
          // just restarting the timer?
          ||
          pending_current->QueueLimit() == 0)    // Tcp finally connected?
	
      {
          // Don't restart the timer if our rate is unlimited...
          if (!pending_current->UnlimitedRate())
              pending_current->RestartTimer();    
      }

        // Restart flow timer if we're below the queue limit
        if (((pending_current->QueueLimit() > 0) &&
             (pending_current->GetPending() < pending_current->QueueLimit())) ||
            ((pending_current->QueueLimit() < 0) &&
             !(pending_current->GetPending() > 0)) || // ljt don't think we need anymore if just timer restart
            (pending_current->QueueLimit() == 0))     // Tcp finally connected?

        {
            // Don't restart the timer if our rate is unlimited...
            if (!pending_current->UnlimitedRate())
                pending_current->RestartTimer();    
        }
      
      // If we've sent all pending messages, 
      // remove flow from pending list.
      
      if (!pending_current->GetPending() &&
          (!pending_current->UnlimitedRate() || !sendMore))
      {
          RemoveFromPendingList();  //ljt remove this function
          // or replace remove flow
          // why istn' this getting called??
          //RemoveFlow(pending_current);
          continue;
      }
      pending_current = pending_current->GetPendingNext();

    // cycle back to head of queue if we
    // reached the end.
    if (!pending_current && pending_head)
        pending_current = pending_head; 
      
      // cycle back to head of queue if we
      // reached the end.
      if (!pending_current && pending_head)
          pending_current = pending_head;

      if (breakOut > pending_message_limit)
      {
          // If we've met our pending_message_limit break out
          // of our tight loop sending messages as fast as possible
          // to service any off events.
          return true;
      }
      
    }
    // Resume normal operations
    if (!IsTransmitting() && !HasPendingFlows()) 
    {
        StopOutputNotification();
    }            
    return true;
    
}  // end MgenTransport::SendPendingMessage()

void MgenTransport::RemoveFromPendingList()
{

    MgenFlow* theFlow = NULL;
    if ((theFlow = pending_current->GetPendingPrev()))
      theFlow->AppendPendingNext(pending_current->GetPendingNext());
    else
      pending_head = pending_current->GetPendingNext();
    
    theFlow = NULL;
    if ((theFlow = pending_current->GetPendingNext()))
      theFlow->AppendPendingPrev(pending_current->GetPendingPrev());
    else
      pending_tail = pending_current->GetPendingPrev();
    
    theFlow = pending_current;
    if (pending_current->GetPendingNext())
      pending_current = pending_current->GetPendingNext();
    else
      pending_current = pending_head;
    theFlow->AppendPendingNext(NULL);
    theFlow->AppendPendingPrev(NULL);
        
}  // end MgenTransport::RemoveFromPendingList()

void MgenTransport::LogEvent(LogEventType eventType, MgenMsg* theMsg, const struct timeval& theTime, UINT32* buffer)
{
    if (!(mgen.GetLogFile()))
      return;  

    if (!theMsg->GetDstAddr().IsValid())
        theMsg->SetDstAddr(dstAddress);


    switch (eventType)
    {
    case SEND_EVENT:
      {
          if (mgen.GetLogTx())
          {
              theMsg->LogSendEvent(mgen.GetLogFile(),
                                   mgen.GetLogBinary(),
                                   mgen.GetLocalTime(),
                                   buffer,
                                   mgen.GetLogFlush(),
                                   theTime);
          }
          break;
      }
    case RECV_EVENT:
      {
          theMsg->SetProtocol(protocol);
          theMsg->LogRecvEvent(mgen.GetLogFile(),
                               mgen.GetLogBinary(), 
                               mgen.GetLocalTime(), 
                               mgen.GetLogRx(),
			                   mgen.GetLogData(),
			                   mgen.GetLogGpsData(),
                               buffer, 
                               mgen.GetLogFlush(),
                               -1, 
                               theTime);

          // Don't we want rapr to get the message regardless of logging??
          // Could this possibly have been broken too? strange... ljt
          if (mgen.GetController())
                mgen.GetController()->OnMsgReceive(*theMsg);

          break;
      }
    case SHUTDOWN_EVENT:
      {
            theMsg->GetSrcAddr().SetPort(GetSrcPort());
            theMsg->LogTcpConnectionEvent(mgen.GetLogFile(),
                                            mgen.GetLogBinary(),
                                            mgen.GetLocalTime(),
                                            mgen.GetLogFlush(),
                                            eventType,
                                            IsClient(),
                                            theTime);
              
          break;
      }
    case OFF_EVENT:
      {
          theMsg->GetSrcAddr().SetPort(GetSrcPort());
          theMsg->LogTcpConnectionEvent(mgen.GetLogFile(),
                                        mgen.GetLogBinary(),
                                        mgen.GetLocalTime(),
                                        mgen.GetLogFlush(),
                                        eventType,
                                        IsClient(),
                                        theTime);
              
          break;
      }
    case ON_EVENT:
      {
          theMsg->LogTcpConnectionEvent(mgen.GetLogFile(),
                                        mgen.GetLogBinary(),
                                        mgen.GetLocalTime(),
                                        mgen.GetLogFlush(),
                                        eventType,
                                        IsClient(),
                                        theTime);
          break;
      }
    case ACCEPT_EVENT:
      {
          theMsg->GetSrcAddr().SetPort(GetSrcPort());
          theMsg->LogTcpConnectionEvent(mgen.GetLogFile(),
                                        mgen.GetLogBinary(), 
                                        mgen.GetLocalTime(),
                                        mgen.GetLogFlush(), 
                                        eventType,
                                        IsClient(),
                                        theTime);
          
          break;
      }
    case CONNECT_EVENT:
      {
          theMsg->LogTcpConnectionEvent(mgen.GetLogFile(),
                                        mgen.GetLogBinary(), 
                                        mgen.GetLocalTime(),
                                        mgen.GetLogFlush(),
                                        eventType,
                                        IsClient(),
                                        theTime);	
          
          break;
      }
    case DISCONNECT_EVENT:
      {
          theMsg->GetSrcAddr().SetPort(GetSrcPort());
          theMsg->LogTcpConnectionEvent(mgen.GetLogFile(),
                                        mgen.GetLogBinary(),
                                        mgen.GetLocalTime(),
                                        mgen.GetLogFlush(),
                                        eventType,
                                        IsClient(),
                                        theTime);		    
          break;
      }
    case RECONNECT_EVENT:
    {
        theMsg->GetSrcAddr().SetPort(GetSrcPort());
        theMsg->LogTcpConnectionEvent(mgen.GetLogFile(),
                                     mgen.GetLogBinary(),
                                     mgen.GetLocalTime(),
                                     mgen.GetLogFlush(),
                                     eventType,
                                     IsClient(),
                                     theTime);
        break;
    }
    case RERR_EVENT:
      {

          theMsg->LogRecvError(mgen.GetLogFile(),
                               mgen.GetLogBinary(), 
                               mgen.GetLocalTime(), 
                               mgen.GetLogFlush(),
                               theTime);
          
          break;
      }
    case LISTEN_EVENT:
    case IGNORE_EVENT:
    case JOIN_EVENT:
    case LEAVE_EVENT:
    case START_EVENT:
    case STOP_EVENT:
    case INVALID_EVENT:
    default:
      DMSG(0,"MgenTransport::LogEvent() Error: Invalid LogEvent type.\n");
      break;
    }

} // End MgenTransport::LogEvent()

MgenSocketTransport::MgenSocketTransport(Mgen& theMgen,
                                         Protocol theProtocol,
                                         UINT16 thePort)
  : MgenTransport(theMgen,theProtocol,thePort),
    broadcast(false),
    tos(0), tx_buffer(0), rx_buffer(0),multicast_ttl(0),unicast_ttl(0),
    df(DF_DEFAULT),
    socket(GetSocketProtocol(theProtocol))
{
    interface_name[0] = '\0';
    socket.SetNotifier(&mgen.GetSocketNotifier());

    // set defaults
    SetBroadcast(theMgen.GetDefaultBroadcast());
    SetMulticastTTL(theMgen.GetDefaultMulticastTtl());
    SetUnicastTTL(theMgen.GetDefaultUnicastTtl());
    SetDF(theMgen.GetDefaultDF());
    SetTOS(theMgen.GetDefaultTos());
    SetTxBufferSize(theMgen.GetDefaultTxBuffer());
    SetRxBufferSize(theMgen.GetDefaultRxBuffer());
    SetMulticastInterface(theMgen.GetDefaultMulticastInterface());
}

MgenSocketTransport::MgenSocketTransport(Mgen& theMgen,
                                         Protocol theProtocol,
                                         UINT16        thePort,
                                         const ProtoAddress&         theDstAddress)
  : MgenTransport(theMgen,theProtocol,thePort,theDstAddress),
    broadcast(false),
    tos(0), tx_buffer(0), rx_buffer(0),multicast_ttl(0),unicast_ttl(0),
    df(DF_DEFAULT),
    socket(GetSocketProtocol(theProtocol))
{
    interface_name[0] = '\0';
    socket.SetNotifier(&mgen.GetSocketNotifier());

    // set defaults
    SetBroadcast(theMgen.GetDefaultBroadcast());
    SetMulticastTTL(theMgen.GetDefaultMulticastTtl());
    SetUnicastTTL(theMgen.GetDefaultUnicastTtl());
    SetDF(theMgen.GetDefaultDF());
    SetTOS(theMgen.GetDefaultTos());
    SetTxBufferSize(theMgen.GetDefaultTxBuffer());
    SetRxBufferSize(theMgen.GetDefaultRxBuffer());
    SetMulticastInterface(theMgen.GetDefaultMulticastInterface());
}

MgenSocketTransport::~MgenSocketTransport()
{

}
void MgenSocketTransport::SetEventOptions(const MgenEvent* event)
{              
    if (event->OptionIsSet(MgenEvent::TXBUFFER))
    {
        if (!SetTxBufferSize(event->GetTxBuffer()))
          DMSG(0, "MgenFlow::Update() error setting socket tx buffer\n");    
    }
    if (event->OptionIsSet(MgenEvent::BROADCAST))
    {
        if (!SetBroadcast(event->GetBroadcast()))
          DMSG(0, "MgenFlow::Update() error setting socket BROADCAST value\n"); 
    }
    if (event->OptionIsSet(MgenEvent::TOS))
    {
        if (!SetTOS(event->GetTOS()))
          DMSG(0, "MgenFlow::Update() error setting socket TOS value\n"); 
    }
    if (event->OptionIsSet(MgenEvent::DF))
    {
        if (!SetDF(event->GetDF()))
            DMSG(0,"MgenFlow::Update() error setting DF value\n");
    }
    if (event->OptionIsSet(MgenEvent::TTL))
    {
        if (event->GetDstAddr().IsMulticast())
        {
            if (!SetMulticastTTL(event->GetTTL()))
                DMSG(0, "MgenFlow::Update() error setting socket multicast TTL\n");    
        }
        else
        {
            if (!SetUnicastTTL(event->GetTTL()))
                DMSG(0, "MgenFlow::Update() error setting socket unicast TTL\n");    
        }
    }
    if (event->OptionIsSet(MgenEvent::INTERFACE) && GetProtocol() == UDP)
    {
        if (!static_cast<MgenUdpTransport*>(this)->SetMulticastInterface(event->GetInterface()))
          DMSG(0, "MgenFlow::Update() error setting socket multicast interface\n");    
    }

} // end MgenSocketTransport::SetEventOptions

bool MgenSocketTransport::SetMulticastTTL(unsigned char ttlValue)
{

    multicast_ttl = ttlValue;
    if (protocol == TCP)
      return true;
    
    if (socket.IsOpen())
    {
        if (!socket.SetTTL(ttlValue)) 
          return false;
    }
    return true;
}  // end MgenSocketTransport::SetTTL()

bool MgenSocketTransport::SetUnicastTTL(unsigned char ttlValue)
{

    unicast_ttl = ttlValue;
    if (protocol == TCP)
      return true;
    
    if (socket.IsOpen())
    {
        if (!socket.SetUnicastTTL(ttlValue)) 
          return false;
    }
    return true;
}  // end MgenSocketTransport::SetTTL()

bool MgenSocketTransport::SetDF(FragmentationStatus dfValue)
{
    df = dfValue;
    if (socket.IsOpen())
    {
        if (!socket.SetFragmentation(dfValue))
            return false;
    }
    return true;
} // MgenSocketTransport::SetDF()

bool MgenSocketTransport::SetTOS(unsigned char tosValue)
{
   if (socket.IsOpen())
   {
       if (!socket.SetTOS(tosValue)) 
           return false;
   }
   tos = tosValue;
   return true;
}  // end MgenSocketTransport::SetTOS()


bool MgenSocketTransport::SetBroadcast(bool broadcastValue)
{
    if (socket.IsOpen())
    {
        if (!socket.SetBroadcast(broadcastValue))
        {
            return false;    
        } 
    }
    broadcast = broadcastValue;
    return true;
}  // end MgenSocketTransport::SetBroadcast()

bool MgenSocketTransport::SetTxBufferSize(unsigned int bufferSize)
{
    if (socket.IsOpen())
    {
        if (!socket.SetTxBufferSize(bufferSize))
        {
            return false;    
        } 
    }
    tx_buffer = bufferSize;
    return true;
}  // end MgenSocketTransport::SetTxBufferSize()

bool MgenSocketTransport::SetRxBufferSize(unsigned int bufferSize)
{
    if (socket.IsOpen())
    {
        if (!socket.SetRxBufferSize(bufferSize)) 
          return false; 
    }
    rx_buffer = bufferSize;
    return true;
}  // end MgenSocketTransport::SetRxBufferSize()



bool MgenSocketTransport::Open(ProtoAddress::Type addrType, bool bindOnOpen)
{
    if (socket.IsOpen())
    {        
        if (socket.GetAddressType() != addrType)
            DMSG(0, "MgenTransport::Open() Warning: socket address type mismatch\n");

        if (bindOnOpen && !socket.IsBound())
        {
            if (!socket.Bind(srcPort))
            {
                DMSG(0, "MgenTransport::Open() socket bind error\n");
                return false;
            }
         }
    }
    else
    {
        if (!socket.Open(srcPort, addrType, false))
        {
            // SetLoopback asserts that the socket is open
            //        socket.SetLoopback(false);  //  by default
            DMSG(0, "MgenTransport::Open() Error: socket open error %s srcPort %d\n",GetErrorString(),srcPort);
            return false;
        }
        else
        {
            // We want to set socket resuse so multiple processes
            // can listen to a common multicast group.  Note that
            // kernel socket selection for reused unicast sockets 
            // is undefined.
            
            if (mgen.GetReuse())
                socket.SetReuse(true);
            if (bindOnOpen)
                socket.Bind(srcPort);
        }
    }
    
    // Reset src port in case it was os generated 
    srcPort = GetSocketPort();
    
    if (tx_buffer)
      socket.SetTxBufferSize(tx_buffer);
    
    if (rx_buffer)
      socket.SetRxBufferSize(rx_buffer);
    
    if (tos)
      socket.SetTOS(tos);
    
    if (socket.GetProtocol() != ProtoSocket::TCP && multicast_ttl >= 0)
      socket.SetTTL(multicast_ttl);

    if (unicast_ttl >= 0)
      socket.SetUnicastTTL(unicast_ttl);
    
    if (df != DF_DEFAULT)
        socket.SetFragmentation(df);

    if ('\0' != interface_name[0])
      socket.SetMulticastInterface(interface_name);

    reference_count++;
    return true;
    
}  // end MgenSocketTransport::Open()

bool MgenSocketTransport::Listen(UINT16 port,ProtoAddress::Type addrType, bool bindOnOpen)
{
    if (IsOpen())
    {
        if (bindOnOpen && !socket.IsBound())
        {
            if (!socket.Bind(port))
            {
                DMSG(0,"MgenTransportList;:MgenSocketTransport::Listen() Error: socket bind error on port %hu\n",port);
                return false; 
            }
        }
        else
        {
            reference_count++;
        }
    }
    // Open increments reference_count initially
    else if (!Open(addrType, bindOnOpen))
    {
        DMSG(0,"MgenTransprotList::MgenSocketTransport::Listen() Error: socket open error on port %hu\n",srcPort);
        return false;
    }
    return true;
} // end MgenSocketTransport::Listen


void MgenSocketTransport::Close()
{
    if (IsOpen() && reference_count == 0)
    {
        PLOG(PL_WARN,"MgenSocketTransport::Close() attempting to close a socket with no references.");
        return;
    }

    if (reference_count)
    {
        reference_count--;
        
        if (!reference_count) 
        {
            StopOutputNotification(); 
            StopInputNotification();
            socket.Close();
        }
    }
}  // end MgenSocketTransport::Close()


MgenUdpTransport::MgenUdpTransport(Mgen& theMgen,
                                   Protocol theProtocol,
                                   UINT16        thePort)
  : MgenSocketTransport(theMgen,theProtocol,thePort),
    group_count(0),connect(false)
{
    socket.SetListener(this,&MgenUdpTransport::OnEvent);
}


MgenUdpTransport::MgenUdpTransport(Mgen& theMgen,
                                   Protocol theProtocol,
                                   UINT16        thePort,
                                   const ProtoAddress&        theDstAddress)
  : MgenSocketTransport(theMgen,theProtocol,thePort,theDstAddress),
    group_count(0),connect(false)
{
    socket.SetListener(this,&MgenUdpTransport::OnEvent);

    // If the dstAddress is set, we're a "connected" udp socket
    // otherwise we don't have a dst address associated with the
    // transport - the flow keeps track of that
    if (theDstAddress.IsValid())
      connect = true; 
}

MgenUdpTransport::~MgenUdpTransport()
{

}

bool MgenUdpTransport::SetMulticastInterface(const char* interfaceName)
{
    if (interfaceName)
    {
        if (socket.IsOpen())
        {
            if (!socket.SetMulticastInterface(interfaceName))
                return false;
        }
        strncpy(interface_name, interfaceName, 16);
    }
    else
    {
        interface_name[0] = '\0';
    }
    return true;
}  // end MgenUdpTransport::SetMulticastInterface()

bool MgenUdpTransport::Open(ProtoAddress::Type addrType, bool bindOnOpen)
{
    if (MgenSocketTransport::Open(addrType,bindOnOpen))
    {
        if (connect && !socket.Connect(dstAddress))
        {
            DMSG(0,"MgenUdpTransport::Open() Error: Failed to connect udp socket.\n");
            return false;
        }
        else
            return true;
    }

    DMSG(0,"MgenUdpTransport::Open() Error: Failed to open udp socket.\n");
    connect = false;  // so the OFF event does not get logged
                      // if we ever start logging udp off events
    return false; 

} // end 

/**
 * We make sure to bind sockets with non-zero port number for WIN32
 * "dummy" sockets created to allow large group number joins on Unix
 *  have zero port number and are left unbound
 */
bool MgenUdpTransport::JoinGroup(const ProtoAddress& groupAddress, 
				                 const ProtoAddress& sourceAddress,
				                 const char*         interfaceName)
{
    bool result;
    //char group_interface_name[16];
    //group_interface_name[0] = '\0';
    //if (interfaceName)
        //strncpy(group_interface_name,interfaceName,16);
#ifdef MACOSX
    //else // use the default interface if not provided - interface required on osx for group joins
        //strncpy(group_interface_name, interface_name, 16);
#endif
    
    if (Open(groupAddress.GetType(), (0 != srcPort)))
    {
#ifdef _PROTOSOCKET_IGMPV3_SSM
        //If source address is valid, it means its a SSM request 
        if ( sourceAddress.IsValid() )
        {
            result = socket.JoinGroup(groupAddress, interfaceName, sourceAddress.IsValid() ? &sourceAddress : NULL);
        }
        else
#endif // _PROTOSOCKET_IGMPV3_SSM
        {
            result = socket.JoinGroup(groupAddress, interfaceName);
        }
        if (result)
	    {
            socket.SetLoopback(true);
            group_count++;
            return true;
	    }
        else
        {
            Close();  // decrement reference count
            DMSG(0, "MgenUdpTransport::JoinGroup() Error: socket join error\n");
            return false;
	    }
    }
    else
    {
        DMSG(0, "MgenTransport::JoinGroup() Error: socket open error\n");
        return false;   
    }
}  // end MgenUdpTransport::JoinGroup()

bool MgenUdpTransport::LeaveGroup(const ProtoAddress& groupAddress, 
                                  const ProtoAddress& sourceAddress,
                                  const char*         interfaceName)
{
    bool result;
#ifdef _PROTOSOCKET_IGMPV3_SSM
    /* If source address is valid, it means its a SSM request */
    if ( sourceAddress.IsValid() )
    {
        result = socket.LeaveGroup(groupAddress, interfaceName, sourceAddress.IsValid() ? &sourceAddress : NULL);
    }
    else
#endif // _PROTOSOCKET_IGMPV3_SSM
    {
        result = socket.LeaveGroup(groupAddress, interfaceName);
    }
    if ( result )
    {
        ASSERT(group_count);
        group_count--;
        Close();  // decrements reference_count, closes socket as needed
        return true;
    }
    else
    {
        return false;
    }
}  // end MgenUdpTransport::LeaveGroup()

// Receive and log a UDP packet
void MgenUdpTransport::OnEvent(ProtoSocket& theSocket, ProtoSocket::Event theEvent)
{
    switch (theEvent)
    {
        case ProtoSocket::RECV:
        {
            UINT32 alignedBuffer[MAX_SIZE/4];
            char* buffer = (char*)alignedBuffer;
            unsigned int len = MAX_SIZE;
            ProtoAddress srcAddr;
            while (theSocket.RecvFrom((char*)buffer, len, srcAddr))
            {
                if (len == 0) break;
                if (mgen.GetLogFile() || mgen.ComputeAnalytics())
                {
                    struct timeval currentTime;
                    ProtoSystemTime(currentTime);
                    MgenMsg theMsg;
                    // the socket recvFrom gives us our srcAddr
                    theMsg.SetSrcAddr(srcAddr);
                    if (theMsg.Unpack(alignedBuffer, len, mgen.GetChecksumForce(), mgen.GetLogData()))
                    {
                        if (mgen.GetChecksumForce() || theMsg.FlagIsSet(MgenMsg::CHECKSUM))
                        {
                            UINT32 checksum = 0;
                            theMsg.ComputeCRC32(checksum,(unsigned char*)buffer,len - 4);
                            checksum = (checksum ^ theMsg.CRC32_XOROT);
                            UINT32 checksumPosition = len - 4;
                            UINT32 recvdChecksum;
                            memcpy(&recvdChecksum,buffer+checksumPosition,4);
                            recvdChecksum = ntohl(recvdChecksum);

                            if (checksum != recvdChecksum)
                            {
                                DMSG(0, "MgenUdpTransport::OnEvent() error: checksum failure\n");
                                theMsg.SetChecksumError();
                            }
                        }
                        if (theMsg.GetError())
                        {
                            if (mgen.GetLogFile())
                                LogEvent(RERR_EVENT, &theMsg, currentTime);
                        }
                        else 
                        {
                            ProcessRecvMessage(theMsg, ProtoTime(currentTime));
                            if (mgen.ComputeAnalytics())
                                mgen.UpdateRecvAnalytics(currentTime, &theMsg, UDP);
                            if (mgen.GetLogFile())
                                LogEvent(RECV_EVENT, &theMsg, currentTime, alignedBuffer);
                        }
                    }
                    else 
                    {
                        if (mgen.GetLogFile())
                            LogEvent(RERR_EVENT, &theMsg, currentTime);
                    }
                }  // end if (NULL != mgen.GetLogFile())
                len = MAX_SIZE;
            }  // end while(theSocket.RecvFrom())
            break;
        }  // end case ProtoSocket::RECV
        case ProtoSocket::SEND:
        {
            SendPendingMessage();
            break;
        }
        default:
            DMSG(0, "MgenUdpTransport::OnEvent() unexpected event type: %d\n", theEvent);
            break;
    }  // end switch(theEvent)
}  // end MgenUdpTransport::OnEvent()

MessageStatus MgenUdpTransport::SendMessage(MgenMsg& theMsg, const ProtoAddress& dstAddr) 
{
    
    // Udp packets are single shot and larger than
    // tcp fragments so use our own tx variables
    UINT32 txChecksum = 0;
    theMsg.SetFlag(MgenMsg::LAST_BUFFER);

    //struct timeval currentTime;
    //ProtoSystemTime(currentTime);
    //theMsg.SetTxTime(currentTime);
    
    UINT32 txBuffer[MAX_SIZE/4 + 1];
    

    unsigned int len = theMsg.Pack(txBuffer, theMsg.GetMsgLen(),mgen.GetChecksumEnable(),txChecksum);
    if (len == 0) 
      return MSG_SEND_FAILED; // no room
    
    if (mgen.GetChecksumEnable() && theMsg.FlagIsSet(MgenMsg::CHECKSUM)) 
        theMsg.WriteChecksum(txChecksum,(unsigned char*)txBuffer,(UINT32)len);

    bool result = socket.SendTo((char*)txBuffer,len,dstAddr);
    
    // Note on BSD systems (incl. Mac OSX) UDP sockets don't really block.
    // On some BSD systems, an ENOBUFS will occur but OSX always acts like the 
    // packet was sent ... so somewhere in the MGEN code we need to handle
    // OSX differently ... Probably the best strategy would be to always go
    // back to select() call ... (i.e. ProtoDispatcher::Wait())

    // If result is true but numBytes == 0 
    // we had an EWOULDBLOCK condition
    if (result && len == 0)
    {
      return MSG_SEND_BLOCKED;
    }

    // We had some other socket failure
    if (!result)
      {
#ifndef _WIN32_WCE
	  if ((EAGAIN != errno) && (ENOBUFS != errno))
	    DMSG(PL_WARN,"MgenUdpTransport::SendMessage() socket.SendTo() error: %s\n", GetErrorString());
	  else
#endif // !_WIN32_WCE
	    DMSG(PL_WARN,"MgenUdpTransport::SendMessage() socket.SendTo() error: %s\n", GetErrorString());
      return MSG_SEND_FAILED;
      }

    LogEvent(SEND_EVENT, &theMsg,theMsg.GetTxTime(), txBuffer);
    return MSG_SEND_OK;

} // end MgenUdpTransport::SendMessage

bool MgenUdpTransport::Listen(UINT16 port,ProtoAddress::Type addrType, bool bindOnOpen)
{
    if (!MgenSocketTransport::Listen(port,addrType,bindOnOpen))
      return false;

    return StartInputNotification();

} // end MgenUdpTransport::Listen
MgenTcpTransport::MgenTcpTransport(Mgen& theMgen,
                                   Protocol theProtocol,
                                   UINT16 thePort,
                                   const ProtoAddress& theDstAddress)
  : MgenSocketTransport(theMgen,theProtocol,thePort,theDstAddress),
    is_client(true),
    tx_msg(),
    tx_buffer_index(0),tx_buffer_pending(0),
    tx_msg_offset(0),tx_fragment_pending(0),tx_checksum(0),
    rx_msg(), rx_buffer_index(0),
    rx_fragment_pending(0),rx_msg_index(0),
    rx_checksum(0),
    retry_count(theMgen.GetDefaultRetryCount()),
    retry_delay(theMgen.GetDefaultRetryDelay())
{
  tx_msg_buffer[0] = '\0';
  rx_msg_buffer[0] = '\0';
  rx_checksum_buffer[0] = '\0';

  tx_msg.SetProtocol(TCP);

  socket.SetListener(this,&MgenTcpTransport::OnEvent);

}

MgenTcpTransport::~MgenTcpTransport()
{

}

void MgenTcpTransport::SetEventOptions(const MgenEvent* event)
{
    MgenSocketTransport::SetEventOptions(event);

    if (event->OptionIsSet(MgenEvent::RETRY))
    {
        SetRetryCount(event->GetRetryCount());
        if (event->GetRetryDelay() != 0)
        {
            SetRetryDelay(event->GetRetryDelay());
        }
    }
}

void MgenTcpTransport::ScheduleReconnect(ProtoSocket& theSocket)
{
    if (GetRetryCount() == 0)
    {
        return;
    }
    
    if (GetRetryCount() > 0)
    {
        SetRetryCount(GetRetryCount() -1);
    } 
    // Pause and schedule resume events for all flows
    // using this transport
    MgenFlow * next = mgen.GetFlowList().Head();
    char cmd [512];

    while (next)
    {
        if (next->GetFlowTransport() && next->GetFlowTransport()->OwnsSocket(theSocket))
        {
            // First pause the flow
            sprintf(cmd, "MOD %u PAUSE", next->GetFlowId());
            mgen.ParseEvent(cmd, 0, true);

            // Then resume after delay
            sprintf(cmd, "%u MOD %u RECONNECT", GetRetryDelay(), next->GetFlowId());
            mgen.ParseEvent(cmd, 0, true);
            
        }
        next = next->Next();
    }

} // MgenTcpTransport::ScheduleReconnect


void MgenTcpTransport::OnEvent(ProtoSocket& theSocket,ProtoSocket::Event theEvent)
{
    switch (theEvent)
    {
    case ProtoSocket::SEND:
      {
          // Send anything pending in the transport buffer 
          // until we have transmission failure.  (Note that 
          // tx_buffer_pending may be zero if we haven't connected
          // yet so we let SendPendingMessages will kick things off 
          // for us.
          while (1)
          {
              unsigned int numBytes = tx_buffer_pending; 
              if (theSocket.Send(((char*)tx_msg_buffer) + tx_buffer_index, numBytes))
              {
                  // if we had an error, let socket notification
                  // tell us when to try again.
                  if (tx_buffer_pending != 0 && numBytes == 0) 
                  {
                      StartOutputNotification(); 
                      return;
                  }

                  // Get next buffer to send, if all done, send any pending
                  // messages that have accumulated. 
                  if (!GetNextTxBuffer(numBytes))
                  {
                      StopOutputNotification();
                      break;
                  }
              } // Some other socket failure!
              else 
              {
                  break;
              }
          }
          SendPendingMessage(); 

          break;
      }
      
    case ProtoSocket::RECV:
      
      {
          while (1)
          {

              unsigned int bufferIndex = ((TX_BUFFER_SIZE + rx_msg_index) % TX_BUFFER_SIZE);
              unsigned int numBytes = GetRxNumBytes(bufferIndex);
	          UINT32 buffer[TX_BUFFER_SIZE/4 + 1];
              if (IsConnected() && numBytes && theSocket.Recv(((char*)buffer)+bufferIndex, numBytes))
              {
                  if (0 != numBytes)
                  {
                    OnRecvMsg(numBytes, bufferIndex, buffer);
                  }
                  else
                  {
#ifdef WIN32
                      // If we've gotten a FD_CLOSE event and havn't
                      // read new data, we're done.
                      if (socket.IsClosing())
                      {
                         ShutdownTransport(OFF_EVENT);
                         socket.SetClosing(false);
                      }
#endif // WIN32
                      break;   
                  }
              } 
              else
              {
                break;
              }
          } // end while (1)
          
          break;
      }
      
    case ProtoSocket::ACCEPT:
      {
          // Get a transport with a closed socket since we need to have 
          // transport with a different socket to hold the accepted connection

          MgenTransport* newMgenTransport = mgen.GetMgenTransport(TCP,theSocket.GetPort(),theSocket.GetDestination(),GetInterface(),true); 
          
          if (!newMgenTransport) 
            return;
          
          MgenTcpTransport* newTcpTransport = static_cast<MgenTcpTransport*>(newMgenTransport);
          
          if (newTcpTransport && newTcpTransport->Accept(theSocket))
          {
              struct timeval currentTime;
              ProtoSystemTime(currentTime);
              newTcpTransport->LogEvent(ACCEPT_EVENT,&tx_msg,currentTime);
          }
          else 
          {
              DMSG(0, "Mgen::OnTcpSocketEvent Error: connection not accepted.\n");
          }
          break;
      }
    case ProtoSocket::CONNECT:
      {
          // log connection events for all flows feeding the transport
          MgenFlow* next = mgen.GetFlowList().Head();
          struct timeval currentTime;
          ProtoSystemTime(currentTime);
          while (next)
          {
              if (next->GetFlowTransport() && next->GetFlowTransport()->OwnsSocket(theSocket))
              {
                  tx_msg.SetFlowId(next->GetFlowId());
                  tx_msg.GetSrcAddr().SetPort(next->GetFlowTransport()->GetSrcPort());
                  LogEvent(CONNECT_EVENT,&tx_msg,currentTime);
              }
              next = next->Next();
          }
          break;
      }
    case ProtoSocket::ERROR_:      
      {
          struct timeval currentTime;
          ProtoSystemTime(currentTime);

          if (GetRetryCount() != 0)
          {
              ScheduleReconnect(theSocket);
          }
          else
          {
              ShutdownTransport(DISCONNECT_EVENT);
          }
          break;
          
      }
    case ProtoSocket::DISCONNECT:
      {
          if (GetRetryCount() != 0)
          {
              ScheduleReconnect(theSocket);
          }
          else
          {
              ShutdownTransport(OFF_EVENT);
          }
          break;
      }
    default:
      DMSG(0, "Mgen::OnTcpSocketEvent() unexpected event type\n");
      break;
    }
    
}  // end MgenTcpTransport::OnEvent()

/**
 * MgenTcpTransport::SendMessage is called once for each
 * mgen message sent (which may be sent in multiple mgen
 * message fragments if the mgen message is > MAX_FRAG_SIZE).
 *
 * Mgen message segments and fragments are sent as quickly
 * as the possible as the transport indicates socket readiness.
 * (e.g. a SEND_EVENT is received by the transport's OnEvent
 * function.)
 */

MessageStatus MgenTcpTransport::SendMessage(MgenMsg& theMsg, const ProtoAddress& dst_addr) 
{        
    // But not if the transport is transmitting anything...
    if (tx_msg_offset != 0)  
      return MSG_SEND_BLOCKED; 

    // Set the transport's tx_msg to the loaded msg
    tx_msg = theMsg; 

    // Gets the size of the first fragment that should be
    // sent and packs the message
    tx_fragment_pending = GetNextTxFragment();
    
    if (!tx_fragment_pending || !tx_buffer_pending) 
    {
        DMSG(0,"SendTcpMessage Error: No fragment pending!\n");
        return MSG_SEND_FAILED;
    }
    
    //  Send message, checking for error (log only on success)
    while (1)
    {
        unsigned int numBytes = tx_buffer_pending;   
        if (socket.IsConnected()
            &&
            socket.Send(((char*)tx_msg_buffer)+tx_buffer_index,numBytes))
        {
            // If we had an error, let socket notification tell 
            // us when to try again...
            if (tx_buffer_pending != 0 && numBytes == 0)
            {
                StartOutputNotification();
                return MSG_SEND_BLOCKED;
            }
            
            // Otherwise keep track of what we've sent
            tx_buffer_index += numBytes;
            tx_buffer_pending -= numBytes;
            tx_fragment_pending -= numBytes;
            
            // Still more to send in buffer.
            if (tx_buffer_pending)
                continue;
        
            // We sent the whole buffer.  Get the next part of
            // the fragment
            if (0 == tx_buffer_pending && tx_fragment_pending)
            {
                SetupNextTxBuffer();
                if (tx_buffer_pending) continue;
            }
 
            // See if there are any more mgen msg fragments to send
            if (tx_buffer_pending == 0 && tx_fragment_pending == 0)
            {
                tx_fragment_pending = GetNextTxFragment();
                if (tx_buffer_pending) continue;
            }

            // We check tx_msg_offset because in windows we get here
            // before the mgen flow transmission timer has started
            if (!tx_fragment_pending && tx_msg_offset > 0)
            {
                // else we've sent everything, clear state and move on
                UINT32 txChecksum = 0;
                UINT32 txBuffer[MAX_SIZE/4 + 1];     
                // Use the time of the first message fragment sent that
                // we squirreled away earlier.  Binary logging uses the
                // tx_time in the message buffer, logfile logging uses
                // the tx_time passed in.  Should be cleaned up.
                tx_msg.SetTxTime(tx_time);
                tx_msg.Pack(txBuffer, MAX_SIZE, mgen.GetChecksumEnable(), txChecksum);
                LogEvent(SEND_EVENT ,&tx_msg,tx_time, txBuffer);
                ResetTxMsgState();
                StopOutputNotification(); // ljt 0516 - check if we need this?
                // we may still have pending stuff!
                return MSG_SEND_OK;
            }
            continue;            
        } // other socket failure
        else 
        {
          DMSG(PL_ERROR, "MgenTcpTransport::SendMessage() socket.Send() error: %s\n", GetErrorString());   

          //            DMSG(0,"MgenTcpTransport:SendMessage() error writing to output! \n");
          ResetTxMsgState();
          StopOutputNotification(); // ljt 0516 delete me?
          return MSG_SEND_FAILED;
        }
    }    
    return MSG_SEND_FAILED; // shouldn't fall thru!
}  // end MgenTcpTransport::SendMessage()

/**
 * We received either an error on the socket or a disconnect
 * event.  Log event if we are a server, or shutdown all flows
 * feeding the transport if we are a client.
 */
void MgenTcpTransport::ShutdownTransport(LogEventType eventType)
{
    if (!is_client)
    {
        // tx_msg isn't used in the log event
        struct timeval currentTime;
        ProtoSystemTime(currentTime);
        tx_msg.GetSrcAddr().SetPort(GetSrcPort());
        LogEvent(eventType,&tx_msg,currentTime);
        Shutdown(); 
        if (IsOpen()) Close();
    }
    else 
    {
        // log disconnect events & turn off timers for all 
        // flows feeding the transport
        MgenFlow* next = mgen.GetFlowList().Head();
        struct timeval currentTime;
        ProtoSystemTime(currentTime);
        while (next)
        {
            if (next->GetFlowTransport() 
                && next->GetFlowTransport()->OwnsSocket(socket))
            {
                tx_msg.SetFlowId(next->GetFlowId());

                // Did we turn off the flow ourselves?
                if (next->OffPending())
                  LogEvent(OFF_EVENT,&tx_msg,currentTime);
                else
                  LogEvent(eventType,&tx_msg,currentTime);
                
                next->OffPending(false);                
                if (next->GetTxTimer().IsActive()) next->GetTxTimer().Deactivate();                
                // Inform rapr that the socket disconnected indepent
                // of an OFF event so it can reuse the flow id ljt?
        
                /* The OFF event should do this - check queuing
                   
                if (mgen.GetController())
                {
                char buffer [512];
                sprintf(buffer,"offevent flow>%lu",next->GetFlowId());
                unsigned int len = strlen(buffer);
                mgen.GetController()->OnOffEvent(buffer,len);
                }
                */
                if (next->GetFlowTransport())
                {
                    
                    if (next->GetFlowTransport()->IsOpen())
                      next->GetFlowTransport()->Close();
                    // We now wait for the dispatcher to notice that
                    // the socket is completely closed so set the flow's
                    // transport to NULL here...
                    next->SetFlowTransport(NULL); // ljt
                }
                
            }
            
            next = next->Next();
        }
    }
    ResetTxMsgState();
    ResetRxMsgState();
    socket.Disconnect();
    socket.StopOutputNotification();
    
} // MgenTcpTransport::ShutdownTransport()

void MgenTcpTransport::ResetTxMsgState()
{  
    tx_msg_buffer[0] = '\0';
    tx_buffer_index = tx_buffer_pending = tx_msg_offset = tx_fragment_pending = tx_checksum = 0;
    tx_msg.SetMgenMsgLen(0);
    tx_msg.SetMsgLen(0);
    tx_msg.SetFlowId(0);
    tx_msg.SetSeqNum(0);
    tx_msg.ClearError();   
    tx_msg.SetFlag(MgenMsg::CLEAR);

} // end MgenTcpTransport::ResetTxMsgState

void MgenTcpTransport::ResetRxMsgState()
{
    rx_msg_buffer[0] = '\0';
    memset(rx_checksum_buffer,0,4);
    rx_checksum_buffer[0] = '\0';  
    rx_msg_index = rx_checksum = rx_fragment_pending = rx_buffer_index = 0;
    rx_msg.SetMgenMsgLen(0);
    rx_msg.SetMsgLen(0);
    rx_msg.SetFlowId(0);
    rx_msg.SetSeqNum(0);
    rx_msg.ClearError();   
    rx_msg.SetFlag(MgenMsg::CLEAR);

} // MgenTcpTransport::ResetRxMsgState

void MgenTcpTransport::CalcRxChecksum(const char* buffer,unsigned int bufferIndex,unsigned int numBytes)
{
    if ((rx_msg_index == rx_msg.GetMsgLen()) && 
        (rx_msg_index >= MSG_LEN_SIZE)) // and we didn't just get msg_len!
    {  
        
        // We have the whole message
        if (mgen.GetChecksumForce() || (rx_msg.FlagIsSet(MgenMsg::CHECKSUM))) 
        {
            if (numBytes > 4)
            {
                unsigned int checksumPosition = (numBytes + bufferIndex) - 4;
                memcpy(rx_checksum_buffer,buffer+checksumPosition,4);
                rx_msg.ComputeCRC32(rx_checksum,(unsigned char*)buffer+bufferIndex,numBytes - 4);	
            }
            else 
            {
                // else our read was split while getting the
                // checksum - copy the rest of it
                memcpy(rx_checksum_buffer+rx_buffer_index,buffer+bufferIndex,numBytes);
            }
            rx_checksum = (rx_checksum ^ rx_msg.CRC32_XOROT);
            
            UINT32 recvdChecksum;
            memcpy(&recvdChecksum,rx_checksum_buffer,4);
            recvdChecksum = ntohl(recvdChecksum);
            
            if (rx_checksum != recvdChecksum)
            {
                DMSG(0, "MgenMsg::Unpack() error: checksum failure\n");
                rx_msg.SetChecksumError();
                rx_msg.SetFlag(MgenMsg::CHECKSUM_ERROR);
            }
        }
    } else {
        unsigned int msgBytes = numBytes;
        // are we reading part of a checksum?
        if (rx_msg_index + 4 > rx_msg.GetMsgLen() && 
            rx_msg.GetMsgLen() != 0)
        {
            unsigned int checksumBytes = (rx_msg_index + 4) - rx_msg.GetMsgLen();
            msgBytes = numBytes - checksumBytes;
            memcpy(rx_checksum_buffer,buffer+(bufferIndex + msgBytes),checksumBytes);
            rx_buffer_index += checksumBytes; 
        }
        rx_msg.ComputeCRC32(rx_checksum,(unsigned char*)buffer+bufferIndex,msgBytes);
    }
    
} // MgenTcpTransport::CalcRxChecksum

bool MgenTcpTransport::Reconnect(ProtoAddress::Type addrType)
{
    if (socket.IsOpen())
    {
        if (IsClient())
        
        {
            if (socket.GetAddressType() != addrType)
                DMSG(0, "MgenTransport::Reconnect() Warning: socket address type mismatch\n");

            if (!socket.Bind(srcPort))
            {
                DMSG(0, "MgenTransport::Reconnect() socket bind error\n");
                return false;
            }
            // Reset src port in case it was os generated
            srcPort = GetSocketPort();
            
            if (!socket.Connect(dstAddress))
            {
                DMSG(0,"MgenTcpTransport::Reconnect() Error: Failed to connect tcp socket.\n");
                return false;
                
            }
#ifdef WIN32
			OnEvent(socket,ProtoSocket::CONNECT);
#endif
        }
    }
    return true;        
} // MgenTcpTransport::Reconnect

bool MgenTcpTransport::Open(ProtoAddress::Type addrType, bool bindOnOpen)
{
    if (socket.IsConnecting() || socket.IsConnected()) 
    {
        reference_count++;
        return true; // attempt to open socket made already by another flow ...
    }
    if (MgenSocketTransport::Open(addrType,bindOnOpen))
    {
        if (IsClient())
        {
            if (!socket.Connect(dstAddress))
            {
                DMSG(0,"MgenTcpTransport::Open() Error: Failed to connect tcp socket.\n");
                return false;
            }
#ifdef WIN32
			OnEvent(socket,ProtoSocket::CONNECT);
#endif
            return true;
        }
        return true;
    }
    
    DMSG(0,"MgenTcpTransport::Open() Error: Failed to open tcp socket.\n");
    return false;
}

bool MgenTcpTransport::Listen(UINT16 port,ProtoAddress::Type addrType, bool bindOnOpen)
{

    IsClient(false);
    
    if (!MgenSocketTransport::Listen(port,addrType,bindOnOpen))
      return false;

    if (IsOpen() && socket.IsBound())
    {
        if (!socket.IsConnected())
        {
            if (!socket.Listen(port))
            {
                DMSG(0,"MgenTcpTransport::Listen() Error: socket open error on port %hu\n",port);
                return false;
            }
        }
        else 
        {
            DMSG(0,"MgenTcpTransport::Listen() Error: socket already connected.\n");
            return false;
        }        
    }
    return true;
} // end MgenTcpTransport::Listen

bool MgenTcpTransport::Accept(ProtoSocket& theSocket) 
{
    if (ProtoSocket::LISTENING == theSocket.GetState())
    {
        if (!theSocket.Accept(&socket)) 
        {
            DMSG(0,"MgenTcpTransport::Accept() error accepting socket connection.\n");
            return false;
        }
        if (tx_buffer) socket.SetTxBufferSize(tx_buffer);
        if (rx_buffer) socket.SetRxBufferSize(rx_buffer);
        if (tos) socket.SetTOS(tos);
        if (df != DF_DEFAULT) socket.SetFragmentation(df);
        // no ttl or multicast interface for tcp sockets   
        reference_count++; 
        SetDstAddr(socket.GetDestination()); 
        IsClient(false);
        // Set listener for the new socket
        socket.SetListener(this,&MgenTcpTransport::OnEvent);
        // Get srcAddr for logging 
        rx_msg.SetSrcAddr(socket.GetDestination());
        return true;
    }
    else
    {
        DMSG(0, "MgenTcpTransport::Accept() error: socket is not listening\n");
        return false;
    }
} // end MgenTcpTransport::Accept()

unsigned int MgenTcpTransport::GetRxNumBytes(unsigned int bufferIndex)
{
    unsigned int numBytes = 0;
    if (rx_msg.GetMsgLen() == 0)
    {
        // Get message length only, as we haven't already
        
         numBytes = MSG_LEN_SIZE;
    }
    else 
    {
        // attempt to get what's left
        numBytes = rx_msg.GetMsgLen() - rx_msg_index;
        
        // Trim attempted read size to our buffer limit
	if (numBytes > (TX_BUFFER_SIZE - bufferIndex))
          numBytes = TX_BUFFER_SIZE - bufferIndex;

    }
    return numBytes;
} // MgenTcpTransport::GetRxNumBytes()


void MgenTcpTransport::OnRecvMsg(unsigned int numBytes, unsigned int bufferIndex, const UINT32* buffer)
{
    
    // This unpacks the message into rx_msg if we 
    // have recevied the mgen msg header...
    CopyMsgBuffer(numBytes, bufferIndex, (char*)buffer);
    
    // Get the message size if we haven't already
    if ((0 == rx_msg.GetMsgLen()) && (rx_msg_index >= MSG_LEN_SIZE))
    {
        UINT16 tmp_msg_len;
        memcpy(&tmp_msg_len, buffer, sizeof(UINT16));
        tmp_msg_len = ntohs(tmp_msg_len);
        rx_msg.SetMsgLen(tmp_msg_len);
        rx_fragment_pending = rx_msg.GetMsgLen() - MSG_LEN_SIZE;
        
    } 
    else 
    {
        rx_fragment_pending -= numBytes;
    }
    
    // Sets an error if checksum is not correct
    CalcRxChecksum((char*)buffer, bufferIndex, numBytes);
    
    // Last fragment in message.  (Only log when  we have whole fragment
    if (rx_msg_index == rx_msg.GetMsgLen() && rx_msg.GetMsgLen() != 0)
    {
        struct timeval currentTime;
        ProtoSystemTime(currentTime);
        if (!rx_msg.GetError())
        {
            if (!rx_msg.GetDstAddr().IsValid())
                rx_msg.SetDstAddr(dstAddress);
            ProcessRecvMessage(rx_msg, ProtoTime(currentTime));
            if (mgen.ComputeAnalytics())
                mgen.UpdateRecvAnalytics(currentTime, &rx_msg, TCP);
            if (mgen.GetLogFile())
                LogEvent(RECV_EVENT, &rx_msg, currentTime, rx_msg_buffer);
        }
        else
        {
            if (mgen.GetLogFile())
                LogEvent(RERR_EVENT, &rx_msg, currentTime);
        } 

        bufferIndex = 0;
        ResetRxMsgState();
    }

    // Do we want to log partially received messages?
    // Disconnects received while processing the above
    // recv might result in partial messages...
    
} // end MgenTcpTransport::OnRecvMsg()

bool MgenTcpTransport::GetNextTxBuffer(unsigned int numBytes)
{
    tx_buffer_index += numBytes;
    tx_buffer_pending -= numBytes;
    tx_fragment_pending -= numBytes;

    // If there is more of the mgen message fragment 
    // to send, set up the next buffer
    if (0 == tx_buffer_pending && tx_fragment_pending)
    {
        SetupNextTxBuffer();
    }
    
    // We've sent the whole fragment & its buffers.
    // See if there is are any more mgen msg fragments.
    if (tx_buffer_pending == 0 && tx_fragment_pending == 0)
    {
        tx_fragment_pending = GetNextTxFragment();        
        
        // We check tx_msg_offset because in windows we get here
        // before the mgen flow transmission timer has started

        // ljt 061808  this was on the tcp box but not windows...?
        // figure out if you need this tx_msg_offset == 0 check

        //    if (!tx_fragment_pending && (tx_msg_offset > 0 || tx_msg_offset == 0))
        if (!tx_fragment_pending && tx_msg_offset > 0)
        {
            // Entire message has been sent, log send event
	        // with the time we squirreled away earlier
            UINT32 txChecksum = 0;
            UINT32 txBuffer[MAX_SIZE/4 + 1];    

            // Tell the flow we sent a message
            MgenFlow* theFlow = mgen.FindFlowById(tx_msg.GetFlowId());
            theFlow->UpdateMessagesSent();

            // Use the time of the first message fragment sent that
            // we squirreled away earlier.  Binary logging uses the
            // tx_time in the message buffer, logfile logging uses
            // the tx_time passed in.  Should be cleaned up.
            tx_msg.SetTxTime(tx_time);
            tx_msg.Pack(txBuffer, MAX_SIZE,mgen.GetChecksumEnable(),txChecksum);
            LogEvent(SEND_EVENT,&tx_msg,tx_time,txBuffer); 
            ResetTxMsgState();    
            return false;
        }
        //ljt 033108 - triger pending messages to get sent??
        if (!tx_fragment_pending && tx_msg_offset == 0)
            return false;
    }
    
    return true;
    
} // MgenTcpTransport::GetNextTxBuffer()

void MgenTcpTransport::SetupNextTxBuffer()
{

    // last buffer in fragment with room for checksum?
    if ((mgen.GetChecksumEnable() && (tx_fragment_pending  <= (TX_BUFFER_SIZE - 4)))
        || (!mgen.GetChecksumEnable() && (tx_fragment_pending <= TX_BUFFER_SIZE)))
    {
        tx_buffer_pending = tx_fragment_pending;
        tx_msg.SetFlag(MgenMsg::LAST_BUFFER);

        if (mgen.GetChecksumEnable())
          CalcTxChecksum();

        tx_msg.ClearFlag(MgenMsg::LAST_BUFFER);
            
        if (mgen.GetChecksumEnable()) 
          tx_msg.WriteChecksum(tx_checksum, (unsigned char*)tx_msg_buffer, tx_fragment_pending);

    }
    else 
    {
        // Create room for checksum.

        if (mgen.GetChecksumEnable() && ((tx_fragment_pending - TX_BUFFER_SIZE) < 4))
            tx_buffer_pending = tx_fragment_pending - 4;
        else
            tx_buffer_pending = TX_BUFFER_SIZE;
        
        if (mgen.GetChecksumEnable())
          CalcTxChecksum();
    }
    tx_buffer_index = 0;
    tx_msg_offset += tx_buffer_pending;

} // MgenTcpTransport::SetupNextTxBuffer()

void MgenTcpTransport::CalcTxChecksum()
{
    if (tx_msg.FlagIsSet(MgenMsg::LAST_BUFFER))
    {
        if (tx_buffer_pending < 4) 
        {
            DMSG(0,"MgenMsg::PackBuffer Not enough room for checksum!\n");
            return;
        }
        
        // Don't calc buffer where checksum will be
        tx_msg.ComputeCRC32(tx_checksum,
                            (unsigned char*)tx_msg_buffer,
                            tx_buffer_pending - 4);
    }
    else {
        tx_msg.ComputeCRC32(tx_checksum,
                            (unsigned char*)tx_msg_buffer,
                            tx_buffer_pending);
        
    }

} // MgenTcpTransport::CalcTxChecksum()

UINT16 MgenTcpTransport::GetNextTxFragment()
{
  /** Set tx_time and other state because we're 
   * packing a new fragment of the mgen message
   */

    UINT16 tx_buffer_size = TX_BUFFER_SIZE; 
    tx_msg_buffer[0] = '\0';
    tx_checksum = 0;
    tx_buffer_index = 0;
    
    /** Sets msg_len and segment flags in the fragment for us.
     *  If GetNextTxFragmentSize() returns 0 we have 
     * sent the entire message.
     */
    if (!GetNextTxFragmentSize()) 
       return 0;  // In windows we might not have packed the msg yet!

    /** If this is our first fragment (tx_msg_offset != 0) squirrel 
     * the initial tx_time away so we have it when we log the send
     * message.  This is also the time being logged as time sent
     * in the mgen message payload.  
     */

    //struct timeval currentTime;
    //ProtoSystemTime(currentTime);
    //tx_msg.SetTxTime(currentTime);

    /** tx_msg_offset is the index into the mgen message
     * not the fragment.  */
    if (0 == tx_msg_offset)
      tx_time = tx_msg.GetTxTime();
    

    /* Account for checksum handling and pack the message */
    if (mgen.GetChecksumEnable()) 
    {
        if (((tx_msg.msg_len - TX_BUFFER_SIZE) < 4) && (tx_msg.msg_len != MIN_FRAG_SIZE))
        {
            tx_buffer_size = TX_BUFFER_SIZE - 4;
        }
    }	  
    
    // save room for checksum!
    if (tx_msg.msg_len > TX_BUFFER_SIZE)
	{
        tx_buffer_pending = tx_msg.Pack(tx_msg_buffer, tx_buffer_size, mgen.GetChecksumEnable(), tx_checksum);
        tx_msg_offset += tx_buffer_pending;
	}
    else

      if (tx_msg.msg_len > 0)
       {
          tx_msg.SetFlag(MgenMsg::LAST_BUFFER);
          tx_buffer_pending = tx_msg.Pack(tx_msg_buffer,tx_msg.msg_len,mgen.GetChecksumEnable(),tx_checksum);
          tx_msg_offset += tx_buffer_pending;
          
          if (mgen.GetChecksumEnable() && tx_msg.FlagIsSet(MgenMsg::CHECKSUM))
	      {
              // We're sending the rest of the fragment and
              // had room for the checksum so write it.
              tx_msg.WriteChecksum(tx_checksum,(unsigned char*)tx_msg_buffer,tx_msg.msg_len);
              
	      }
	}
      else
      {
          // no more to send
          tx_buffer_pending = 0;
	  }

    return tx_msg.msg_len;
    
} // MgenTcpTransport::GetNextTxFragment

/**
 * Sets msg_len and message flags in tx_msg
 *
 * tx_msg_offset is the offset into the mgen 
 * message not the mgen message fragment.
 */

UINT16 MgenTcpTransport::GetNextTxFragmentSize()
{
    tx_msg.ClearFlag(MgenMsg::CONTINUES);
    tx_msg.msg_len = 0;

    unsigned int remainingMsgSize = tx_msg.mgen_msg_len - tx_msg_offset;
    if (!remainingMsgSize)
      return 0; // message has been sent

    if (remainingMsgSize > MAX_FRAG_SIZE)
    {
        /// We don't want the last fragment to be less than
        /// the minimum fragment size

        if (remainingMsgSize < (MAX_FRAG_SIZE + MIN_FRAG_SIZE))
          tx_msg.msg_len = MAX_FRAG_SIZE - MIN_FRAG_SIZE;
        else
          tx_msg.msg_len = MAX_FRAG_SIZE;
    }
    else
      tx_msg.msg_len = remainingMsgSize;

    if (tx_msg.mgen_msg_len > MAX_FRAG_SIZE)
    {
        if ((tx_msg.mgen_msg_len - tx_msg_offset) > tx_msg.msg_len)
          tx_msg.SetFlag(MgenMsg::CONTINUES);
        else
          tx_msg.SetFlag(MgenMsg::END_OF_MSG);
    }
    if (tx_msg.msg_len == 0)
      TRACE(" MgenTcpTransport::GetNextTxFragmentSize() tx_msg.msg_len == 0!\n");
    return tx_msg.msg_len;
    
}// MgenTcpTransport::GetNextTxFragmentSize


void MgenTcpTransport::CopyMsgBuffer(unsigned int numBytes,unsigned int bufferIndex,const char* buffer)
{

    // Copy message header to persistant buffer
    if (rx_msg_index < TX_BUFFER_SIZE) 
    {
        
        // If we've received past the buffer size
        // just copy buffer size, else copy it all
        char* rxBuffer = (char*)rx_msg_buffer;
        if (numBytes > (unsigned int)(TX_BUFFER_SIZE - rx_msg_index))
            memcpy(rxBuffer+bufferIndex,buffer+bufferIndex,(TX_BUFFER_SIZE - bufferIndex));
        else
            memcpy(rxBuffer+bufferIndex,buffer+bufferIndex,numBytes);
    }
    
    rx_msg_index += numBytes;
    
    // This could be cleaned up a bit!
    
    if ((((rx_msg.GetMsgLen() - rx_fragment_pending) < TX_BUFFER_SIZE)
         && (rx_msg_index >= TX_BUFFER_SIZE))
        ||
        ((rx_msg.GetMsgLen() == rx_msg_index) 
         && (rx_msg_index <= TX_BUFFER_SIZE)))
    {
        if (mgen.GetLogFile()) 
        {
            if (rx_msg_index <= TX_BUFFER_SIZE) 
              rx_msg.Unpack(rx_msg_buffer, rx_msg_index, mgen.GetChecksumForce(), mgen.GetLogData());
            else
              rx_msg.Unpack(rx_msg_buffer, TX_BUFFER_SIZE, mgen.GetChecksumForce(), mgen.GetLogData());
        }
    }
    
} // MgenTcpTransport::CopyMsgBuffer()

MgenSinkTransport::MgenSinkTransport(Mgen& theMgen,
                                     Protocol theProtocol,
                                     UINT16        thePort,
                                     const ProtoAddress&   theDstAddress)
  : MgenTransport(theMgen,theProtocol,thePort,theDstAddress),
    sink_non_blocking(true),
    is_source(false),
    msg_length(0),msg_index(0)

{
  msg_buffer[0] = '\0';
  path[0] = '\0';
}

MgenSinkTransport::~MgenSinkTransport()
{

}

MgenSinkTransport::MgenSinkTransport(Mgen& theMgen,
                                     Protocol theProtocol)
  : MgenTransport(theMgen,theProtocol),
    sink_non_blocking(true),
    is_source(false),
    msg_length(0),msg_index(0)

{
  msg_buffer[0] = '\0';
  path[0] = '\0';
}

bool MgenSinkTransport::Open(ProtoAddress::Type addrType, bool bindOnOpen)
{
  return true;
} // end MgenSinkTransport::Open



bool MgenSinkTransport::OnOutputReady()
{
    return true;
}  // MgenSinkTransport::OnOutputReady()

bool MgenSinkTransport::Open()
{
  return true;

} // end MgenSinkTransport::Open()
  
bool MgenSinkTransport::OnInputReady()
{
 return true;
} // end MgenSinkTransport::OnInputReady()

void MgenSinkTransport::HandleMgenMessage(UINT32* alignedBuffer, unsigned int len,const ProtoAddress& srcAddr)
{
    MgenMsg theMsg;
    theMsg.SetSrcAddr(srcAddr);
    if (NULL != mgen.GetLogFile())
    {
        struct timeval currentTime;
        ProtoSystemTime(currentTime);
        if (theMsg.Unpack(alignedBuffer,len,mgen.GetChecksumForce(),mgen.GetLogData()))
        {
            if (mgen.GetChecksumForce() || theMsg.FlagIsSet(MgenMsg::CHECKSUM))
            {
                UINT32 checksum = 0;
                theMsg.ComputeCRC32(checksum, (unsigned char*)alignedBuffer, len-4);
                checksum = (checksum ^ theMsg.CRC32_XOROT);
                UINT32 checksumPosition = len-4;
                UINT32 recvdChecksum;
                memcpy(&recvdChecksum, ((char*)alignedBuffer)+checksumPosition ,4);
                recvdChecksum = ntohl(recvdChecksum);
                
                if (checksum != recvdChecksum)
                {
                    DMSG(0,"MgenSinkTransport::HandleMgenMessage() error: checksum failure.\n");
                    theMsg.SetChecksumError();
                }
            }
        }
        if (theMsg.GetError())
        {
            if (mgen.GetLogFile())
                LogEvent(RERR_EVENT,&theMsg,currentTime);
        }
        else 
        {
            ProcessRecvMessage(theMsg, ProtoTime(currentTime));
            if (mgen.ComputeAnalytics())
                mgen.UpdateRecvAnalytics(currentTime, &theMsg, SINK);
            if (mgen.GetLogFile())
                LogEvent(RECV_EVENT, &theMsg, currentTime, alignedBuffer);       
        } 
    }
  
} // end MgenSinkTransport::HandleMgenMessage


void MgenTransport::ProcessRecvMessage(MgenMsg& msg, const ProtoTime& theTime)
{
    // Parse received message payload for any received commands
    if (MgenMsg::MGEN_DATA != msg.GetPayloadType()) return;
    unsigned int bufferLen = msg.GetPayloadLength();
    UINT32* bufferPtr = msg.AccessPayloadData();
    while (bufferLen > 0)
    {
        if (MgenDataItem::DATA_ITEM_FLOW_CMD == MgenDataItem::GetItemType(bufferPtr))
        {
            MgenFlowCommand cmd;
            if (!cmd.InitFromBuffer(bufferPtr, bufferLen))
            {
                PLOG(PL_ERROR, "MgenTransport::ProcessRecvMessage() warning: received invalid MGEN_DATA payload\n");
                break;
            }
            UINT32 maxFlowId = cmd.GetMaxFlowId();
            if (maxFlowId > MgenEvent::FlowStatus::MAX_FLOW)
            {
                PLOG(PL_ERROR, "MgenTransport::ProcessRecvMessage() warning: received flow command out-of-range flowId\n");
                maxFlowId = MgenEvent::FlowStatus::MAX_FLOW;
            }
            for (UINT32 i = 1; i <= maxFlowId; i++)
            {
                MgenFlowCommand::Status status = cmd.GetStatus(i);
                if (MgenFlowCommand::FLOW_UNCHANGED != status)
                    mgen.ProcessFlowCommand(status, i);
            }
            UINT16 cmdLen = cmd.GetLength();
            bufferLen -= cmdLen;
            bufferPtr += cmdLen / sizeof(UINT32);
        }
        else if ((NULL != mgen.GetController()) &&
                 ((UINT8)MgenDataItem::GetItemType(bufferPtr) > 0x0f))
        {
            // Let the MgenController see the received report
            MgenAnalytic::Report report;
            if (report.InitFromBuffer(bufferPtr, bufferLen))
            {
                mgen.GetController()->OnRecvReport(theTime, report, msg.GetSrcAddr(), ProtoTime(msg.GetTxTime()));
                UINT8 reportLen = report.GetLength();
                bufferLen -= reportLen;
                bufferPtr += (reportLen/sizeof(UINT32));
            }
            else
            {
                PLOG(PL_ERROR, "MgenMsg::LogRecvEvent() warning: received invalid REPORT payload\n");
                break;
            }
        }
        else
        {
            // Skip over other items for now 
            MgenDataItem item(bufferPtr, bufferLen);
            UINT16 itemLen = item.GetLength();
            bufferLen -= itemLen;
            bufferPtr += itemLen / sizeof(UINT32);
        }
    }
}  // end MgenTransport::ProcessRecvMessage()



