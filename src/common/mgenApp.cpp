#include "mgenGlobals.h"
#include "mgen.h"
#include "mgenMsg.h"
#include "mgenVersion.h"
#include "mgenApp.h"
#include "protokit.h"

#include <string.h>
#include <stdio.h>   // for stdout/stderr printouts
#include <ctype.h>   // for toupper()
#include <errno.h>

#ifdef WIN32
#include <IpHlpApi.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif // UNIX
MgenApp::MgenApp()
  :  mgen(GetTimerMgr(), GetSocketNotifier()),
     control_pipe(ProtoPipe::MESSAGE), control_remote(false),
     cmd_descriptor(ProtoDispatcher::INVALID_DESCRIPTOR),
#ifdef HAVE_GPS
     gps_handle(NULL), payload_handle(NULL),
#endif // HAVE_GPS
     have_ports(false), convert(false), 
     ifinfo_tx_count(0), ifinfo_rx_count(0)
{
    control_pipe.SetNotifier(&GetSocketNotifier());
    control_pipe.SetListener(this, &MgenApp::OnControlEvent);
    ifinfo_name[0] = '\0';
}

MgenApp::~MgenApp()
{
    
}

void MgenApp::Usage()
{
    fprintf(stderr, "mgen [ipv4][ipv6][input <scriptFile>][save <saveFile>]\n"
            "     [output <logFile>][log <logFile>][hostAddr {on|off}\n"
            "     [logData {on|off}][logGpsData {on|off}]\n"
            "     [binary][txlog][nolog][flush]\n"
            "     [event \"<mgen event>\"][port <recvPortList>]\n"
            "     [instance <name>][command <cmdInput>]\n"
            "     [sink <sinkFile>][block][source <sourceFile>]\n"
            "     [interface <interfaceName>][ttl <multicastTimeToLive>]\n"
            "     [unicast_ttl <unicastTimeToLive>]\n"
            "     [df <on|off>]\n"
            "     [tos <typeOfService>][label <value>]\n"
            "     [txbuffer <txSocketBufferSize>][rxbuffer <rxSocketBufferSize>]\n"
            "     [start <hr:min:sec>[GMT]][offset <sec>]\n"
            "     [precise {on|off}][ifinfo <ifName>]\n"
            "     [txcheck][rxcheck][check]\n"
            "     [queue <queueSize>][broadcast {on|off}]\n"
            "     [convert <binaryLog>][debug <debugLevel>]\n"
            "     [gpskey <gpsSharedMemoryLocation>]\n"
            "     [boost] [reuse {on|off}]\n"
            "     [epochtimestamp]\n");
}  // end MgenApp::Usage()


const char* const MgenApp::CMD_LIST[] =
{
    "+port",
    "-ipv6",       // open IPv6 sockets by default
    "-ipv4",       // open IPv4 sockets by default
    "+convert",    // convert binary logfile to text-based logfile
    "+sink",       // set Mgen::sink to stream sink
    "-block",      // set Mgen::sink to blocking I/O
    "+source",     // specify an MGEN stream source
    "+ifinfo",     // get tx/rx frame counts for specified interface
    "+precise",    // turn on/off precision packet timing
    "+instance",   // indicate mgen instance name 
    "-stop",       // exit program instance
    "+command",    // specifies an input command file/device
    "+hostaddr",   // turn "host" field on/off in sent messages
    "-boost",      // boost process priority
    "+seed",       // Seed for random number generation (possion & jitter patterns)
    "-help",       // print usage and exit
    "+gpskey",    // Override default gps shared memory location
    "+logdata",    // log optional data attribute? default ON
    "+loggpsdata", // log gps data? default ON
    "-epochtimestamp", // epoch timesetamps? default OFF
//   "-analytics",  // enables MGEN analytics reporting on received flows
    NULL
};

MgenApp::CmdType MgenApp::GetCmdType(const char* cmd)
{
    if (!cmd) return CMD_INVALID;
    unsigned int len = strlen(cmd);

    bool matched = false;
    CmdType type = CMD_INVALID;
    const char* const* nextCmd = CMD_LIST;
    while (*nextCmd)
    {
        char lowerCmd[32];  // all commands < 32 characters    
        len = len < 31 ? len : 31;
        unsigned int i;
        for (i = 0; i < (len + 1); i++)
            lowerCmd[i] = tolower(cmd[i]);

        if (!strncmp(lowerCmd, *nextCmd+1, len))
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
		if (len == 3 && 
		    !strncmp(lowerCmd,"log", len))
		  // let mgen handle log events as name conflicts with logdata * loggpsdata
		  type = CMD_INVALID;

            }
        }
        nextCmd++;
    }
    return type; 
}  // end MgenApp::GetCmdType()

bool MgenApp::ProcessCommands(int argc, const char*const* argv)
{
    // Group command line arguments into MgenApp or Mgen commands
    // and their (if applicable) respective arguments
    // Parse command line
    int i = 1;
    convert = false;  // initialize conversion flag
    while (i < argc)
    {
        CmdType cmdType = GetCmdType(argv[i]);
        if (CMD_INVALID == cmdType)
        {
            // Is it a class MgenApp command?
            switch(Mgen::GetCmdType(argv[i]))
            {
                case Mgen::CMD_INVALID:
                    break;
                case Mgen::CMD_NOARG:
                    cmdType = CMD_NOARG;
                    break;
                case Mgen::CMD_ARG:
                    cmdType = CMD_ARG;
                    break;
            }
        }
        switch (cmdType)
        {
            case CMD_INVALID:
                DMSG(0, "MgenApp::ProcessCommands() error: invalid command:%s\n", argv[i]);
                return false;
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    DMSG(0, "MgenApp::ProcessCommands() OnCommand(%s) error\n", 
                            argv[i]);
                    return false;
                }
                i++;
                break;
            case CMD_ARG:
                /*
                if (NULL == argv[i+1])
                {
                    DMSG(0, "MgenApp::ProcessCommands() error: missing \"%s\" command argument!\n", argv[i]);
                    return false;
                }
                */
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    DMSG(0, "MgenApp::ProcessCommands() OnCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    return false;
                }
                i += 2;
                break;
        }
    }  
    return true;
}  // end MgenApp::ProcessCommands()

bool MgenApp::OnCommand(const char* cmd, const char* val)
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
            DMSG(0, "MgenApp::OnCommand(%s) error sending command to remote process\n", cmd);    
            return false;
        }        
    }
    
    CmdType type = GetCmdType(cmd);
    unsigned int len = strlen(cmd);    
    char lowerCmd[32];  // all commands < 32 characters    
    len = len < 31 ? len : 31;
    unsigned int i;
    for (i = 0; i < (len + 1); i++)
        lowerCmd[i] = tolower(cmd[i]);

    if (CMD_INVALID == type)
    {
        Mgen::CmdType mgenType = Mgen::GetCmdType(cmd);
        if (Mgen::CMD_INVALID == mgenType)
        {
            DMSG(0, "MgenApp::ProcessCommand(%s) error: invalid command\n", cmd);
            return false;
        }
        else
        {
            // It is a core mgen command, process it as "overriding"
            // double check there's a "val" if it needs one
            if ((Mgen::CMD_ARG == mgenType) && (NULL == val))
            {
                const char* cmdName = Mgen::GetCmdName(Mgen::GetCommandFromString(cmd));
                if (Mgen::INVALID_COMMAND == Mgen::GetCommandFromString(cmdName)) cmdName = NULL;  // because of funny business with "OFF" command 
                DMSG(0, "MgenApp::ProcessCommand(%s) error: missing \"%s\" argument!\n", cmd, (NULL != cmdName) ? cmdName : cmd);
                return false;
            }
            return mgen.OnCommand(Mgen::GetCommandFromString(cmd), val, true);   
        }
    }
    else if ((CMD_ARG == type) && (NULL == val))
    {
        DMSG(0, "MgenApp::ProcessCommand(%s) missing argument\n", cmd);
        return false;
    }
    else if (!strncmp("port", lowerCmd, len))
    {
        // "port" == implicit "0.0 LISTEN UDP <val>" script
        char* string = new char[strlen(val) + 64];
        if (!string)
        {
            DMSG(0, "MgenApp::ProcessCommand(port) memory allocation error: %s\n",
                    GetErrorString());
            return false;   
        }
        sprintf(string, "0.0 LISTEN UDP %s", val);
        DrecEvent* event = new DrecEvent;
        if (!event)
        {
            DMSG(0, "MgenApp::ProcessCommand(port) memory allocation error: %s\n",
                    GetErrorString());
            return false;   
        }
        if (!event->InitFromString(string))
        {
            DMSG(0, "MgenApp::ProcessCommand(port) error parsing <portList>\n");
            return false;      
        }
        have_ports = true;
        mgen.InsertDrecEvent(event);
    }
    else if (!strncmp("ipv4", lowerCmd, len))
    {
        mgen.SetDefaultSocketType(ProtoAddress::IPv4);
    }
    else if (!strncmp("ipv6", lowerCmd, len))
    {
#ifdef HAVE_IPV6 
        if (ProtoSocket::HostIsIPv6Capable())
            mgen.SetDefaultSocketType(ProtoAddress::IPv6);
        else
#endif // HAVE_IPV6
            DMSG(0, "MgenApp::ProcessCommand(ipv6) Warning: system not IPv6 capable?\n");
        
    }
    else if (!strncmp("background", lowerCmd, len))
    {
        // do nothing (this command was scanned immediately at startup)
    }
    else if (!strcmp("convert", lowerCmd))
    {
        convert = true;             // set flag to do the conversion
        strcpy(convert_path, val);  // save path of file to convert
    }
    else if (!strncmp("sink", lowerCmd, len))
    {
        mgen.SetSinkPath(val);
    }
    else if (!strncmp("block", lowerCmd, len))
    {
        mgen.SetSinkBlocking(false);
    }
    else if (!strncmp("source", lowerCmd, len))
    {
        mgen.SetSourcePath(val);
        ProtoAddress tmpAddress;
        MgenTransport* theMgenTransport = mgen.GetMgenTransport(SOURCE,0,tmpAddress,NULL,true,false);
        if (!theMgenTransport)          
        {
            DMSG(0,"MgenApp::OnCommand() Error getting MgenAppSinkTransport.\n");
            return false;
        }
        MgenAppSinkTransport* theTransport = static_cast<MgenAppSinkTransport*>(theMgenTransport);
        theTransport->SetSource(true);
        if (!theTransport->Open()) return false;
#ifdef WIN32
		// on windows read the whole file synchronously
		theTransport->StopInputNotification();
		while (theTransport->OnInputReady());
		return true;
#else
		theTransport->StartInputNotification();        
#endif  // !WIN32        
    }
    else if (!strncmp("ifinfo", lowerCmd, len))
    {
        strncpy(ifinfo_name, val, 63);  
        ifinfo_name[63] = '\0'; 
    }
    else if (!strncmp("precise", lowerCmd, len))
    {
        char status[4];  // valid status is "on" or "off"
        strncpy(status, val, 3);
        status[3] = '\0';
        unsigned int len = strlen(status);
        for (unsigned int i = 0; i < len; i++)
            status[i] = tolower(status[i]);
        if (!strncmp("on", status, len))
        {
            dispatcher.SetPreciseTiming(true);
        }
        else if (!strncmp("off", status, len))
        {
            dispatcher.SetPreciseTiming(false);
        }
        else
        {
            DMSG(0, "MgenApp::ProcessCommand(precise) Error: invalid <status>\n");
            return false;
        }
    }
    else if (!strncmp("seed", lowerCmd, len))
    {
        int seed;
        int result = sscanf(val, "%i", &seed);
        if (1 != result || seed < 0)
        {
            DMSG(0,"MgenApp::OnCommand() - invalid seed value");
            return false;
        }
        srand(seed);
        DMSG(0,"Seed %d First number: %d\n", seed, rand());
    }
    else if (!strncmp("instance", lowerCmd, len))
    {
        if (control_pipe.IsOpen())
            control_pipe.Close();
        if (control_pipe.Connect(val))
        {
            control_remote = true;
        }        
        else if (!control_pipe.Listen(val))
        {
            DMSG(0, "MgenApp::ProcessCommand(instance) error opening control pipe\n");
            return false; 
        }
    }
    else if (!strncmp("command", lowerCmd, len))
    {
        if (!OpenCmdInput(val))
        {
            DMSG(0, "MgenApp::ProcessCommand(command) error opening command input file/device\n");
            return false;
        }
    }
    else if (!strncmp("hostaddr", lowerCmd, len))
    {
       char status[4];  // valid status is "on" or "off"
       strncpy(status, val, 3);
       status[3] = '\0';
       unsigned int len = strlen(status);
       for (unsigned int i = 0; i < len; i++)
         status[i] = tolower(status[i]);
       if (!strncmp("on", status, len))
       {
           // (TBD) control whither IPv4 or IPv6 ???
           ProtoAddress localAddress;
           if (localAddress.ResolveLocalAddress())
           {
               mgen.SetHostAddress(localAddress);   
           }
           else
           {
               DMSG(0, "MgenApp::ProcessCommand(hostAddr) error getting local addr\n");
               return false;
           }
       }
    }
#ifdef HAVE_GPS
    else if (!strncmp("gpskey", lowerCmd, len))
      {
	gps_handle = GPSSubscribe(val);
	if (gps_handle)
	  mgen.SetPositionCallback(MgenApp::GetPosition, gps_handle);
      }
#endif //HAVE_GPS
    else if (0 == strncmp("analytics", lowerCmd, len))
    {
        // Enable mgen analytics measurement
        mgen.SetComputeAnalytics(true);
    }
    else if (!strncmp("logdata", lowerCmd, len))
    {
      char status[4];  // valid status is "on" or "off"
      strncpy(status, val, 3);
      status[3] = '\0';
      unsigned int len = strlen(status);
      for (unsigned int i = 0; i < len; i++)
	    status[i] = tolower(status[i]);
      if (!strncmp("on", status, len))
	    mgen.SetLogData(true);
      else if (!strncmp("off",status,len))
	    mgen.SetLogData(false);
      else
      {
	DMSG(0, "MgenApp::ProcessCommand(logData) Error: wrong argument to logData:%s\n",status);
	return false;
      }
    }
    else if (!strncmp("loggpsdata", lowerCmd, len))
    {
      char status[4];  // valid status is "on" or "off"
      strncpy(status, val, 3);
      status[3] = '\0';
      unsigned int len = strlen(status);
      for (unsigned int i = 0; i < len; i++)
	status[i] = tolower(status[i]);
      if (!strncmp("on", status, len))
	mgen.SetLogGpsData(true);
      else if (!strncmp("off",status,len))
	mgen.SetLogGpsData(false);
      else
      {
	DMSG(0, "MgenApp::ProcessCommand(loggpsdata) Error: wrong argument to loggpsdata:%s\n",status);
	return false;
      }
    }
    else if (!strncmp("stop", lowerCmd, len))
    {
      Stop();
    }
    else if (!strncmp("boost", lowerCmd, len))
    {
        if (!dispatcher.BoostPriority())
            fprintf(stderr, "Unable to boost process priority.\n");
    }
    else if (!strncmp("epochtimestamp", lowerCmd, len))
    {
        mgen.SetEpochTimestamp(true);
    }
    else if (!strncmp("help", lowerCmd, len))
    {
        fprintf(stderr, "mgen: version %s\n", MGEN_VERSION);
        Usage();
        return false;
    }
    return true;
 }  // end MgenApp::OnCommand()

bool MgenApp::OnStartup(int argc, const char*const* argv)
{          
    // Seed the system rand() function
    struct timeval currentTime;
    ProtoSystemTime(currentTime);
    srand(currentTime.tv_sec^currentTime.tv_usec);
    
    mgen.SetLogFile(stdout);  // log to stdout by default
    
#ifdef HAVE_GPS

    gps_handle = GPSSubscribe(NULL);
    if (gps_handle)
      mgen.SetPositionCallback(MgenApp::GetPosition, gps_handle);
    // This payload stuff shouldn't be here!
    payload_handle = GPSSubscribe("/tmp/mgenPayloadKey");
    if (payload_handle)
      mgen.SetPayloadHandle(payload_handle);

#endif // HAVE_GPS   
    
    if (!ProcessCommands(argc, argv))
    {
        fprintf(stderr, "mgen: error while processing startup commands\n");
        return false;
    }
    
    if (control_remote)
    {
        // We remoted commands, so we're finished ...
        return false;   
    }
    
    fprintf(stderr, "mgen: version %s\n", MGEN_VERSION);
    
    if (convert)
    {
        fprintf(stderr, "mgen: beginning binary to text log conversion ...\n");
        MgenMsg theMsg;
        theMsg.ConvertBinaryLog(convert_path,mgen);
        fprintf(stderr, "mgen: conversion complete (exiting).\n");
    }
    else
    {
        if (mgen.DelayedStart())
        {
            char startTime[256];
            mgen.GetStartTime(startTime);
            fprintf(stderr, "mgen: delaying start until %s ...\n", startTime);
        }
        else
        {
            fprintf(stderr, "mgen: starting now ...\n");
        }
    }
    
    if ('\0' != ifinfo_name[0])
      GetInterfaceCounts(ifinfo_name, ifinfo_tx_count, ifinfo_rx_count);
    else
      ifinfo_tx_count = ifinfo_rx_count = 0;  
    return mgen.Start();
}  // end MgenApp::OnStartup()

void MgenApp::OnShutdown()
{
    mgen.Stop();
    
    if ('\0' != ifinfo_name[0])
    {
        UINT32 txCount, rxCount;
        if (GetInterfaceCounts(ifinfo_name, txCount, rxCount))
          fprintf(stderr, "mgen: iface>%s txFrames>%d rxFrames>%d\n", 
                  ifinfo_name,
                  txCount - ifinfo_tx_count,
                  rxCount - ifinfo_rx_count);
        else
          fprintf(stderr, "mgen: Warning! Couldn't get interface frame counts\n");
    }
    
    
#ifdef HAVE_GPS
    if (gps_handle) 
    {
        GPSUnsubscribe(gps_handle);
        gps_handle = NULL;
        mgen.SetPositionCallback(NULL, NULL);
    }
    if (payload_handle)
    {
        GPSUnsubscribe(payload_handle);
        payload_handle = NULL;
        mgen.SetPayloadHandle(NULL);
    }
#endif // HAVE_GPS    

    control_pipe.Close();
    CloseCmdInput();
}  // end MgenApp::OnShutdown()


#ifdef HAVE_GPS

bool MgenApp::GetPosition(const void* gpsHandle, GPSPosition& gpsPosition)
{
    GPSGetCurrentPosition((GPSHandle)gpsHandle, &gpsPosition);
    return true;    
}

#endif // HAVE_GPS

/**
 * This routine uses the system's "netstat" command
 * to retrieve tx/rx frame counts for a given interface
 * (WIN32 Note: Counts are cumulative for all interfaces?)
 */
bool MgenApp::GetInterfaceCounts(const char* ifName, 
                                 UINT32&     txCount,
                                 UINT32&     rxCount)
{
#ifdef _WIN32_WCE
    // For the moment, we'll assume our WINCE platform
    // has one interface and we'll use GetIpStatistics()
    // ignoring the "ifName"
    MIB_IPSTATS ipStats;
    if (NO_ERROR != GetIpStatistics(&ipStats))
    {
        DMSG(0, "MgenApp::GetInterfaceCounts() GetIpStatistics() error: %s\n", GetErrorString());
        rxCount = txCount = 0;
        return false;
    }
    else
    {
        rxCount = ipStats.dwInReceives;
        txCount = ipStats.dwInDelivers;
        return true;
    }
#else
    bool result = false;
    txCount = rxCount = 0;
#ifdef WIN32
    FILE* p = _popen("netstat -e", "r");
    int lineCount = 0;
#else
    FILE* p = popen("netstat -i", "r");
#endif  // if/else WIN32
    if (NULL == p)
    {
        DMSG(0, "MgenApp::GetInterfaceCounts() popen() error: %s\n",
             GetErrorString());
        return result;   
    }
    Mgen::FastReader reader;
    char linebuffer[256];
    unsigned int len = 256;
    while (Mgen::FastReader::OK == reader.Readline(p, linebuffer, &len))
    {
#ifdef WIN32
        // On WIN32, we must collect two lines from "netstat -e" output
        if (!strncmp(linebuffer, "Unicast", strlen("Unicast")) ||
            !strncmp(linebuffer, "Non-unicast", strlen("Non-unicast")))
        {
            char* ptr = strstr(linebuffer, "packets");
            if (ptr)
            {
                UINT32 received, sent;
                if (2 == sscanf(ptr, "packets %lu %lu", &received, &sent))
                {
                    txCount += sent;
                    rxCount += received;  
                    if (2 == ++lineCount)
                    {
                        result = true;
                        break;
                    }
                }
            }
        }
#else
        if (!strncmp(linebuffer, ifName, strlen(ifName)))
        {
#ifdef LINUX
            char format[256];
            // Linux format: "iface mtu metric rxOk rxErr rxDrp rxOvr txOk ..."
            sprintf(format, "%s %%lu %%lu %%lu %%lu %%lu %%lu %%lu", ifName);
            UINT32 mtu, metric, rxOk, rxErr, rxDrp, rxOvr, txOk;
            if (7 == sscanf(linebuffer, format, &mtu, &metric, &rxOk, &rxErr, &rxDrp, &rxOvr, &txOk))
            {
                txCount = txOk;
                rxCount = rxOk;
                result = true;
                break;
            }
#endif // LINUX   
#ifdef MACOSX
            // MacOS X format: "iface mtu net addr ipkts ierrs opkts ..."
            char format[256];
            char net[64], addr[64];
            UINT32 mtu, ipkts, ierrs, opkts;
            sprintf(format, "%s %%lu %%s %%s %%lu %%lu %%lu", ifName);
            if (6 == sscanf(linebuffer, format, &mtu, net, addr, &ipkts, &ierrs, &opkts))
            {
                txCount = opkts;
                rxCount = ipkts;
                result = true;
                break;
            }   
#endif // MACOSX
        }
#endif // if/else WIN32
        len = 256;
    }  // end while(reader.Readline())
#ifdef WIN32
    _pclose(p);
#else
    pclose(p);
#endif // if/else WIN32
    return result;
#endif // if/else _WIN32_WCE
}  // end MgenApp::GetInterfaceCounts()

void MgenApp::OnControlEvent(ProtoSocket& /*theSocket*/,
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
                DMSG(0, "MgenApp::OnControlEvent() error processing command\n");
            }
        }
        else
        {
            DMSG(0, "MgenApp::OnControlEvent() receive error\n"); 
        }   
    }
    else
    {
        DMSG(0, "MgenApp::OnControlEvent() unhandled event type\n");  
    }
}  // end MgenApp::OnControlEvent

/**
 * Mgen run-time control via "stdin" (or other file/device)
 * @note On WIN32, MGEN must be built as a "console" application
 *       for this stuff to work.
 */
bool MgenApp::OpenCmdInput(const char* path)
{
#ifdef _WIN32_WCE
    DMSG(0, "MgenApp::OpenCmdInput() \"command\" option not supported on WinCE\n");
    return false;
#else
    CloseCmdInput();  // in case it was open
#ifdef WIN32
    if (!strcmp(path, "STDIN"))
    {
		cmd_descriptor = (ProtoDispatcher::Descriptor)GetStdHandle(STD_INPUT_HANDLE);
        // (TBD) set stdin for overlapped I/O ???
    }
    else
    {
        cmd_descriptor = (ProtoDispatcher::Descriptor)CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 
                                                                 NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    }
    if ((ProtoDispatcher::Descriptor)INVALID_HANDLE_VALUE == cmd_descriptor)
    {
        DMSG(0, "MgenApp::OpenCmdInput() error opening file\n");
        return false;
    }
    /*// For non-block I/O on Win32 we use overlapped I/O
      if (NULL == (cmd_read_event = CreateEvent(NULL, TRUE, FALSE, NULL)))
      {
      DMSG(0, "MgenApp::OpenCmdInput() error creating overlapped I/O event\n");
      CloseHandle((HANDLE)cmd_descriptor);
      return false;
      } */ 
#else
    if (!strcmp(path, "STDIN"))
    {
        cmd_descriptor = fileno(stdin);
        // Make the stdin non-blocking
        if(-1 == fcntl(cmd_descriptor, F_SETFL, fcntl(cmd_descriptor, F_GETFL, 0) | O_NONBLOCK))
        {
            cmd_descriptor = -1;
            DMSG(0, "MgenApp::OpenCmdInput() fcntl(stdout, F_SETFL(O_NONBLOCK)) error: %s",
                 GetErrorString());
        }
    }
    else
    {
        cmd_descriptor = open(path, O_RDONLY | O_NONBLOCK);
    }
    if (cmd_descriptor < 0)
    {
        DMSG(0, "MgenApp::OpenCmdInput() error opening file: %s", GetErrorString());
        cmd_descriptor = ProtoDispatcher::INVALID_DESCRIPTOR;
        return false;
    }  
#endif // if/else WIN32
    cmd_length = 0;
    if (!dispatcher.InstallGenericInput(cmd_descriptor, MgenApp::DoCmdInputReady, this))
    {
        DMSG(0, "MgenApp::OpenCmdInput() Error: couldn't install cmd input\n");
        CloseCmdInput();
        return false;       
    }  
    return true;
#endif // if/else _WIN32_WCE
}  // end MgenApp::OpenCmdInput()

void MgenApp::CloseCmdInput()
{
#ifndef _WIN32_WCE
    
    if (ProtoDispatcher::INVALID_DESCRIPTOR != cmd_descriptor)
    {
        dispatcher.RemoveGenericInput(cmd_descriptor);
#ifdef WIN32
        CloseHandle((HANDLE)cmd_descriptor);
#else
        close(cmd_descriptor);
#endif // if/else WIN32/UNIX
        cmd_descriptor = ProtoDispatcher::INVALID_DESCRIPTOR;
    } 
#endif // _WIN32_WCE
}  // end MgenApp::CloseCmdInput()

void MgenApp::DoCmdInputReady(ProtoDispatcher::Descriptor /*descriptor*/, 
                              ProtoDispatcher::Event      /*theEvent*/, 
                              const void*                 userData)
{
    ((MgenApp*)userData)->OnCmdInputReady();
}  // end MgenApp::DoCmdInputReady()

void MgenApp::OnCmdInputReady()
{
    // We read the command input stream one byte at a time
    // just to save some buffer manipulation
    char byte;
    unsigned int len = 1;
    if (ReadCmdInput(&byte, len))
    {
        if (IsCmdDelimiter(byte))
        {
            cmd_buffer[cmd_length] = '\0';
            if (cmd_length)
            {
                char* cmd = cmd_buffer;
                char* arg = NULL;
                for (unsigned int i = 0; i < cmd_length; i++)
                {
                    if (isspace(cmd_buffer[i]))
                    {
                        cmd_buffer[i] = '\0';
                        arg = cmd_buffer + i + 1;
                        break;
                    } 
                }
                if (!OnCommand(cmd, arg))
                    DMSG(0, "MgenApp::OnCmdInputReady() error processing command\n");       
            }
            cmd_length = 0;
        }        
        else if (cmd_length < 8191)
        {
            cmd_buffer[cmd_length++] = byte;
        }
        else
        {
            DMSG(0, "MgenApp::OnCmdInputReady() error: maximum command length exceeded\n");   
        }
    }
    else
    {
        DMSG(0, "MgenApp::OnCmdInputReady() error reading input\n");
    }    
}  // end MgenApp::OnCmdInputReady()

bool MgenApp::ReadCmdInput(char* buffer, unsigned int& numBytes)
{
#ifdef WIN32
    DWORD want = numBytes;
    DWORD got;
    if (ReadFile(cmd_descriptor, buffer, want, &got, NULL))
    {
        numBytes = got;
        return true;
    }
    else
    {
        DMSG(0, "MgenApp::ReadCmdInput() ReadFile error: %s\n", GetErrorString());
        return false;
    }
#else
    ssize_t result = read(cmd_descriptor, buffer, numBytes);
    if (result <= 0)
    {
        switch (errno)
        {
            case EINTR:
            case EAGAIN:
                numBytes = 0;
                return true;
            default:
                DMSG(0, "MgenApp::ReadCmdInput() read error: %s\n", GetErrorString());
            return false;   
        }
    }
    else
    {
        numBytes = result;
        return true;   
    }
#endif  // end if/else WIN32/UNIX
}  // end MgenApp::ReadCmdInput()


// This macro instantiates our MgenApp instance
PROTO_INSTANTIATE_APP(MgenApp)


