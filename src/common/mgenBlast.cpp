/*
 * This program blasts udp packets at a destination as fast as possible.
 * It attempts to boost the process priority.
 */

#include "protoSocket.h"
#include "mgenMsg.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

void usage()
{
  fprintf(stderr,"Usage() mgenBlast <dstAddress> <dstPort> <packetSize> <count> [<txBufferSize>]  \n");
}

bool BoostPriority()
{
#ifdef HAVE_SCHED
    // (This _may_ work on Linux-only at this point)
    // (TBD) We should probably look into pthread-setschedparam() instead
    struct sched_param schp;
    memset(&schp, 0, sizeof(schp));
    schp.sched_priority =  sched_get_priority_max(SCHED_FIFO);
    if (sched_setscheduler(0, SCHED_FIFO, &schp))
    {
        schp.sched_priority =  sched_get_priority_max(SCHED_OTHER);
        if (sched_setscheduler(0, SCHED_OTHER, &schp))
        {
            PLOG(PL_ERROR, "ProtoDispatcher::BoostPriority() error: sched_setscheduler() error: %s\n", GetErrorString());
            return false;
        }   
    }
#else
    // (TBD) Do something differently if "pthread sched param"?
    if (0 != setpriority(PRIO_PROCESS, getpid(), -20))
    {
        PLOG(PL_ERROR, "PrototDispatcher::BoostPriority() error: setpriority() error: %s\n", GetErrorString());
        return false;
    }
#endif // HAVE_SCHED
    return true;
}
INT32 sent = 0;
void signalHandler(int s) 
{
  fprintf(stderr,"Caught signal %d\n",s);
  fprintf(stderr,"mgenBlast: sent %d packets... \n",sent);

  exit(1);
}

int main(int argc, char* argv[])
{
    UINT16 dstPort = 0;
    ProtoAddress dstAddr;
    UINT16 packetSize = 0;
    INT32 count = 0;
    UINT32 txBufferSize = 0;
    //    INT32 sent = 0;
    MgenMsg theMsg;

    struct sigaction sigHandler;
    sigHandler.sa_handler = signalHandler;
    sigemptyset(&sigHandler.sa_mask);
    sigHandler.sa_flags = 0;

    sigaction(SIGINT, &sigHandler, NULL);

    if (argc >= 4) //  mgenBlast <dstAddress> <dstPort> <packetSize> <count> [<txBufferSize>]
    {
      if (1 != (sscanf(argv[2], "%hu", &dstPort)))
	{
	  fprintf(stderr, "mgenBlast: bad <serverPort>\n");
	  dstPort = 0;    
	}            
      else if (!dstAddr.ResolveFromString(argv[1]))
	{
	  fprintf(stderr, "mgenBlast: bad <dstAddr>\n");
	  dstPort = 0;
	}  
      else
	{
	  dstAddr.SetPort(dstPort);   
	}
      if (1 != sscanf(argv[3], "%hu", &packetSize))
	{
	  fprintf(stderr,"mgenBlast: bad <packetSize>\n");
	  usage();
	  return -1;
	}
      if (1 != sscanf(argv[4], "%d", &count))
	{
	  fprintf(stderr,"mgenBlast: bad <count>\n");
	  usage();
	  return -1;
	}
       if (argc > 5 && 1 != sscanf(argv[5], "%ul", &txBufferSize))
	{
	  fprintf(stderr,"mgenBlast: bad <txBuffer>\n");
	  usage();
	  return -1;
	}
    }   
    
    if ((!dstAddr.IsValid() && (0 == dstPort)) || count == 0)
    {
        usage();
        return -1;   
    }

    ProtoSocket clientSocket(ProtoSocket::UDP);
    fprintf(stderr, "mgenBlast: Opening UDP socket: %s/%hu ...\n", 
	    dstAddr.GetHostString(), dstAddr.GetPort());
    
    // Set up the message
    UINT32 txChecksum = 0;
    int flow_id = 1;
    UINT32 seq_num = 0;
    theMsg.SetFlag(MgenMsg::LAST_BUFFER);
    struct timeval currentTime;
    
    theMsg.SetProtocol(UDP);
    theMsg.SetMgenMsgLen(packetSize);
    theMsg.SetMsgLen(packetSize); // Is overridden with fragment size in pack() for tcp
    theMsg.SetFlowId(flow_id);
    
    theMsg.SetDstAddr(dstAddr);
    
    // GPS info
    theMsg.SetGPSStatus(MgenMsg::INVALID_GPS); 
    theMsg.SetGPSLatitude(999);
    theMsg.SetGPSLongitude(999); 
    theMsg.SetGPSAltitude(-999);
    
    char txBuffer[MAX_SIZE];
    unsigned int len = packetSize;
    
    if (!clientSocket.Open()) 
      {
	fprintf(stderr,"mgenBlast: error opening socket ...\n");
	return -1;
      }
    ProtoAddress srcAddr;
    fprintf(stderr,"mgenMark: source port>%d\n",clientSocket.GetPort());
    srcAddr.SetPort(clientSocket.GetPort());
    theMsg.SetSrcAddr(srcAddr);


    if (txBufferSize > 0 && clientSocket.IsOpen())
      {
	if (!clientSocket.SetTxBufferSize(txBufferSize))
	  {
	    fprintf(stderr,"mgenBlast: error setting tx buffer size ...\n");
	    return -1;
	  }
      }
    // boost process priority
    if (!BoostPriority())
      fprintf(stderr,"mgenBlast: error boosting process priority ... \n");
    // Send message, checking for error
    fprintf(stderr, "mgenBlast: sending traffic ...\n");
       
    while (1)
      {
	if (count > 0 && (sent > count || count > 1000000000))
	  {
	    fprintf(stderr,"mgenBlast: sent %d packets... \n",sent);
	    return -1;
	  }
	theMsg.SetSeqNum(seq_num++);
	ProtoSystemTime(currentTime);
	theMsg.SetTxTime(currentTime);
	len = theMsg.Pack(txBuffer,theMsg.GetMsgLen(),false,txChecksum);
	if (!clientSocket.SendTo(txBuffer,len,dstAddr))
	  {
	    fprintf(stderr, "mgenBlast: error sending to server\n");
	    seq_num--;
	    // clientSocket.Close();
	    // return -1;
	  }
	else
	  {
	    sent ++;
	  }               
      }  // end for (;;)
    fprintf(stderr, "mgenBlast: Done traffic to server ...\n");

    clientSocket.Close();
}  // end main();

