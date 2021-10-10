#include "mgenGlobals.h"
#include "mgen.h"
#include "mgenMsg.h"
#include "mgenVersion.h"
#include "mgenEvent.h"
#include "protoString.h"  // for ProtoTokenator

#include <string.h>
#include <stdio.h>   
#include <time.h>       // for gmtimeon()
#include <errno.h>      // for errno
#include <ctype.h>      // for toupper(
#ifdef UNIX
#include <sys/types.h>  // for stat
#include <sys/stat.h>   // for stat
#include <unistd.h>     // for stat
#endif 

// For WinCE, we provide an option logging function
// to get log output to our debug window

#include <errno.h>
Mgen::LogFunction Mgen::Log = fprintf;
void (*Mgen::LogTimestamp)(FILE*, const struct timeval&, bool) = Mgen::LogLegacyTimestamp;

#ifdef _WIN32_WCE
/**
 * This function is used on our WinCE build where there is no
 * real "stdout" for default MGEN output
 */
int Mgen::LogToDebug(FILE* /*filePtr*/, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    char charBuffer[2048];
    charBuffer[2047] = '\0';
    int count = _vsnprintf(charBuffer, 2047, format, args);
    va_end(args);
    return count;
}  // end Mgen::LogToDebug()
#endif // _WIN32_WCE

void Mgen::SetEpochTimestamp(bool enable)
{
  if(enable)
    {
      LogTimestamp = LogEpochTimestamp;
    }
  else
    {
      LogTimestamp = LogLegacyTimestamp;
    }
}

void Mgen::LogEpochTimestamp(FILE* filePtr, const struct timeval& theTime, bool localTime)
{
    Mgen::Log(filePtr, "%lu.%06lu ", theTime.tv_sec, theTime.tv_usec);
}

void Mgen::LogLegacyTimestamp(FILE* filePtr, const struct timeval& theTime, bool localTime)
{
#ifdef _WIN32_WCE
    struct tm timeStruct;
    timeStruct.tm_hour = theTime.tv_sec / 3600;
    UINT32 hourSecs = 3600 * timeStruct.tm_hour;
    timeStruct.tm_min = (theTime.tv_sec - hourSecs) / 60;
    timeStruct.tm_sec = theTime.tv_sec - hourSecs - (60*timeStruct.tm_min);
    timeStruct.tm_hour = timeStruct.tm_hour % 24;
    struct tm* timePtr = &timeStruct;
#else
    struct tm* timePtr;
    time_t secs = theTime.tv_sec;

    if (localTime)
        timePtr = localtime((time_t*)&secs);
    else
        timePtr = gmtime((time_t*)&secs);

#endif // if/else _WIN32_WCE
     Mgen::Log(filePtr, "%02d:%02d:%02d.%06lu ",
                  timePtr->tm_hour, timePtr->tm_min, timePtr->tm_sec, 
                  (UINT32)theTime.tv_usec);
}  // end Mgen::LogTimeStamp()


Mgen::Mgen(ProtoTimerMgr&         timerMgr,
           ProtoSocket::Notifier& socketNotifier)
  : 
  controller(NULL), socket_notifier(socketNotifier),
  timer_mgr(timerMgr), 
  save_path(NULL), save_path_lock(false), started(false),  
  start_hour(0), start_min(0), start_sec(-1.0),
  start_gmt(false), start_time_lock(false),
  offset(-1.0), offset_lock(false), offset_pending(false),
  checksum_force(false), 
  default_flow_label(0), default_label_lock(false),
  default_tx_buffer(0), default_rx_buffer(0),
  default_broadcast(true), default_tos(0), 
  default_multicast_ttl(1), default_unicast_ttl(255), 
  default_df(DF_DEFAULT),
  default_queue_limit(0),
  default_retry_count(0), default_retry_delay(5),
  default_broadcast_lock(false),
  default_tos_lock(false), default_multicast_ttl_lock(false), 
  default_unicast_ttl_lock(false),
  default_df_lock(false),
  default_tx_buffer_lock(false), default_rx_buffer_lock(false), 
  default_interface_lock(false), default_queue_limit_lock(false),
  default_retry_count_lock(false), default_retry_delay_lock(false),
  sink_non_blocking(true),
  log_data(true), log_gps_data(true),
  checksum_enable(false), 
  addr_type(ProtoAddress::IPv4), 
  analytic_window(MgenAnalytic::DEFAULT_WINDOW),
  compute_analytics(false), report_analytics(false),
  get_position(NULL), get_position_data(NULL),
  log_file(NULL), log_binary(false), local_time(false), log_flush(false), 
  log_file_lock(false), log_tx(false), log_rx(true), log_open(false), log_empty(true),
  reuse(true)

{
    start_timer.SetListener(this, &Mgen::OnStartTimeout);
    start_timer.SetInterval(0.0);
    start_timer.SetRepeat(0);
    
    drec_event_timer.SetListener(this, &Mgen::OnDrecEventTimeout);
    drec_event_timer.SetInterval(0.0);
    drec_event_timer.SetRepeat(-1);

    default_interface[0] = '\0';
    sink_path[0] = '\0';
    source_path[0] = '\0';
}

Mgen::~Mgen()
{
    Stop();
    analytic_table.Destroy();
    if (save_path) delete save_path;
}

/**
 * Activates transmit flows according to any "offset" time.
 * Pre-processes drec events occuring prior to "offset" time.
 * Activates any group joins deferred during "offset" processing.
 * Schedules next pending event.
 */
bool Mgen::Start()
{
#ifdef HAVE_IPV6
    //Set default IPv6 flow labels for each flow
    flow_list.SetDefaultLabel(default_flow_label);
#endif //HAVE_IPV6  
    struct timeval currentTime;
    ProtoSystemTime(currentTime);
    // Initialize remote control flow_status bitmasks
    if (!flow_status.Init())
    {
        PLOG(PL_ERROR, "Mgen::Start() error: unable to initialize flow_status (out of memory?)\n");
        return false;
    }
                
    if (start_sec < 0.0)
    {
        // Start immediately
        if (log_file)
        {
            // Log START event
            if (log_binary)
            {
                char buffer[128];
                if (log_empty)
                {
                    // write binary log header line plus NULL character
                    strcpy(buffer, "mgen version=");
                    strcat(buffer, MGEN_VERSION);
                    strcat(buffer, " type=binary_log\n");
                    fwrite(buffer, sizeof(char), strlen(buffer)+1, log_file);
                }
                unsigned int index = 0;
                // set "eventType" field
                buffer[index++] = (char)START_EVENT;
                // zero "reserved" field
                buffer[index++] = 0;
                // set "recordLength" field
                UINT16 temp16 = htons(8); 
                memcpy(buffer+index, &temp16, sizeof(INT16));
                index += sizeof(INT16);
                // set "eventTime" fields
                UINT32 temp32 = htonl(currentTime.tv_sec);
                memcpy(buffer+index, &temp32, sizeof(INT32));
                index += sizeof(INT32);
                temp32 = htonl(currentTime.tv_usec);
                memcpy(buffer+index, &temp32, sizeof(INT32));
                index += sizeof(INT32);
                if (fwrite(buffer, sizeof(char), index, log_file) < index)
                {
                  DMSG(0, "Mgen::Start() fwrite() error: %s\n", GetErrorString());   
                }
            }
            else
            {

                Mgen::LogTimestamp(log_file, currentTime, local_time);
                Mgen::Log(log_file, "START Mgen Version %s\n", MGEN_VERSION);
            }
            if (log_empty) log_empty = false;
            fflush(log_file);
        }
        
        // Activate transmit flows according to "offset" time  
        flow_list.Start(offset);
        
        // Pre-process drec events occurring prior to "offset" time
        DrecEvent* nextEvent = (DrecEvent*)drec_event_list.Head();
        offset_pending = true;
        while (nextEvent)
        {
            if (nextEvent->GetTime() <= offset)
            {
                ProcessDrecEvent(*nextEvent);
            }
            else
            {
                double currentTime = offset > 0.0 ? offset : 0.0;
                drec_event_timer.SetInterval(nextEvent->GetTime() - currentTime);
                timer_mgr.ActivateTimer(drec_event_timer);
                break;  
            }
            nextEvent = (DrecEvent*)nextEvent->Next();
        }
        offset_pending = false;
        next_drec_event = nextEvent;

        // Activate any group joins deferred during "offset" processing.
        drec_group_list.JoinDeferredGroups(*this);
    }
    else  // Schedule absolute start time
    {
        // Make sure there are pending events
        //if(!drec_event_list.IsEmpty() || !flow_list.IsEmpty()) 
        {
            // Calculate start time delta and schedule start_timer 
            // (can delay start up to 12 hours into future)
#ifdef _WIN32_WCE
            DMSG(0, "Mgen::Start() warning: WinCE absolute start time might not be correct!\n");
            UINT32 hours = currentTime.tv_sec / 3600;
            UINT32 minutes = (currentTime.tv_sec - (3600*hours)) / 60;
            UINT32 seconds = currentTime.tv_sec - (3600*hours) - (60*minutes);
            hours = hours % 24;
            double nowSec = hours*3600 + minutes*60 + seconds + 1.0e-06*((double)currentTime.tv_usec);   
#else
            struct tm now;
            if (start_gmt)
                memcpy(&now, gmtime((time_t*)&currentTime.tv_sec), sizeof(struct tm));
            else
                memcpy(&now, localtime((time_t*)&currentTime.tv_sec), sizeof(struct tm));
            double nowSec = now.tm_hour*3600 + now.tm_min*60 + now.tm_sec + 1.0e-06*((double)currentTime.tv_usec);
#endif // if/else _WIN32_WCE
            double startSec = start_hour*3600 + start_min*60 + start_sec;
            double delta = startSec - nowSec;
            if (delta < 0.0) delta += (24*3600);
            if (delta > (12*3600))
            {
                DMSG(0, "Mgen::Start() Error: Specified start time has already elapsed\n");
                return false;   
            }
            if (start_timer.IsActive()) start_timer.Deactivate();

            start_timer.SetInterval(delta);
            timer_mgr.ActivateTimer(start_timer); 
        }
    }

    started = true; 
    return true;
}  // end Mgen::Start()

MgenTransport* Mgen::JoinGroup(const ProtoAddress&   groupAddress, 
                               const ProtoAddress&   sourceAddress,
                               const char*           interfaceName,
                               UINT16        thePort)
{
    
#ifndef IP_MAX_MEMBERSHIPS
#ifdef WIN32
    // WIN32 allows one IP multicast membership per socket
#define IP_MAX_MEMBERSHIPS 1
#else
    // Solaris (and perhaps other Unix) have no 
    // pre-defined limit on group memberhips
#define IP_MAX_MEMBERSHIPS -1    
#endif // if/else WIN32  
#endif // !IP_MAX_MEMBERSHIPS
    
#ifdef SIMULATE
#ifdef IP_MAX_MEMBERSHIPS
#undef IP_MAX_MEMBERSHIPS
#endif // IP_MAX_MEMBERSHIPS
#define IP_MAX_MEMBERSHIPS -1
#endif // SIMULATE
    
    // Sockets with space to join are at top of list
    // find socket of approprate ProtoAddress::Type 

    bool newTransport = false;
    MgenTransport* mgenTransport = FindTransportByInterface(interfaceName, thePort,groupAddress.GetType());
    
    if (!mgenTransport || 
        ((IP_MAX_MEMBERSHIPS > 0) &&
         (mgenTransport->GroupCount() >= (unsigned int)IP_MAX_MEMBERSHIPS)))
    {
        // Create new "dummy" socket for group joins only
        ProtoAddress tmpAddress;
        if (!(mgenTransport = GetMgenTransport(UDP,thePort,tmpAddress,interfaceName,true)))
        {
            DMSG(0, "Mgen::JoinGroup() memory allocation error: %s\n",
                 GetErrorString());
            return NULL;
        }
        newTransport = true;
    } 
    if (mgenTransport->JoinGroup(groupAddress, sourceAddress, interfaceName))
    {
        if ((IP_MAX_MEMBERSHIPS > 0) &&
            (mgenTransport->GroupCount() >= (unsigned int)IP_MAX_MEMBERSHIPS))
        {
            transport_list.Remove(mgenTransport);  // move "full" group socket to end of list
            transport_list.Append(mgenTransport);   
        }
        return mgenTransport;   
    }
    else
    {
        if (newTransport)
        {
            transport_list.Remove(mgenTransport);
            delete(mgenTransport);
        }
        return (MgenTransport*)NULL;   
    }
}  // end Mgen::JoinGroup()

bool Mgen::LeaveGroup(MgenTransport* mgenTransport,
                      const ProtoAddress& groupAddress, 
		      const ProtoAddress& sourceAddress,
                      const char*           interfaceName)
{
  if (mgenTransport->LeaveGroup(groupAddress, sourceAddress, interfaceName))
    {
        transport_list.Remove(mgenTransport);
        transport_list.Prepend(mgenTransport);  
        return true; 
    }
    else
    {
        return false;
    }
}  // end Mgen::LeaveGroup()

/**
 * This finds us a mgenTransport with matching attributes  
 * If a valid mgenTransport ptr is passed, we get the next 
 * match in the list
 */
MgenTransport* Mgen::FindMgenTransport(Protocol theProtocol,
                                       UINT16 srcPort,
                                       const ProtoAddress& dstAddress,
                                       bool closedOnly,
                                       MgenTransport* mgenTransport,
                                       const char* interfaceName)
{ 
    MgenTransport* next = (NULL == mgenTransport) ? transport_list.head : mgenTransport->next;
    while (NULL != next)
    {
         // Same protocol and srcPort?
        if (((next->GetProtocol() == theProtocol) &&
            ((0 == srcPort) || (next->srcPort == srcPort)) &&
             ((NULL == interfaceName ) ||
              ((NULL != next->GetInterface()) && (0 == strcmp(interfaceName,next->GetInterface())))) &&
             // ignore events && unconnected udp sockets
             // have invalid addrs and should match
             ((!dstAddress.IsValid() && !next->dstAddress.IsValid() && (theProtocol == UDP)) ||
              // For tcp ignore events we want to close the
              // listening socket too which won't have a dst address
              (theProtocol == TCP && !dstAddress.IsValid()) ||
              (dstAddress.IsValid() && next->dstAddress.IsValid() && next->dstAddress.IsEqual(dstAddress)))) ||
              // We should only have one sink
             (next->GetProtocol() == SINK && theProtocol == SINK))
        {
	        if (!closedOnly) return next;
	        if (!next->IsOpen()) return next;
        }
        next = next->next;   
    }
    return (MgenTransport*)NULL;
    
}  // end MgenTransport::FindMgenTransport()

/**
 * This search finds an mgenTransport suitable for joining a multicast group
 * We want a matching multicast interface name & type, and a matching port number
 * if "port" is non-zero
 */
MgenTransport* Mgen::FindTransportByInterface(const char*           interfaceName, 
                                              UINT16        thePort,
                                              ProtoAddress::Type addrType)
{

    MgenTransport* nextTransport = transport_list.head;
    while (nextTransport)
    {
        if (nextTransport->GetAddressType() != addrType)
        {
            nextTransport = nextTransport->next;   
            continue;
        }
      
        if (nextTransport->GetProtocol() == UDP)
        {
            MgenUdpTransport* next = static_cast<MgenUdpTransport*>(nextTransport);
            // If "thePort" is non-zero we must a socket of same port
            if (0 != thePort)               
            {
                if (thePort == next->srcPort)
                {
                    // OK, protocol and port matches, so ...
                    if (NULL != interfaceName)
                    {
                        if ((NULL == next->GetMulticastInterface()) && // unspecified interface &&
                            (0 == next->GroupCount()))           // no joins yet, so we'll 
                        {                                       // call it a "match"
                            next->SetMulticastInterface(interfaceName);
                            return nextTransport;
                        }
                        else
                        {
                            if ((next->GetMulticastInterface() && interfaceName) &&
                                !strncmp(next->GetMulticastInterface(), interfaceName, 16))
                            {
                                return nextTransport;  // it's a "match"
                            }
                            else
                            {
                                DMSG(0, "Mgen::FindMgenTransportByInterface() matching port but interface mismatch!\n");
                                return NULL;
                            }
                        }
                    }
                    else
                    {
                        if (0 == next->GroupCount())
                        {
                            next->SetMulticastInterface(NULL);
                            return nextTransport;
                        }
                        else
                        {
                            if (NULL == next->GetMulticastInterface())
                              return nextTransport;
                            else
                            {
                                DMSG(0, "Mgen::FindMgenTransportByInterface() matching port but interface mismatch!\n");
                                return NULL;
                            }
                        }
                    }
                }
            }
            else
            {
               bool match = interfaceName && next->GetMulticastInterface() ? (0 == strncmp(interfaceName, next->GetMulticastInterface(), 16)) :
                  (NULL == next->GetMulticastInterface());
                if (match) return nextTransport;
                
            }
        }
        nextTransport = nextTransport->next;   
    }
    return (MgenTransport*)NULL;
}  // end Mgen::FindTransportByInterface()

MgenTransport* Mgen::FindMgenTransportBySocket(const ProtoSocket& socket)
{
    MgenTransport* next = transport_list.head;
    while (next)
    {
        if (next->OwnsSocket(socket)) 
          return next;
        next = next->next;   
    }
    return (MgenTransport*)NULL;
}  // end Mgen::FindMgenTransportBySocket()


/**
 * @note We get a transport with a matching dstAddress for connected udp sockets
 * The set dst address should not be set for unconnected udp sockets as multiple
 * destinations can share the same socket
 */
MgenTransport* Mgen::GetMgenTransport(Protocol theProtocol,
                                      UINT16 srcPort,
                                      const ProtoAddress& dstAddress,
                                      const char* theInterface,
                                      bool closedOnly,
                                      bool connect)
{
    ProtoAddress tmpAddress;
    if (connect || theProtocol == TCP ) 
      tmpAddress = dstAddress;// ignore events have invalid addrs

    MgenTransport* theTransport =  FindMgenTransport(theProtocol,srcPort,tmpAddress,closedOnly,NULL,theInterface);      

    if (theTransport)
      return theTransport;
    
    switch (theProtocol)
    {
    case UDP:
    case TCP:
      {
          MgenSocketTransport* theTransport;

          if (theProtocol == UDP)
            theTransport = new MgenUdpTransport(*this,theProtocol,srcPort,tmpAddress);
          else
            theTransport = new MgenTcpTransport(*this,theProtocol,srcPort,dstAddress);
          if (!theTransport)
          {
              DMSG(0, "Mgen::GetMgenTransport() mgen transport allocation error: %s\n",
                   GetErrorString());
              return NULL; 
          }

          transport_list.Prepend(theTransport);          
          return theTransport;

          break;
      }
    case SINK:
    case SOURCE:
      {          
          MgenSinkTransport* theTransport = MgenSinkTransport::Create(*this, theProtocol);

          if (!theTransport)
          {
              DMSG(0, "Mgen::GetMgenTransport() mgen transport allocation error: %s\n",
                   GetErrorString());
              return NULL; 
          }
          // ljt set these in constructor's too?
          if (SINK == theProtocol)
            theTransport->SetPath(sink_path);
          else
            theTransport->SetPath(source_path);
          theTransport->SetSinkBlocking(sink_non_blocking);
          transport_list.Prepend(theTransport);          
          return theTransport;
          
          break;
      }
    default:
      DMSG(0,"Mgen::GetMgenTransport() Error: Invalid protocol specified.\n");
      return NULL;
    }        

    return NULL;
} // end Mgen::GetMgenTransport


void Mgen::Stop()
{
    if (started)
    {
        if (NULL != log_file) 
        {
            // Log STOP event
            struct timeval currentTime;
            ProtoSystemTime(currentTime);
            if (log_binary)
            {
                    unsigned int index = 0;
                    char buffer[128];
                    buffer[index++] = (char)STOP_EVENT;
                    // zero "reserved" field
                    buffer[index++] = 0;
                    // set "recordLength" field
                    UINT16 temp16 = htons(8);
                    memcpy(buffer+index, &temp16, sizeof(INT16));
                    index += sizeof(INT16);
                    // set "eventTime" fields
                    UINT32 temp32 = htonl(currentTime.tv_sec);
                    memcpy(buffer+index, &temp32, sizeof(INT32));
                    index += sizeof(INT32);
                    temp32 = htonl(currentTime.tv_usec);
                    memcpy(buffer+index, &temp32, sizeof(INT32));
                    index += sizeof(INT32);
                    // write the record
                    if (fwrite(buffer, sizeof(char), index, log_file) < index)
                        DMSG(0, "Mgen::Stop() fwrite error: %s\n", GetErrorString());
            }
            else
            {
                Mgen::LogTimestamp(log_file, currentTime, local_time);
                Mgen::Log(log_file, "STOP\n");
            }  // end if/else(log_binary)
            
        }  //end if(log_file)
        
        // Save current offset and pending flow sequence state
        if (save_path)
        {
            FILE* saveFile = fopen(save_path, "w+");
            if (saveFile)
            {
                flow_list.SaveFlowSequences(saveFile);
                Mgen::Log(saveFile, "OFFSET %f\n", GetCurrentOffset());
                fclose(saveFile);
            }
            else
            {
                DMSG(0, "Mgen::Stop() error opening save file: %s\n", GetErrorString());   
            }
        }
        started = false;
    } // end if (started)

    
    if (NULL != log_file)
    {
        fflush(log_file);
        if ((log_file != stdout) && (stderr != log_file))
        {
            fclose(log_file);
            log_file = NULL;   
        }
    }
    

    if (start_timer.IsActive()) start_timer.Deactivate();
    flow_list.Destroy();
    if (drec_event_timer.IsActive()) drec_event_timer.Deactivate();
    drec_event_list.Destroy();
    drec_group_list.Destroy(*this);
    transport_list.Destroy();
}  // end Mgen::Stop()

void Mgen::ProcessFlowCommand(MgenFlowCommand::Status status, UINT32 flowId)
{
    MgenFlow* flow = flow_list.FindFlowById(flowId);
    if (NULL == flow)
    {   
        PLOG(PL_ERROR, "Mgen::ProcessFlowCommand() error: unknown flow id\n");
        return;
    }
    switch (status)
    {
    case MgenFlowCommand::FLOW_SUSPEND:
        flow->Suspend();
        break;
    case MgenFlowCommand::FLOW_RESUME:
        flow->Resume();
        break;
    case MgenFlowCommand::FLOW_RESET:
        flow->Reset();
        break;
    default:
        break;  // no change
    }
}  // Mgen::ProcessFlowCommand()

bool Mgen::OnStartTimeout(ProtoTimer& /*theTimer*/)
{
    start_sec = -1.0;
    Start();
    return true;   
}  // Mgen::OnStartTimeout()

bool Mgen::OnDrecEventTimeout(ProtoTimer& /*theTimer*/)
{    
    // 1) Process next pending drec event
    ASSERT(next_drec_event);
    ProcessDrecEvent(*next_drec_event);
    
    // 2) Install next DREC event timeout (or kill timer)
    double eventTime = next_drec_event->GetTime();
    next_drec_event = (DrecEvent*)next_drec_event->Next();
    if (next_drec_event)
    {
        double nextInterval = next_drec_event->GetTime() - eventTime;
        nextInterval = nextInterval > 0.0 ? nextInterval : 0.0;
        drec_event_timer.SetInterval(nextInterval);
        return true;
    }
    else
    {
        drec_event_timer.Deactivate();
        return false;
    }
}  // end Mgen::OnDrecEventTimeout()

/**
 * Process JOIN, LEAVE, IGNORE, LISTEN events
 */

bool Mgen::ProcessDrecEvent(const DrecEvent& event)
{       
    // 1) Process the next DREC event
    switch (event.GetType())
    {
    case DrecEvent::JOIN:
      {
      const UINT16* port = event.GetPortList();
      UINT16 portCount = event.GetPortCount();

      // if no port given find first available socket to join
      // this is the legacy behavior (the preferred command
      // is to set the port on the join statement)
      if (portCount == 0)
      {
          if (!drec_group_list.JoinGroup(*this,
                                         event.GetGroupAddress(),
                                         event.GetSourceAddress(),
                                         event.GetInterface(),
                                         event.GetGroupPort(),
                                         offset_pending))
          {
              DMSG(0, "Mgen::ProcessDrecEvent(JOIN) Warning: error joining group\n");
              return false;
          }	  
          else
          {
              MgenMsg theMsg; 
              theMsg.LogDrecEvent(JOIN_EVENT, &event, event.GetGroupPort(),*this);
          } 
      }
      else 
      {
          for (UINT16 i = 0; i < portCount; i++)
          {
              if (!drec_group_list.JoinGroup(*this,
                                             event.GetGroupAddress(),
                                             event.GetSourceAddress(),
                                             event.GetInterface(),
                                             port[i],
                                             offset_pending))
              {
                  DMSG(0,"Mgen::ProcessDrecEvent(JOIN) Warning: error joining group\n");
                  return false;
              }
              else
              {
                  MgenMsg theMsg;
                  theMsg.LogDrecEvent(JOIN_EVENT,&event, port[i], *this);
              }
          }
      }
      }
      
      break;
      
    case DrecEvent::LEAVE:
    {
        const UINT16* port = event.GetPortList();
        UINT16 portCount = event.GetPortCount();

        if (portCount == 0)
        {
            if (!drec_group_list.LeaveGroup(*this,
                                            event.GetGroupAddress(),
                                            event.GetSourceAddress(),
                                            event.GetInterface(),
                                            event.GetGroupPort()))
            {
                DMSG(0, "Mgen::ProcessDrecEvent(LEAVE) Warning: error leaving group\n");
                return false;
            }
            else
            {
                MgenMsg theMsg;
                theMsg.LogDrecEvent(LEAVE_EVENT, &event, event.GetGroupPort(),*this);
            }
        } else
        {
            for (UINT16 i = 0; i < portCount; i++)
            {
                if (!drec_group_list.LeaveGroup(*this,
                                                event.GetGroupAddress(),
                                                event.GetSourceAddress(),
                                                event.GetInterface(),
                                                port[i]))
                    {
                        DMSG(0,"Mgen::ProcessDrecEvent(LEAVE) warning: error leaving group\n");
                        return false;
                    }
                    else
                    {
                        MgenMsg theMsg;
                        theMsg.LogDrecEvent(LEAVE_EVENT, &event, port[i] ,*this);
                    }
            }
        }
    }
        break;
      
    case DrecEvent::LISTEN:
      {
          const UINT16* port = event.GetPortList();
          UINT16 portCount = event.GetPortCount();
          bool result = false;
          for (UINT16 i = 0; i < portCount; i++)
          {
              
              ProtoAddress tmpAddress;
              MgenTransport* theMgenTransport = GetMgenTransport(event.GetProtocol(),port[i],tmpAddress,event.GetInterface());
              if (theMgenTransport)
                
              {
                 // Listen increments reference count
                 if (theMgenTransport->Listen(port[i],addr_type,true))
                 {
                     result = true;
                 }
                 else
                 {
                     DMSG(0, "Mgen::ProcessDrecEvent(LISTEN) Error: socket listen() failed\n");
                     continue;
                 }
              }
              else
              {
                  DMSG(0, "Mgen::ProcessDrecEvent(LISTEN) Error: no socket available\n");
                  continue;
              }
              // If a socket Rx buffer size is specified
              unsigned int rxBufferSize = event.GetRxBuffer();
              if (0 != rxBufferSize)
              {
                  if (!theMgenTransport->SetRxBufferSize(rxBufferSize))
                    DMSG(0, "Mgen::ProcessDrecEvent(LISTEN) error setting socket rx buffer\n");  
              }  
              MgenMsg theMsg;
              theMsg.LogDrecEvent(LISTEN_EVENT, &event, port[i],*this);
          }
          return result;
      }
      break;            
      
    case DrecEvent::IGNORE_:
      {
          Protocol theProtocol;
          switch (event.GetProtocol())
          {
          case UDP:
            theProtocol = UDP;
            break; 
          case TCP:
            theProtocol = TCP;
            break;  
          default:
            DMSG(0, "Mgen::ProcessDrecEvent(LISTEN) invalid protocol\n");
            return false;
          } 
          const UINT16* port = event.GetPortList();
          UINT16 portCount = event.GetPortCount();
          for (UINT16 i = 0; i < portCount; i++)
          {
              
              /* Delete all sockets associated with the proto/port. */
              /* (regardless of source port)                        */
              
              MgenTransport* mgenTransport;
              MgenTransport* next = NULL;
              ProtoAddress tmpAddress;
              while ((mgenTransport = FindMgenTransport(theProtocol, port[i],tmpAddress,false,next,event.GetInterface())))
              {
                  if (mgenTransport->HasListener()) 
                  {
                      // Disable "listen"
                      mgenTransport->StopInputNotification(); 
                      if (mgenTransport->IsOpen())
                      {
                          // When we get an IGNORE event, close the
                          // listening socket right away, but shutdown
                          // any accepted connections.
                          MgenMsg theMsg;
                          if (mgenTransport->IsListening())
                          {
                              mgenTransport->Close();
                              theMsg.LogDrecEvent(IGNORE_EVENT, &event, port[i], *this);
                          }
                          else
                          {
                              if (mgenTransport->Shutdown())
                              {
                                struct timeval currentTime;
                                ProtoSystemTime(currentTime);
                                mgenTransport->LogEvent(SHUTDOWN_EVENT,&theMsg,currentTime);
                              }
                          }
                     }
                  }  
                  else 
                  {
                      DMSG(0,"Mgen::ProcessDrecEvent(IGNORE) Error: no socket on port %hu\n",port[i]);
                  }
                  next = mgenTransport;
              }
          }
          break;
      }
      
    case DrecEvent::INVALID_TYPE:
      ASSERT(0);
      break; 
    }  // end switch(event.GetType())
    return true;
}  // end Mgen::ProcessDrecEvent()


bool Mgen::OpenLog(const char* path, bool append, bool binary)
{
    if (append)
    {
        
#ifdef WIN32
        WIN32_FILE_ATTRIBUTE_DATA fileAttr;
#ifdef _UNICODE
        wchar_t wideBuffer[PATH_MAX];
        int pathLen = strlen(path);
        if (pathLen > PATH_MAX) 
          pathLen = PATH_MAX;
        else
          pathLen += 1;
        mbstowcs(wideBuffer, path, pathLen);
        LPCTSTR pathPtr = wideBuffer;
#else
        LPCTSTR pathPtr = path;
#endif // if/else _UNICODE
        if (0 == GetFileAttributesEx(pathPtr, GetFileExInfoStandard, &fileAttr))
          log_empty = true;  // error -- assume (nonexistent) file is empty
        else if ((0 == fileAttr.nFileSizeLow) && (0 == fileAttr.nFileSizeHigh))
          log_empty = true;
        else
          log_empty = false;
#else
        struct stat buf;
        if (stat(path, &buf))   // zero return value == success
          log_empty = true;  // error -- assume file is empty
        else if (buf.st_size == 0)
          log_empty = true;
        else
          log_empty = false;
#endif  // if/else WIN32/UNIX
        
    }
    else
    {
        log_empty = true;   
    }
    const char* mode;
    if (binary)
    {
        mode = append ? "ab" : "wb+";
        log_binary = true;
    }
    else
    {
        mode = append ? "a" : "w+";
        log_binary = false;
    }
    FILE* logFile = fopen(path, mode);
    if (!logFile)
    {
        DMSG(0, "Mgen::OpenLog() fopen() Error: %s\n", GetErrorString());   
        return false;    
    }   
    SetLogFile(logFile);
    log_open = true;
    return true;
}  // end Mgen::OpenLog()

void Mgen::SetLogFile(FILE* filePtr)
{
    CloseLog();
    log_file = filePtr;
#ifdef _WIN32_WCE
    if ((stdout == log_file) || (stderr == log_file))
        Log = Mgen::LogToDebug;
    else
        Log = fprintf;
#endif // _WIN32_WCEgoto

}  // end Mgen::SetLogFile()

void Mgen::CloseLog()
{
    if (log_file)
    {
        if ((stdout != log_file) && (stderr != log_file))
            fclose(log_file);
        log_file = NULL;
    }
}  // end Mgen::CloseLog()


void Mgen::RemoveAnalytic(Protocol                protocol,
                          const ProtoAddress&     srcAddr,
                          const ProtoAddress&     dstAddr,
                          UINT32                  flowId)
{
    // TBD - should we include 'protocol' in analytic indexing
    // (MGEN unique flowIds likely make this unnecessary)
    MgenAnalytic* analytic = analytic_table.FindFlow(srcAddr, dstAddr, flowId);
    if (NULL == analytic) return;
    MgenFlow* nextFlow = flow_list.Head();
    while (NULL != nextFlow)
    {
        nextFlow->RemoveAnalyticReport(*analytic);
        nextFlow = flow_list.GetNext(nextFlow);
    }       
    analytic_table.Remove(*analytic);
    delete analytic;
}  // end Mgen::RemoveAnalytic()


void Mgen::UpdateRecvAnalytics(const ProtoTime& theTime, MgenMsg* theMsg, Protocol theProtocol)
{
    // This is a work in progress.  Eventually an option to report back measured
    // analytics in the MGEN payload will use this code. The printout to STDERR
    // here is just a temporary "feature" 
    if (!compute_analytics) return;
    if (NULL == theMsg) return; // TBD - support timeout driven update
    MgenAnalytic* analytic = analytic_table.FindFlow(theMsg->GetSrcAddr(), theMsg->GetDstAddr(), theMsg->GetFlowId());
    if (NULL == analytic)
    {
        if (NULL == (analytic = new MgenAnalytic()))
        {
            PLOG(PL_ERROR, "Mgen::UpdateRecvAnalytics() new MgenAnalytic() error: %s\n", GetErrorString());
            return;
        }
        if (!analytic->Init(theProtocol, theMsg->GetSrcAddr(), theMsg->GetDstAddr(), theMsg->GetFlowId(), analytic_window))
        {
            PLOG(PL_ERROR, "Mgen::UpdateRecvAnalytics() MgenAnalytic() initialization error: %s\n", GetErrorString());
            return;
        }
        if (!analytic_table.Insert(*analytic))
        {
            PLOG(PL_ERROR, "Mgen::UpdateRecvAnalytics() unable to add new flow analytic: %s\n", GetErrorString());
            delete analytic;
            return;
        }
    }
    
    if (analytic->Update(theTime, theMsg->GetMsgLen(), ProtoTime(theMsg->GetTxTime()), theMsg->GetSeqNum()))
    {
        MgenFlow* nextFlow = flow_list.Head();
        while (NULL != nextFlow)
        {
            if (nextFlow->GetReportAnalytics())
                nextFlow->UpdateAnalyticReport(*analytic);
            nextFlow = flow_list.GetNext(nextFlow);
        }       
        const MgenAnalytic::Report& report = analytic->GetReport(theTime);
        if (NULL != controller)  // e.g., Mgendr GUI
            controller->OnUpdateReport(theTime, report);
        analytic->Log(log_file, theTime, theTime, local_time);
    }
    
}  // end Mgen::UpdateRecvAnalytics()


/**
 * Query flow_list and drec_event_list for an idea
 * of the current (or greatest) estimate of 
 * current relative script time offset
 */
double Mgen::GetCurrentOffset() const
{
    if (!started) 
        return -1.0;
    if (next_drec_event)
        return (next_drec_event->GetTime() - drec_event_timer.GetTimeRemaining());
    const DrecEvent* lastEvent = (const DrecEvent*)drec_event_list.Tail();
    double drecOffset = lastEvent ? lastEvent->GetTime() : -1.0;
    double mgenOffset = flow_list.GetCurrentOffset();
    return (drecOffset > mgenOffset) ? drecOffset : mgenOffset;   
}  // end Mgen::GetCurrentOffset()

/**
 * Parse an MGEN script
 */
bool Mgen::ParseScript(const char* path)
{
    // Open script file
    FILE* scriptFile = fopen(path, "r");
    if (!scriptFile)
    {
        DMSG(0, "Mgen::ParseScript() fopen() Error: %s\n", GetErrorString());   
        return false;
    }
    
    // Read script file line by line using FastReader
    FastReader reader;
    unsigned int lineCount = 0;
    unsigned int lines = 0;
    while (1)
    {
        lineCount += lines;  // for grouped (continued) lines
        char lineBuffer[SCRIPT_LINE_MAX+1];
        unsigned int len = SCRIPT_LINE_MAX;
        switch (reader.ReadlineContinue(scriptFile, lineBuffer, &len, &lines))
        {
            case FastReader::OK:
                lineCount++;
                lines--;
                break;
            case FastReader::DONE:
                fclose(scriptFile);
                return true; // done with script file
            case FastReader::ERROR_:
                DMSG(0, "Mgen::ParseScript() Error: script file read error\n");
                fclose(scriptFile);
                return false;
        }
        if (!ParseEvent(lineBuffer, lineCount, false))
        {
            DMSG(0, "Mgen::ParseScript() Error: invalid mgen script line: %lu\n", 
                    lineCount);
            fclose(scriptFile);
            return false;   
        }
    }  // end while (1)
    return true;
}  // end Mgen::ParseScript()

bool Mgen::ParseEvent(const char* lineBuffer, unsigned int lineCount, bool internalCmd)
{
    const char *ptr = lineBuffer;
    // Strip leading white space
    while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
    // Check for comment line (leading '#')
    if ('#' == *ptr) return true;
    char fieldBuffer[SCRIPT_LINE_MAX+1];
    // Script lines are in form {<globalCmd>|<eventTime>} ...
    if (1 != sscanf(ptr, "%s", fieldBuffer))
    {
        // Blank line?
        return true;   
    }
    // Set ptr to next field in line, stripping white space
    ptr += strlen(fieldBuffer);
    while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
    
    Command cmd = GetCommandFromString(fieldBuffer);
    if (EVENT == cmd)
    {
        // read in <eventTime> or <eventType>
        if (1 != sscanf(ptr, "%s", fieldBuffer))
        {
            DMSG(0, "Mgen::ParseEvent() Error: empty EVENT command at line: %lu\n", lineCount);
            return false;    
        }
        // Set ptr to next field in line, stripping white space
        ptr += strlen(fieldBuffer);
        while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;          
    }
    
    // If it's not a "global command", assume it's an event.
    if (INVALID_COMMAND == cmd) cmd = EVENT;
    
    
    switch (cmd)
    {
    case EVENT:
      {
          // EVENT line can begin with the <eventTime> _or_ the <eventType>
          // for implicit, immediate events.
          double eventTime;
          if (1 == sscanf(fieldBuffer, "%lf", &eventTime))
          {
              // Read event command
              if (1 != sscanf(ptr, "%s", fieldBuffer))
              {
                  DMSG(0, "Mgen::ParseEvent() Error: missing command at line: %lu\n", lineCount);
                  return false;   
              }
              // Set ptr to next field in line, stripping white space
              ptr += strlen(fieldBuffer);
              while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
          }
          else
          {
              eventTime = -1.0;   
          }
          
          // Is it an MgenEvent or a DrecEvent?
          if (MgenEvent::INVALID_TYPE != MgenEvent::GetTypeFromString(fieldBuffer))
          {
              // It's an MGEN event   
              // 1) Read the flow id
              if (1 != sscanf(ptr, "%s", fieldBuffer))
              {
                  DMSG(0, "Mgen::ParseEvent() Error: missing <flowId> at line: %lu\n", lineCount);
                  return false;
              }
              unsigned long flowId;
              if (1 != sscanf(fieldBuffer, "%lu", &flowId))
              {
                  DMSG(0, "Mgen::ParseEvent() Error: invalid <flowId> at line: %lu\n", lineCount);
                  return false;
              }
              
              // 2) Find the flow
              MgenFlow* theFlow = flow_list.FindFlowById((UINT32)flowId);
              if (!theFlow)
              {
                  if (!(theFlow = new MgenFlow((UINT32)flowId,
                                               timer_mgr, 
                                               controller,
                                               *this,
                                               default_queue_limit,
                                               default_flow_label)))
                  {
                      DMSG(0, "Mgen::ParseEvent() Error: MgenFlow memory allocation error: %s\n",
                           GetErrorString());   
                      return false;
                  }
                  theFlow->SetReportAnalytics(report_analytics);
                  theFlow->SetPositionCallback(get_position, get_position_data);
#ifdef HAVE_GPS
                  theFlow->SetPayloadHandle(payload_handle);
#endif // HAVE_GPS
                  flow_list.Append(theFlow);
              }
              
              // 3) Create event object
              MgenEvent* theEvent = new MgenEvent();
              if (!theEvent)
              {
                  DMSG(0, "MgenFlow::ParseEvent() mgen event allocation error: %s\n",
                       GetErrorString());
                  return false; 
              }
              if (!theEvent->InitFromString(lineBuffer))
              {
                  DMSG(0, "MgenFlow::ParseEvent() event init error\n");
                  delete theEvent;
                  return false; 
              }
              // Update host_addr port now that we know it
              // TBD - this is not right.  The host_addr source port 
              // should be on a per-flow basis
              if (host_addr.IsValid() && theEvent->GetSrcPort() != 0)
              { 
                  host_addr.SetPort(theEvent->GetSrcPort());
                  //   theFlow->SetHostAddress(host_addr);
              }
              // Update flow specific queue limit if specified
              if (theEvent->GetQueueLimit())
                theFlow->SetQueueLimit(theEvent->GetQueueLimit());
              
              bool reallyStarted = (started && !start_timer.IsActive());
              double currentTime =  reallyStarted ? GetCurrentOffset() : 0.0;

              if (currentTime < 0.0) currentTime = 0.0;

              if (theEvent->IsInternalCmd())
              {
                  if (!internalCmd)
                  {
                      DMSG(0, "MgenFlow::ParseEvent() event init error\n");
                      delete theEvent;
                      return false;
                  }
                  // Don't consider offset for internal commands
                  currentTime = 0.0;
              }       

              if (!theFlow->InsertEvent(theEvent, reallyStarted, currentTime))
              {
                  DMSG(0, "Mgen::ParseEvent() Error: invalid mgen script line: %lu\n", lineCount);
                  delete theEvent;
                  return false;
              }
          }  // End MGEN event processing
          else if (DrecEvent::INVALID_TYPE != DrecEvent::GetTypeFromString(fieldBuffer))
          {
              // It's a DREC event
              DrecEvent* theEvent = new DrecEvent();
              if (!theEvent)
              {
                  DMSG(0, "MgenFlow::ParseEvent() drecevent allocation error: %s\n",
                       GetErrorString());
                  return false; 
              }
              
              
              if (!theEvent->InitFromString(lineBuffer))
              {
                  DMSG(0, "Mgen::ParseEvent() Error: invalid mgen script line: %lu\n", lineCount);
                  delete theEvent;
                  return false;   
              }
              theEvent->SetTime(eventTime);
              bool reallyStarted = (started && !start_timer.IsActive());
              double currentTime = reallyStarted ? GetCurrentOffset() : 0.0;
              if (currentTime < 0.0) currentTime = 0.0;
              theEvent->SetTime(currentTime + eventTime);
              InsertDrecEvent(theEvent);
          }   
          else
          {
              DMSG(0, "Mgen::ParseEvent() Error: invalid command \"%s\" at line: %lu\n", 
                   fieldBuffer, lineCount);
              return false;
          }
          break;
      }
      
    default:
      // Is it a global command
      if (INVALID_COMMAND != cmd)
      {
          if (!OnCommand(cmd, ptr))
          {
              DMSG(0, "Mgen::ParseEvent() Error: bad global command at line: %lu\n", lineCount);
              return false;
          }
      }
      else
      {
          DMSG(0, "Mgen::ParseEvent() Error: invalid command at line: %lu\n", lineCount);
          return false;
      }
      break;
    }
    return true;
}  // end Mgen::ParseEvent()

bool Mgen::ProcessMgenEvent(const MgenEvent& event)
{
    MgenFlow* theFlow = flow_list.FindFlowById(event.GetFlowId());
    if (NULL == theFlow)
    {
        if (!(theFlow = new MgenFlow(event.GetFlowId(),
                                     timer_mgr, 
                                     controller,
                                     *this,
                                     default_queue_limit,
                                     default_flow_label)))
        {
            DMSG(0, "Mgen::ParseEvent() Error: MgenFlow memory allocation error: %s\n",
                 GetErrorString());   
            return false;
        }
        // Set any flow global defaults
        theFlow->SetReportAnalytics(report_analytics);
        theFlow->SetPositionCallback(get_position, get_position_data);
#ifdef HAVE_GPS
        theFlow->SetPayloadHandle(payload_handle);
#endif // HAVE_GPS
        flow_list.Append(theFlow);
    }
    return theFlow->Update(&event);
}  // end Mgen::ProcessMgenEvent()

void Mgen::InsertDrecEvent(DrecEvent* theEvent)
{
    double eventTime = theEvent->GetTime();
    if (started && !start_timer.IsActive())
    {
        double currentTime = GetCurrentOffset();
        if (currentTime < 0.0) currentTime = 0.0;
        if (eventTime < currentTime)
        {
            theEvent->SetTime(currentTime);
            drec_event_list.Precede(next_drec_event, theEvent);
            ProcessDrecEvent(*theEvent);
        }
        else
        {
            theEvent->SetTime(eventTime);
            drec_event_list.Insert(theEvent);
            // Reschedule next drec timeout if needed
            if (drec_event_timer.IsActive())
            {
                double nextTime = next_drec_event->GetTime();
                if (eventTime < nextTime)
                {
                    next_drec_event = theEvent;
                    drec_event_timer.SetInterval(eventTime - currentTime);
                    drec_event_timer.Reschedule();
                }
            }
            else
            {
                next_drec_event = theEvent;
                drec_event_timer.SetInterval(eventTime - currentTime);
                timer_mgr.ActivateTimer(drec_event_timer);
            }
        }
    }   
    else
    {
        eventTime = eventTime > 0.0 ?  eventTime : 0.0;
        theEvent->SetTime(eventTime);
        drec_event_list.Insert(theEvent);
    }
}  // end Mgen::InsertDrecEvent()

// Global command processing
const StringMapper Mgen::COMMAND_LIST[] =
{
    {"+EVENT",      EVENT},
    {"+START",      START},
    {"+INPUT",      INPUT},
    {"+OUTPUT",     OUTPUT},
    {"+LOG",        LOG},
    {"+SAVE",       SAVE},
    {"+DEBUG",      DEBUG_LEVEL},
    {"+OFFSET",     OFFSET},
    {"-TXLOG",	    TXLOG},
    {"+RXLOG",      RXLOG},
    {"-LOCALTIME",  LOCALTIME},
    {"-NOLOG",      NOLOG},
    {"+DLOG",       DLOG},
    {"-BINARY",     BINARY},
    {"-FLUSH",      FLUSH},
    {"+LABEL",      LABEL},
    {"+RXBUFFER",   RXBUFFER},
    {"+TXBUFFER",   TXBUFFER},
    {"+BROADCAST",  BROADCAST},
    {"+TOS",        TOS},
    {"+TTL",        TTL},
    {"+UNICAST_TTL",UNICAST_TTL},
    {"+DF",         DF},
    {"+INTERFACE",  INTERFACE},
    {"-CHECKSUM",   CHECKSUM},
    {"-TXCHECKSUM", TXCHECKSUM},
    {"-RXCHECKSUM", RXCHECKSUM},
    {"+QUEUE",      QUEUE},
    {"+REUSE",      REUSE},
    {"-ANALYTICS",  ANALYTICS},
    {"-REPORT",     REPORT},
    {"+WINDOW",     WINDOW},
    {"+SUSPEND",    SUSPEND},
    {"+RESUME",     RESUME},
    {"+RESET",      RESET},
    {"+RETRY",      RETRY},
    {"+PAUSE",      PAUSE},
    {"+RECONNECT",  RECONNECT},
    {"-EPOCHTIMESTAMP", EPOCH_TIMESTAMP},
    {"+OFF",        INVALID_COMMAND},  // to deconflict "offset" from "off" event
    {NULL,          INVALID_COMMAND}   
};

const char* Mgen::GetCmdName(Command cmd)
{
    const StringMapper* m = COMMAND_LIST;
    while (NULL != m->string)
    {
        if ((Command)(m->key) == cmd)
        {
            if (NULL != m->string)
                return m->string + 1;
            else
                return NULL;
        }
    }
    return NULL;
}  // end Mgen::GetCmdName()

Mgen::Command Mgen::GetCommandFromString(const char* string)
{
    // Make comparison case-insensitive
    char upperString[16];
   size_t len = strlen(string);

    len = len < 16 ? len : 16;
    
    for (unsigned int i = 0 ; i < len; i++)
        upperString[i] = toupper(string[i]);
    const StringMapper* m = COMMAND_LIST;
    Mgen::Command theCommand = INVALID_COMMAND;
    while (NULL != (*m).string)
    {
        if (!strncmp(upperString, (*m).string+1, len))
            theCommand = ((Command)((*m).key));
        m++;
    }
    return theCommand;
}  // end Mgen::GetCommandFromString()

// This tells if the command is valid and whether args are expected
Mgen::CmdType Mgen::GetCmdType(const char* cmd)
{
    if (!cmd) return CMD_INVALID;
    char upperCmd[32];  // all commands < 32 characters
    size_t len = strlen(cmd);
    len = len < 31 ? len : 31;
    unsigned int i;
    for (i = 0; i < (len + 1); i++)
        upperCmd[i] = toupper(cmd[i]);
    
    bool matched = false;
    const StringMapper* m = COMMAND_LIST;
    CmdType type = CMD_INVALID;
    while (INVALID_COMMAND != (*m).key)
    {
        if (!strncmp(upperCmd, (*m).string+1, len))
        {
            if (matched)
            {
                // ambiguous command (command should match only once)
                return CMD_INVALID;
            }
            else
            {
                matched = true;   
                if ('+' == (*m).string[0])
                    type = CMD_ARG;
                else
                    type = CMD_NOARG;
            }
        }
        m++;
    }
    return type; 
}  // end Mgen::GetCmdType()

void Mgen::SetAnalyticWindow(double windowSize)
{
    // Note the window size is adjusted to nearest quantized value
    if (windowSize <= 0.0) return;
    UINT8 q = MgenAnalytic::Report::QuantizeTimeValue(windowSize);
    analytic_window = MgenAnalytic::Report::UnquantizeTimeValue(q);
    // Update existing averaging windows
    MgenAnalyticTable::Iterator iterator(analytic_table);
    MgenAnalytic* next;
    while (NULL != (next = iterator.GetNextItem()))
        next->SetWindowSize(windowSize);
}  // end Mgen::SetAnalyticWindow()

bool Mgen::OnCommand(Mgen::Command cmd, const char* arg, bool override)
{ 

    switch (cmd)
    {
    case START:
      {
          if (!arg)
          {
              DMSG(0, "Mgen::OnCommand() Error: missing <startTime>\n");
              return false;   
          }
          if (override || !start_time_lock)
          {
              // convert to upper case for case-insensitivity
              // search for "GMT" or "NOW" keywords
              char temp[32];
              size_t len = strlen(arg);
              len = len < 31 ? len : 31;
              unsigned int i;
              for (i = 0 ; i < len; i++)
                temp[i] = toupper(arg[i]);
              temp[i] = '\0';
              unsigned int startHour, startMin;
              double startSec;
              // arg should be "hr:min:sec[GMT]" or "NOW"
              if (3 == sscanf(temp, "%u:%u:%lf", &startHour, &startMin, &startSec))
              {
                  start_hour = startHour;
                  start_min = startMin;
                  start_sec = startSec;
                  if (strstr(temp, "GMT"))
                    start_gmt = true;
                  else
                    start_gmt = false;
                  if (start_hour == 0 && start_min == 0 && start_sec == 0)
                    start_sec = -1.0;
              }
              else
              {
                  // Check for "NOW" keywork (case-insensitive)
                  if (strstr(temp, "NOW"))
                  {
                      // negative start_time_sec indicates immediate start
                      start_sec = -1.0; 
                  }
                  else
                  {
                      DMSG(0, "Mgen::OnCommand() Error: invalid START time\n");
                      return false;
                  }
              }
              start_time_lock = override;  // record START command precedence
              if (started) Start();  // Adamson reschedule any prior START directive
          }  // end if (override || !start_time_lock)
          break;
      }  // end case START
      
    case EVENT:
      if (!ParseEvent(arg, 0, false))
      {
          DMSG(0, "Mgen::OnCommand() - error parsing event\n");
          return false;
      }
      break;
      
    case INPUT:
      if (!ParseScript(arg))
      {
          DMSG(0, "Mgen::OnCommand() - error parsing script\n");
          return false;
      }
      break;
      
    case TXBUFFER:	
      unsigned int sizeTemp; 
      if (1 != sscanf(arg, "%u", &sizeTemp))
      {
          DMSG(0, "Mgen::OnCommand() - invalid tx buffer size\n");
          return false;
      }
      SetDefaultTxBufferSize(sizeTemp, override);
      break;            
    case RXBUFFER:
      {
          unsigned int sizeTemp; 
          if (1 != sscanf(arg, "%u", &sizeTemp))
          {
              DMSG(0, "Mgen::OnCommand() - invalid rx buffer size\n");
              
          }
          SetDefaultRxBufferSize(sizeTemp, override);
          break;
      }      
    case BROADCAST:
      if (!arg)
      {
          DMSG(0, "Mgen::OnCommand() Error: missing argument to BROADCAST\n");
          return false;   
      }
      {
          bool broadcastTemp;
          // convert to upper case for case-insensitivity
          char temp[4];
          size_t len = strlen(arg);
          len = len < 4 ? len : 4;
          unsigned int i;
          for (i = 0 ; i < len; i++)
            temp[i] = toupper(arg[i]);
          temp[i] = '\0';
          if(!strncmp("ON", temp, len))
              broadcastTemp = true;
          else if(!strncmp("OFF", temp, len))
              broadcastTemp = false;
          else
          {
              DMSG(0, "Mgen::OnCommand() Error: wrong argument to BROADCAST: %s\n", arg);
              return false;   
          }
          SetDefaultBroadcast(broadcastTemp, override);
          break;
      }

    case TOS:	
      {
          int tosTemp; 
          int result = sscanf(arg, "%i", &tosTemp);
          if ((1 != result) || (tosTemp < 0) || (tosTemp > 255))
          {                
              DMSG(0, "Mgen::OnCommand() - invalid tos value");
              return false;
          }            
          SetDefaultTos(tosTemp, override);
          break;
      }
      
    case INTERFACE:
      SetDefaultMulticastInterface(arg, override);
      break;

    case DF:
    {
        FragmentationStatus df = DF_DEFAULT;
        char temp[4];
        size_t len = strlen(arg);
        len = len < 4 ? len : 4;
        unsigned int i;
        for (i = 0 ; i < len; i++)
            temp[i] = toupper(arg[i]);
        temp[i] = '\0';
        if (!strncmp("ON", temp, len))
            df = DF_ON;
        else if (!strncmp("OFF", temp, len))
            df = DF_OFF;
        else
        {
            DMSG(0,"Mgen::OnCommand() Error: wrong argument to DF: %s\n",arg);
            return false;
        }
        SetDefaultDF(df, override);
        break;
    }
      // For backwards compatability TTL refers to multicast ttl
    case TTL:	
      {
          int ttlTemp;
          int result = sscanf(arg, "%i", &ttlTemp);
          if ((1 != result) || (ttlTemp < 0) || (ttlTemp > 255))
          {
              DMSG(0, "Mgen::OnCommand() - invalid ttl value");
              return false;
          }
          
          SetDefaultMulticastTtl(ttlTemp, override);
          break;
      }
    case UNICAST_TTL:	
      {
          int ttlTemp;
          int result = sscanf(arg, "%i", &ttlTemp);
          if ((1 != result) || (ttlTemp < 0) || (ttlTemp > 255))
          {
              DMSG(0, "Mgen::OnCommand() - invalid ttl value");
              return false;
          }
          
          SetDefaultUnicastTtl(ttlTemp, override);
          break;
      }
      
    case LABEL:
      // (TBD) make flow_list.SetDefaultFlowLabel()
      if (override || !default_label_lock)
      {
          int tempLabel;
          if (1 != sscanf(arg, "%i", &tempLabel))
          {
              DMSG(0, "Mgen::OnCommand() - invalid flow label value");
              return false;
          }
          default_flow_label = tempLabel;
          default_label_lock = override; 
      }
      break;
      
    case OUTPUT:
      if (override || !log_file_lock)
      {
          if (!OpenLog(arg, false, log_binary))
          {
              DMSG(0, "Mgen::OnCommand() Error: log file open error: %s\n",
                   GetErrorString());
              return false;
          }
          log_file_lock = override;
      }
      break;
      
    case LOG:
      if (override || !log_file_lock)
      {
          if (!OpenLog(arg, true, log_binary))
          {
              DMSG(0, "Mgen::OnCommand() Error: log file open error: %s\n",
                   GetErrorString());
              return false;
          }
          log_file_lock = override;
      }
      break;
      
    case NOLOG:
      if (override || !log_file_lock)
      {
          SetLogFile(NULL);
          log_file_lock = override;
      }
      break;
      
    case DLOG:
      if (override)
      {
          if (!OpenDebugLog(arg))
          {
              DMSG(0, "Mgen::OnCommand(dlog) error opening debug log file: %s\n",
                   GetErrorString());
              return false;
          }
      }
      break;
      
    case SAVE:
      if (override || !save_path_lock)
      {
          FILE* filePtr = fopen(arg, "w+");
          if (filePtr)
          {
              char* path = new char[strlen(arg) + 1];
              if (path)
              {
                  strcpy(path, arg);
                  if (save_path) delete save_path;
                  save_path = path;
                  save_path_lock = override;
              }
              else
              {
                  DMSG(0, "Mgen::OnCommand() Error: memory allocation error: %s\n",
                       GetErrorString());
                  return false;
              }
          }
          else
          {
              DMSG(0, "Mgen::OnCommand() Error: save file open error: %s\n",
                   GetErrorString());
              return false;
          }
      }
      break;
      
    case DEBUG_LEVEL:
      SetDebugLevel(atoi(arg));
      break;
      
    case OFFSET:
      if (override || !offset_lock)
      {
          double timeOffset;
          if (1 == sscanf(arg, "%lf", &timeOffset))
          {
              offset = timeOffset;
              offset_lock = override;
          }
          else
          {
              DMSG(0, "Mgen::OnCommand() Error invalid OFFSET\n");
              return false;
          }
      }
      break;
	  
    case TXLOG:
      log_tx = true;
      break;
      
    case RXLOG:
    {
      if (!arg)
      {
          DMSG(0, "Mgen::OnCommand() Error: missing argument to RXLOG\n");
          return false;
      }
      bool rxLogTmp;
      // convert to upper case for case-insensitivity
      char temp[4];
      size_t len = strlen(arg);
      len = len < 4 ? len : 4;
      unsigned int i;
      for (i = 0 ; i < len; i++)
        temp[i] = toupper(arg[i]);
      temp[i] = '\0';
      if(!strncmp("ON", temp, len))
          rxLogTmp = true;
      else if(!strncmp("OFF", temp, len))
          rxLogTmp = false;
      else
      {
          DMSG(0, "Mgen::OnCommand() Error: wrong argument to RXLOG: %s\n", arg);
          return false;   
      }
      log_rx = rxLogTmp;
      break;
    }

    case REUSE:
    {
      if (!arg)
      {
          DMSG(0, "Mgen::OnCommand() Error: missing argument to REUSE\n");
          return false;   
      }
      bool reuseTemp;
      // convert to upper case for case-insensitivity
      char temp[4];
      size_t len = strlen(arg);
      len = len < 4 ? len : 4;
      unsigned int i;
      for (i = 0 ; i < len; i++)
        temp[i] = toupper(arg[i]);
      temp[i] = '\0';
      if(!strncmp("ON", temp, len))
          reuseTemp = true;
      else if(!strncmp("OFF", temp, len))
          reuseTemp = false;
      else
      {
          DMSG(0, "Mgen::OnCommand() Error: wrong argument to REUSE: %s\n", arg);
          return false;   
      }
      SetDefaultReuse(reuseTemp);
      break;
    }
      
    case ANALYTICS:
        compute_analytics = true;
        break;
        
    case REPORT:
        report_analytics = true;
        compute_analytics = true;
        break;
        
    case WINDOW:
        if (NULL != arg)
        {
            double windowSize;
            if ((1 != sscanf(arg, "%lf", &windowSize)) || (windowSize <= 0.0))
            {
                 PLOG(PL_ERROR, "Mgen::OnCommand() Error: invalid WINDOW interval\n");
                 return false;
            }
            SetAnalyticWindow(windowSize);
        }
        else
        {
            PLOG(PL_ERROR, "Mgen::OnCommand() Error: missing argument to WINDOW\n");
            return false;
        }
        break;

    case PAUSE:
    case RECONNECT:
        if (NULL != arg)
        {
            unsigned long flowId;
            if (1 != sscanf(arg, "%lu", &flowId))
            {
                DMSG(0, "Mgen::ParseEvent() Error: pause/reconnect missing <flowId>\n");
            }
            MgenFlow* flow = flow_list.Head();
            while (NULL != flow)
            {
                if (flowId == flow->GetFlowId())
                {
                    switch (cmd)
                    {
                    case PAUSE:
                        flow->Pause();
                        break;
                    case RECONNECT:
                        flow->Reconnect();
                        break;
                    default:
                        break;  // wont' occur
                    }
                    return true;
                }
                flow = flow_list.GetNext(flow);
            }
            return false;
        }

        break;
    case SUSPEND:
    case RESUME:
    case RESET:
        if (NULL != arg)
        {   
            // arg is comma-delimited flow list and can include dash-delimited ranges
            // Use ProtoTokenator to parse flow id list
            ProtoTokenator tk1(arg, ',');
            const char* item;
            while (NULL != (item = tk1.GetNextItem()))
            {
                ProtoTokenator tk2(item, '-');
                const char* flowIdText = tk2.GetNextItem();
                unsigned int startId = 0;
                unsigned int endId = 0;
                if (NULL == flowIdText)
                {
                    PLOG(PL_ERROR, "Mgen::OnCommand(suspend/resume/reset) error: empty  flowId list\n");
                    return false;
                }
                else if (1 != sscanf(flowIdText, "%u", &startId))
                {
                    // Look for "all" keyword in case-insenstive manner
                    const char* ptr1 = flowIdText;
                    const char* ptr2 = "ALL";
                    unsigned int score = 0;
                    while (('\0' != *ptr1) && (toupper(*ptr1++) == *ptr2++)) score++;
                    if (3 != score) 
                    {
                        PLOG(PL_ERROR, "Mgen::OnCommand(suspend/resume/reset) error: invalid flowId list\n");
                        return false;
                    }
                }
                else if (0 == startId)
                {
                    PLOG(PL_ERROR, "Mgen::OnCommand(suspend/resume/reset) error: invalid flowId list value\n");
                    return false;
                }
                else
                {
                    flowIdText = tk2.GetNextItem();
                    if (NULL == flowIdText)
                    {
                        endId = startId;
                    }
                    else if ((1 != sscanf(flowIdText, "%u", &endId)) || (endId < startId))
                    {
                        PLOG(PL_ERROR, "Mgen::OnCommand(suspend/resume/reset) error: invalid flowId list\n");
                        return false;
                    }
                }
                // At this point, if 0 == startId, that means apply to "all" flows
                unsigned int count = endId - startId + 1;
                MgenFlow* flow = flow_list.Head();
                while ((NULL != flow) && (0 != count))
                {
                    UINT32 flowId = flow->GetFlowId();
                    if ((0 == startId) || ((flowId >= startId) && (flowId <= endId)))
                    {
                        switch (cmd)
                        {
                            case SUSPEND:
                                flow->Suspend();
                                break;
                            case RESUME:
                                flow->Resume();
                                break;
                            case RESET:
                                flow->Reset();
                                break;
                            default:
                                break;  // wont' occur
                        }
                        if (0 != startId) count--;
                    }
                    flow = flow_list.GetNext(flow);
                }
                if ((0 != startId) && (0 != count))
                {
                    PLOG(PL_ERROR, "Mgen::OnCommand(suspend/resume/reset) warning: flowId list contained unknown flowIds\n");
                }
            }  // end while (NULL != tk1.GetNextItem())
        }
        else
        {
            PLOG(PL_ERROR, "Mgen::OnCommand(suspend/resume/reset) error: missing  flowId list\n");
            return false;
        }
        break;
      
    case LOCALTIME:
	  local_time = true;
	  break;
      
    case BINARY:
      if (log_open)
      {
          DMSG(0, "Mgen::OnCommand() Error: BINARY option must precede OUTPUT and LOG commands\n");
          return false;
      }
      log_binary = true;
      break;
      
    case FLUSH:
      log_flush = true;
      break;
      
    case TXCHECKSUM:
      checksum_enable = true;
      break;
      
    case RXCHECKSUM:
      checksum_force = true;
      break;
      
    case CHECKSUM:
      checksum_enable = true;
      checksum_force = true;
      break;

    case QUEUE:	
      int tmpQueueLimit;
      if (1 != sscanf(arg, "%d", &tmpQueueLimit))
      {
          DMSG(0, "Mgen::OnCommand() - invalid queue limit.\n");
          return false;
      }
      SetDefaultQueueLimit(tmpQueueLimit,override);
      break;            

    case RETRY:	
    {
        char fieldBuffer[Mgen::SCRIPT_LINE_MAX];
        if (1 != sscanf(arg, "%s", fieldBuffer))
        {
            DMSG(0, "Mgen::OnCommand() invalid retry argument: retry [<count>[/<delay>]]");
            return false;
        }

        int retryCountValue;
        if (1 != sscanf(fieldBuffer, "%d", &retryCountValue))
        {
            DMSG(0, "Mgen::OnCommand() invalid retry argument: retry [<count>[/<delay>]]");
            return false;
        }
        char* delayPtr = strchr(fieldBuffer, '/');
        if (delayPtr)
        {
            *delayPtr++ = '\0';
            unsigned int retryDelayValue;
            int result = sscanf(delayPtr, "%u", &retryDelayValue);
            if (1 != result)   
            {
                DMSG(0, "Mgen::OnCommand() invalid retry argument: retry [<count>[/<delay>]]");
                return false;
            }
            SetDefaultRetryDelay(retryDelayValue, override);
        }
        
        SetDefaultRetryCount(retryCountValue, override);
        break;
    }
    case EPOCH_TIMESTAMP:
      SetEpochTimestamp(true);
      break;
    case INVALID_COMMAND:
      DMSG(0, "Mgen::OnCommand() Error: invalid command\n");
      return false;   
    }  // end switch(cmd)
    return true; 
}  // end Mgen::OnCommand()


////////////////////////////////////////////////////////////////
// DrecGroupList implementation

DrecGroupList::DrecGroupList()
 : head(NULL), tail(NULL)
{
    
}

DrecGroupList::~DrecGroupList()
{
    
}
void DrecGroupList::Destroy(Mgen& mgen)
{
    DrecMgenTransport* next = head;
    while (next)
    {
        DrecMgenTransport* current = next;
        next = next->next;
        if (NULL != current->flow_transport)
          mgen.LeaveGroup(current->flow_transport, 
                                    current->group_addr, 
			  current->source_addr,
                                    current->GetInterface());
        
        delete current;
    }
    head = tail = (DrecMgenTransport*)NULL;
}  // end DrecGroupList::Destroy()

bool DrecGroupList::JoinGroup(Mgen&                 mgen,
                              const ProtoAddress&   groupAddress, 
                              const ProtoAddress&   sourceAddress,
                              const char*           interfaceName,
                              UINT16        thePort,
                              bool                  deferred)
{
  DrecMgenTransport* transport = FindMgenTransportByGroup(groupAddress, sourceAddress, interfaceName, thePort);
    if (transport && transport->IsActive())
    {
        DMSG(0, "DrecGroupList::JoinGroup() Error: group already joined\n");
        return false;
    }
    else
    {
        if (!transport)
        {
	  if (!(transport = new DrecMgenTransport(groupAddress, sourceAddress, interfaceName, thePort)))
            { 
                DMSG(0, "DrecGroupList::JoinGroup() Error: DrecGroupList::DrecMgenTransport memory allocation error: %s\n",
                     GetErrorString());
                return false;  
            }
            Append(transport);
        }
        if (deferred)
          return true;
        else
          return transport->Activate(mgen);
    }
}  // end DrecGroupList::JoinGroup()

bool DrecGroupList::LeaveGroup(Mgen&                 mgen,
                               const ProtoAddress&   groupAddress, 
                               const ProtoAddress&   sourceAddress,
                               const char*           interfaceName,
                               UINT16        thePort)
{
  DrecMgenTransport* transport = FindMgenTransportByGroup(groupAddress, sourceAddress, interfaceName, thePort);
    if (transport)
    {
        if (transport->IsActive())
        {
            if (!mgen.LeaveGroup(transport->flow_transport, 
				 groupAddress,
				 sourceAddress,
				 interfaceName))
            {
                DMSG(0, "DrecGroupList::LeaveGroup() Error: group leave error\n");
                return false;
            }
            
        }
        Remove(transport);
        delete transport;
        return true;
    }
    else
    {
        DMSG(0, "DrecGroupList::LeaveGroup() Error: group transport not found\n");
        return false;   
    }
}  // end DrecGroupList::LeaveGroup()

bool DrecGroupList::JoinDeferredGroups(Mgen& mgen)
{
    bool result = true;
    DrecMgenTransport* next = head;
    while (next)
    {
        if (!next->IsActive()) 
            result &= next->Activate(mgen);
        next = next->next;
    }
    return result;
}  // end DrecGroupList::JoinDeferredGroups()


DrecGroupList::DrecMgenTransport* DrecGroupList::FindMgenTransportByGroup(const ProtoAddress& groupAddr,
                                                                          const ProtoAddress& sourceAddr,
                                                                          const char*         interfaceName,
                                                                          UINT16              thePort)
{
    DrecMgenTransport* next = head;
    while (next)
    {
        const char* nextInterface = next->GetInterface();
        bool interfaceIsEqual =  (NULL == interfaceName) ? 
                (NULL == nextInterface) : 
                ((NULL != nextInterface) && 
                 !strcmp(nextInterface, interfaceName));
        bool groupIsEqual = next->group_addr.IsValid() ?
            next->group_addr.HostIsEqual(groupAddr) : true;
        bool sourceIsEqual = !next->source_addr.IsValid() ?
                             !sourceAddr.IsValid() :
                             sourceAddr.IsValid() && next->source_addr.HostIsEqual(sourceAddr);        
        bool portIsEqual = thePort == next->GetPort();
        if (interfaceIsEqual && groupIsEqual && sourceIsEqual && portIsEqual) 
            return next;
        next = next->next;
    }
    return (DrecMgenTransport*)NULL;
}  // end DrecGroupList::FindMgenTransportByGroup()

void DrecGroupList::Append(DrecMgenTransport* transport)
{
    transport->next = NULL;
    if ((transport->prev = tail))
        transport->prev->next = transport;
    else
        head = transport;
    tail = transport;
}  // end DrecGroupList::Append()

void DrecGroupList::Remove(DrecMgenTransport* transport)
{
    if (transport->prev)
        transport->prev->next = transport->next;
    else
        head = transport->next;
    if (transport->next)
        transport->next->prev = transport->prev;
    else
        tail = transport->prev;
}  // end DrecGroupList::Remove()
        
//////////////////////////////////////////////////
// DrecGroupList::DrecMgenTransport implementation

DrecGroupList::DrecMgenTransport::DrecMgenTransport(const ProtoAddress&  groupAddr, 
						    const ProtoAddress& sourceAddr,
                          const char*            interfaceName,
                          UINT16         thePort)
  : flow_transport(NULL), group_addr(groupAddr), source_addr(sourceAddr), port(thePort),
   prev(NULL), next(NULL)
{
    if (interfaceName)
        strncpy(interface_name, interfaceName, 16);
    else
        interface_name[0] = '\0';
} 

DrecGroupList::DrecMgenTransport::~DrecMgenTransport()
{
    
}

bool DrecGroupList::DrecMgenTransport::Activate(Mgen& mgen)
{
    const char* iface = GetInterface();
    flow_transport = mgen.JoinGroup(group_addr, source_addr, iface, port);
    return IsActive();
}


////////////////////////////////////////////////////////////////
// Mgen::FastReader implementation

Mgen::FastReader::FastReader()
    : savecount(0)
{
    
}

Mgen::FastReader::Result Mgen::FastReader::Read(FILE*           filePtr, 
                                                char*           buffer, 
                                                unsigned int*   len)
{
    size_t want = *len;
    if (savecount)
    {
        size_t ncopy = want < savecount ? want : savecount;
        memcpy(buffer, saveptr, ncopy);
        savecount -= ncopy;
        saveptr += ncopy;
        buffer += ncopy;
        want -= ncopy;
    }
    while (want)
    {
        size_t result = fread(savebuf, sizeof(char), BUFSIZE, filePtr);
        if (result)
        {
            size_t ncopy= want < result ? want : result;
            memcpy(buffer, savebuf, ncopy);
            savecount = (unsigned int)(result - ncopy);
            saveptr = savebuf + ncopy;
            buffer += ncopy;
            want -= ncopy;
        }
        else  // end-of-file
        {
#ifndef _WIN32_WCE
            if (ferror(filePtr))
            {
                if (EINTR == errno) continue;   
            }
#endif // !_WIN32_WCE
            *len -= want;
            if (*len)
                return OK;  // we read at least something
            else
                return DONE; // we read nothing
        }
    }  // end while(want)
    return OK;
}  // end Mgen::FastReader::Read()

/**
 * An OK text readline() routine (reads what will fit into buffer incl. NULL termination)
 * if *len is unchanged on return, it means the line is bigger than the buffer and 
 * requires multiple reads
 */

Mgen::FastReader::Result Mgen::FastReader::Readline(FILE*         filePtr, 
                                                    char*         buffer, 
                                                    unsigned int* len)
{   
    unsigned int count = 0;
    unsigned int length = *len;
    char* ptr = buffer;
    while (count < length)
    {
        unsigned int one = 1;
        switch (Read(filePtr, ptr, &one))
        {
            case OK:
                if (('\n' == *ptr) || ('\r' == *ptr))
                {
                    *ptr = '\0';
                    *len = count;
                    return OK;
                }
                count++;
                ptr++;
                break;
                
            case ERROR_:
                return ERROR_;
                
            case DONE:
                return DONE;
        }
    }
    // We've filled up the buffer provided with no end-of-line 
    return ERROR_;
}  // end Mgen::FastReader::Readline()

/**
 * This reads a line with possible ending '\' continuation character.
 * Such "continued" lines are returned as one line with this function
 *  and the "lineCount" argument indicates the actual number of lines
 *  which comprise the long "continued" line.
 *
 * @note Lines ending with an even number '\\' are considered ending 
 *        with one less '\' instead of continuing.  So, to actually 
 *        end a line with an even number of '\\', continue it with 
 *        an extra '\' and follow it with a blank line.)
 */
Mgen::FastReader::Result Mgen::FastReader::ReadlineContinue(FILE*         filePtr, 
                                                            char*         buffer, 
                                                            unsigned int* len,
                                                            unsigned int* lineCount)
{   
    unsigned int lines = 0;
    unsigned int count = 0;
    unsigned int length = *len;
    char* ptr = buffer;
    while (count < length)
    {
        unsigned int space = length - count;
        switch (Readline(filePtr, ptr, &space))
        {
            case OK:
            {
                lines++;
                // 1) Does the line continue?
                char* cptr = space ? ptr + space - 1 : ptr;
                // a) skip trailing white space
                while (((' ' == *cptr) ||  ('\t' == *cptr)) && (cptr > ptr)) 
                {
                    space--;
                    cptr--;
                }
                
                // If line "continues" to a blank line, skip it
                if ((cptr == ptr) && ((*cptr == '\0') || isspace(*cptr))) 
                    continue;
                
                if ('\\' == *cptr)
                {
                    // Make sure line ends with odd number of '\' to continue
                    bool lineContinues = false;
                    while ((cptr >= ptr) && ('\\' == *cptr))
                    {
                        cptr--;
                        lineContinues = lineContinues ? false : true;    
                    }  
                    // lose trailing '\' (continuation or extra '\' char)
                    *(ptr+space-1) = '\0';
                    space -= 1;
                    if (lineContinues)
                    {
                        // get next line to continue
                        count += space;
                        ptr += space;
                        continue;
                    }
                }
                *len = count + space;
                if (lineCount) *lineCount = lines;
                return OK;  
                break;
            }
                
            case ERROR_:
                return ERROR_;
                
            case DONE:
                return DONE;
        }
    }
    // We've filled up the buffer provided with no end-of-line 
    return ERROR_;
}  // end Mgen::FastReader::Readline()
