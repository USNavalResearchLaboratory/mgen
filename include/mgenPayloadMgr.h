#ifndef _MGENPAYLOADMGR
#define _MGENPAYLOADMGR

#ifndef _MGENPAYLOADMGR_VERSION
#define MGENPAYLOADMGR_VERSION "0.0.1"
#endif // _MGENPAYLOADMGR_VERSION

enum {SCRIPT_LINE_MAX = 512}; 

#include "mgen.h"
#include "protokit.h"

#include <stdio.h>

class MgenPayloadMgr
{
 public:
  MgenPayloadMgr(ProtoTimerMgr& timerMgr,
		 ProtoSocket::Notifier& socketNotifier,
		 Mgen& mgen);
  virtual ~MgenPayloadMgr();
  Mgen& GetMgen() {return mgen;}
  bool Start();
  void Stop();
  void SetSocketNotifier(ProtoSocket::Notifier* socketNotifier)
  {
    socket_notifier = socketNotifier;
  }
  // A little utility class for reading script files line by line
  class FastReader
    {
    public:
      enum Result {OK, ERROR_, DONE};  // trailing '_' for WIN32
      FastReader();
      FastReader::Result Read(FILE* filePtr, char* buffer, unsigned int* len);
      FastReader::Result Readline(FILE* filePtr, char* buffer, 
				  unsigned int* len);
      FastReader::Result ReadlineContinue(FILE* filePtr, char* buffer, 
					  unsigned int* len,
					  unsigned int* lineCount = NULL);
      
    private:
      enum {BUFSIZE = 1024};
      char         savebuf[BUFSIZE];
      char*        saveptr;
      unsigned int savecount;

    };  // end class FastReader        


  // MgenPayloadMgr "global command" set
  enum Command
    {
      // Mgen Global commands
      INVALID_COMMAND,
      EVENT,
      START,     // specify absolute start time
      INPUT,     // input and parse an RAPR script
      OVERWRITE_MPMLOG,// open rapr output (log) file 
      MPMLOG,   // open rapr log file for appending
      OVERWRITE_MGENLOG,// open mgen output (log) file 
      MGENLOG,   // open mgen log file for appending
      SAVE,      // save info on exit.
      DEBUG,     // specify debug level
      TXLOG,     // turn transmit logging on
      LOCALTIME, // turn on localtime logging in mgen rather than gmtime (the default)
      NOLOG,     // no output
      BINARY,    // mgen binary log 
      FLUSH,     //
      LABEL,
      RXBUFFER,
      TXBUFFER,
      TOS,
      TTL,
      INTERFACE,
      CHECKSUM,
      TXCHECKSUM,
      RXCHECKSUM,
      // MgenPayloadMgr Global Commands
      //      LOAD_DICTIONARY, // loads dictionary file
      // MgenPayloadMgrApp Global Commands
      MGENEVENT  // mgen recv events
    };

  enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
  static CmdType GetCmdType(const char* cmd);
  static Command GetCommandFromString(const char* string);
  bool OnCommand(MgenPayloadMgr::Command cmd, const char* arg,bool override = false);
  bool ParseEvent(const char* lineBuffer,unsigned int lineCount);
  bool ParseScript(const char* path);
  void LogEvent(const char* cmd,const char* val);
  bool SendMgenCommand(const char* cmd, const char* val);
  bool OpenLog(const char* path, bool append, bool binary);
  void CloseLog();
  void SetLogFile(FILE* filePtr);
  bool OnTxTimeout(ProtoTimer& theTimer);

 private:
  ProtoTimerMgr&         timer_mgr;
  Mgen&                  mgen;
  static const StringMapper COMMAND_LIST[]; 
  ProtoTimer               payload_timer;
  bool                     started;
  bool                     stopped;
  ProtoTimer               start_timer;
  unsigned int             start_hour;  //absolute start time
  unsigned int             start_min;
  double                   start_sec;
  bool                     start_gmt;
  bool                     start_time_lock;
  FILE*        log_file;
  bool         log_binary; // not yet implemented but needed for common mgen functions
  bool         local_time;
  bool         log_flush;
  bool         log_open;
  bool         log_empty;

  ProtoSocket::Notifier* socket_notifier;
  
}; // end class mgenPayloadMgr

#endif // _MGENPAYLOADMGR
