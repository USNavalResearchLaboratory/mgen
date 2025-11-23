#ifndef _OPNET_MGEN_PROCESS
#define _OPNET_MGEN_PROCESS

// JPH MGEN 11/21/2005

#include "opnet.h"
#include "opnetProtoSimProcess.h"
#include "mgen.h" // this includes protokit.h


// This class lets us "sink" MGEN messages to a
// generic opnet transport "Agent"
class OpMgenSink : public MgenSink
{
    public:
        OpMgenSink();
        ~OpMgenSink();
        
        virtual bool SendMgenMessage(const char*           txBuffer,
                                     unsigned int          len,
                                     const ProtoAddress&   dstAddr);
	
		virtual bool Open();		
		virtual bool Close();
	
	
};  // end class OpMgenSink


class OpnetMgenProcess : public OpnetProtoSimProcess
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

		
        void SetSink(MgenSink* theSink) {mgen.SetSink(theSink);}
        void HandleMgenMessage(char*               buffer, 
                               unsigned int        len, 
                               const ProtoAddress& srcAddr)
        	{
            mgen.HandleMgenMessage(buffer, len, srcAddr);   
			}
		            
        bool SendMgenMessage(const char*           txBuffer,
                                     unsigned int          len,
                                     const ProtoAddress&   dstAddr);
		
		void OnTcpSocketEvent(ProtoSocket& theSocket,ProtoSocket::Event theEvent)
		{
			mgen.OnTcpSocketEvent(theSocket,theEvent);
		}
			
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

};  // end class OpnetMgenProcess


#endif // _OPNET_MGEN_PROCESS


