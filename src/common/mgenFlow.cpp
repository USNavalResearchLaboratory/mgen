#include "mgenFlow.h"
#include "mgenMsg.h"
#include "mgen.h"
#include "mgenGlobals.h"
#include <time.h>  // for gmtime(), struct tm, etc

#ifndef ENABLE_EVENT_VALIDATION
#define _VALIDATE_EVENTS_OFF
#endif

MgenFlow::MgenFlow(unsigned int         flowId, 
                   ProtoTimerMgr&       timerMgr,
                   MgenController*      theController,
                   Mgen&                theMgen,
                   int                  defaultQueueLimit,
                   UINT32               defaultV6Label)
  : off_pending(false),old_transport(NULL),queue_limit(defaultQueueLimit),
    message_limit(-1), 
    flow_id(flowId), flow_label(defaultV6Label),
    report_analytics(false), report_feedback(false),
    flow_suspended(false), flow_paused(false),
    keep_alive(true),
    flow_transport(NULL), seq_num(0), 
    pending_messages(0), messages_sent(0), next_event(NULL), 
    started(false), socket_error(false),timer_mgr(timerMgr),
    controller(theController), mgen(theMgen),
    pending_next(NULL), pending_prev(NULL)
{ 
    tx_timer.SetListener(this, &MgenFlow::OnTxTimeout);
    tx_timer.SetInterval(1.0);
    tx_timer.SetRepeat(-1);
    
    event_timer.SetListener(this, &MgenFlow::OnEventTimeout);
    event_timer.SetInterval(1.0);
    event_timer.SetRepeat(-1);

    flow_command.InitIntoBuffer(MgenFlowCommand::DATA_ITEM_FLOW_CMD, command_buffer, 12);
}

MgenFlow::~MgenFlow()
{
    if (event_timer.IsActive()) event_timer.Deactivate();
    if (tx_timer.IsActive()) tx_timer.Deactivate();
    event_list.Destroy();
    if (flow_transport && flow_transport->IsOpen()) flow_transport->Close(); 
}
/**
 * Process "immediate events" or enqueues "scheduled" events.
 */
bool MgenFlow::InsertEvent(MgenEvent* theEvent, bool mgenStarted, double currentTime)
{
    double eventTime = theEvent->GetTime();

    if (mgenStarted)
    {   
        // Process "immediate" events or enqueue "scheduled" events     
        if (eventTime < currentTime)
        {
            theEvent->SetTime(currentTime);
            event_list.Precede(next_event, theEvent);
            if (ValidateEvent(theEvent))
            {
                Update(theEvent);
#ifdef _VALIDATE_EVENTS_OFF
		event_list.Remove(theEvent);
#endif // _VALIDATE_EVENTS_OFF
            }
            else
            {
                event_list.Remove(theEvent);
                return false;
            }
        }
        else
        {
            event_list.Insert(theEvent);
            if (!ValidateEvent(theEvent))
            {
                event_list.Remove(theEvent);
                return false;
            }
            // Activate/reschedule "event_timer" as needed
            if (event_timer.IsActive())
            {
                double nextTime = next_event->GetTime();
                if (eventTime < nextTime)
                {
                    next_event = theEvent;
                    event_timer.SetInterval(eventTime - currentTime);
                    event_timer.Reschedule();
                }
            }
            else
            {
                next_event = theEvent;
                event_timer.SetInterval(eventTime - currentTime);
                timer_mgr.ActivateTimer(event_timer);
            }
        }
    }
    else
    {
        eventTime = eventTime > 0.0 ? eventTime : 0.0;
        theEvent->SetTime(eventTime);
        event_list.Insert(theEvent);
        if (!ValidateEvent(theEvent))
        {
            event_list.Remove(theEvent);
            return false;
        }
    }
    return true;
}  // end MgenFlow::InsertEvent()

/**
 *  Validate the event by it's position in the list with respect to its neighbor types   
 */
bool MgenFlow::ValidateEvent(const MgenEvent* event)
{
#ifndef _VALIDATE_EVENTS_OFF
    const MgenEvent* prevEvent = (MgenEvent*)event->Prev();
    MgenEvent::Type prevType = prevEvent ? prevEvent->GetType() : 
                                           MgenEvent::INVALID_TYPE;
    switch (event->GetType())
    {
        case MgenEvent::ON:
            if ((MgenEvent::MOD == prevType) ||
                (MgenEvent::ON  == prevType))
            {
                DMSG(0, "MgenFlow::InsertEvent() inappropriate ON event.\n");
                return false;   
            }
            break;
        case MgenEvent::MOD:
            if ((MgenEvent::OFF == prevType) ||
                (MgenEvent::INVALID_TYPE  == prevType))
            {
                DMSG(0, "MgenFlow::InsertEvent() inappropriate MOD event.\n");
                return false;   
            }
            break;
        case MgenEvent::OFF:
            if ((MgenEvent::OFF == prevType) ||
                (MgenEvent::INVALID_TYPE  == prevType))
            {
                DMSG(0, "MgenFlow::InsertEvent() inappropriate OFF event.\n");
                return false;   
            }
            break;
        case MgenEvent::INVALID_TYPE:
            ASSERT(0);  // this should never occur
            return false;                
    }  // end switch(event->GetType())
#endif // !_VALIDATE_EVENTS_OFF
    return true;
}  // end MgenFlow::ValidateEvent()

/**
 * Defer actual start until "offsetTime", but process events
 * so flow state is appropriate for given offset.
 */
bool MgenFlow::Start(double offsetTime)
{
    MgenEvent* nextEvent = (MgenEvent*)event_list.Head();
    if (!nextEvent) 
    {
        DMSG(0, "MgenFlow::Start() flow with empty event list!\n");
        return false; 
    }
    while (nextEvent)
    {
        if (nextEvent->GetTime() <= offsetTime)
        {
            Update(nextEvent);
            nextEvent = (MgenEvent*)nextEvent->Next();
        }
        else
        {
            break;   
        }   
    }
    if ((next_event = nextEvent))
    {
        double currentTime = (offsetTime > 0.0) ? offsetTime : 0.0;
        double nextInterval = next_event->GetTime() - currentTime;
        nextInterval = nextInterval > 0.0 ? nextInterval : 0.0;
        event_timer.SetInterval(nextInterval);
        timer_mgr.ActivateTimer(event_timer);
        started = true;
        return true;
    }
    started = true;
    return true;
}  // end MgenFlow::Start()

bool MgenFlow::DoOnEvent(const MgenEvent* event)
{
    // Save some state in case we are changing transports
    orig_dst_addr = dst_addr;
    old_transport = flow_transport;
    protocol = event->GetProtocol();

    src_port = (event->OptionIsSet(MgenEvent::SRC)) ? event->GetSrcPort() : 0;
    if (event->OptionIsSet(MgenEvent::DST))
      dst_addr = event->GetDstAddr();

    if (event->OptionIsSet(MgenEvent::COUNT))
    {
        message_limit = event->GetCount();
        messages_sent = 0;

        keep_alive = event->GetKeepAlive();
    }
#ifdef WIN32
    if (protocol == TCP)
    { 
        /// Windows has issues with timer granularity which affects
        /// the performance of tcp flows sent at high message rates.
        /// Setting the queue limit = 2 helps address this problem.
        
        // If a default queue_limit has not been provided set to 2.
        // Flow specific queue_limits will override this.
        if (queue_limit < 2)
        {
            SetQueueLimit(2);
        }
    }
#endif // WIN32
    
    if (event->GetQueueLimit())
      queue_limit = event->GetQueueLimit();
    
    report_analytics |= event->GetReportAnalytics();
    report_feedback |= event->GetReportFeedback();
    
    // Update flow_command as needed
    const MgenEvent::FlowStatus& flowStatus = event->GetFlowStatus();

    if (flowStatus.IsSet()) 
    {
        for (UINT32 id = 1; id <= MgenEvent::FlowStatus::MAX_FLOW; id++)
        {
            MgenFlowCommand::Status status = flowStatus.GetStatus(id);
            if (MgenFlowCommand::FLOW_UNCHANGED != status)
            {
                if (!flow_command.SetStatus(id, status))
                {
                    PLOG(PL_ERROR, "MgenFlow::DoOnEvent() error: unable to set flow_command status!\n");
                    return false;
                }
            }
        }   
    }

    // When we have a message_limit the transport keeps track of
    // how many messages we have successfully sent so (for now
    // we should provide a better fix for this - have flow keep track?)
    // we should find closed sockets only.

    // Temporarily commenting this code out in order to reuse any
    // existing sockets as having multiple sockets listening/sending
    // to a common port is broken when we have socket errors (like
    // no route).  The listening socket will not get all packets from
    // all sources when a sending socket is closed that is sending
    // to the same port.  We can not guarantee that all the messages
    // are sent before the flow is turned off with this change as the
    // transport keeps track of the message limit, not the flow.  Not sure
    // what the permanent solution is yet.
    //if (message_limit > 0)
    //  flow_transport = mgen.GetMgenTransport(protocol, src_port,event->GetDstAddr(),true,event->GetConnect());
    //else
    flow_transport = mgen.GetMgenTransport(protocol, src_port,event->GetDstAddr(),event->GetInterface(),false,event->GetConnect());
        
    if (!flow_transport)
    {
        DMSG(0, "MgenFlow::DoOnEvent() Error: unable to get %s flow transport.\n",MgenEvent::GetStringFromProtocol(protocol));
        return false; 
    }
    // Call DoGenericEvent so TOS is set prior to TCP connect in
    // flow_transport open so that TCP Syn is set to requested
    // value.  (We also call it again in the event of a MOD due
    // to the fall through code).
    // TBD - clean this mess up!
    DoGenericEvent(event);
    
    // This increments socket reference_count for any new flows
    // using the transport and opens socket as needed.  Note that
    // socket must be opened as IPv4 or IPv6 type
    if (!flow_transport->Open(event->GetDstAddr().GetType(),true))
    {
        DMSG(0, "MgenFlow::DoOnEvent() Error: socket open error flow>%d\n",flow_id);
        return false;
    }
    
    
    
    // Set up msg for logging
    MgenMsg theMsg;
    theMsg.SetFlowId(flow_id);
    struct timeval currentTime;
    ProtoSystemTime(currentTime);

    if (off_pending)
    {
        off_pending = false;

        // Have we moved to a new transport?
        if (old_transport && old_transport != flow_transport)
        {
            // Reset the pending message count since we are sending
            // to a new destination.  Get old src/dst for logging.
            pending_messages = 0;
            theMsg.SetDstAddr(old_transport->GetDstAddr());
            theMsg.GetSrcAddr().SetPort(old_transport->GetSrcPort());
                        
            // If we're still sending a tcp message to the old transport
            // let OnTxTimeout notice our pending off and shutdown the 
            // transport when the message has been sent.  Otherwise do 
            // immediately
            if (!old_transport->TransmittingFlow(flow_id))
            {
                if (protocol == TCP)
                {
                    old_transport->Shutdown();
                    // Only log shutdown event if the shutdown event was
                    // invoked properly, e.g. when we wait for socket 
                    // notification before calling close... (we'll get 
                    // duplicate events as well if we log here.)

                    //   old_transport->LogEvent(SHUTDOWN_EVENT,&theMsg,currentTime);
                }
                old_transport->LogEvent(OFF_EVENT,&theMsg,currentTime); 
                if (old_transport->IsOpen()) old_transport->Close();
                old_transport->RemoveFlow(this);
                old_transport = NULL;
            }
        }        
        else 
        {
            // else decrement the transport's reference count
            // since we are reusing it for the same flow and have
            // overridden off_pending, and log off event
            flow_transport->DecrementReferenceCount();
            flow_transport->LogEvent(OFF_EVENT,&theMsg,currentTime);
            old_transport = NULL;

            // for udp sockets we won't get a new flow transport if
            // destination changes but we still don't want to spam 
            // the new dst so remove it from the pending queue
            if (GetPending() && (event->OptionIsSet(MgenEvent::DST) && (!dst_addr.IsEqual(orig_dst_addr))))
            {
                pending_messages = 0;
                flow_transport->RemoveFlow(this);      
            }
            
        }
    }
    theMsg.SetDstAddr(dst_addr);
    theMsg.GetSrcAddr().SetPort(flow_transport->GetSrcPort());
    flow_transport->LogEvent(ON_EVENT,&theMsg,currentTime);
    return true;

} // MgenFlow::DoOnEvent

bool MgenFlow::IsTransportChanging(const MgenEvent* event, UINT16 tmpSrcPort, ProtoAddress tmpDstAddr)
{
    // If we're not changing socket src/dst addr 
    // connection status, or transport, we are not
    // changing the socket so return.
    if ((tmpSrcPort == 0) || (tmpSrcPort == src_port))
    {
        if (dst_addr.IsEqual(tmpDstAddr))
        {
            if (((event->OptionIsSet(MgenEvent::INTERFACE)) &&
                ((strcmp(event->GetInterface(), flow_transport->GetInterface())) == 0))
                ||
                (!event->OptionIsSet(MgenEvent::INTERFACE)))
                {
                    if ((event->OptionIsSet(MgenEvent::CONNECT) 
                         && flow_transport->IsConnected())
                        ||
                        (!event->OptionIsSet(MgenEvent::CONNECT)))
                    {
                        // The transport always reports IsConnected() == true
                        // if the socket has not been previously connected.
                        // So for now, MOD's with no connect option continue to
                        // use the existing transport for the flow (that
                        // may or may not have been previously connected.)
                        return false;
                    }
                }
        }
    }

    return true;
}

bool MgenFlow::DoModEvent(const MgenEvent* event)
{ 
    if (event->GetType() != MgenEvent::MOD) return false;
    UINT16 tmpSrcPort = 0;

    // Get the flow's src port from the transport so we don't change it
    // if the flow gets turned off for any reason (paused flow for
    // example per [0 100]
    if (flow_transport != NULL)
      src_port = flow_transport->GetSrcPort();
    
    ProtoAddress tmpDstAddr = dst_addr;
     
    if (event->OptionIsSet(MgenEvent::SRC)) {tmpSrcPort = event->GetSrcPort();}
    if (event->OptionIsSet(MgenEvent::DST)) {tmpDstAddr = event->GetDstAddr();}

    // internally generated commands for transport pause/reconnect
    if (event->OptionIsSet(MgenEvent::PAUSE)) {Pause();}
    if (event->OptionIsSet(MgenEvent::RECONNECT)) {Reconnect();}

    if (event->OptionIsSet(MgenEvent::COUNT))
    {
        message_limit = event->GetCount();
        messages_sent = 0;
        keep_alive = event->GetKeepAlive();
    }

    struct timeval currentTime;

    // If the previous event paused the flow [0 n], log an
    // ON event if our transport is not changing.  If it is
    // we will log the on event after we successfully get
    // a new transport.
    if (pattern.FlowPaused()
        &&
        !IsTransportChanging(event, tmpSrcPort, tmpDstAddr))
    {
        MgenMsg theMsg;
        theMsg.SetFlowId(flow_id); 
        theMsg.SetDstAddr(tmpDstAddr);
        ProtoSystemTime(currentTime);
        flow_transport->LogEvent(ON_EVENT,&theMsg,currentTime); 
    }

    // If the current event is pausing the flow, log
    // an OFF event.
    if (event->OptionIsSet(MgenEvent::PATTERN))
    {
        // Don't override the flow's pattern - we use that
        // below to decide whether to log an OFF event
        // or not.
        MgenPattern eventPattern = event->GetPattern();
        // The flow was set to 0 packets a second [0 n]
        if (eventPattern.FlowPaused())
        {
            MgenMsg theMsg;
            theMsg.SetFlowId(flow_id); 
            theMsg.SetDstAddr(dst_addr); // Previous flow is being turned off
            ProtoSystemTime(currentTime);
            flow_transport->LogEvent(OFF_EVENT,&theMsg,currentTime); 
            return true;
        }
    }

    // If we're changing the destination of a connected udp socket
    // we can't share the same src port so reset it..
    if (event->OptionIsSet(MgenEvent::CONNECT) 
        &&
        event->OptionIsSet(MgenEvent::DST))
      src_port = 0;
      
    if (!IsTransportChanging(event, tmpSrcPort, tmpDstAddr))
    {
        return true;
    }
    
    // Else we have options set that will result in our changing the
    // socket.
    

    /** 
     * Can't bind to a tcp src port if another tcp socket is 
     * already bound to it.  We ~could~ iterate the flow list to
     * find out if that is the case, but for now we'll just let 
     * the proto code report the bind error.
     *
     * Similarly, ~connected~ udp sockets with different 
     * destinations cannot both bind to the same src port 
     * (unconnected udp sockets with different destination would
     * share the same socket) yet we don't establish whether 
     * another socket has bound to the port (as yet) relying on
     * proto code to reoprt the error.
     *
     * We could rewrite the get transport routines to better
     * address this problem, anyway...
     *
     * When SRC or DST changes 
     *
     * - pending message count reset
     * - flow is removed from transport
     *
     * If reference count <= 1 original transport is closed
     * and new one is opened.
     *
     */ 

    old_transport = flow_transport;

    // Don't want to flood the new transport just because the old
    // one got backed up.
    pending_messages = 0;

    //  old_transport->RemoveFlow(this);

    MgenMsg theMsg;
    theMsg.SetFlowId(flow_id); 
    theMsg.SetDstAddr(dst_addr);
    
    // Shutdown the transport if we are not still transmitting
    // (TCP only, UDP Messages are single shot)  If we are still
    // sending a TCP message, the dispatcher will notice when
    // we get the final fin (e.g. error condition on socket
    // or numBytes read == 0) and we'll log the off/disconnect
    // event then.  The else condition will be followed for any
    // UDP sockets.  (This should be cleaned up!)
    if (old_transport && !old_transport->TransmittingFlow(flow_id))
      {
        if (old_transport->Shutdown())
        {
            // Log shutdown event if we've shutdown the socket
            ProtoSystemTime(currentTime);
            old_transport->LogEvent(SHUTDOWN_EVENT,&theMsg,currentTime);

            old_transport->Close();
            ProtoSystemTime(currentTime);
            old_transport->LogEvent(OFF_EVENT,&theMsg,currentTime); 
            old_transport = NULL;

        }
        else
        {
            // else we have other references to the socket, or are 
            // UDP. Log off event and set transport to NULL so
            // we don't log another off event for this flow
            if (!pattern.FlowPaused())
            {
                // But not if the flow was paused, we already
                // logged the off event.
                ProtoSystemTime(currentTime);
                old_transport->LogEvent(OFF_EVENT,&theMsg,currentTime);
            }
            if (protocol == UDP)
            {
              if (old_transport->IsOpen())
              {
                  old_transport->Close();
              }
            }
            old_transport = NULL;
        }
      }

    // If we're not changing the src port, get the transport
    // that matches the original src port as we want to retain
    // the same src port when we mod...
    if (tmpSrcPort != 0) {src_port = tmpSrcPort;}
    if (tmpDstAddr.IsValid()) {dst_addr = tmpDstAddr;}
    
    // Get a transport that matches the new src/dst/interface/connection status
    flow_transport = mgen.GetMgenTransport(protocol,
                                           src_port,
                                           dst_addr,
                                           event->GetInterface(),
                                           false,
                                           event->GetConnect());
    if (!flow_transport) return false;

    /**
     *This increments socket reference_count for any new flows 
     * using the transport and opens socket as needed.  (We could
     *be sharing it)
     */
    if (!flow_transport->Open(event->GetDstAddr().GetType(),true))
    {
        DMSG(0, "MgenFlow::DoModEvent() Error: socket open error flow>%d\n",flow_id);
        return false;
    }

    // We are sending the flow over a different socket/transport so
    // log on event
    theMsg.SetDstAddr(dst_addr);
    ProtoSystemTime(currentTime);

    flow_transport->LogEvent(ON_EVENT,&theMsg,currentTime);

    return true;

} // end MgenFlow::DoModEvent()

bool MgenFlow::DoGenericEvent(const MgenEvent* event)
{
    // ON/MOD flow options	
    if (event->OptionIsSet(MgenEvent::PATTERN))
      pattern = event->GetPattern();

    if (event->OptionIsSet(MgenEvent::SEQUENCE)) 
    {
#ifndef _RAPR_JOURNAL
        seq_num = event->GetSequence();
#else
        MgenSequencer::SetSequence(flow_id,event->GetSequence());
#endif
    }

    if (event->OptionIsSet(MgenEvent::LABEL))
      flow_label = event->GetFlowLabel();

    if (event->OptionIsSet(MgenEvent::COUNT))
    {
        messages_sent = 0;
        message_limit = event->GetCount();
    }          

    char* payloadString = event->GetPayload();
    if (payloadString != NULL) 
    {
        if (!user_payload.SetPayloadString(payloadString))
        {
            PLOG(PL_ERROR, "MgenFlow::DoGenericEvent() error setting payload: %s\n", GetErrorString());
            return false;
        }
    }
          
    // Socket-specific MGEN flow options
    if (flow_transport)
    {
        flow_transport->SetEventOptions(event);
    }
    
	return true;
} // end MgenFlow::DoGenericEvent()

bool MgenFlow::Update(const MgenEvent* event)
{
    switch (event->GetType())
    {
    case MgenEvent::ON:
      {   
#ifndef _VALIDATE_EVENTS_OFF
          ASSERT(!tx_timer.IsActive());
#endif // _VALIDATE_EVENTS_OFF
          if (!DoOnEvent(event)) return false;

          // Note the lack of "break" here is _intentional_
          
      }
    case MgenEvent::MOD:
      {
          // Do MOD specific events, remember we fall thru!
          if (!flow_transport)
          {
              DMSG(0,"MgenFlow::Update() Error MgenFlow() flow>%d not started yet.\n",flow_id);
              break;
          }
          DoModEvent(event);  
          
          // If analytic reporting is set, make sure we are computing them
          if (report_analytics)  mgen.SetComputeAnalytics(true);

          // Do ON ~and~ MOD events, remember we fall thru!
          DoGenericEvent(event);

          // Schedule tx_timer as needed
          double nextInterval = GetPktInterval();
          if (nextInterval < 0.0)  // 0.0 pkt/sec transmission rate
          {
              if (tx_timer.IsActive()) 
                tx_timer.Deactivate();
          }
          else if (nextInterval > 0.0)
          {	    
              if (tx_timer.IsActive())
              {
                  double elapsedTime = last_interval - tx_timer.GetTimeRemaining();
                  if (nextInterval < elapsedTime)
                    tx_timer.SetInterval(0.0);
                  else
                    tx_timer.SetInterval(nextInterval - elapsedTime);
                  last_interval = nextInterval;
              }
              else
              {
                  tx_timer.SetInterval(0.0);
                  if (!flow_suspended && !flow_paused)
                    timer_mgr.ActivateTimer(tx_timer);
                  last_interval = 0.0;
              }
          }
          else
          {
              if (tx_timer.IsActive()) tx_timer.Deactivate();
              tx_timer.SetInterval(0.0);
              if (!flow_suspended && !flow_paused)
                timer_mgr.ActivateTimer(tx_timer);
              last_interval = 0.0;
          }
          break;
      }
      
    case MgenEvent::OFF:
      {
          // Set off pending if transport is sending to flow
          // or if the flow has pending messages to be sent.
          // and off event is received
          if (flow_transport 
          && (flow_transport->TransmittingFlow(flow_id) || GetPending()))
          {
              // make sure the flow is in the pending flow
              // list so that it will get turned off
              // when we're done sending the message(s) since
              // we've already handled the drec event

              off_pending = true;
              flow_transport->AppendFlow(this);  
              // If we had a socket error, we stopped output notification
              // for the transport and turned on a tx_timer to check 
              // socket readiness. We only set socket_error for flows
              // with unlimited transmission rates and checking output
              // readiness for these sockets can overwhelm mgen, therefore
              // don't restart it here!  Let the flow notice the off_pending
              // when it tries to send the next message.
              if (!socket_error)
              {
                  flow_transport->StartOutputNotification();

                  if (tx_timer.IsActive()) tx_timer.Deactivate();
                  break;
              }
          }

          // Inform rapr so it can reuse the flowid
          if (controller)
          {

              char buffer [512];
              sprintf(buffer, "offevent flow>%lu", (unsigned long)flow_id);
              size_t len = strlen(buffer);
              controller->OnOffEvent(buffer, (int)len);
          }  

          StopFlow();
          break;
      }
      
    case MgenEvent::INVALID_TYPE:
      ASSERT(0);  // this should never occur
      return false;
    }  // end switch(event->GetType())
    return true;
}  // end MgenFlow::Update()

bool MgenFlow::GetNextInterval()
{
  double nextInterval = GetPktInterval();
  if (nextInterval > 0.0) // normal scheduled transmission event
    {
        tx_timer.SetInterval(nextInterval);
        if (!tx_timer.IsActive())
        {
            if (!flow_suspended && !flow_paused)
                timer_mgr.ActivateTimer(tx_timer);
            last_interval = 0.0;
        }
        else
        {
            last_interval = nextInterval;
        }
        return true;
    }
    else if (nextInterval < 0.0)  // Flow pattern rate is 0.0 message/sec
    {
#ifdef HAVE_PCAP
      // Fudge the interval if packet time stamps are out of sequence
      // for clone patterns.
      if (pattern.GetType() == MgenPattern::CLONE && pattern.ReadingPcapFile())
      {
          PLOG(PL_WARN,"MgenFlow::GetNextInterval() Cloned file has out of order timestamps. Sending packet immediately.\n");
          nextInterval = 0.0;
          tx_timer.SetInterval(nextInterval);
          if (!tx_timer.IsActive())
          {
              if (!flow_suspended && !flow_paused)
                timer_mgr.ActivateTimer(tx_timer);
              last_interval = 0.0;
          }
          else
          {
              last_interval = nextInterval;
          }
          return true;
      }

#endif
        if(tx_timer.IsActive()) tx_timer.Deactivate();
        last_interval = 0.0;

        // If we aren't keeping flows alive that have
        // exceeded count, stop the flow
        if (!keep_alive)
        {
            StopFlow();
        }
        return false;
    }
    else  // (nextInterval == 0.0) // Flow pattern rate is unlimited
    {
      if (tx_timer.IsActive()) tx_timer.Deactivate();
	    
      tx_timer.SetInterval(0.0);
      if (!tx_timer.IsActive() && !flow_suspended && !flow_paused) 
          timer_mgr.ActivateTimer(tx_timer);
      
      last_interval = 0.0;
      return false;
    }

} // end MgenFlow::GetNextInterval()

//  Stop Flow is called when a flow has been stopped due to
//  an OFF_EVENT or a COUNT has been exceeded (no keep alive option)
void MgenFlow::StopFlow()
{
    message_limit = -1;
    pending_messages = messages_sent = 0;
    off_pending = false;

  // Inform rapr so it can reuse the flowid
  if (controller)
  {
      char buffer [512];
      sprintf(buffer, "stopFlow flow>%lu", (unsigned long)flow_id);
      size_t len = strlen(buffer);
      controller->OnStopEvent(buffer, (int)len);
  }  
  // remove flow from pending flows list 
  if (flow_transport) flow_transport->RemoveFlow(this);
  if (tx_timer.IsActive()) tx_timer.Deactivate();

  if (flow_transport) 
    {
        MgenMsg theMsg;
        theMsg.SetFlowId(flow_id); 
        theMsg.SetDstAddr(dst_addr);
        struct timeval currentTime;
        ProtoSystemTime(currentTime);

        // Shutdown the transport. The dispatcher will notice when
        // we get the final fin (e.g. error condition on socket
        // or numBytes read == 0) and we'll log the off/disconnect
        // event then.
        if (flow_transport->Shutdown())
        {
            // Log shutdown event if we've shutdown the socket
            flow_transport->LogEvent(SHUTDOWN_EVENT,&theMsg,currentTime);

            // we get an error condition when the server calls
            // connect to disconnect the socket so set an off pending
            // so we can log an OFF event versus a DISCONNECT error evenet
            off_pending = true;
        }
        else
        {
            // else we have other references to the socket, or are 
            // UDP.  (flow_transport.Shutdown() always returns false
            // for UDP sockets.)
            //
            // Log off event and set transport to NULL so
            // we don't log another off event for this flow
            // later on in the shutdown process
            flow_transport->LogEvent(OFF_EVENT,&theMsg,currentTime);

            // Tell rapr we've turned the flow off, may result in 
            // multiple attempts to unlock the flow_id but that should
            // be ok
            if (controller)
            {
                char buffer [512];
                sprintf(buffer, "offevent flow>%lu",(unsigned long)flow_id);
                size_t len = strlen(buffer);
                controller->OnOffEvent(buffer, (int)len);
            }

            // the close event will ensure were aren't closing a 
            // socket another flow is using
            flow_transport->Close(); 
            flow_transport = NULL;
        }
    }
  
} // end MgenFlow::StopFlow

void MgenFlow::RestartTimer()
{
    // Resume normal operations
    if (!tx_timer.IsActive())
    {
        tx_timer.SetInterval(0.0);
        if (!flow_suspended && !flow_paused)
            timer_mgr.ActivateTimer(tx_timer);
    }
    
} // end MgenFlow::RestartTimer()

bool MgenFlow::UpdateAnalyticReport(MgenAnalytic& analytic)
{
    // Removing/adding analytic to reporter updates its status to "unreported",
    // thus boosting its precedence for reporting in our round-robin reporting procedure
    analytic_reporter.Remove(analytic);
    if (!analytic_reporter.Add(analytic))
    {
        PLOG(PL_ERROR, "MgenFlow::UpdateAnalyticReport() unable to add new flow reporter: %s\n", GetErrorString());
        return false;
    }
    return true;
}  // end MgenFlow::UpdateAnalyticReport()

bool MgenFlow::SendMessage()
{
    // If we have an off event for flows with unlimited
    // pkt rate, stop the flow immediately.  We have 
    // already disabled the transmission timer.
    if (OffPending() && pattern.UnlimitedRate())
    {
        StopFlow();
        return false;
    }

    if (!flow_transport || ((message_limit >= 0) && (messages_sent >= message_limit)))
    {
        // Deactivate timer but wait for an OFF_EVENT to actually 
        // stop flow
        if (tx_timer.IsActive()) tx_timer.Deactivate();
        // Reset messages sent so subsequent flow mods will take affect
        //message_limit = messages_sent = 0;
        return false;  
    }

    MgenMsg theMsg;
    theMsg.SetProtocol(protocol);
    unsigned int len = pattern.GetPktSize();
    theMsg.SetMgenMsgLen(len);
    theMsg.SetMsgLen(len); // Is overridden with fragment size in pack() for tcp
    theMsg.SetFlowId(flow_id);
#ifndef _RAPR_JOURNAL
    theMsg.SetSeqNum(seq_num++);
#else
    theMsg.SetSeqNum(MgenSequencer::GetNextSequence(flow_id));
#endif
    struct timeval currentTime;
    ProtoSystemTime(currentTime);

    // TxTime is set here initially but updated in the transport's
    // SendMessage function when the message is actually 
    // transmitted.  (We may be delayed by pending messages and the
    // tx time needs to be as close as possible to when we start
    // sending data to the socket - for tcp in particular)
    theMsg.SetTxTime(currentTime);

    theMsg.SetDstAddr(dst_addr);
    ProtoAddress hostAddr = mgen.GetHostAddr();
    if (hostAddr.IsValid())
    {
        
        hostAddr.SetPort(flow_transport->GetSocketPort());
        theMsg.SetHostAddr(hostAddr);
    }

    ProtoAddress srcAddr;
    srcAddr.SetPort(flow_transport->GetSocketPort());
    theMsg.SetSrcAddr(srcAddr);

    // GPS info
    theMsg.SetGPSStatus(MgenMsg::INVALID_GPS); 
    theMsg.SetGPSLatitude(999);
    theMsg.SetGPSLongitude(999); 
    theMsg.SetGPSAltitude(-999);
#ifdef HAVE_GPS
    if (NULL != get_position)
    {
        GPSPosition pos;
        get_position(get_position_data, pos);
        if (pos.xyvalid)
        {
            theMsg.SetGPSLatitude(pos.y);
            theMsg.SetGPSLongitude(pos.x);
            if (pos.zvalid)
              theMsg.SetGPSAltitude((INT32)(pos.z+0.5));
            if (pos.stale)
              theMsg.SetGPSStatus(MgenMsg::STALE);
            else
              theMsg.SetGPSStatus(MgenMsg::CURRENT);
        }
    }
#endif // HAVE_GPS
#ifdef NEVER_EVER
    // For Android, we have implemented a temporary hack that depends upon
    // the freely available "GPSLogger" Android application.  This mgen
    // code assumes that GPSLogger is configured to log (in text file format)
    // to the file "sdcard/Android/data/com.mendhak.gpslogger/files/gpslogger.txt"
    // If the file is not available or empty, a null GPS position is reported
    GPSPosition pos;
    memset(&pos, 0, sizeof(GPSPosition));  // init to invalid (null) position
    const char* cmd = "tail -1 /storage/sdcard0/Android/data/com.mendhak.gpslogger/files/gpslogger.txt";
    FILE* filePtr = popen(cmd, "r");
    if (NULL != filePtr)
    {
        int year, month, day, hour, minute, second;
        double lat,lon, alt;
        if (9 == fscanf(filePtr, "%d-%d-%dT%d:%d:%dZ,%lf,%lf,%lf",
                         &year, &month, &day, &hour, &minute, &second,
                         &lat, &lon, &alt))
        {
            pos.x = lon;
            pos.y = lat;
            pos.z = alt;
            pos.sys_time = currentTime;  // TBD - parse the logged time and see if not stale
            pos.gps_time = currentTime;  // TBD - parse the logged time and see if not stale
            pos.xyvalid = pos.zvalid = true;
            pos.tvalid = pos.stale = false;
            theMsg.SetGPSLatitude(pos.y);
            theMsg.SetGPSLongitude(pos.x);
            if (pos.zvalid)
              theMsg.SetGPSAltitude((INT32)(pos.z+0.5));
            if (pos.stale)
              theMsg.SetGPSStatus(MgenMsg::STALE);
            else
              theMsg.SetGPSStatus(MgenMsg::CURRENT);
        }
        fclose(filePtr);
    } 
#endif  // ANDROID    
    if (report_analytics || flow_command.IsSet())
    {
        analytic_reporter.Reset();  // resets iteration loop detector
        ProtoTime reportTime(currentTime);
        // sets MgenMsg::MGEN_DATA payload
        // 1) How much space for reports do we have?
        UINT16 headerLen = 4*11 + dst_addr.GetLength() + (hostAddr.IsValid() ? hostAddr.GetLength() : 0);
        unsigned int space = (len < MAX_FRAG_SIZE) ? len : MAX_FRAG_SIZE;  // should this be protocol-dependent?
        space =  (space > headerLen) ? (space - headerLen) : 0;
        if (mgen_payload.Allocate(space))
        {
            char* bufPtr = (char*)mgen_payload.AccessPayloadBuffer();
            UINT16 payloadLen = 0;
            if (flow_command.IsSet())
            {
                UINT16 cmdLength = flow_command.GetLength();
                if (cmdLength <= space) 
                {
                    memcpy(bufPtr, flow_command.GetBuffer(), cmdLength);
                    bufPtr += cmdLength;
                    space -= cmdLength;
                    payloadLen += cmdLength;
                }
                // TBD - else warn there was no room?
            }
            while (space > 0)
            {
                const MgenAnalytic::Report* report = analytic_reporter.PeekNextReport(reportTime);
                if ((NULL == report) || (report->GetLength() > space)) break;
                if (report_feedback)
                {
                    // Only include report(s) that are reciprocal to this flow
                    // (i.e. where report src addr equals this flow's destination address)
                    // Used for pointed feedback reporting
                    ProtoAddress reportSrcAddr;
                    report->GetSrcAddr(reportSrcAddr);
                    if (!dst_addr.IsEqual(reportSrcAddr))
                    {
                        analytic_reporter.Advance();
                        continue;
                    }
                }
                UINT16 reportLength = report->GetLength();
                memcpy(bufPtr, (const char*)report->GetBuffer(), reportLength);
                bufPtr += reportLength;
                payloadLen += reportLength;
                space -= reportLength;
                analytic_reporter.Advance();
            }
            if (payloadLen > 0)
            {
                mgen_payload.SetLength(payloadLen);
                theMsg.SetPayload(MgenMsg::MGEN_DATA, mgen_payload.AccessPayloadBuffer(), mgen_payload.GetLength());
            }
            else if (0 != user_payload.GetLength())
            {
                theMsg.SetPayload(MgenMsg::USER_DATA, user_payload.AccessPayloadBuffer(), user_payload.GetLength());
            }
            else
            {
                theMsg.SetPayload(MgenMsg::USER_DATA, NULL, 0);
            }
        }
        else
        {
            PLOG(PL_ERROR, "MgenFlow::SendMessage() mgen_payload.Allocate() error: %s\n", GetErrorString());
        }
    }
    else if (0 != user_payload.GetLength())
    {
        theMsg.SetPayload(MgenMsg::USER_DATA, user_payload.AccessPayloadBuffer(), user_payload.GetLength());
    }
#ifdef HAVE_GPS
    else if (NULL != payload_handle)
    {
        unsigned char payloadLen = 0;
        GPSGetMemory(payload_handle, 0,(char*)&payloadLen, 1);
        // The (UINT32*) cast here creates a warning but it's OK (trust me)
        if (0 != payloadLen)
        {
            // Code to suppress alignment warning (but it's OK, trust me)
            char* ptr1 = const_cast<char*>(GPSGetMemoryPtr(payload_handle,1));
            UINT32* ptr2 = reinterpret_cast<UINT32*>(ptr1);
            theMsg.SetPayload(MgenMsg::USER_DATA, ptr2, (UINT16)payloadLen);
        }
    }
#endif // HAVE_GPS
    else
    {
        theMsg.SetPayload(MgenMsg::USER_DATA, NULL, 0);
    }
    
#ifdef HAVE_IPV6
    if (ProtoAddress::IPv6 == dst_addr.GetType())
    {
        flow_transport->SetFlowLabel(flow_label);
    }
#endif //HAVE_IPV6
    // Send message, checking for error
    // (log only on success)
    MessageStatus result;
    // txbuffer only used by udp and sink transports
    if (flow_transport != NULL)
      result = flow_transport->SendMessage(theMsg, dst_addr);
    else
      result = MSG_SEND_FAILED;

    if (result == MSG_SEND_OK)
    {
        if (GetPending()) pending_messages--;

        messages_sent++;

        // If we had a previous socket failure for flows with
        // unlimited transmission we may have set a timer.  If
        // so deactivate it and let the flow be managed by
        // socket output readiness
        if (pattern.UnlimitedRate() && tx_timer.IsActive() && socket_error)
        {
            socket_error = false;
            tx_timer.Deactivate();
            flow_transport->StartOutputNotification();
        }
        return true;
    }

    // message was not sent, so sequence number is decremented back one
#ifndef _RAPR_JOURNAL
    PLOG(PL_DEBUG, "MgenFlow::SendMessage() error sending message flow>%d seq>%d.\n",flow_id,(seq_num -1));
    seq_num--;
#else
    PLOG(PL_DEBUG, "MgenFlow::SendMessage() error sending message flow>%d seq>%d.\n",flow_id,MgenSequencer::GetSequence(flow_id));
    // rollback sequence number...
    MgenSequencer::GetPrevSequence(flow_id);
#endif
    
    // Let the transport notify us when it's ready
    // Note that we may have already shutdown the transport
    // if the send failed due to a socket disconnect, and 
    // set flow_transport to NULL... 

    if ((queue_limit != 0 || pattern.UnlimitedRate()) &&
	    flow_transport && flow_transport->IsConnected())
    {
        pending_messages++;
        flow_transport->AppendFlow(this);
	
        if (queue_limit > 0 && pending_messages >= queue_limit)
            if (tx_timer.IsActive()) tx_timer.Deactivate();

        // We only want to start output notification if
        // the socket was blocked (EWOULDBLOCK)
        if (result == MSG_SEND_BLOCKED)
            flow_transport->StartOutputNotification();

        // If we have an unlimited transmission rate and socket
        // failure, stop output notification.  MgenFlow::OnTxTimeout
        // will set a timer to test socket readiness at larger
        // intervals that socket output notification uses
        if ((result == MSG_SEND_FAILED)
            && pattern.UnlimitedRate())
        {	    
            flow_transport->StopOutputNotification();
            socket_error = true;
            tx_timer.SetInterval(0.001);
            if (!tx_timer.IsActive() && !flow_suspended && !flow_paused)
                timer_mgr.ActivateTimer(tx_timer);
        }
    }
    return false;
    
} // MgenFlow::SendMessage

void MgenFlow::Pause()
{
    if (tx_timer.IsActive()) 
        tx_timer.Deactivate();
    flow_paused = true;
}  // end MgenFlow::Pause()


void MgenFlow::Reconnect()
{
    if (!flow_transport->IsConnected())
    {
        if (!flow_transport->Reconnect(dst_addr.GetType()))
        {
            DMSG(0, "MgenFlow::Reconnect() Error: socket open reconnect error flow> %d\n", flow_id);
        }
        struct timeval currentTime;
        ProtoSystemTime(currentTime);
        MgenMsg theMsg;
        theMsg.SetFlowId(flow_id);
        theMsg.SetDstAddr(dst_addr);
        flow_transport->LogEvent(RECONNECT_EVENT, &theMsg, currentTime);
    }
    
    if (!flow_suspended && flow_paused && !tx_timer.IsActive())
    {
        tx_timer.SetInterval(0.0);
        timer_mgr.ActivateTimer(tx_timer);
    }
    flow_paused = false;
}  // end MgenFlow::Reconnect()


void MgenFlow::Suspend()
{
    if (tx_timer.IsActive()) 
        tx_timer.Deactivate();
    flow_suspended = true;
}  // end MgenFlow::Suspend()


void MgenFlow::Resume()
{
    if (flow_suspended && !tx_timer.IsActive())
    {
        tx_timer.SetInterval(0.0);
        timer_mgr.ActivateTimer(tx_timer);
    }
    flow_suspended = false;
}  // end MgenFlow::Resume()


void MgenFlow::Reset()
{
    if (flow_suspended && !tx_timer.IsActive())
    {
        tx_timer.SetInterval(0.0);
        timer_mgr.ActivateTimer(tx_timer);
    }
    messages_sent = 0;
    flow_suspended = false;
}  // end MgenFlow::Reset()


bool MgenFlow::OnTxTimeout(ProtoTimer& /*theTimer*/)
{
    // This code (and MgenTransport) needs to be redone because
    // the code is way too complicated for the simple jobs it
    // should be doing,  The MgenFlow tx_timer should only be used
    // to queue messages to the underlying transport and the flow
    // sequence number should only be incremented when a message
    // is successfully enqueued.  We could pause the timer upon
    // enqueue failure and the underlying MgenTransport would reactivate
    // the timer when there is space in the transport queue available.
    // It looks like transport handoff of some sort has been implemented that
    // is perhaps when a flow attribute change gets the flow assigned to 
    // a different socket?  E.g., a different source port, TOS, etc?
    
    if (!flow_transport || ((message_limit >= 0) && (messages_sent >= message_limit)))
    {
        if (tx_timer.IsActive()) tx_timer.Deactivate();
        // reset messages sent so subsequent flow mods will take affect
        //message_limit = messages_sent = 0;
        if (!keep_alive)
        {
            StopFlow();
        }
        return false;
    }
    // If we moved to a new transport, finish sending
    // messages to the old one before sending any new
    if (old_transport)
    {
        // If the old transport is done sending a message
        // for this flow, remove it from the pending queue.
        if (!old_transport->TransmittingFlow(flow_id))
          {
              MgenMsg theMsg;
              theMsg.SetFlowId(flow_id); 
              theMsg.SetDstAddr(orig_dst_addr);
              struct timeval currentTime;
              ProtoSystemTime(currentTime);
              
              old_transport->RemoveFlow(this);
              if (old_transport->Shutdown())
              {
                  old_transport->LogEvent(SHUTDOWN_EVENT,&theMsg,currentTime);
              }
              
              old_transport->LogEvent(OFF_EVENT,&theMsg,currentTime);
              if (old_transport->IsOpen()) old_transport->Close();
              old_transport = NULL;

              // Reactivate timer for new transport
              if (flow_transport)
                RestartTimer();
          }
        else
        {
            // Deactivate the timer until the msg is sent 
            if (tx_timer.IsActive()) tx_timer.Deactivate();
            return false;

        }
        return true;
    }
    // Don't process OFF event if we have pending messages
    // or are still sending a message for this flow.  If we have
    // a socket_error however, we are sending messages
    // as fast as possible  and have already stopped output 
    // notification and started a timer to keep mgen from
    // thrashing - just go ahead and stop the flow at this point.
    if (off_pending)
    {
        if (flow_transport->TransmittingFlow(flow_id) || (!socket_error && GetPending() > 0))      
            //if (flow_transport->TransmittingFlow(flow_id) || GetPending() > 0)
        {
            if (tx_timer.IsActive()) tx_timer.Deactivate();            
            flow_transport->AppendFlow(this);
            flow_transport->StartOutputNotification();
            return true;
        }
        // Inform rapr so it can reuse the flowid
        if (controller)
        {
            char buffer [512];
            sprintf(buffer, "offevent flow>%lu", (unsigned long)flow_id);
            size_t len = strlen(buffer);
            controller->OnOffEvent(buffer, (int)len);
        }  
        StopFlow();
        return false;        
    }

      if (flow_transport && flow_transport->IsConnecting())
      {
          pending_messages++;
          flow_transport->AppendFlow(this);
          
          // If we've exceeded our queue limit, turn off the timer
          // we'll restart it when the queue gets below the limit, 
          // unless we have an unlimited queue size (queue_limit -1)
          if (queue_limit > 0 && pending_messages >= queue_limit)
          {	  
              if (tx_timer.IsActive()) tx_timer.Deactivate(); 
              return false; 
          }
          else
          {
            // Don't turn on timer if we have an unlimited rate
	        if (!pattern.UnlimitedRate())
	            return GetNextInterval();
          }            	
      }
    
      // If we have an unlimited rate and have had a socket
      // error - try to send the message to test socket
      // readiness.  Otherwise service any pending flows
      // the transport might have first using socket output
      // notification to send as fast as possible
    if ((flow_transport && flow_transport->HasPendingFlows())
	     && (!pattern.UnlimitedRate() && !socket_error))

    {
        // If we have an unlimited transmission rate and have had
        // a socket error don't start output notification in this
        // case we will use a tx_timer to prevent thrashing until 
        // the condition is resolved
        flow_transport->StartOutputNotification();

        if (queue_limit > 0)
        {
            pending_messages++;
            flow_transport->AppendFlow(this);                
        }
        // If we've exceeded our queue limit, turn off the timer
        // we'll restart it when the queue gets below the limit, 
        // unless we have an unlimited queue size (queue_limit -1)
        // or we're sending packets as fast as possible
        if ((queue_limit > 0 && pending_messages >= queue_limit)
             && (!pattern.UnlimitedRate())) // ljt should we allow queue limits for unlimited rates?
        {	  
            if (tx_timer.IsActive()) tx_timer.Deactivate(); 
            return false; // don't want to fail twice!
        }
        else
        {
            return GetNextInterval();
        }
    }

    if (!flow_transport) 
    {
        if (tx_timer.IsActive()) tx_timer.Deactivate();
        return false;
    }
    SendMessage();

    // If we have an unlimited rate, turn off the transmission
    // timer.  If we add it to the pending queue the transport send 
    // functions will send messages as fast as possible.
    if (pattern.UnlimitedRate() && !socket_error)
    {
        flow_transport->AppendFlow(this);
        flow_transport->StartOutputNotification();
        if (tx_timer.IsActive()) tx_timer.Deactivate();
        return false;
    }
    else if (pattern.UnlimitedRate() && socket_error)
    {
        // we have an unlimited rate and a socket_error
        // schedule a transmission timer at 100 milliseconds
        // to prevent thrashing
        tx_timer.SetInterval(0.001);
        if (!tx_timer.IsActive())
            timer_mgr.ActivateTimer(tx_timer);
        return true;
	}
    else
    {
	    return GetNextInterval();
    }

}   // end MgenFlow::OnTxTimeout()


bool MgenFlow::IsActive() const
{
    if (!started) return false;
    if (next_event)
    {
        if (MgenEvent::ON != next_event->GetType())
          return true;
        else
          return false;
    }
    else
    {
        const MgenEvent* lastEvent = (const MgenEvent*)event_list.Tail();
        if (lastEvent && (MgenEvent::OFF != lastEvent->GetType()))
          return true;
        else
          return false;   
    }    
}  // end MgenFlow::IsActive();

double MgenFlow::GetCurrentOffset() const
{
   double currentOffset = -1.0;
   if (started) 
   {
       if (next_event)
       {
           currentOffset = next_event->GetTime() - 
                           event_timer.GetTimeRemaining();
       }
       else
       {
           const MgenEvent* event = (const MgenEvent*)event_list.Tail();
           currentOffset =  event ? event->GetTime() : -1.0;
       }  
   }
   return currentOffset;
}  // end MgenFlow::CurrentTime()

bool MgenFlow::OnEventTimeout(ProtoTimer& /*theTimer*/)
{
    ASSERT(next_event);
    
    // 1) Update flow as needed using "next_event"
    Update(next_event);
    
#ifdef _VALIDATE_EVENTS_OFF
    MgenEvent* processedEvent = next_event;
#endif // _VALIDATE_EVENTS_OFF

    // 2) Set (or kill) event_timer according to "next_event->next"
    double currentTime = next_event->GetTime();
    next_event = (MgenEvent*)next_event->Next();
    if (next_event)
    {
        double nextInterval = next_event->GetTime() - currentTime;
        nextInterval = nextInterval > 0.0 ? nextInterval : 0.0;
        event_timer.SetInterval(nextInterval);
#ifdef _VALIDATE_EVENTS_OFF
	event_list.Remove(processedEvent);
#endif // _VALIDATE_EVENTS_OFF
        return true;
    }
    else
    {
#ifdef _VALIDATE_EVENTS_OFF
      if (event_timer.IsActive())
#endif // _VALIDATE_EVENTS_OFF
        event_timer.Deactivate();
#ifdef _VALIDATE_EVENTS_OFF
        event_list.Remove(processedEvent);
#endif // _VALIDATE_EVENTS_OFF
        return false;
    }
}  // end MgenFlow::OnEventTimeout()



//////////////////////////////////////////////////////////////////
// MgenFlowList implementation
MgenFlowList::MgenFlowList()
 : head(NULL), tail(NULL)
{
}

MgenFlowList::~MgenFlowList()
{
    Destroy();
}

void MgenFlowList::Destroy()
{
    MgenFlow* next = head;
    while (next)
    {
        MgenFlow* current = next;
        next = next->next;
        delete current;   
    }
    head = tail = (MgenFlow*)NULL;
}  // end MgenFlowList::Destroy()

void MgenFlowList::Append(MgenFlow* theFlow)
{
  theFlow->next = NULL;
  if (NULL != (theFlow->prev = tail)) 
    tail->next = theFlow;
  else
    head = theFlow;
  tail = theFlow;
}  // end MgenFlowList::Append()


void MgenFlowList::Remove(MgenFlow& theFlow)
{
    if (NULL != theFlow.prev)
        theFlow.prev->next = theFlow.next;
    else
        head = theFlow.next;
    if (NULL != theFlow.next)
        theFlow.next->prev = theFlow.prev;
    else
        tail = theFlow.prev;
}  // end MgenFlowList::Remove()

//Set the default IPv6 flow label.
#ifdef HAVE_IPV6
void MgenFlowList::SetDefaultLabel(UINT32 label)
{
    MgenFlow* current = head;
    while (current)
    {
        current->SetLabel(label); 
        current = current->next;
    }
}  // MgenFlowList::SetDefaultLabel()
#endif //HAVE_IPV6

MgenFlow* MgenFlowList::FindFlowById(unsigned int flowId)
{
    MgenFlow* prev = tail;
    while (prev)
    { 
        if (flowId == prev->flow_id) return prev;
        prev = prev->prev;
    }
    return (MgenFlow*)NULL;
}  // end MgenFlowList::FindFlowById()

bool MgenFlowList::Start(double offsetTime)
{
    bool result = false;
    MgenFlow* next = head;
    while (next)
    {
        result |= next->Start(offsetTime);
        next = next->next;
    }
    return result;
}  // end MgenFlowList::Start()

double MgenFlowList::GetCurrentOffset() const
{
    double currentOffset = -1.0;
    MgenFlow* next = head;
    while (next)
    {
        if (next->event_timer.IsActive())
        {
            return next->GetCurrentOffset();
        }
        else
        {
            double nextOffset =  next->GetCurrentOffset();
            if (nextOffset > currentOffset) 
                currentOffset = nextOffset;  
        }   
        next = next->next;
    }
    return currentOffset;
}  // end MgenFlowList::GetCurrentOffset()

bool MgenFlowList::SaveFlowSequences(FILE* file) const
{
    MgenFlow* next = head;
    while (next)
    {
        if (next->IsActive() || (NULL != next->next_event))
        {
            double offset;
            if (next->IsActive())
            {
                offset = next->GetCurrentOffset();
            }
            else
            {
                offset = next->next_event->GetTime();
            }
            Mgen::Log(file, "%f MOD %lu SEQUENCE %lu\n",
#ifndef _RAPR_JOURNAL
                          offset, next->flow_id, next->seq_num);                
#else
                          offset, next->flow_id, MgenSequencer::GetSequence(next->flow_id));                
#endif
        }
        next = next->next;
    }
    return true;
}  // end MgenFlowList::SaveFlowState()


