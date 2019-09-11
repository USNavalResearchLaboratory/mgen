#include "nsMgenAgent.h"
static class NsMgenAgentClass : public TclClass
{
	public:
		NsMgenAgentClass() : TclClass("Agent/MGEN") {}
	 	TclObject* create(int argc, const char*const* argv) 
			{return (new NsMgenAgent());}
} class_mgen_agent;	

NsMgenAgent::NsMgenAgent()
  : mgen(GetTimerMgr(), GetSocketNotifier())

{
    mgen.SetPositionCallback(NsMgenAgent::GetPosition, this);    
    mgen.SetLogFile(stdout);  // log to stdout by default

    sink_transport = MgenSinkTransport::Create(mgen);
    sink_transport->SetSink(this);
}

NsMgenAgent::~NsMgenAgent()
{
}

bool NsMgenAgent::OnStartup(int argc, const char*const* argv)
{
    if (ProcessCommands(argc, argv))
    {
        return mgen.Start();
    }
    else
    {
        fprintf(stderr, "NsMgenAgent::OnStartup() error processing commands\n");
        return false;
    }
} // end NsMgenAgent::OnStartup()

void NsMgenAgent::OnShutdown()
{
    mgen.Stop();   
}

bool NsMgenAgent::ProcessCommands(int argc, const char*const* argv)
{
    int i = 1;
    while (i < argc)
    {
        // Intercept some "special" commands
        if (!strcmp(argv[i], "start"))
        {
            if (OnStartup(argc-i, argv+i))
            {
                return true;
            }
            else
            {
                fprintf(stderr, "NsMgenAgent::command(startup) error\n");
                return false;   
            }
        }
        else if (!strcmp(argv[i], "stop"))
        {
            OnShutdown();
            i++;
            continue;
        }
        else if (!strcmp(argv[i], "sink"))
        {
            if (++i >= argc)
            {
                fprintf(stderr, "NsMgenAgent::command(sink) insufficient arguments\n");
                return TCL_ERROR;   
            }
            
            ProtoAddress dstAddress;
            MgenTransport* mgenTransport = mgen.FindMgenTransport(SINK,0,dstAddress,false,NULL);
            
            if (mgenTransport) 
            {
                DMSG(0,"NsMgenAgent::ProcessCommands() Only one sink can be in use.  Creating new sink..\n");
                mgen.GetTransportList().Remove(mgenTransport);
            }
            
            
            Tcl& tcl = Tcl::instance();

            ProtoMessageSink* msgSink = dynamic_cast<ProtoMessageSink*> (tcl.lookup(argv[i]));
            
            if (NULL == msgSink)
            {
                Agent* agent = dynamic_cast<Agent*> (tcl.lookup(argv[i]));
                if (NULL != agent)
                    msgSink = static_cast<ProtoMessageSink*>(new NsGenericAgentSink(*agent));
            }
            
            if (NULL != msgSink)
            {
                sink_transport->SetSink(msgSink);
                mgen.GetTransportList().Prepend(sink_transport);
                i++;
                continue;
            }
            else
            {
                fprintf(stderr, "NsMgenAgent::command(sink) invalid argument\n");
                return TCL_ERROR;
            }
            
        }
        switch(Mgen::GetCmdType(argv[i]))
        {    
        case Mgen::CMD_INVALID:
          return false;
          
        case Mgen::CMD_NOARG:
          if (!mgen.OnCommand(Mgen::GetCommandFromString(argv[i]), NULL, true))
          {
              fprintf(stderr, "NsMgenAgent::ProcessCommands(%s) error\n", 
                      argv[i]);
              return TCL_ERROR;
          }
          i++;
          break;
          
        case Mgen::CMD_ARG:
          if (!mgen.OnCommand(Mgen::GetCommandFromString(argv[i]), argv[i+1], true))
          {
              fprintf(stderr, "NsMgenAgent::ProcessCommands(%s, %s) error\n", 
                      argv[i], argv[i+1]);
              return false;
          }
          i += 2;
          break;
        }
    }  // end while(i < argc)
    return true;
}  // end NsMgenAgent::ProcessCommands()

bool NsMgenAgent::GetPosition(const void* agentPtr, GPSPosition& gpsPosition)
{
    // (TBD) get node's position
    return true;   
}  // end NsMgenAgent::GetPosition()

MgenSinkTransport* MgenSinkTransport::Create(Mgen& theMgen)
{
    return static_cast<MgenSinkTransport*>(new MgenSimSinkTransport(theMgen,SINK));
}
