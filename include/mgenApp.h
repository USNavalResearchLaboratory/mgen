#ifndef _MGENAPP
#define _MGENAPP

#include "mgenGlobals.h"
#include "mgen.h"
#include "mgenVersion.h"
#include "protokit.h"

/**
 * @class MgenApp
 *
 * @brief The Mgen application, derived from ProtoApp.
 */
class MgenApp : public ProtoApp
{
    public:
        enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
    
        MgenApp();
        virtual ~MgenApp();

        virtual bool OnStartup(int argc, const char*const* argv);
        virtual bool ProcessCommands(int argc, const char*const* argv);
        virtual void OnShutdown(); 
    
    private:    
        void OnControlEvent(ProtoSocket& theSocket, ProtoSocket::Event theEvent);
        static CmdType GetCmdType(const char* string);
        bool OnCommand(const char* cmd, const char* val);
        void Init(char* scriptFile);
        void Usage();
        static const char* const CMD_LIST[];
        static bool GetInterfaceCounts(const char* ifName, 
                                       UINT32&     txCount,
                                       UINT32&     rxCount);
        
        bool OpenCmdInput(const char* path);
        void CloseCmdInput();
        static void DoCmdInputReady(ProtoDispatcher::Descriptor descriptor, 
                                    ProtoDispatcher::Event      theEvent, 
                                    const void*                 userData); 
        void OnCmdInputReady();
        bool ReadCmdInput(char* buffer, unsigned int& numBytes);
        bool IsCmdDelimiter(char byte)
        {return (('\n' == byte) || ('\r' == byte) || (';' == byte)); }

        Mgen              mgen;         // Mgen engine
        ProtoPipe         control_pipe; // remote control message pipe
        bool              control_remote;

        ProtoDispatcher::Descriptor cmd_descriptor;   // "stdin" control option
        char                        cmd_buffer[8192];
        unsigned int                cmd_length;
        
#ifdef HAVE_GPS
        static bool GetPosition(const void* gpsHandle, GPSPosition& gpsPosition);
        GPSHandle         gps_handle;
        GPSHandle         payload_handle;
#endif // HAVE_GPS
        bool              have_ports;
        bool              convert;
        char              convert_path[PATH_MAX];
        char              ifinfo_name[64];
        UINT32            ifinfo_tx_count;
        UINT32            ifinfo_rx_count;

}; // end class MgenApp

#endif // _MGENAPP
