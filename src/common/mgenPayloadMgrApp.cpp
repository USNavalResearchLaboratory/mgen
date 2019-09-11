#include "mgenPayloadMgr.h"
#include "mgenPayloadMgrApp.h"
#include "mgen.h"

MgenPayloadMgrApp::MgenPayloadMgrApp()
  : mgenPayloadMgr(GetTimerMgr(), GetSocketNotifier(),
		   mgen),
    mgen(GetTimerMgr(), GetSocketNotifier()),
    control_pipe(ProtoPipe::MESSAGE), control_remote(false),
    socket(ProtoSocket::UDP)
{
  mgen.SetController(this);
  control_pipe.SetNotifier(&GetSocketNotifier());
  control_pipe.SetListener(this,&MgenPayloadMgrApp::OnControlEvent);

  UINT16 port = 55000;

  dstAddr.ResolveFromString("127.0.0.1");
  dstAddr.SetPort(port);
  // Set up our output socket 
  socket.SetNotifier(&mgen.GetSocketNotifier());
  if (!socket.Open(port,ProtoAddress::IPv4,false))
    {
      DMSG(0,"MgenPayloadMgrApp::Open() Error: Socket open error %s srcPort %d\n",GetErrorString(),port);
    }

} // end MgenPayloadMgrApp::MgenPayloadMgrApp

const char* const MgenPayloadMgrApp::CMD_LIST[] = 
  {
    "+instance",    // mgenPayloadMgrApp instance name
    "+error",       // mgen error
    "-stop",        // exit program instance
    "-version",     // print MgenPayloadMgr version and exit
    "-help",        // print usage and exit
    NULL
  };

void MgenPayloadMgrApp::Usage()
{
    fprintf(stderr,"mgenPayloadMgr\n [input <scriptFile>]\n"
            "[overwrite_mgenlog <logFile>|mgenlog <logFile>]\n"
            "[overwrite_mpmlog <logFile>|mpmlog <logFile>]\n"
            "[event \"<mgen event>\"]\n"
	    "[instance <name>]\n"
            "[start <startTime>]\n"
            "[verbose]\n"
            "[txlog] [nolog] [flush]\n"
            "[localtime]\n"
            "[interface <name>]\n"
            "[ttl <timeToLive>]\n"
            "[tos <typeOfService>]\n"
            "[txbuffer <bufferSize>]\n"
            "[rxbuffer <bufferSize>]\n"
            "[txcheck] [rxcheck] [check]\n");

} // end MgenPayloadMgrApp::Usage

MgenPayloadMgrApp::~MgenPayloadMgrApp()
{
}

bool MgenPayloadMgrApp::OnStartup(int argc, const char*const* argv)
{
  mgen.SetLogFile(stdout); // log to stdout by default
  mgenPayloadMgr.SetLogFile(stdout);

#ifdef HAVE_IPV6    
    if (ProtoSocket::HostIsIPv6Capable()) 
      mgen.SetDefaultSocketType(ProtoAddress::IPv6);
#endif // HAVE_IPV6
    
    if (!ProcessCommands(argc, argv))
      {
	fprintf(stderr,"MgenPayloadMgrApp::OnStartup() error while processing startup commands\n");
	return false;
      }
    
    if (control_remote)
      {
	// We remoted commands, so we're finished...
	return false;
      }

    return mgenPayloadMgr.Start();
}
void MgenPayloadMgrApp::OnControlEvent(ProtoSocket& /*theSocket*/,
                             ProtoSocket::Event theEvent)
{
    if (ProtoSocket::RECV == theEvent)
    {
        char buffer[8192];
        unsigned int len = 8191;
        if (control_pipe.Recv(buffer, len))
        {
            // (TBD) Delimit commands by line breaks and/or other delimiters ???
            buffer[len] = '\0';
            char* cmd = buffer;
            char* arg = NULL;
            for (unsigned int i = 0; i < len; i++)
            {
                if ('\0' == buffer[i])
                {
                    break;
                }
                else if (isspace(buffer[i]))
                {
                    buffer[i] = '\0';
                    arg = buffer+i+1;
                    break;
                }
            }
            if (!OnCommand(cmd, arg)) 
            {
                DMSG(0, "MgenPayloadMgrApp::OnControlEvent() error processing command\n");
            }
        }
        else
        {
            DMSG(0, "MgenPayloadMgrApp::OnControlEvent() receive error\n"); 
        }   
    }
    else
    {
        DMSG(0, "MgenPayloadMgrApp::OnControlEvent() unhandled event type\n");  
    }
}  // end MgenPayloadMgrApp::OnControlEvent


void MgenPayloadMgrApp::OnShutdown()
{
  control_pipe.Close();
  mgenPayloadMgr.Stop();
  mgen.Stop();
}
MgenPayloadMgrApp::CmdType MgenPayloadMgrApp::GetCmdType(const char* cmd)
{
    if (!cmd) return CMD_INVALID;
    unsigned int len = strlen(cmd);
    bool matched = false;
    CmdType type = CMD_INVALID;
    const char* const* nextCmd = CMD_LIST;
    while (*nextCmd)
    {
        if (!strncmp(cmd, *nextCmd+1, len))
        {
            if (matched)
            {
                // ambiguous command (command should match only once)
                return CMD_INVALID;
            }
            else
            {
                matched = true;   
                if ('+' == *nextCmd[0])
                  type = CMD_ARG;
                else
                  type = CMD_NOARG;
            }
        }
        nextCmd++;
    }
    return type; 
}  // end MgenPayloadMgrApp::GetCmdType()

bool MgenPayloadMgrApp::ProcessCommands(int argc, const char*const* argv)
{
    int i = 1;
    while (i < argc)
    {
      CmdType cmdType = GetCmdType(argv[i]);
      if (CMD_INVALID == cmdType)
        {
	  // Is it a mgenPayloadMgr command?
	  switch(MgenPayloadMgr::GetCmdType(argv[i]))
            {
            case MgenPayloadMgr::CMD_INVALID:
              break;
            case MgenPayloadMgr::CMD_NOARG:
              cmdType = CMD_NOARG;
              break;
            case MgenPayloadMgr::CMD_ARG:
              cmdType = CMD_ARG;
              break;
            }
        }
      switch (cmdType)
        {
        case CMD_INVALID:
          DMSG(0,"MgeyPayloadMgrApp::ProcessCommands() Error: invalid command: %s\n",
               argv[i]);
          return false;
        case CMD_NOARG:
          if (!OnCommand(argv[i],NULL))
	    {
              DMSG(0,"MgenPayloadMgrApp::ProcessCommands() Error: OnCommand(%s) error\n",
                   argv[i]);
              return false;
	    }
          i++;
          break;
        case CMD_ARG:
          if (!OnCommand(argv[i],argv[i+1]))
	    {
              DMSG(0,"MgenPayloadMgrApp::ProcessCommands() Error: OnCommand(%s, %s) error\n",
                   argv[i],argv[i+1]);
              return false;	      
	    }
          i += 2;
          break;
        }
      
    }
    return true;
}
bool MgenPayloadMgrApp::OnCommand(const char* cmd, const char* val)
{

    if (control_remote)
    {
        char buffer[8192];
        strcpy(buffer, cmd);
        if (val)
        {
            strcat(buffer, " ");
            strncat(buffer, val, 8192 - strlen(buffer));
        }
        unsigned int len = strlen(buffer);
        if (control_pipe.Send(buffer, len))
        {
            return true;
        }
        else
        {
            DMSG(0, "MgenPayloadMgrApp::ProcessCommand(%s) error sending command to remote process\n", cmd);    
            return false;
        }        
    }
  // Is it a mgenPayloadMgrApp command?  
  CmdType type = GetCmdType(cmd);
  unsigned int len = strlen(cmd);
  if (CMD_INVALID == type)
    {
      if (MgenPayloadMgr::CMD_INVALID != MgenPayloadMgr::GetCmdType(cmd))
        {
	  return mgenPayloadMgr.OnCommand(MgenPayloadMgr::GetCommandFromString(cmd),val,true);
        }
        else {
            
            DMSG(0,"MgenPayloadMgrApp::OnCommand(%s) Error: invalid command \n",cmd);
            return false;
        }
    }
    else if ((CMD_ARG == type) && !val)
    {
        DMSG(0,"MgenPayloadMgrApp::OnCommand(%s) Error: missing argument\n",cmd);
        return false;
    }
    else if (!strncmp("stop",cmd,len))
    {
        Stop();
    }
    else if (!strncmp("instance", cmd, len))
    {
        if (control_pipe.IsOpen())
            control_pipe.Close();
        if (control_pipe.Connect(val))
	  {
            control_remote = true;
        }        
        else if (!control_pipe.Listen(val))
        {
            DMSG(0, "MgenPayloadMgrApp::ProcessCommand(instance) error opening control pipe\n");
            return false; 
        }
    }
    else if (!strncmp("error",cmd,len))
    {
        char msgBuffer[512];
        sprintf(msgBuffer,"type>Error action>mgenCmd cmd>\"%s\"",val);
        mgenPayloadMgr.LogEvent("MGEN",msgBuffer);
        
        DMSG(0,"MgenPayloadMgrApp::Oncommand() Error: received from mgen for command: (%s)\n",val);
        
        return true;
    }
    else if (!strncmp("-version",cmd,len) || !strncmp("-v",cmd,len))
    {
        fprintf(stderr,"version %s\n",MGENPAYLOADMGR_VERSION);
        exit(0);
        //      return false;
    }
    
    else if (!strncmp("help",cmd,len))
    {
        fprintf(stderr,"version %s\n",MGENPAYLOADMGR_VERSION);
        Usage();
        return false;
    }
    return true;
} // end MgenPayloadMgrApp::OnCommand()



void MgenPayloadMgrApp::OnOffEvent(char* buffer, int len)
{
  DMSG(0,"MgenPayloadMgrApp::OnOffEvent %s\n",buffer);

} // end MgenPayloadMgrApp::OnOffEvent
void MgenPayloadMgrApp::OnStopEvent(char* buffer, int len)
{
    
} // end MgenPayloadMgrApp::OnOffEvent

void MgenPayloadMgrApp::OnMsgReceive(MgenMsg& msg)
{

  char buffer[8192];
  DMSG(2,"MgenPayloadMgrApp::OnMsgReceive() src> %s/%hu dst> %s/%hu flowId>%d seq>%d payload>%s \n",msg.GetSrcAddr().GetHostString(),msg.GetSrcAddr().GetPort(),msg.GetDstAddr().GetHostString(),msg.GetDstAddr().GetPort(),msg.GetFlowId(),msg.GetSeqNum(),msg.GetPayload());

  sprintf(buffer,"src>%s flow_id>%d",msg.GetSrcAddr().GetHostString(),msg.GetFlowId());
  mgenPayloadMgr.LogEvent("OnMsgReceive",buffer);

  if (!socket.SendTo(buffer,sizeof(buffer),dstAddr))
    {
      DMSG(PL_ERROR,"MgenPayloadMgrApp::OnMsgReceive() socket.SendTo() error %s\n",GetErrorString());
    }

  /*
  if (msg.GetFlowId() == 1)
    {
      sprintf(buffer,"1.0 ON 2 UDP DST %s/5001 PERIODIC [1 1024] DATA [050607] COUNT 1",msg.GetSrcAddr().GetHostString());
      mgenPayloadMgr.SendMgenCommand("event",buffer);
    }
  else
    if (msg.GetFlowId() == 2)
    {
      sprintf(buffer,"1.0 ON 3 UDP DST %s/5000 PERIODIC [1 1024] DATA [050607] COUNT 1",msg.GetSrcAddr().GetHostString());
      mgenPayloadMgr.SendMgenCommand("event",buffer);

    }
    else
      {
	// do nothing
      }
  */
} // end MgenPayloadMgrApp::OnMsgReceive

#ifdef _INSTANTIATE_APP
PROTO_INSTANTIATE_APP(MgenPayloadMgrApp)
#endif
