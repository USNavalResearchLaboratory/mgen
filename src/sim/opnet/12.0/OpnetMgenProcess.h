#ifndef _OPNET_MGEN_PROCESS
#define _OPNET_MGEN_PROCESS

// JPH MGEN 11/21/2005

#include "opnet.h"
#include "opnetProtoSimProcess.h"
#include "mgen.h" // this includes protokit.h
#include "mgenTransport.h"



// This class lets us "sink" MGEN messages to a
// generic opnet transport "Agent"
class OpMgenSink : public MgenSinkTransport
{
    public:
        OpMgenSink(Mgen& mgen,Protocol theProtocol);
        ~OpMgenSink();
	
		bool SendMessage(MgenMsg& theMsg,const ProtoAddress& dst_addr,char* txBuffer);
		bool Open();
		void Close();
    	void SetSink(ProtoMessageSink* theSink) {msg_sink = theSink;}
    	ProtoMessageSink* msg_sink;
        	
};  // end class OpMgenSink


class OpnetMgenProcess : public OpnetProtoSimProcess, public OpnetProtoMessageSink
{
    public:
		OpnetMgenProcess();
		~OpnetMgenProcess();
	
		// OpnetProtoSimProcess's base class overrides
		bool OnStartup(int argc, const char*const* argv);
		bool ProcessCommands(int argc, const char*const* argv);
		void OnShutdown();
        void ReceivePacketMonitor(Ici* ici, Packet* pkt);
        void TransmitPacketMonitor(Ici* ici, Packet* pkt);
	
        void SetHostAddress(const ProtoAddress hostAddr)
        {
            mgen.SetHostAddress(hostAddr);
        }
        void ClearHostAddress()
        {
            mgen.ClearHostAddress();
        }
		
		//void OnTcpSocketEvent(ProtoSocket& theSocket,ProtoSocket::Event theEvent)
		//{
		//	tcp_transport->OnEvent(theSocket,theEvent);
		//}
		
		//void SetTcpTransport(MgenTcpTransport*  tcp_ptr)
		//{
		//	tcp_transport = tcp_ptr;
		//}
		
		//Mgen& GetMgen (){return mgen;}
		
			
    private:
		enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
        static bool GetPosition(const void* agentPtr, GPSPosition& gpsPosition);
        static CmdType GetCmdType(const char* string);
        bool OnCommand(const char* cmd, const char* val);
	
		static const char* const CMD_LIST[];
        bool              have_ports;
        bool              convert;
        char              convert_path[PATH_MAX];
        bool              sink_non_blocking;
        char              ifinfo_name[64];
        UINT32            ifinfo_tx_count;
        UINT32            ifinfo_rx_count;
	    Mgen              mgen;         // Mgen engine
        MgenSinkTransport* 	sink_transport; 
		MgenTcpTransport*	tcp_transport;

};  // end class OpnetMgenProcess


#endif // _OPNET_MGEN_PROCESS


