
#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "ControlComponent.h"

#include "protoDispatcher.h"
#include "mgen.h"


// Wrap a JUCE thread around a ProtoDispatcher
class DispatcherThread : public Thread
{
    public:
        DispatcherThread() 
            : Thread(String("MgenThread")), priority_boost(false), controller(NULL) {}
        virtual ~DispatcherThread() {}
        
        ProtoDispatcher& GetDispatcher()
            {return dispatcher;}
        
        bool SuspendThread()
            {return dispatcher.SuspendThread();}
        void ResumeThread()
            {dispatcher.ResumeThread();}
        
        bool StartThread(bool                         priorityBoost = false,
                         ProtoDispatcher::Controller* theController = NULL)
        {
            priority_boost = priorityBoost;
            controller = theController;
            startThread();
            return true;
        }
        
        void Stop()
        {
            dispatcher.Stop();
            waitForThreadToExit(-1);
        }
        
        operator ProtoTimerMgr&()
            {return dispatcher;}
        operator ProtoSocket::Notifier&()
            {return dispatcher;}
        
    private:
        void run() override
        {
            dispatcher.StartThread(priority_boost, controller, (ProtoDispatcher::ThreadId)getThreadId());
        }
        ProtoDispatcher                 dispatcher;
        bool                            priority_boost;
        ProtoDispatcher::Controller*    controller;
        
};  // end class DispatcherThread

//==============================================================================
class MgenderApp  : public JUCEApplication, 
                    public MgenController,
                    //public ApplicationCommandTarget,
                    public MenuBarModel
{
public:
    //==============================================================================
    MgenderApp();
    ~MgenderApp();

    // These will be part of a juceProtoApp class at some point
    ProtoTimerMgr& GetTimerMgr() 
        {return dispatcher;}
    ProtoSocket::Notifier& GetSocketNotifier() 
        {return dispatcher;}
    
    const String getApplicationName() override       {return ProjectInfo::projectName; }
    const String getApplicationVersion() override    {return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override       {return true;}

    //==============================================================================
    void initialise (const String& commandLine) override;
    void shutdown() override;

    //==============================================================================
    void systemRequestedQuit() override
    {
        // This is called when the app is being asked to quit: you can ignore this
        // request and let the app carry on running, or call quit() to allow the app to close.
        quit();
    }

    void anotherInstanceStarted (const String& commandLine) override
    {
        // When another instance of the app is launched while this one is running,
        // this method is invoked, and the commandLine parameter tells you what
        // the other instance's command-line arguments were.
    }

    //==============================================================================
    /*
        This class implements the desktop window that contains an instance of
        our MainComponent class.
    */
    class MainWindow : public DocumentWindow
    {
        public:
            MainWindow(String name, ControlComponent* theControl);
            
            void closeButtonPressed() override
            {
                // This is called when the user tries to close this window. Here, we'll just
                // ask the app to quit when this happens, but you can change this to do
                // whatever you need.
                JUCEApplication::getInstance()->systemRequestedQuit();
            }

            /* Note: Be careful if you override any DocumentWindow methods - the base
               class uses a lot of them, so by overriding you might break its functionality.
               It's best to do all your work in your content component instead, but if
               you really have to override any DocumentWindow methods, make sure your
               subclass also calls the superclass's method.
            */

        private:
            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };  // end class MgenderApp::MainWindow
    
    ApplicationCommandManager& GetCommandManager() {return cmd_manager;}
    enum CommandIDs
    {
        CMD_ABOUT_APP = 1,
        CMD_LOAD_SCRIPT,
        CMD_SET_LOG,
        CMD_FILTER_FEEDBACK,
        CMD_DYNAMIC_ADDR
    };
    void getAllCommands(Array<CommandID>& cmds) override;
    void getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) override;
    ApplicationCommandTarget* getNextCommandTarget() override
        {return nullptr;}
    bool perform(const InvocationInfo& info) override;
    StringArray getMenuBarNames() override;
    PopupMenu getMenuForIndex(int topLevelMenuIndex, const String &menuName) override;
    void menuItemSelected(int menuItemID, int menuIndex) override;
    void ShowAppInfo();
    
    bool ProcessCommandLine();
            
    Mgen& GetMgen() {return mgen;}
    //UINT16 GetProbePort() const
    //    {return probe_addr.GetPort();}
    bool SetProbeAddress(const ProtoAddress& probeAddr, const char* ifaceName);
    const ProtoAddress& GetProbeAddress() const
        {return probe_addr;}
    const char* GetInterface() const
        {return (('\0' != iface_name[0]) ? iface_name : NULL);}
    bool StartProbing(const ProtoAddress&   probeAddr,
                      const char*           probePattern,
                      double                probeRate,
                      unsigned int          probeSize,
                      const char*           ifaceName);
    void StopProbing();
    bool IsProbing() const
        {return (0 != probe_src_port);}
    
    
    void LoadScriptFile();
    void SetLogFile();
    
private:
    class Report : public ProtoQueue::Item
    {
        public:
            Report(Protocol                       theProtocol,
                   const ProtoAddress&            srcAddr, 
                   const ProtoAddress&            dstAddr, 
                   UINT32                         flowId, 
                   const ProtoAddress*            reporterAddr = NULL);
            ~Report();
            
            static unsigned int BuildKey(char*                          buffer,
                                         unsigned int                   buflen,
                                         Protocol                       theProtocol,
                                         const ProtoAddress&            srcAddr,      
                                         const ProtoAddress&            dstAddr,      
                                         UINT32                         flowId,       
                                         const ProtoAddress*            reporterAddr);
            
            bool Update(const ProtoTime&            theTime, 
                        const MgenAnalytic::Report& report, 
                        const ProtoTime&            sentTime);
            
            Protocol GetProtocol() const
                {return protocol;}
            const ProtoAddress& GetSrcAddr() const
                {return src_addr;}
            const ProtoAddress& GetDstAddr() const
                {return dst_addr;}
            UINT32 GetFlowId() const
                {return flow_id;}
            const ProtoAddress& GetReporterAddr() const
                {return reporter_addr;}
            
            double GetRateAve() const 
                {return rate_ave;}
            double GetLossFraction() const
                {return loss_fraction;}
            double GetLatencyAve() const
                {return late_ave;}
            double GetLatencyMin() const
                {return late_min;}
            double GetLatencyMax() const
                {return late_max;}
            const ProtoTime& GetUpdateTime() const
                {return update_time;}
            UINT32 GetFeedbackId() const
                {return feedback_id;}
            
            void SetFeedbackId(UINT32 feedbackId)
                {feedback_id = feedbackId;}
            
            // Worst case key size is protocol + 3 IPv6 addrs, 3 ports, and a flowId
            enum {MAX_KEY = (1 + 3*16 + 3*2 + 4)};
            const char* GetKey() const
                {return report_key;}  
            unsigned int GetKeysize() const
                {return report_keysize;}  
        private:
            char            report_key[MAX_KEY];
            unsigned int    report_keysize;
            
            Protocol                        protocol;
            ProtoAddress                    src_addr;
            ProtoAddress                    dst_addr;
            UINT32                          flow_id;
            ProtoAddress                    reporter_addr;
            
            double                          rate_ave;
            double                          loss_fraction;
            double                          late_ave;
            double                          late_min;
            double                          late_max;
            ProtoTime                       report_time;
            double                          report_window;
            double                          report_offset;
            ProtoTime                       update_time;
            UINT32                          feedback_id;
   
    };  // end class MgenderApp::Report    

    class ReportTable : public ProtoIndexedQueueTemplate<Report>
    {
        public:
            Report* FindReport(Protocol                       theProtocol,
                               const ProtoAddress&            srcAddr, 
                               const ProtoAddress&            dstAddr, 
                               UINT32                         flowId, 
                               const ProtoAddress*            reporterAddr = NULL);
        private:
             // Required overrides for ProtoIndexedQueue subclasses
            // (Override these to determine how items are sorted)
            virtual const char* GetKey(const Item& item) const
                {return (static_cast<const Report&>(item).GetKey());}
            virtual unsigned int GetKeysize(const Item& item) const
                {return (static_cast<const Report&>(item).GetKeysize());}
    };  // end class MgenderApp::ReportTable
    
    class ReportList : public ProtoSimpleQueueTemplate<Report> {};
        
    static int DoMgenLogEvent(FILE*, const char*, ...);   
    void OnMgenLogEvent(const char* text);
    
    // MGEN flow id values starting at 9001 are used for
    // the as the probing flow id and (incrementally)
    // feedback flow ids
    enum {PROBE_FLOW_ID = 9001};
    enum {MAX_REPORT_FLOWS = 256};
    enum {MAX_REPORT_LINES = 256};
    
    void OnUpdateReport(const ProtoTime&              theTime, 
                        const MgenAnalytic::Report&   report) override;
    void OnRecvReport(const ProtoTime&              theTime, 
                      const MgenAnalytic::Report&   report,
                      const ProtoAddress&           senderAddr,
                      const ProtoTime&              sentTime) override;
    void OnPruneTimeout(ProtoTimer& theTimer);
    
    void RemoveFlowReport(Report& report);
    void PruneFlowReports(const ProtoTime& theTime);
    
    void AppendReportContent(Report& report, const ProtoTime& theTime);
    
    //ScopedPointer<MainWindow>   main_window;
    std::unique_ptr<MainWindow> main_window;
    ControlComponent*           ctrl_window;    
    ApplicationCommandManager   cmd_manager;
    
    //ProtoDispatcher             dispatcher;                                              
    DispatcherThread            dispatcher;                                                
    Mgen                        mgen;               
    ProtoTimer                  prune_timer;                                       
    ProtoAddress                probe_addr;
    char                        iface_name[256];
    UINT16                      probe_src_port;                                            
    UINT32                      next_feedback_flow_id;                                     
    ReportTable                 report_table;  // indexed by flow                          
    ReportList                  report_list;   // linked list ordered, oldest -> freshest  
    unsigned int                report_flow_count;                                         
    StringArray                 report_lines;   
    bool                        filter_feedback;
    bool                        dynamic_probe_addr;
    FileChooser*                active_chooser;
    
    char                        event_buffer[512];                                         
    unsigned int                event_index;                                               
};  // end class MgenderApp
