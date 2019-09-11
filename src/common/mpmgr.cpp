#include "protokit.h"
#include "gpsPub.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

class MgenPayloadMgr 
{
    public:
        MgenPayloadMgr();
    
        bool Init(int argc, char* argv[]);
        void Run() {dispatcher.Run();}
        void Stop(int exitCode) {dispatcher.Stop(exitCode);}
        void Shutdown();  
            
    private:
        enum {PAYLOAD_MAX = 61};
        static const char* const KEYFILE; 
            
            
        void OnSocketRecv(ProtoSocket& theSocket, ProtoSocket::Event theEvent);
        static void SignalHandler(int sigNum);
        
        ProtoDispatcher dispatcher;
        ProtoSocket     socket;
        GPSHandle       payload_handle;
        unsigned char   payload_size;
    
}; // end class MgenPayloadMgr

MgenPayloadMgr::MgenPayloadMgr()
 : socket(ProtoSocket::UDP), payload_handle(NULL), payload_size(0)
{
    socket.SetListener(this, &MgenPayloadMgr::OnSocketRecv);
    socket.SetNotifier(static_cast<ProtoSocket::Notifier*>(&dispatcher));
}


const char* const MgenPayloadMgr::KEYFILE = "/tmp/mgenPayloadKey";


bool MgenPayloadMgr::Init(int /*argc*/, char** /*argv*/)
{
    if (!(payload_handle = GPSMemoryInit(KEYFILE, PAYLOAD_MAX+1)))
    {
        fprintf(stderr, "mpmgr: Error creating shared memory for payload!\n");
        return false;   
    }
    char buffer[PAYLOAD_MAX+1];
    memset(buffer, 0, (PAYLOAD_MAX+1));
    if ((PAYLOAD_MAX+1) != GPSSetMemory(payload_handle, 0, buffer, (PAYLOAD_MAX+1)))
        fprintf(stderr, "mpmgr: Error initing shared payload memory!\n"); 
    payload_size = 0;
        
    if (!socket.Open(5523))
    {
        fprintf(stderr, "mpmgr: Error opening UDP socket!\n");
        return false;
    }    
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    
    // Uncomment to create a dummy example payload
    /*char thePayload[32];
    thePayload[0] = 31;  // 31 bytes of payload
    for (int i = 1; i < 32; i++) thePayload[i] = i; 
    if (32 != GPSSetMemory(payload_handle, 0, thePayload, 32))
        fprintf(stderr, "mpmgr: Error setting dummy payload!\n"); // 
    */        
    return true;
}  // end MgenPayloadMgr::Init()

void MgenPayloadMgr::Shutdown()
{
    socket.Close();
    if (payload_handle)
        GPSPublishShutdown(payload_handle, KEYFILE);
}  // end MgenPayloadMgr::Shutdown()


void MgenPayloadMgr::OnSocketRecv(ProtoSocket& /*theSocket*/, ProtoSocket::Event /*theEvent*/)
{
    char buffer[512];
    unsigned int len = 512;
    ProtoAddress addr;
    if (socket.RecvFrom(buffer, len, addr))
    {    
        unsigned int offset = (unsigned int) buffer[0];
        len -= 1;
        if ((offset+len) > PAYLOAD_MAX)
        {
            fprintf(stderr, "mpmgr: Received invalid message from \"%s\" ...\n",
                            addr.GetHostString());
        }
        else
        {
            // Update shared memory "payload size" byte (offset ZERO) if needed
            if ((unsigned char)(offset+len) > payload_size)
            {
                payload_size = (unsigned char)(offset+len);
                if (1 != GPSSetMemory(payload_handle, 0, (char*)&payload_size, 1))
                    fprintf(stderr, "mpmgr: Error setting shared payload size memory!\n");   
            } 
            if (len != GPSSetMemory(payload_handle, offset+1, &buffer[1], len))
                fprintf(stderr, "mpmgr: Error setting shared payload memory!\n");       
        }
        //fprintf(stderr, "mpmgr: Received \"%s\" from \"%s\"\n",
        //        &buffer[1], addr.GetHostString());
    }
}  // end MgenPayloadMgr::OnSocketRecv()


MgenPayloadMgr theApp;  // global for signal handling
           
        
int main(int argc, char* argv[])
{
    if (theApp.Init(argc, argv))
    {
        theApp.Run();
        theApp.Shutdown();
        fprintf(stderr, "mpmgr: Done.\n");
        exit(0);
    }
    else
    {
         fprintf(stderr, "mpmgr: Error initializing application!\n");
         exit(-1);  
    }        
}  // end main()


void MgenPayloadMgr::SignalHandler(int sigNum)
{
    switch(sigNum)
    {
        case SIGTERM:
        case SIGINT:
            theApp.Stop(sigNum);
            break;
            
        default:
            fprintf(stderr, "gpsLogger: Unexpected signal: %d\n", sigNum);
            break; 
    }  
}  // end MgenPayloadMgr::SignalHandler()
