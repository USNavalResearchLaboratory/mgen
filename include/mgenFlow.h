#ifndef _MGEN_FLOW
#define _MGEN_FLOW

#include "mgenEvent.h"
#include "mgenTransport.h"
#include "gpsPub.h"
#include "protokit.h"
#include <stdio.h>  // for FILE

#ifdef _RAPR_JOURNAL
#include "mgenSequencer.h"
#endif

class MgenFlowList;

/**
 * Prototype for function to retrieve position/status
 * (See gpsPub.h for GPSPosition definition)
 */
typedef bool (MgenPositionFunc)(const void*     clientData,
                                GPSPosition&    gpsPosition);
/**
 * @class MgenFlow
 *
 * @brief Maintains state for an MGEN transmission flow.  A "flow" is a
 * time-ordered sequence of message transmissions from a specific
 * source to a specific destination (although flow destinations
 * can be modified dynamically).
 */
class MgenFlow
{
    friend class MgenFlowList;
  
    public:
    MgenFlow(unsigned int       flowId, 
             ProtoTimerMgr&     timerMgr,
             MgenController*    controller,
             Mgen&              mgen,
             int                queue_limit,
             UINT32      defaultV6Label = 0);
    
    ~MgenFlow();
    
    void SetPositionCallback(MgenPositionFunc*  callback, 
                             const void*        clientData) 
    {
        get_position = callback;
        get_position_data = clientData;
    }
    
#ifdef HAVE_GPS
    void SetPayloadHandle(GPSHandle payloadHandle) 
    {payload_handle = payloadHandle;}
#endif // HAVE_GPS
    
	void SetQueueLimit(int queueLimit) {queue_limit = queueLimit;}
	void SetMessageLimit(int messageLimit) {message_limit = messageLimit;}
    
    bool InsertEvent(MgenEvent* event, bool mgenStarted, double currentTime);
    bool ValidateEvent(const MgenEvent* event);
    bool Start(double offsetTime);
    bool Update(const MgenEvent* event);
	bool DoOnEvent(const MgenEvent* event);
    bool DoModEvent(const MgenEvent* event);
    bool DoGenericEvent(const MgenEvent* event);
    bool IsActive() const;
    double GetCurrentOffset() const;
#ifdef HAVE_IPV6
    void SetLabel(UINT32 label) {flow_label = label;}
#endif //HAVE_IPV6
    void Notify() {OnTxTimeout(tx_timer);}
	void StopFlow();
	int GetPending() {return pending_messages;}
	MgenFlow* Next() {return next;}
	bool OnTxTimeout(ProtoTimer& theTimer);
	UINT32 GetFlowId() {return flow_id;} 
	UINT32 GetSeqNum() {return seq_num;} 
	void RestartTimer();
	ProtoTimer& GetTxTimer() {return tx_timer;}
	int QueueLimit() {return queue_limit;}
	bool SendMessage();
	MgenTransport* GetFlowTransport() {return flow_transport;}
    void SetFlowTransport(MgenTransport* theTransport) {flow_transport = theTransport;}
	bool OffPending() {return off_pending;}
	void OffPending(bool offPending) {off_pending = offPending;}

    // Methods used by MgenTransport
    MgenFlow* GetPendingNext() {return pending_next;}
    void AppendPendingNext(MgenFlow* theFlow) {pending_next = theFlow;}
    MgenFlow* GetPendingPrev() {return pending_prev;}
    void AppendPendingPrev(MgenFlow* theFlow) {pending_prev = theFlow;}
    double GetPktInterval() {return pattern.GetPktInterval();}

  private:
	bool GetNextInterval();
    bool OnEventTimeout(ProtoTimer& theTimer);	
	bool                    off_pending;
    MgenTransport*          old_transport;
	bool                    new_transport; 
	int                     queue_limit;
	int                     message_limit;
	int                     messages_sent;
    
    UINT32                  flow_id;                       
    Protocol                protocol;                      
    ProtoAddress            dst_addr;                      
    UINT16                  src_port;    
    
	char*		            payload;
    MgenPattern             pattern;                     
    UINT32                  flow_label;                  
    
    ProtoTimer              tx_timer;  

    
    MgenTransport*          flow_transport;               
    UINT32                  seq_num;                     
	int                     pending_messages;
    double                  last_interval;               
    
    MgenEventList           event_list;                  
    MgenEvent*              next_event;                  
    ProtoTimer              event_timer;                 
    bool                    started;                     
    
    ProtoTimerMgr&          timer_mgr;                 
    
    MgenPositionFunc*       get_position;              
    const void*             get_position_data;         
#ifdef HAVE_GPS
    GPSHandle               payload_handle;              
#endif // HAVE_GPS
	MgenController*         controller;
    Mgen&                   mgen;
    MgenFlow*               prev;                        
    MgenFlow*               next;  
    MgenFlow*               pending_next;
    MgenFlow*               pending_prev;
};  // end class MgenFlow

/**
 * @class MgenFlowList
 * 
 * @brief Maintains a list of MgenFlows which implement scripted MGEN
 *  message transmission.
 */
class MgenFlowList
{
  public:
    MgenFlowList();
    ~MgenFlowList();
    
    void Destroy();
    void Append(MgenFlow* theFlow);
    
    MgenFlow* FindFlowById(unsigned int flowId);
    bool IsEmpty() {return (NULL == head);}
    
    bool Start(double offsetTime);
    
    double GetCurrentOffset() const;
    bool SaveFlowSequences(FILE* file) const;
    void SetDefaultLabel(UINT32 label);
	MgenFlow* Head() {return head;}
  private:
    MgenFlow* head; 
    MgenFlow* tail;  
}; // end class MgenFlowList 

#endif  // _MGEN_FLOW
