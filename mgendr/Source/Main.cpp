
#include "Main.h"
#include "ControlComponent.h"
#include"protoString.h"
#include <stdarg.h>  // for va_start, etc
#include "mgenVersion.h"

const char* MGENDR_VERSION  = "0.1.3";

static MgenderApp* app_ptr = NULL;

int MgenderApp::DoMgenLogEvent(FILE*, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    char text[256];
    text[255] = '\0';
    int result = vsnprintf(text, 255, format, args);
    va_end(args);
    app_ptr->OnMgenLogEvent(text);
    return result;
}  // end MgenderApp::DoMgenLog()

MgenderApp::MgenderApp() 
: mgen(GetTimerMgr(), GetSocketNotifier()), 
  probe_src_port(0), next_feedback_flow_id(PROBE_FLOW_ID + 1),
  filter_feedback(true), dynamic_probe_addr(false),
  active_chooser(nullptr), event_index(0)
{
    probe_addr.ConvertFromString("127.0.0.1");
    probe_addr.SetPort(0);
    iface_name[0] = iface_name[255] = '\0';
    app_ptr = this;
#ifdef JUCE_MAC
    PopupMenu extraMenu;
    extraMenu.addCommandItem(&cmd_manager, CMD_ABOUT_APP);
    extraMenu.addItem(CMD_ABOUT_APP, String("About mgendr ..."));
    MenuBarModel::setMacMainMenu(this, &extraMenu);
#endif // JUCE_MAC
    setApplicationCommandManagerToWatch(&cmd_manager);
    cmd_manager.registerAllCommandsForTarget(this);
    prune_timer.SetListener(this, &MgenderApp::OnPruneTimeout);
}

MgenderApp::~MgenderApp()
{
    report_table.Destroy();
}

bool MgenderApp::ProcessCommandLine()
{    
    StringArray argv = getCommandLineParameterArray();
    unsigned int argc = argv.size();
    // Group command line arguments into MgenApp or Mgen commands
    // and their (if applicable) respective arguments
    // Parse command line
    int i = 0;
    while (i < argc)
    {     
        Mgen::CmdType cmdType = Mgen::GetCmdType(argv[i].toRawUTF8());
        switch (cmdType)
        {
            case Mgen::CMD_INVALID:
            {
#ifdef JUCE_MAC
                if (0 == strcmp(argv[i].toRawUTF8(), "-NsXXX"))
                {
                    // Skip baked-in command-line option by Xcode issued for MacOSX debugging
                    i += 2;
                    break;
                }
#endif // JUCE_MAC
                String warn = String("Invalid command-line command: \"") + argv[i] + String("\"");
#ifdef ANDROID
                AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, String("mgender"), warn);
#else
                AlertWindow::showMessageBox(AlertWindow::WarningIcon, String("mgender"), warn);
#endif  // if/else ANDROID
                i++;
                return false;
            }
            case Mgen::CMD_NOARG:
            {
                if (!mgen.OnCommand(mgen.GetCommandFromString(argv[i].toRawUTF8()), NULL))
                {
                    String warn = String("Error invoking command: \"") + argv[i] + String("\"");
#ifdef ANDROID
                    AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, String("mgender"), warn);
#else
                    AlertWindow::showMessageBox(AlertWindow::WarningIcon, String("mgender"), warn);
#endif  // if/else ANDROID     
                    return false;
                }
                i++;
                break;
            }
            case Mgen::CMD_ARG:
            {
                if (argc <= (i + 1))
                {
                    String warn = String("Mission command-line argument for command: \"") + argv[i] + String("\"");
#ifdef ANDROID
                    AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, String("mgender"), warn);
#else
                    AlertWindow::showMessageBox(AlertWindow::WarningIcon, String("mgender"), warn);
#endif  // if/else ANDROID      
                    return false;                 
                }
                if (!mgen.OnCommand(mgen.GetCommandFromString(argv[i].toRawUTF8()), argv[i+1].toRawUTF8()))
                {
                    String warn = String("Error invoking command: \"") + argv[i] + String(" ") + argv[i+1] + String("\"");
#ifdef ANDROID
                    AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, String("mgender"), warn);
#else
                    AlertWindow::showMessageBox(AlertWindow::WarningIcon, String("mgender"), warn);
#endif  // if/else ANDROID     
                    return false;
                }
                i += 2;
                break;
            }
        }
    }  
    return true;
}  // end MgenderApp::ProcessCommandLine()

void MgenderApp::getAllCommands(Array<CommandID>& cmds)
{
    Array<CommandID> commands {CMD_ABOUT_APP, CMD_LOAD_SCRIPT, CMD_SET_LOG, CMD_FILTER_FEEDBACK, CMD_DYNAMIC_ADDR};
    cmds.addArray(commands);
}  // end MgenderApp::getAllCommands()

void MgenderApp::getCommandInfo(CommandID commandID, ApplicationCommandInfo& result)
{
    char shortcut = '\0';
    switch (commandID)
    {
        case CMD_ABOUT_APP:
            result.setInfo ("About mgendr ...", "mgendr Application Information", "Menu", 0);
            result.setTicked (false);
            break;
        case CMD_LOAD_SCRIPT:
            shortcut = 'O';
            result.setInfo ("Load Script ...", "Load MGEN script", "Menu", 0);
            result.setTicked (false);
            break;
        case CMD_SET_LOG:
            shortcut = 'S';
            result.setInfo ("Set Logfile ...", "Set MGEN log file", "Menu", 0);
            result.setTicked (false);
            break;
        case CMD_FILTER_FEEDBACK:
            result.setInfo ("Filter feedback", "Filter out probe feedback flows in flow report display", "Menu", 0);
            result.setTicked (filter_feedback);
            break;
        case CMD_DYNAMIC_ADDR:
            result.setInfo ("Dynamic probe address", "Enable dynamic update of probe address based on feedback", "Menu", 0);
            result.setTicked (dynamic_probe_addr);
        default:
            break;
    }
#ifndef JUCE_ANDROID
    if ('\0' != shortcut)
        result.addDefaultKeypress(shortcut, ModifierKeys::commandModifier);
#endif // !JUCE_ANDROID    

}  // end MgenderApp::getCommandInfo()

bool MgenderApp::perform(const InvocationInfo& info)
{
    switch (info.commandID)
    {
        case CMD_ABOUT_APP:
            ShowAppInfo();
            break;
        case CMD_LOAD_SCRIPT:
            // TBD - open load script file dialog
            LoadScriptFile();
            break;
        case CMD_SET_LOG:
            SetLogFile();
            break;
        case CMD_FILTER_FEEDBACK:
            // toggle state
            filter_feedback = filter_feedback ? false : true;
            break;
        case CMD_DYNAMIC_ADDR:
            // toggle state
            dynamic_probe_addr = dynamic_probe_addr ? false : true;
            break;
        default:
            return false;
    }
    return true;
}  // end MgenderApp::perform()
         
StringArray MgenderApp::getMenuBarNames()
{
    return StringArray(String("File"), String("Options"), String("About"));
}  // end MgenderApp::getMenuBarNames()

PopupMenu MgenderApp::getMenuForIndex(int menuIndex, const String &menuName)
{
    PopupMenu menu;
    switch (menuIndex)
    {
        /*case -1:  // Mac OSX "extra" menu
            menu.addCommandItem(&cmd_manager, CMD_ABOUT_APP);
            break;*/
        case 0: // "File" menu
            menu.addCommandItem(&cmd_manager, CMD_LOAD_SCRIPT);
            menu.addCommandItem(&cmd_manager, CMD_SET_LOG);
            break;
        case 1: // "Options" menu
            menu.addCommandItem(&cmd_manager, CMD_FILTER_FEEDBACK);
            menu.addCommandItem(&cmd_manager, CMD_DYNAMIC_ADDR);
            break;
        case 2:  // "About" menu
            menu.addCommandItem(&cmd_manager, CMD_ABOUT_APP);
            break;
        default:
            break;
    }
    return menu; 
}  // end  MgenderApp::getMenuForIndex()

void MgenderApp::menuItemSelected(int menuItemID, int menuIndex)
{
    if ((-1 == menuIndex) && (CMD_ABOUT_APP == menuItemID))
    {
        ShowAppInfo();
    }
}

void MgenderApp::ShowAppInfo()
{
    String info = String("mgendr Message Generator\nVersion ") + String(MGENDR_VERSION) +
                  String("\n(MGEN version ") + String(MGEN_VERSION) + String(")\n") +
                  String("written by Brian Adamson\nU.S. Naval Research Laboratory\nJune 2018");
#ifdef ANDROID
    AlertWindow::showMessageBoxAsync(AlertWindow::InfoIcon, String("mgendr"), info);
#else
    AlertWindow::showMessageBox(AlertWindow::InfoIcon, String("mgendr"), info);
#endif  // if/else ANDROID
}  // end MgenderApp::ShowAppInfo()

void MgenderApp::LoadScriptFile()
{
    
    FileChooser* fileChooser = new FileChooser(String("mgender"), File::getCurrentWorkingDirectory(), String("*"));
    ASSERT(nullptr == active_chooser);
    active_chooser = fileChooser;
    int flags = FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles; 
    fileChooser->launchAsync(flags,
        [this](const FileChooser& fc)
        {
            if (fc.getResults().size() > 0)
            {
                String path = fc.getResult().getFullPathName();
                dispatcher.SuspendThread();
                if (!mgen.ParseScript(path.toRawUTF8()))
                {
                    String warn = String("Invalid MGEN script \"") + path + String("\"");
#ifdef ANDROID
                    AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, String("mgender"), warn);
#else
                    AlertWindow::showMessageBox(AlertWindow::WarningIcon, String("mgender"), warn);
#endif  // if/else ANDROID        
                }
                dispatcher.ResumeThread();
            }
            delete &fc;
            active_chooser = nullptr;
        });        
}  // end MgenderApp::LoadScriptFile()

void MgenderApp::SetLogFile()
{
    
    FileChooser* fileChooser = new FileChooser(String("mgender"), File::getCurrentWorkingDirectory(), String("*"));
    ASSERT(nullptr == active_chooser);
    active_chooser = fileChooser;
    int flags = FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles; 
    fileChooser->launchAsync(flags,  
        [this](const FileChooser& fc)
        {
            if (fc.getResults().size() > 0)
            {
                String path = fc.getResult().getFullPathName();
                dispatcher.SuspendThread();
                if (!mgen.OpenLog(path.toRawUTF8(), false, false))
                {
                    String warn = String("Unable to create log \"") + path + String("\"");
#ifdef ANDROID
                    AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, String("mgender"), warn);
#else
                    AlertWindow::showMessageBox(AlertWindow::WarningIcon, String("mgender"), warn);
#endif  // if/else ANDROID        
                }
                dispatcher.ResumeThread();
            }
            delete &fc;
            active_chooser = nullptr;
        });
}  // end MgenderApp::SetLogFile()

void MgenderApp::initialise(const String& commandLine)
{
    // This method is where you should put your application's initialisation code ...
    dispatcher.StartThread();
    ctrl_window = new ControlComponent(*this);
    main_window.reset(new MainWindow(getApplicationName(), ctrl_window));
    main_window->addKeyListener(cmd_manager.getKeyMappings());
    report_flow_count = 0;
    dispatcher.SuspendThread();
    //mgen.SetLogFunction(DoMgenLogEvent);
    //mgen.SetLogFile(stdout);
    mgen.SetController(this);
    mgen.SetComputeAnalytics(true);
    mgen.SetAnalyticWindow(5.0);
    ProcessCommandLine();
    mgen.Start();
    // We "start probing" (and immediately stop it) so an
    // ephemeral port is established as the probe source port
    ProtoAddress addr;
    addr.ConvertFromString("127.0.0.1");
    addr.SetPort(8001);
    StartProbing(addr, "periodic", 1.0, 512, NULL);
    StopProbing();
    SetProbeAddress(addr, mgen.GetDefaultMulticastInterface());  // 8001 is our default probing port (TBD - make configurable)
    ctrl_window->OnUpdateProbeAddress();
    prune_timer.SetInterval(5.0);
    prune_timer.SetRepeat(-1.0);
    dispatcher.GetDispatcher().ActivateTimer(prune_timer);
    dispatcher.ResumeThread();
    
}  // end mgenderApp::initialise()

void MgenderApp::shutdown()
{
    // Add your application's shutdown code here ...
    if (nullptr != active_chooser)
    {
        delete active_chooser;
        active_chooser = nullptr;
    }
    if (prune_timer.IsActive())
        prune_timer.Deactivate();
    mgen.Stop();
    dispatcher.Stop();
#ifdef JUCE_MAC
    MenuBarModel::setMacMainMenu(nullptr);
#endif // JUCE_MAC
    main_window = nullptr; // (deletes our window)
}  // end MgenderApp::shutdown()

bool MgenderApp::SetProbeAddress(const ProtoAddress& probeAddr, const char* ifaceName)
{
    // Also joins multicast address if applicable
    char eventText[256];
    UINT16 thePort = probeAddr.GetPort();
    UINT16 probePort = probe_addr.GetPort();
    if (thePort != probePort)
    {
        if (0 != probePort)
        {
            // Stop listening on old port
            sprintf(eventText, "ignore udp %hu", probePort);
            DrecEvent event;
            event.InitFromString(eventText);
            dispatcher.SuspendThread();
            mgen.ProcessDrecEvent(event);
            dispatcher.ResumeThread();
            probePort = 0;
        }
        if (0 != thePort)
        {
            // Try to listen on new port
            sprintf(eventText, "listen udp %hu", thePort);
            DrecEvent event;
            event.InitFromString(eventText);
            dispatcher.SuspendThread();
            bool result = mgen.ProcessDrecEvent(event);
            dispatcher.ResumeThread();
            if (result)
            {
                probePort = thePort;
            }
            else
            {
                PLOG(PL_ERROR, "MgendrApp::SetProbeAddress() error: listen failed\n");
                return false;
            }
        }
    }
    // If mcast_addr or iface_name has changed leave and rejoin
    if (!probeAddr.HostIsEqual(probe_addr) ||
        ((NULL != ifaceName) && (0 != strcmp(ifaceName, iface_name))) ||
        ((NULL == ifaceName) && ('\0' != iface_name[0])))
    {
        if (probe_addr.IsMulticast())
        {
            // Leave the probe group (note this could conflict with an MGEN scripted membership)
            if ('\0' != iface_name[0])
                sprintf(eventText, "leave %s port %hu interface %s", probe_addr.GetHostString(), probe_addr.GetPort(), iface_name);
            else
                sprintf(eventText, "leave %s port %hu", probe_addr.GetHostString(), probe_addr.GetPort());
            DrecEvent drecEvent;
            drecEvent.InitFromString(eventText);
            if (!mgen.ProcessDrecEvent(drecEvent))
                PLOG(PL_WARN, "MgendrApp::SetProbeAddress() warning: Multicast leave failure?!\n");
        }
        
        if (probeAddr.IsMulticast())
        {
            if (NULL != ifaceName)
                sprintf(eventText, "join %s port %hu interface %s", probeAddr.GetHostString(), probeAddr.GetPort(), ifaceName);
            else
                sprintf(eventText, "join %s port %hu", probeAddr.GetHostString(), probeAddr.GetPort());
            DrecEvent drecEvent;
            drecEvent.InitFromString(eventText);
            if (!mgen.ProcessDrecEvent(drecEvent))
            {
                PLOG(PL_ERROR, "MgendrApp::SetProbeAddress error: unable to join multicast group!\n");
                return false;
            }    
        }
    }
    // Update probe_addr/port and iface_name in any case since one of them has changed
    probe_addr = probeAddr;
    if (NULL == ifaceName)
        iface_name[0] = '\0';
    else
        strncpy(iface_name, ifaceName, 255);
    return true;  // probing was disabled
}  // end MgenderApp::SetProbeAddress()

bool MgenderApp::StartProbing(const ProtoAddress&   probeAddr, 
                              const char*           pattern,
                              double                rate,
                              unsigned int          size,
                              const char*           ifaceName)
{
    dispatcher.SuspendThread();
    MgenFlow* probeFlow = mgen.FindFlowById(PROBE_FLOW_ID);
    const char* cmd = (NULL != probeFlow) ? "mod" : "on";
    char eventString[256];
    if (probeAddr.IsMulticast())
    {
        if (NULL != ifaceName)
            sprintf(eventString, "%s %u udp dst %s/%hu %s [%lf %u] ttl 255 interface %s",
                    cmd, PROBE_FLOW_ID, probeAddr.GetHostString(), probeAddr.GetPort(),
                    pattern, rate, size, ifaceName);
        else
            sprintf(eventString, "%s %u udp dst %s/%hu %s [%lf %u] ttl 255",
                    cmd, PROBE_FLOW_ID, probeAddr.GetHostString(), probeAddr.GetPort(),
                    pattern, rate, size);
    }
    else
    {
        sprintf(eventString, "%s %u udp dst %s/%hu %s [%lf %u]",
                cmd, PROBE_FLOW_ID, probeAddr.GetHostString(), probeAddr.GetPort(),
                pattern, rate, size);
    }
    MgenEvent event;
    event.InitFromString(eventString);
    bool result = mgen.ProcessMgenEvent(event);
    if (result)
    {
        if (NULL == probeFlow)
            probeFlow = mgen.FindFlowById(PROBE_FLOW_ID);
        ASSERT(NULL != probeFlow);
        probeFlow->Resume();
        UINT16 srcPort = probeFlow->GetSrcPort();
        // Listen on the probe_src_port for unicast responses to probing
        sprintf(eventString, "listen udp %hu", srcPort);
        DrecEvent drecEvent;
        drecEvent.InitFromString(eventString);
        if (mgen.ProcessDrecEvent(drecEvent))
            probe_src_port = srcPort;
        else
            StopProbing();
    }
    dispatcher.ResumeThread();
    return result;
}  // end MgenderApp::StartProbing()

void MgenderApp::StopProbing()
{
    dispatcher.SuspendThread();
    MgenFlow* flow = mgen.FindFlowById(PROBE_FLOW_ID);
    if (NULL != flow) flow->Suspend();
    if (0 != probe_src_port)
    {
        char eventText[64];
        sprintf(eventText, "ignore udp %hu", probe_src_port);
        DrecEvent drecEvent;
        drecEvent.InitFromString(eventText);
        mgen.ProcessDrecEvent(drecEvent);
        probe_src_port = 0;
    }    
    dispatcher.ResumeThread();
}  // end MgenderApp::StopProbing()

void MgenderApp::OnMgenLogEvent(const char* text)
{
   size_t index = 0;
   size_t len = strlen(text);
   while (index < len)
   {
       size_t space = 511 - event_index;
       size_t nbytes = (space > len) ? len : space;
       const char* ptr = strchr(text+index, '\n');
       if (NULL !=  ptr)
       {
           size_t offset = (ptr - text) - index + 1;
           if (offset < nbytes)
               nbytes = offset;
       }
       strncpy(event_buffer+event_index, text+index, nbytes);
       space -= nbytes;
       index += nbytes;
       event_index += nbytes;
       if ((NULL != ptr) || (0 == space))
       {
            if ((NULL != ptr) && (0 == space))
               index = ptr - text + 1; // advance to next line  
            // Process and reset our 'event_buffer'
            //ProtoTokenator tk(event_buffer);
            //const char* eventType = tk.GetNextItem();  // skip first field (timestamp)
            /*eventType = tk.GetNextItem();
            if ((NULL != eventType) && (0 == strcmp(eventType, "REPORT")))
            {
                MessageManagerLock mml;
                if (mml.lockWasGained())
                    ctrl_window->AppendReportContent(event_buffer);
            }*/
            event_index = 0; 
       }
   }
}  // end MgenderApp::OnMgenLogEvent()


MgenderApp::Report::Report(Protocol                       theProtocol,
                          const ProtoAddress&             srcAddr,      
                          const ProtoAddress&             dstAddr,      
                          UINT32                          flowId,       
                          const ProtoAddress*             reporterAddr) 
  : feedback_id(0)
{
    report_keysize = BuildKey(report_key, MAX_KEY, theProtocol, srcAddr, dstAddr, flowId, reporterAddr);
    protocol = theProtocol;
    src_addr = srcAddr;
    dst_addr = dstAddr;
    flow_id = flowId;
    if (NULL != reporterAddr)
        reporter_addr = *reporterAddr;
    else
        reporter_addr.Invalidate();
}

MgenderApp::Report::~Report()
{
}

unsigned int MgenderApp::Report::BuildKey(char*                          buffer,
                                          unsigned int                   buflen,
                                          Protocol                       theProtocol,
                                          const ProtoAddress&            srcAddr,      
                                          const ProtoAddress&            dstAddr,      
                                          UINT32                         flowId,       
                                          const ProtoAddress*            reporterAddr) 
{
    // TBD - we could be more efficient and encode the address type 
    //       info into the "report_key" and pull these from the key
    //       instead of keeping separate member variables
    if (buflen < MAX_KEY)
    {
        PLOG(PL_ERROR, "MgenderApp::Report::BuildKey() error: 'buffer' must be at least %u bytes\n", MAX_KEY);
        return 0;
    }
    unsigned index = 0;
    buffer[index++] = (char)theProtocol;
    memcpy(buffer+index, srcAddr.GetRawHostAddress(), srcAddr.GetLength());
    index += srcAddr.GetLength();
    UINT16 p = srcAddr.GetPort();
    memcpy(buffer+index, &p, 2);
    index += 2;
    memcpy(buffer+index, dstAddr.GetRawHostAddress(), dstAddr.GetLength());
    index += dstAddr.GetLength();
    p = dstAddr.GetPort();
    memcpy(buffer+index, &p, 2);
    index += 2;
    memcpy(buffer+index, &flowId, 4);
    index += 4;
    if (NULL != reporterAddr)
    {
        memcpy(buffer+index, reporterAddr->GetRawHostAddress(), reporterAddr->GetLength());
        index += reporterAddr->GetLength();
        p = reporterAddr->GetPort();
        memcpy(buffer+index, &p, 2);
        index += 2;
    }
    return (index << 3);
}  // end MgenderApp::Report::BuildKey()

bool MgenderApp::Report::Update(const ProtoTime&            theTime, 
                                const MgenAnalytic::Report& report, 
                                const ProtoTime&            sentTime)
{
    if (reporter_addr.IsValid())
    {
        // Make sure it's not a duplicate of the current report
        // (and not an older one) 
        
        ProtoTime newEnd = sentTime - report.GetWindowOffset();
        ProtoTime newStart = newEnd - report.GetWindowSize();
        ProtoTime oldEnd = report_time - report_offset;
        ProtoTime oldStart = oldEnd - report_window;
        if (newEnd < oldStart)
            return false; // an older report
        if (newStart < oldEnd)
        {
            double overlap = oldEnd - newStart;
            if (overlap > 0.5*report_window)
                return false;  // it's redundant (TBD - could check for matching values?)
        }
    }
    rate_ave = report.GetRateAve();
    loss_fraction = report.GetLossFraction();
    late_ave = report.GetLatencyAve();
    late_min = report.GetLatencyMin();
    late_max = report.GetLatencyMax();
    report_window = report.GetWindowSize();
    report_offset = report.GetWindowOffset();
    report_time = sentTime;
    update_time = theTime;
    return true;
            
}  // end MgenderApp::Report::Update()

MgenderApp::Report* MgenderApp::ReportTable::FindReport(Protocol                       theProtocol,
                                                        const ProtoAddress&            srcAddr,      
                                                        const ProtoAddress&            dstAddr,      
                                                        UINT32                         flowId,       
                                                        const ProtoAddress*            reporterAddr) 
{
    char key[Report::MAX_KEY];
    unsigned int keysize = 
        Report::BuildKey(key, Report::MAX_KEY, theProtocol, srcAddr, dstAddr, flowId, reporterAddr); 
    return Find(key, keysize);
}  // end MgenderApp::ReportTable::FindReport()


// MgenController method implementations
void MgenderApp::OnUpdateReport(const ProtoTime&              theTime, 
                                const MgenAnalytic::Report&   localReport)
{
    ProtoAddress srcAddr, dstAddr;
    localReport.GetSrcAddr(srcAddr);
    localReport.GetDstAddr(dstAddr);
    Report* report = report_table.FindReport(localReport.GetProtocol(),
                                             srcAddr, dstAddr,
                                             localReport.GetFlowId());
    if (NULL == report)
    {
        report = new Report(localReport.GetProtocol(),
                            srcAddr, dstAddr,
                            localReport.GetFlowId());
        if (NULL == report)
        {
            PLOG(PL_ERROR,"MgenderApp::OnUpdateReport() new Report error: %s\n", GetErrorString());
            return;
        }
        if (!report_table.Insert(*report))
        {
            PLOG(PL_ERROR,"MgenderApp::OnUpdateReport() error: unable to insert new flow report\n");
            delete report;
            return;
        }
        if (!report_list.Append(*report))
        {
            PLOG(PL_ERROR,"MgenderApp::OnUpdateReport() error: unable to insert new flow report\n");
            report_table.Remove(*report);
            delete report;
            return;
        }
        if (report_flow_count >= MAX_REPORT_FLOWS)
        {
            Report* oldest = report_list.GetHead();
            ASSERT(NULL != oldest);
            RemoveFlowReport(*oldest);
            delete oldest;
        }
        else
        {
            report_flow_count++;
        }
        if (PROBE_FLOW_ID == report->GetFlowId())
        {
            // This is a new probing flow from a remote, so set up
            // a reciprocal feedback flow. 
            char eventString[256];
            UINT32 feedbackId = next_feedback_flow_id;
            const ProtoAddress& reportSrcAddr = report->GetSrcAddr();
            // Send 5 feedback message per averaging window for robust feedback
            double feedbackRate = 5.0 / (mgen.GetAnalyticWindow());
            sprintf(eventString, "on %u udp dst %s/%hu periodic [%lf 96] feedback",
                        feedbackId, reportSrcAddr.GetHostString(), 
                        reportSrcAddr.GetPort(), feedbackRate);
            MgenEvent event;
            event.InitFromString(eventString);
            dispatcher.SuspendThread();
            if (mgen.ProcessMgenEvent(event))
            {
                // TBD - we could keep trying until we find a flow id that works?
                report->SetFeedbackId(next_feedback_flow_id++);
            }            
            else
            {
                PLOG(PL_ERROR, "MgenderApp::OnUpdateReport() error: unable to initiate feedback\n");
            }
            dispatcher.ResumeThread();
        }
    }
    if (report->Update(theTime, localReport, theTime))
    {
        report_list.Remove(*report);
        report_list.Append(*report);
        if ((probe_src_port != report->GetDstAddr().GetPort()) || (!filter_feedback)) // filter out feedback flows
            AppendReportContent(*report, theTime);
    }
    PruneFlowReports(theTime);
}  // end MgenderApp::OnUpdateReport()

void MgenderApp::OnRecvReport(const ProtoTime&              theTime, 
                              const MgenAnalytic::Report&   recvReport,
                              const ProtoAddress&           senderAddr,
                              const ProtoTime&              sentTime)
{
    ProtoAddress srcAddr, dstAddr;
    recvReport.GetSrcAddr(srcAddr);
    recvReport.GetDstAddr(dstAddr);
    Report* report = report_table.FindReport(recvReport.GetProtocol(),
                                             srcAddr, dstAddr,
                                             recvReport.GetFlowId(),
                                             &senderAddr);
    if (NULL == report)
    {
        report = new Report(recvReport.GetProtocol(),
                            srcAddr, dstAddr,
                            recvReport.GetFlowId(),
                            &senderAddr);
        if (NULL == report)
        {
            PLOG(PL_ERROR,"MgenderApp::OnRecvReport() new Report error: %s\n", GetErrorString());
            return;
        }
        if (!report_table.Insert(*report))
        {
            PLOG(PL_ERROR,"MgenderApp::OnRecvReport() error: unable to insert new flow report\n");
            delete report;
            return;
        }
        if (!report_list.Append(*report))
        {
            PLOG(PL_ERROR,"MgenderApp::OnRecvReport() error: unable to insert new flow report\n");
            report_table.Remove(*report);
            delete report;
            return;
        }
        if (report_flow_count >= MAX_REPORT_FLOWS)
        {
            Report* oldest = report_list.GetHead();
            ASSERT(NULL != oldest);
            RemoveFlowReport(*oldest);
            delete oldest;
        }
        else
        {
            report_flow_count++;
        }
    }
    if (report->Update(theTime, recvReport, sentTime))
    {
        report_list.Remove(*report);
        report_list.Append(*report);
        AppendReportContent(*report, theTime);
    }
    
    if (dynamic_probe_addr &&
        probe_addr.IsUnicast() &&
        report->GetDstAddr().IsEqual(probe_addr) &&
        !report->GetReporterAddr().IsEqual(probe_addr))
    {
        // The probe recipient may have changed it's address?
        ProtoAddress probeAddr = report->GetReporterAddr();
        probeAddr.SetPort(probe_addr.GetPort());
        SetProbeAddress(probeAddr, GetInterface());
        ctrl_window->OnUpdateProbeAddress();
    }
    
    PruneFlowReports(theTime);
}  // end MgenderApp::OnRecvReport()


void MgenderApp::OnPruneTimeout(ProtoTimer& /*theTimer*/)
{
    ProtoTime currentTime;
    currentTime.GetCurrentTime();
    PruneFlowReports(currentTime);
}

void MgenderApp::PruneFlowReports(const ProtoTime& theTime)
{
    // Oldest flows are at head of report list
    double ageMax = 10.0*mgen.GetAnalyticWindow();
    ReportList::Iterator iterator(report_list);
    Report* nextReport;
    while (NULL != (nextReport = iterator.GetNextItem()))
    {
        double age = theTime - nextReport->GetUpdateTime();
        if (age > ageMax) 
        {
            RemoveFlowReport(*nextReport);
            mgen.RemoveAnalytic(nextReport->GetProtocol(),
                                nextReport->GetSrcAddr(),
                                nextReport->GetDstAddr(),
                                nextReport->GetFlowId());
            delete nextReport;
            report_flow_count -= 1;
        }
    }    
}  //  end MgenderApp::PruneFlowReports()

void MgenderApp::RemoveFlowReport(Report& report)
{
    // Remove from list and deactivate/remove feedback flow if applicable
    report_list.Remove(report);
    report_table.Remove(report);
    if (0 != report.GetFeedbackId())
    {
        MgenFlow* feedbackFlow = mgen.FindFlowById(report.GetFeedbackId());
        if (NULL != feedbackFlow)
        {
            feedbackFlow->StopFlow();
            mgen.RemoveFlow(*feedbackFlow);
            delete feedbackFlow;
        }
        // Conserve feedback flow id a little if possible
        if (report.GetFeedbackId() == (next_feedback_flow_id - 1))
            next_feedback_flow_id -= 1;  
        report.SetFeedbackId(0);
    }
}  // end MgenderApp::RemoveFlowReport()


void MgenderApp::AppendReportContent(Report& report, const ProtoTime& theTime)
{
    int64 msec = theTime.sec()*1.0e+03 + theTime.usec()*1.0e-03;
    Time juceTime(msec);    
    if (report_lines.size() >= MAX_REPORT_LINES)
            report_lines.remove(0);
    // Append line to report_lines string array
    String line = juceTime.toString(false, true, true, true);
    line += String(".") + String::formatted("%06u", theTime.usec());
    switch(report.GetProtocol())
    {
        case UDP:
            line += String(" UDP:");
            break;
        case TCP:
            line += String(" TCP:");
            break;
        default:
            line += String(" ???:");
            break;
    }
    line += String(report.GetSrcAddr().GetHostString()) + String("/");
    line += String(report.GetSrcAddr().GetPort()) + String("->");
    line += String(report.GetDstAddr().GetHostString()) + String("/");
    line += String(report.GetDstAddr().GetPort()) + String(",");
    line += String(report.GetFlowId()) + String(" ");
    line += String("rate>") + String(report.GetRateAve()*8.0e-03) + String(" kbps loss>");
    line += String(report.GetLossFraction()) + String(" latency ave>");
    line += String(report.GetLatencyAve()) + String(" min>");
    line += String(report.GetLatencyMin()) + String(" max>");
    line += String(report.GetLatencyMax()) + String(" sec");
    if (report.GetReporterAddr().IsValid())
        line += String(" reporter>") + String(report.GetReporterAddr().GetHostString());
    report_lines.add(line);
    //const MessageManagerLock mml(Thread::getCurrentThread());;
    //if (mml.lockWasGained())
    //    ctrl_window->SetReportContentDirect(report_lines.joinIntoString(StringRef("\n\n")));
    // This invokes the ControlComponent AsyncUpdate for thread safety
    ctrl_window->SetReportContent(report_lines.joinIntoString(StringRef("\n\n")));
    
}  // end MgenderApp::AppendReportContent()

///////////////////////////////////////////////////
// JUCE "boiler plate" stuff we should embed into
// a "juceProtoApp" base class

MgenderApp::MainWindow::MainWindow(String name, ControlComponent* theControl)
 : DocumentWindow(name, 
                  Desktop:: getInstance()
                    .getDefaultLookAndFeel()
                        .findColour(ResizableWindow::backgroundColourId),
                  DocumentWindow::allButtons)
{
    setUsingNativeTitleBar(true); 
    setResizable(true, false);  
#ifdef LINUX
    Image mgenIcon = ImageCache::getFromMemory(ControlComponent::mgenLogo_png, 
                                               ControlComponent::mgenLogo_pngSize);
    setIcon(mgenIcon);
    ComponentPeer* peer = getPeer();
    if (NULL != peer)
        peer->setIcon(mgenIcon);
#endif // LINUX
    centreWithSize(getWidth(), getHeight());
    setContentOwned(theControl, true);   
    setVisible (true);
#ifdef ANDROID
    setFullScreen(true);    
#endif // ANDROID
}

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION(MgenderApp)
