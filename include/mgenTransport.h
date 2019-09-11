#ifndef _MGEN_TRANSPORT_LIST
#define _MGEN_TRANSPORT_LIST
#include "protokit.h"
#include "protoChannel.h"
#include "mgenGlobals.h"
#include "mgenMsg.h"
#include "mgenEvent.h"

class MgenController;
class MgenFlowList;
class MgenFlow;
class Mgen;
class MgenTransport;
/**
 * @class MgenTransportList
 *
 * @brief Maintains list of transport items (e.g. sockets, pipes) 
 * for transmission, reception,
 * and multicast group join/leave. 
*/
class MgenTransportList
{
    friend class Mgen;

  public:
    
    enum
    {
      DEFAULT_TTL = 3,  // default multicast ttl
      DEFAULT_TOS = 0   // default type of service
    };   
    
    MgenTransportList();
    virtual ~MgenTransportList();
    void Destroy();

    // nsMgenAgent accesses these... - make them friends?
    void Prepend(MgenTransport* transport);
    void Remove(MgenTransport* transport);
    
  protected:
    void Append(MgenTransport* transport);

  private:
    MgenTransport*          head;                  
    MgenTransport*          tail;    
};  // end class MgenTransportList

/**
 * @class MgenTransport
 *
 * @brief Base class for mgen transport types.
 */

class MgenTransport
{ 
    friend class MgenTransportList;
    friend class Mgen;

  public:
    MgenTransport(Mgen& theMgen,Protocol theProtocol);
    MgenTransport(Mgen& theMgen,Protocol theProtocol,UINT16 thePort);
    MgenTransport(Mgen& theMgen,Protocol theProtocol,UINT16 thePort, const ProtoAddress& theAddress);
    virtual ~MgenTransport();
    
    // ljt probably should make sure we need all
    // of these now...
    virtual bool IsSocketTransport() {return false;}
    virtual bool OwnsSocket(const ProtoSocket& theSocket) {return false;}
    virtual UINT16 GetSocketPort() {return 0;}
    // TBD add client state to udp sockets?
    virtual bool IsClient() {return true;} 
    virtual void IsClient(bool isClient) {;}
    virtual bool IsConnected() {return true;}
    virtual bool IsConnecting() {return false;}
    virtual bool IsListening() {return true;}
    virtual bool Listen(UINT16 port,ProtoAddress::Type addrType,bool bindOnOpen) {return false;}
    virtual bool SetRxBufferSize(unsigned int rxBufferSize) {return false;} // for multicast joins in mgen
    virtual void SetEventOptions(const MgenEvent* theEvent) {;}
    virtual bool IsTransmitting() {return false;}
    virtual bool TransmittingFlow(UINT32 flowId) {return false;}
    virtual unsigned int GroupCount() {return 0;}
    virtual bool JoinGroup(const ProtoAddress& groupAddress, 
			               const ProtoAddress& sourceAddress,
                           const char* interfaceName = NULL) {return false;}
    virtual ProtoAddress::Type GetAddressType() { return ProtoAddress::INVALID; }
    virtual bool LeaveGroup(const ProtoAddress& theAddress, 
			    const ProtoAddress& sourceAddress,
                            const char* interfaceName = NULL) {return false;}
#ifdef HAVE_IPV6	  
    virtual void SetFlowLabel(UINT32 label) {;}
#endif //HAVE_IPV6

    // virtual functions
    virtual bool Open(ProtoAddress::Type addrType, bool bindOnOpen) = 0;

    virtual bool IsOpen() = 0;
    virtual void Close() = 0;
    virtual bool HasListener() = 0;
    virtual bool SendMessage(MgenMsg& theMsg,
                             const ProtoAddress& dst_addr,
                             char* txBuffer) = 0;
    virtual bool StartOutputNotification() {return true;}
    virtual void StopOutputNotification() {;}
    virtual bool StartInputNotification() {return true;}
    virtual void StopInputNotification() {;}
    virtual bool Shutdown() {return false;}

    // base class implementation
    UINT16 GetSrcPort() {return srcPort;}
    ProtoAddress& GetDstAddr() {return dstAddress;}
    void PrintList(); // ljt
    bool SendPendingMessage();
    void RemoveFromPendingList();
    void LogEvent(LogEventType theEvent,MgenMsg* theMsg,const struct timeval& theTime,char* buffer = NULL);
    Protocol GetProtocol() {return protocol;}
    void SetDstAddr(const ProtoAddress& theAddress) {dstAddress = theAddress;}

    void AppendFlow(MgenFlow* const theFlow);
    void RemoveFlow(MgenFlow* const theFlow);
    bool HasPendingFlows()
    {
        if (pending_head) return true;
        return false;
    }
    void DecrementReferenceCount() 
    {
        reference_count--;
    }
    int GetReferenceCount() {return reference_count;}
    static ProtoSocket::Protocol GetSocketProtocol(Protocol theProtocol)
    {
        switch (theProtocol)
        {
        case UDP:
          return ProtoSocket::UDP;
          break;
        case TCP:
          return ProtoSocket::TCP;
          break;
        default:
          DMSG(0,"SocketTransport::SocketTransport() Error: Invalid protocol specified.\n");
          return ProtoSocket::INVALID_PROTOCOL;
          break;
        }
        DMSG(0,"SocketTransport::SocketTransport() Error: Invalid protocol specified.\n");		
        return ProtoSocket::INVALID_PROTOCOL;
    }
    int GetMessagesSent() {return messages_sent;}
    void SetMessagesSent(int messagesSent) {messages_sent = messagesSent;}
    Mgen& GetMgen() {return mgen;}
  private: 
    MgenTransport*  prev;  
    MgenTransport*  next;  
  protected:	      
    UINT16  srcPort;
    UINT16  dstPort;
    ProtoAddress    dstAddress;
    Protocol        protocol;
    unsigned int    reference_count; //mgen flows and drec-multicast ports
    Mgen&           mgen;
    MgenFlow*       pending_head;
    MgenFlow*       pending_tail;
    MgenFlow*       pending_current;
    int             messages_sent;
};  // end class MgenTransport

/** 
 * @class MgenSocketTransport
 *
 * @brief Base class for mgen socket transports.
 */
class MgenSocketTransport : public MgenTransport
{    
  public:
    bool IsOpen() {return socket.IsOpen();};
    bool Open(ProtoAddress::Type addrType, bool bindOnOpen);
    void Close();
    bool StartOutputNotification() 
    {
      return socket.StartOutputNotification(); 
    };
    void StopOutputNotification()
    {
      socket.StopOutputNotification();
    }
    bool StartInputNotification() 
    {
      return socket.StartInputNotification();
    };
    void StopInputNotification()
    {
      socket.StopInputNotification();
    }
    bool IsSocketTransport() {return true;}
    UINT16 GetSocketPort() {return socket.GetPort();}
    bool OwnsSocket(const ProtoSocket& theSocket) 
    {
        if (&socket == &theSocket)
          return true;
        return false;
    }
    virtual void SetEventOptions(const MgenEvent* theEvent); 
    
#ifdef HAVE_IPV6	  
    void SetFlowLabel(UINT32 label) 
    {
        if (!socket.SetFlowLabel(label)) 
          DMSG(0,"MgenFlow::OnTxTimeout cannot set flow label\n");
    }
#endif // HAVE_IPV6
    bool SetTOS(unsigned char tos);
    bool SetBroadcast(bool broadcast);
    bool SetTxBufferSize(unsigned int txBufferSize);
    bool SetRxBufferSize(unsigned int rxBufferSize);
    virtual bool SetTTL(unsigned char TTL);
    virtual bool SetMulticastInterface(const char* interfaceName) 
    {
        if (interfaceName != NULL && '\0' != interfaceName[0])
          strcpy(interface_name,interfaceName);
        return true;
    }    
    bool HasListener() {return (socket.HasListener());}

  private:
  protected:
    MgenSocketTransport(Mgen& mgen,Protocol theProtocol,UINT16 thePort);
    MgenSocketTransport(Mgen& mgen,Protocol theProtocol,UINT16 thePort, const ProtoAddress& theAddress);
    
    virtual ~MgenSocketTransport();
    bool Listen(UINT16 port,ProtoAddress::Type addrType, bool bindOnOpen);
    ProtoAddress::Type GetAddressType() { return socket.GetAddressType(); }

    bool            broadcast;
    unsigned char   tos;
    unsigned int    tx_buffer;
    unsigned int    rx_buffer;        	  
    unsigned char   ttl;   // multicast time-to-live
    char            interface_name[16];
    ProtoSocket     socket;
    
}; // end class MgenTransport::MgenSocketTransport

/**
 * @class MgenUdpTransport
 *
 * @brief Mgen UDP transport class.
 */
class MgenUdpTransport : public MgenSocketTransport
{
  public:
    MgenUdpTransport(Mgen& mgen,Protocol theProtocol, UINT16 thePort);
    MgenUdpTransport(Mgen& mgen,Protocol theProtocol, UINT16 thePort, const ProtoAddress& theDstAddress);
    
    ~MgenUdpTransport();
    void OnEvent(ProtoSocket& theSocket,ProtoSocket::Event theEvent);
    bool Open(ProtoAddress::Type addrType, bool bindOnOpen);    
    bool JoinGroup(const ProtoAddress& groupAddress, 
		   const ProtoAddress& sourceAddress,
                   const char* interfaceName = NULL);
    bool LeaveGroup(const ProtoAddress& theAddress, 
		    const ProtoAddress& sourceAddress,
                    const char* interfaceName = NULL);
    bool SendMessage(MgenMsg& theMsg,const ProtoAddress& dst_addr,char* txBuffer);
    bool Listen(UINT16 port,ProtoAddress::Type addrType, bool bindOnOpen);
    
    unsigned int GroupCount() {return group_count;}

    bool SetMulticastInterface(const char* interfaceName);
    const char* GetMulticastInterface()
    {
        return (('\0' != interface_name[0]) ? 
                interface_name : NULL);
    }
    virtual bool IsConnected() 
      {
          if (connect)
          {
              if (socket.GetState() == ProtoSocket::CONNECTED)
                return true;
              else
                return false;
          }
          else
            return true;
      }

  private:	  
    unsigned int    group_count;	  
    bool            connect;
}; // end class MgenUdpTransport

/**
 * @class MgenTcpTransport
 *
 * @brief Mgen TCP transport class
 */
class MgenTcpTransport : public MgenSocketTransport
{
  public:
    MgenTcpTransport(Mgen& mgen,Protocol theProtocol, UINT16 thePort);
    MgenTcpTransport(Mgen& mgen,Protocol theProtocol,UINT16 thePort, const ProtoAddress& theAddress);
    ~MgenTcpTransport();

    void OnEvent(ProtoSocket& theSocket,ProtoSocket::Event theEvent);
    bool Open(ProtoAddress::Type addrType, bool bindOnOpen);
    bool Listen(UINT16 port,ProtoAddress::Type addrType, bool bindOnOpen);
    bool Accept(ProtoSocket& theSocket);
    void ShutdownTransport(LogEventType eventType);
    bool Shutdown()
    {
        if (!reference_count)
            return false;

        // We're the last reference to the socket so we can
        // shut it down.
        if (reference_count == 1 && (socket.IsConnected() || socket.IsListening()))
		{	
		  return socket.Shutdown();
		}
	else
		{
		  reference_count--;
		  return false; // socket not shutdown 
		}
    }
    bool IsConnected() {return socket.IsConnected();}
    bool IsConnecting() {return socket.IsConnecting();}
    bool IsListening() {return socket.IsListening();}
    void OnRecvMsg(unsigned int numBytes,unsigned int bufferIndex,const char* buffer);
    bool SendMessage(MgenMsg& theMsg,const ProtoAddress& dst_addr,char* txBuffer);
    bool GetNextTxBuffer(unsigned int numBytes);
    void SetupNextTxBuffer();
    UINT16 GetNextTxFragment();
    UINT16 GetNextTxFragmentSize();
    unsigned int GetRxNumBytes(unsigned int bufferIndex);
    void CopyMsgBuffer(unsigned int numBytes,unsigned int bufferIndex,const char* buffer);
    void CalcRxChecksum(const char* buffer,unsigned int bufferIndex,unsigned int numBytes);
    void CalcTxChecksum(); 
    bool IsTransmitting() 
    {
	    if (tx_msg.GetMsgLen()) 
          if (tx_msg.GetMgenMsgLen() - tx_msg_offset > 0 || tx_fragment_pending)
                return true;
	    return false;
    }    
    bool TransmittingFlow(UINT32 flowId) 
    {
	    if ((tx_msg.GetFlowId() == flowId)
            && IsTransmitting())
	      return true;
        
	    return false;
    }
    void ResetTxMsgState();
    void ResetRxMsgState();
    bool IsClient() {return is_client;};
    void IsClient(bool isClient) {is_client = isClient;};
  private:
    bool                    is_client;
    MgenMsg                 tx_msg;
    char                    tx_msg_buffer[TX_BUFFER_SIZE];
    unsigned int            tx_buffer_index;
    unsigned int            tx_buffer_pending;
    unsigned int            tx_msg_offset;
    UINT16                  tx_fragment_pending;
    UINT32                  tx_checksum;
    struct timeval          tx_time; // send time of first tcp fragment
	
    MgenMsg                 rx_msg;
    char                    rx_msg_buffer[TX_BUFFER_SIZE];
    unsigned int            rx_buffer_index;
    char                    rx_checksum_buffer[4];
    UINT16                  rx_fragment_pending;
    UINT16                  rx_msg_index;
    UINT32                  rx_checksum;
	
}; // end class MgenTcpTransport

/**
 * @class MgenSinkTransport
 *
 * @brief Mgen sink transport base class
 */
class MgenSinkTransport : public MgenTransport
{
  public:
    MgenSinkTransport(Mgen& mgen,Protocol theProtocol);
    MgenSinkTransport(Mgen& mgen,Protocol theProtocol, UINT16 thePort, const ProtoAddress& theDstAddress);
    ~MgenSinkTransport();

    // MgenSinkTransport implementation
    static MgenSinkTransport* Create(Mgen& theMgen, Protocol theProtocol);  // SINK or SOURCE
    bool IsOpen() {return true;} 
    void Close() {return;} 
    bool HasListener() {return true;}
    void SetPath(char* theSinkPath) {

		 strncpy(path,theSinkPath,(strlen(theSinkPath) + 1));
	}
    // sink open
    bool Open(ProtoAddress::Type addrType, bool bindOnOpen);
    // source open
    bool Open();
    bool OnOutputReady();
    void HandleMgenMessage(const char* buffer, unsigned int len,const ProtoAddress& srcAddr);
    bool OnInputReady();
    char* GetMsgBuffer() {return msg_buffer;}
    int GetMsgLength() {return msg_length;}
    void SetMsgLength(int inLength) {msg_length = inLength;}
    void SetMsgIndex(int inIndex) {msg_index = inIndex;}
    void SetSinkBlocking(bool sinkNonBlocking) {sink_non_blocking = sinkNonBlocking;}
    void SetSource(bool inVal) {is_source = inVal;}
    
    virtual void SetSink(class ProtoMessageSink* theSink) = 0; // ljt

  protected:
    bool sink_non_blocking;
    char path[PATH_MAX];
    bool is_source;
	enum {BUFFER_MAX = 8192}; // ljt
    UINT32                      msg_length;
    UINT32                      msg_index;
    char                        msg_buffer[MAX_SIZE];
    
}; // end class MgenTransport::MgenSinkTransport

/**
 * @class MgenAppSinkTransport
 *
 * @brief Mgen sink transport for used in mgen applications.
 */

class MgenAppSinkTransport : public MgenSinkTransport, public ProtoChannel
{
  public:
    MgenAppSinkTransport(Mgen& mgen,Protocol theProtocol);
    MgenAppSinkTransport(Mgen& mgen,Protocol theProtocol, UINT16 thePort, const ProtoAddress& theDstAddress);

     ~MgenAppSinkTransport();

    // MgenSinkTransport implementation
    bool IsOpen() {return ProtoChannel::IsOpen();}
    void Close() {ProtoChannel::Close();}
    bool StartOutputNotification() {return ProtoChannel::StartOutputNotification();}
    void StopOutputNotification() {ProtoChannel::StopOutputNotification();}
    bool StartInputNotification() {return ProtoChannel::StartInputNotification();}
    void StopInputNotification() {ProtoChannel::StopInputNotification();}
    bool HasListener() {return ProtoChannel::HasListener();};
    // sink open
    bool Open(ProtoAddress::Type addrType, bool bindOnOpen);
    // source open
    bool Open();
    bool OnOutputReady();
	bool Write(char* buffer, unsigned int* nbytes);
	bool SendMessage(MgenMsg& theMsg,const ProtoAddress& dst_addr,char* txBuffer); 
    bool OnInputReady();
	bool Read(char* buffer, UINT32 nBytes, UINT32& bytesRead);
	void OnEvent(ProtoChannel& theChannel,ProtoChannel::Notification theNotification);
    void SetSink(class ProtoMessageSink* theSink) {;}

 private:

#ifdef WIN32
    ProtoDispatcher::Descriptor     descriptor;
	OVERLAPPED  write_overlapped; 
#endif // WIN32
    
    
}; // end class MgenTransport::MgenAppSinkTransport

#endif // _MGEN_TRANSPORT_LIST
