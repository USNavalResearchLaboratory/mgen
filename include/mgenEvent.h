#ifndef _MGEN_EVENT
#define _MGEN_EVENT

#include "mgenGlobals.h"
#include "mgenPattern.h"
#include "mgenPayload.h"  // for MgenFlowCommand stuff
#include "protoBitmask.h"
#include "protokit.h"
/**
 * @class MgenBaseEvent
 *
 * @brief Generic base class for MgenEvent and DrecEvent
 */
class MgenBaseEvent
{
    friend class MgenEventList;
    
  public:
    enum Category {MGEN, DREC};
	static Protocol GetProtocolFromString(const char* string);
    static const char* GetStringFromProtocol(Protocol protocol);
    
    Category GetCategory() const {return category;}
    void SetTime(double theTime) {event_time = theTime;}
    double GetTime() const {return event_time;}
    const MgenBaseEvent* Next() const {return next;}
    const MgenBaseEvent* Prev() const {return prev;}
    
  protected:
    MgenBaseEvent(Category cat);
    // for mapping protocol types from script line fields
    static const StringMapper PROTOCOL_LIST[]; 
    
    
    Category        category;
    double          event_time;
    MgenBaseEvent*  prev;
    MgenBaseEvent*  next;
    
};  // end class MgenBaseEvent
/**
 * @class DrecEvent
 *
 * @brief DrecEvent indicates a specific reception event (i.e. LISTEN, IGNORE, JOIN, or LEAVE)
*/
class DrecEvent : public MgenBaseEvent
{
  public:
    // DREC script event types
    enum Type
    {
      INVALID_TYPE,
      JOIN,
      LEAVE,
      LISTEN,
      IGNORE_  // trailing '_' for WIN32
    };
    static Type GetTypeFromString(const char* string);  
    
    DrecEvent();
    bool InitFromString(const char* string);
    
    Type GetType() const {return event_type;};
    Protocol GetProtocol() const {return protocol;}
    const ProtoAddress& GetGroupAddress() const {return group_addr;}
    const ProtoAddress& GetSourceAddress() const {return source_addr;}
    const char* GetInterface()  const
    {return (('\0' != interface_name[0]) ? interface_name : (const char*)NULL);}
    UINT16 GetPortCount() const {return port_count;}
    const UINT16* GetPortList() const {return port_list;}
    UINT16 GetGroupPort() const {return port_count;}        
    unsigned int GetRxBuffer() const {return rx_buffer_size;}            
    
    // moved this so rapr could use it
    static const StringMapper TYPE_LIST[];     // for mapping event types
    enum Option
    {
      INTERFACE,
      PORT,
      RXBUFFER,
      SRC,
      INVALID_OPTION  
    };
    static const StringMapper OPTION_LIST[];
    static Option GetOptionFromString(const char* string);          
    static UINT16* CreatePortArray(const char* portList,
                                           UINT16* portCount);
    
  private:
    Type            event_type;
    
    Protocol        protocol;
    // JOIN/LEAVE event parameters
    ProtoAddress  group_addr; 
    ProtoAddress  source_addr;  // Source address for SSM
    char            interface_name[16];
    // LISTEN/IGNORE event parameters
    UINT16  port_count;  // (port_count is also used to hold the JOIN event PORT option)
    UINT16* port_list;    
    unsigned int    rx_buffer_size;
    
};  // end class DrecEvent
/**
 * @class MgenEvent
 * 
 * @brief MgenEvent indicates a specific transmit flow event (i.e. ON, MOD, or OFF)
 *
*/
class MgenEvent : public MgenBaseEvent
{    
  public:
    // MGEN script event types
    enum Type
    {
      INVALID_TYPE,
      ON,
      MOD,
      OFF   
    };
    static Type GetTypeFromString(const char* string);
    
    // MGEN script option types/flags (indicates which options were invoked)
    enum Option
    {
      INVALID_OPTION = 0x00000000,
      PROTOCOL =       0x00000001,  // flow protocol was set
      DST =            0x00000002,  // flow destination address was set
      SRC =            0x00000004,  // flow source port was set
      PATTERN =        0x00000008,  // flow pattern was set
      TOS =            0x00000010,  // flow TOS was set
      RSVP =           0x00000020,  // flow RSVP spec was set
      INTERFACE =      0x00000040,  // flow multicast interface was set
      TTL =            0x00000080,  // flow ttl was set
      SEQUENCE =       0x00000100,  // flow sequence number was set        
      LABEL =          0x00000200,  // flow label option for IPV6
      TXBUFFER  =      0x00000400,  // Tx socket buffer size
      DATA =           0x00000800,  // payload data
      QUEUE =          0x00001000,  // queue limit
      COUNT =          0x00002000,  // count
      CONNECT =        0x00004000,  // connect src port for udp
      BROADCAST =      0x00008000,  // send/receive broadcasts
      DF =             0x00010000,  // fragmentation status
      REPORT =         0x00020000,  // enable analytic reporting 
      FEEDBACK =       0x00040000,  // feedback flow reporting to specific remote addr/port
      SUSPEND =        0x00080000,  // send SUSPEND command for given flow list
      RESUME =         0x00100000,  // send RESUME command for given flow list
      RESET =          0x00200000,  // send RESET command for given flow list
      RETRY =          0x00400000,  // tcp retry enabled
      PAUSE =          0x00800000,  // pause flow during tcp retry
      RECONNECT =      0x01000000   // attempt reconnect during tcp retyr
    };
    
    MgenEvent();
	~MgenEvent();
    
    bool InitFromString(const char* string);
    
    unsigned int GetFlowId() const {return flow_id;}
	void SetFlowId(unsigned int flowId) {flow_id = flowId;};
    Type GetType() const {return event_type;}
	void SetType(Type eventType) {event_type = eventType;};
    UINT16 GetSrcPort() const {return src_port;}
	void SetSrcPort(UINT16 srcPort) {src_port = srcPort;}
    const ProtoAddress& GetDstAddr() const {return dst_addr;}
    const MgenPattern& GetPattern() const {return pattern;}
	int GetCount() const {return count;}
    bool GetKeepAlive() const {return keep_alive;}
    Protocol GetProtocol() const {return protocol;}
	void SetProtocol(Protocol theProtocol) {protocol = theProtocol;}
    bool GetBroadcast() const {return broadcast;}
    int GetTOS() const {return tos;}
    UINT32 GetFlowLabel() const {return flow_label;}
    unsigned char GetTTL() const {return ttl;}
    int GetRetryCount() const {return retry_count;}
    unsigned int GetRetryDelay() const {return retry_delay;}
    unsigned int GetTxBuffer() const {return tx_buffer_size;}
    FragmentationStatus GetDF() const {return df;}
	int GetQueueLimit() const {return queue;}
    UINT32 GetSequence() const {return sequence;}
    char* GetPayload() const {return payload;}
    const char* GetInterface()  const
    {return (('\0' != interface_name[0]) ? interface_name : NULL);}
    bool GetConnect() const {return connect;}
    bool GetReportAnalytics() const {return report_analytics;}
    bool GetReportFeedback() const {return report_feedback;}
    bool IsInternalCmd();
    
    bool OptionIsSet(Option option) const
        {return (0 != (option & option_mask));}
        
    class FlowStatus
    {
        public:
            enum {MAX_FLOW = 40};  // 2*MAX_FLOW/8 MUST satisfy (N*4 + 2) for N = 0,1,2, ...
            FlowStatus();
            ~FlowStatus();
            bool Init();
            
            void SetRange(UINT32 start, UINT32 end, MgenFlowCommand::Status status);
            void Clear()
            {
                status_lo.Clear();
                status_hi.Clear();
            }
            bool IsSet() const
                {return (status_hi.IsSet() || status_lo.IsSet());}
            
            MgenFlowCommand::Status GetStatus(UINT32 flowId) const
            {
                UINT8 status = status_lo.Test(flowId - 1) ? 0x01 : 0x00;
                status |= status_hi.Test(flowId -1) ? 0x02 : 0x00;
                return (MgenFlowCommand::Status)status;
            }
                    
        private:
            ProtoBitmask       status_hi;
            ProtoBitmask       status_lo;
    };  // end class MgenEvent::FlowStatus
    
    const FlowStatus& GetFlowStatus() const
        {return flow_status;}
    
  private:
    static const StringMapper TYPE_LIST[];     // for mapping event types
    static const StringMapper OPTION_LIST[];   // for mapping event options
	static Option GetOptionFromString(const char* string);
    static const char* GetStringFromOption(Option option);
    static const unsigned int ON_REQUIRED_OPTIONS;
    
    // Event parameters and options
    UINT32           flow_id;
    Type             event_type;
    UINT16           src_port;
    ProtoAddress     dst_addr;
    char	     *payload;        
    MgenPattern      pattern; 
    int              count;
    bool             keep_alive;
    Protocol         protocol;            
    bool             broadcast;
    int              tos;
    UINT32           flow_label;
    unsigned int     tx_buffer_size;
    unsigned char    ttl;
    int              retry_count;
    unsigned int     retry_delay;
    FragmentationStatus df;
    UINT32           sequence;
    char             interface_name[16];
    unsigned int     option_mask;
    int              queue;
    bool             connect;
    bool             report_analytics;
    bool             report_feedback;
    FlowStatus       flow_status;
    
};  // end class MgenEvent

/**
 * @class MgenEventList
 *
 * @brief Time ordered, linked list of MgenEvent(s) or DrecEvent(s)
*/
class MgenEventList
{
  public:
    MgenEventList();
    ~MgenEventList();
    void Destroy();
    void Insert(MgenBaseEvent* theEvent);
    void Remove(MgenBaseEvent* theEvent);
    // This places "theEvent" _before_ "nextEvent" in the list.
    // (If "nextEvent" is NULL, "theEvent" goes to the end of the list)
    void Precede(MgenBaseEvent* nextEvent, MgenBaseEvent* theEvent);
    
    bool IsEmpty() {return (NULL == head);}
    const MgenBaseEvent* Head() const {return head;}
    const MgenBaseEvent* Tail() const {return tail;}
    
  private:
    MgenBaseEvent* head; 
    MgenBaseEvent* tail; 
}; // end class MgenEventList 


#endif // _MGEN_EVENT
