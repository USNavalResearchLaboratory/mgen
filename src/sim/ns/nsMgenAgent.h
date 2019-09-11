#ifndef _NS_MGEN_AGENT
#define _NS_MGEN_AGENT
#include "nsProtoSimAgent.h"   // from Protolib
#include "mgen.h"
#include "mgenSimSinkTransport.h"

class NsMgenAgent : public NsProtoSimAgent, public ProtoMessageSink
{
    public:
        NsMgenAgent();
        ~NsMgenAgent();
                
        // NsProtoSimAgent base class overrides
        virtual bool OnStartup(int argc, const char*const* argv);
        virtual bool ProcessCommands(int argc, const char*const* argv);
        virtual void OnShutdown();
       

        Mgen* GetMgenInstance() {return &mgen;}
        bool HandleMessage(const char* txBuffer,unsigned int len,const ProtoAddress& srcAddr)
        {
            sink_transport->HandleMgenMessage(txBuffer,len,srcAddr);
            return true;
        }
            
    private:  
        static bool GetPosition(const void* agentPtr, GPSPosition& gpsPosition);
        Mgen                mgen;
        MgenSinkTransport* sink_transport;
 };  // end class NsMgenAgent

class NsGenericAgentSink : public ProtoMessageSink
{
    public:
        NsGenericAgentSink(Agent& theAgent) : agent(theAgent) {}
        
        bool HandleMessage(const char* txBuffer, unsigned int len,const ProtoAddress& srcAddr)
        {
            agent.send(len);
            return true;
        }
    
    private:
        Agent&  agent;
};  // end class NsGenericAgentSink

#endif // _NS_MGEN_AGENT
