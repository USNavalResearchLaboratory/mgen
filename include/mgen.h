#ifndef _MGEN
#define _MGEN

#include "protokit.h"
#include "mgenFlow.h"
#include "mgenGlobals.h"
#include "mgenMsg.h"

class MgenController
{
  public:
    virtual ~MgenController() {};
    
    virtual void OnMsgReceive(MgenMsg& msg) = 0;
    virtual void OnOffEvent(char * buffer,int len) = 0;
  protected:
    MgenController() {};

}; // end MgenController class

/**
 * @class DrecGroupList
 * 
 * @brief Maintains list of scripted MGEN reception events.  
 *  The "drec_event_timer" is used to schedule event execution.
 */
class DrecGroupList 
{
  public:
    DrecGroupList();
    ~DrecGroupList();
    
    void Destroy(Mgen& mgen);
    
    bool JoinGroup(Mgen&                     mgen,
                   const ProtoAddress&       groupAddress, 
                   const char*               interfaceName = NULL,
                   UINT16                    thePort = 0,
                   bool                      deferred = false);
    
    bool LeaveGroup(Mgen&                    mgen,
                    const ProtoAddress&      groupAddress, 
                    const char*              interfaceName = NULL,
                    UINT16                   thePort = 0);
    
    bool JoinDeferredGroups(Mgen& mgen);
    
  private:
    class DrecMgenTransport
    {
        friend class DrecGroupList;
      public:
        DrecMgenTransport(const ProtoAddress&  groupAddr, 
                          const char*            interfaceName,
                          UINT16         thePort);
        ~DrecMgenTransport();
        void SetInterface(const char* interfaceName)
        {
            if (interfaceName)
              strncpy(interface_name, interfaceName, 16);
            else
              interface_name[0] = '\0';
        }
        const char* GetInterface()
        {return (('\0' != interface_name[0]) ? interface_name : NULL);}
 
        bool Activate(Mgen& mgen);
        bool IsActive() {return (NULL != flow_transport);}
        UINT16 GetPort() {return port;}
        
      private:
        MgenTransport*          flow_transport;
        ProtoAddress            group_addr;
        char                    interface_name[16];
        UINT16          port;
        DrecMgenTransport*      prev;
        DrecMgenTransport*      next;
    };  // end class DrecGroupList::DrecMgenTransport
    
    DrecMgenTransport* FindMgenTransportByGroup(const ProtoAddress& groupAddr,
                                                const char*           interfaceName = NULL,
                                                UINT16        thePort = 0);
    
    
    void Append(DrecMgenTransport* item);
    void Remove(DrecMgenTransport* item);
    
    DrecMgenTransport* head;  
    DrecMgenTransport* tail;
};  // end class DrecGroupList

/**
 * @class Mgen
 *
 * @brief Mgen is the top level state and controller of an MGEN instance.  Intended to be embedded into applications or network simulation agents.  Contains primary elemenets: MgenFlowList, MgenEventList, MgenTransportList, DrecGroupList, ProtocolTimerMgr and various default parameters.
*/

class Mgen 
{
  public:
    enum {SCRIPT_LINE_MAX = 8192};  // maximum script line length
    
    Mgen(ProtoTimerMgr&         timerMgr, 
         ProtoSocket::Notifier& socketNotifier);
    ~Mgen();
    
    /**
     * MGEN "global command" set
     */
    enum Command
    {
      INVALID_COMMAND,
      EVENT,
      START,     // specify absolute start time
      INPUT,     // input and parse an MGEN script
      OUTPUT,    // open output (log) file 
      LOG,       // open log file for appending
      NOLOG,     // no output
      TXLOG,     // turn transmit logging on
      LOCALTIME, // print log messages in localtime rather than gmtime (the default)
      DLOG,      // set debug log file
      SAVE,      // save pending flow state/offset info on exit.
      DEBUG,     // specify debug level
      OFFSET,    // time offset into script
      TXBUFFER,  // Tx socket buffer size
      RXBUFFER,  // Rx socket buffer size
      LABEL,     // IPv6 flow label
      BROADCAST, // send/receive broadcasts from socket
      TOS,       // IPV4 Type-Of-Service 
      TTL,       // IPV4 Time-To-Live
      INTERFACE, //Multicast Interface
      BINARY,    // turn binary logfile mode on
      FLUSH,     // flush log after _each_ event
      CHECKSUM,  // turn on _both_ tx and rx checksum options
      TXCHECKSUM,// include checksums in transmitted MGEN messages
      RXCHECKSUM, // force checksum validation at receiver _always_
      LOGDATA,   // log payload data on/off
      QUEUE      // Turn off tx_timer when pending queue exceeds this limit
    };
    static Command GetCommandFromString(const char* string);
    enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
    static const char* GetCmdName(Command cmd);
    static CmdType GetCmdType(const char* cmd);
	void SetController(MgenController* theController)
    {controller = theController;}
    MgenController* GetController() {return controller;}
    ProtoSocket::Notifier& GetSocketNotifier() {return socket_notifier;}
    MgenTransportList& GetTransportList() {return  transport_list;} // for ns2

    bool OnCommand(Mgen::Command cmd, const char* arg, bool override = false);
    bool GetChecksumEnable() {return checksum_enable;}
    bool GetChecksumForce() {return checksum_force;}
    bool GetLogData() {return log_data;}
    bool OpenLog(const char* path, bool append, bool binary);
    void CloseLog();
    void SetLogFile(FILE* filePtr);
    FILE* GetLogFile() {return log_file;}
    bool GetLogBinary() {return log_binary;}
    bool GetLocalTime() {return local_time;}
    bool GetLogFlush() {return log_flush;}
    bool GetLogTx() {return log_tx;}
    typedef int (*LogFunction)(FILE*, const char*, ...);
#ifndef _WIN32_WCE
    static LogFunction Log;
#else
    static LogFunction Log;
    // Alternative logging function for WinCE debug window when logging to stdout/stderr
    static int Mgen::LogToDebug(FILE* filePtr, const char* format, ...);
#endif  // if/else _WIN32_WCE
    
    bool ParseScript(const char* path);
#if OPNET  // JPH 11/16/2005
    bool ParseScript(List*);
#endif  // OPNET
    bool ParseEvent(const char* lineBuffer, unsigned int lineCount);
    
    double GetCurrentOffset() const;
    bool GetOffsetPending() {return offset_pending;}
    
    void InsertDrecEvent(DrecEvent* event);

    void SetDefaultSocketType(ProtoAddress::Type addrType) 
    {addr_type = addrType;}
    ProtoAddress::Type GetDefaultSocketType() {return addr_type;}
    
    void SetPositionCallback(MgenPositionFunc* callback,
                             const void*       clientData) 
    {
        get_position = callback;
        get_position_data = clientData;
    }
    void SetSinkPath(const char* theSinkPath) 
	{
		strncpy(sink_path,theSinkPath,strlen(theSinkPath) + 1);
	}
    void SetSinkBlocking(bool sinkNonBlocking) {sink_non_blocking = sinkNonBlocking;}
    void SetHostAddress(const ProtoAddress hostAddr)
    {
        host_addr = hostAddr;
    }
    void SetLogData(bool logData)
    {
      log_data = logData;
    }
    void ClearHostAddress()
    {
        host_addr.Invalidate();
    }
    
#ifdef HAVE_GPS
    void SetPayloadHandle(GPSHandle payloadHandle) 
    {
        payload_handle = payloadHandle;
    }
#endif // HAVE_GPS

    
    bool Start(); 
    void Stop();
    
    bool DelayedStart() {return start_timer.IsActive();}
    void GetStartTime(char* buffer)
    {
        sprintf(buffer, "%02d:%02d:%02.0f%s", 
                start_hour, start_min, start_sec,
                (start_gmt ? "GMT" : ""));
    }        
    bool ConvertBinaryLog(const char* path);  
    
    bool IsStarted () {return started;};
    
    MgenTransport* GetMgenTransport(Protocol theProtocol,
                                    UINT16 srcPort,
                                    const ProtoAddress& dstAddress,
                                    bool closedOnly = false,
                                    bool connect = false);

    MgenTransport* FindMgenTransport(Protocol theProtocol,
                                     UINT16 srcPort,
                                     const ProtoAddress& dstAddress,
                                     bool closedOnly,
                                     MgenTransport* mgenTransport);

    MgenTransport* FindMgenTransportBySocket(const ProtoSocket& socket);

    MgenTransport* FindTransportByInterface(const char*           interfaceName,
                                            UINT16        thePort = 0);

    bool LeaveGroup(MgenTransport* transport,
                    const ProtoAddress& groupAddress, 
                    const char*           interfaceName = NULL);
	    
    MgenTransport* JoinGroup(const ProtoAddress&   groupAddress, 
                             const char*           interfaceName,
                             UINT16                thePort);    

    /**
     * @class FastReader
     * 
     * @brief A little utility class for reading script files line by line.
     */
    class FastReader
    {
      public:
        enum Result {OK, ERROR_, DONE};  // trailing '_' for WIN32
        FastReader();
        FastReader::Result Read(FILE* filePtr, char* buffer, unsigned int* len);
        FastReader::Result Readline(FILE* filePtr, char* buffer, 
                                    unsigned int* len);
        FastReader::Result ReadlineContinue(FILE* filePtr, char* buffer, 
                                            unsigned int* len,
                                            unsigned int* lineCount = NULL);
        
      private:
        enum {BUFSIZE = 1024};
        char         savebuf[BUFSIZE];
        char*        saveptr;
        unsigned int savecount;
    };  // end class FastReader        

    ProtoAddress& GetHostAddr() {return host_addr;}
    MgenFlowList& GetFlowList() {return flow_list;}

    bool GetDefaultBroadcast() {return default_broadcast;}
    unsigned int GetDefaultTtl() {return default_ttl;}
    unsigned int GetDefaultTos() {return default_tos;}
    unsigned int GetDefaultTxBuffer() {return default_tx_buffer;}
    unsigned int GetDefaultRxBuffer() {return default_rx_buffer;}
    const char* GetDefaultMulticastInterface() 
    {return (('\0' != default_interface[0]) ? default_interface : NULL);}
    int GetDefaultQueuLimit() {return default_queue_limit;}
  private:
    // MGEN script command types ("global" commands)
    void SetDefaultBroadcast(bool broadcastValue, bool override)
    {
        default_broadcast = default_broadcast_lock ?
          (override ? broadcastValue : default_broadcast) :
          broadcastValue;
        default_broadcast_lock = override ? true : default_broadcast_lock;
    }
    void SetDefaultTtl(unsigned int ttlValue, bool override)
    {
        default_ttl = default_ttl_lock ?
          (override ? ttlValue : default_ttl) :
          ttlValue;
        default_ttl_lock = override ? true : default_ttl_lock;
    }
    void SetDefaultTos(unsigned int tosValue, bool override)
    {
        default_tos = default_tos_lock ?
          (override ? tosValue : default_tos) :
          tosValue;  
        default_tos_lock = override ? true : default_tos_lock;
    }
	void SetDefaultTxBufferSize(unsigned int bufferSize, bool override)
    {
        default_tx_buffer = default_tx_buffer_lock ?
          (override ? bufferSize : default_tx_buffer) :
          bufferSize;  
        default_tx_buffer_lock = override ? true : default_tx_buffer_lock;
    }
	void SetDefaultRxBufferSize(unsigned int bufferSize, bool override)
    {
        default_rx_buffer = default_rx_buffer_lock ?
          (override ? bufferSize : default_rx_buffer) :
          bufferSize;  
        default_rx_buffer_lock = override ? true : default_rx_buffer_lock;
    }
    void SetDefaultMulticastInterface(const char* interfaceName, bool override)
    {
        if (override || !default_interface_lock)
        {
            if (interfaceName)
              strncpy(default_interface, interfaceName, 16);
            else
              default_interface[0] = '\0'; 
        }
        default_interface_lock = override ? true : default_interface_lock;   
    }
    void SetDefaultQueueLimit(unsigned int queueLimitValue, bool override)
    {
        default_queue_limit = default_queue_limit_lock ?
          (override ? queueLimitValue : default_queue_limit) :
          queueLimitValue;
        default_queue_limit_lock = override ? true : default_queue_limit_lock;
    }

    // for mapping protocol types from script line fields
    static const StringMapper COMMAND_LIST[]; 
    
    bool OnStartTimeout(ProtoTimer& theTimer);
    bool OnDrecEventTimeout(ProtoTimer& theTimer);
    void ProcessDrecEvent(const DrecEvent& event);

    // Common state
	MgenController*    controller; // optional mgen controller
    ProtoSocket::Notifier&  socket_notifier;

    ProtoTimerMgr&     timer_mgr;
    char*              save_path;
    bool               save_path_lock;
    bool               started;
    
    ProtoTimer         start_timer;
    unsigned int       start_hour;            // absolute start time
    unsigned int       start_min;
    double             start_sec;
    bool               start_gmt;
    bool               start_time_lock;
    
    double             offset;
    bool               offset_lock;
    bool               offset_pending; 
    bool               checksum_force;       // force checksum validation at rcvr
    UINT32             default_flow_label;
    bool		       default_label_lock; 

    unsigned int       default_tx_buffer;                             
    unsigned int       default_rx_buffer;                             
    bool               default_broadcast;
    unsigned char      default_tos;                                        
    unsigned char      default_ttl;           // multicast ttl            
    char               default_interface[16]; // multicast interface name  
    int                default_queue_limit;
    // Socket state
    bool               default_broadcast_lock;
    bool               default_tos_lock;
    bool               default_ttl_lock;
    bool               default_tx_buffer_lock;     
    bool               default_rx_buffer_lock;     
    bool               default_interface_lock;
    bool               default_queue_limit_lock;
    
    char               sink_path[PATH_MAX];
    bool               sink_non_blocking;
    
    bool               log_data;
    ProtoAddress       host_addr;
    bool               checksum_enable;       
    
	MgenFlowList       flow_list;
    MgenTransportList  transport_list;
    
    // Drec state
    ProtoTimer         drec_event_timer;
    MgenEventList      drec_event_list;
    DrecEvent*         next_drec_event;      // for iterating drec_event_list
    DrecGroupList      drec_group_list;
    ProtoAddress::Type addr_type;
    
    MgenPositionFunc*  get_position;
    const void*        get_position_data;
#ifdef HAVE_GPS
    GPSHandle          payload_handle;
#endif // HAVE_GPS   

  protected:
    FILE*              log_file;
    bool               log_binary;
    bool               local_time;
    bool               log_flush;
    bool               log_file_lock;
    bool               log_tx;
    bool               log_open;
    bool               log_empty;
    
}; // end class Mgen 

#endif  // _MGEN
