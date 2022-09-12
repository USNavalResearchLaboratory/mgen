#ifndef _MGENPAYLOADMGRAPP
#define _MGENPAYLOADMGRAPP

#include "protokit.h"
#include "mgen.h"
#include "mgenPayloadMgr.h"

class MgenPayloadMgrApp : public ProtoApp, public MgenController
{
 public:
  MgenPayloadMgrApp();
  virtual ~MgenPayloadMgrApp();

  enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
  static const char* const CMD_LIST[];
  void Usage();

  // MgenController virtual methods
  void OnMsgReceive(MgenMsg& msg);
  void OnOffEvent(char* buffer, int len);
  void OnStopEvent(char* buffer, int len);

  virtual bool OnStartup(int argc, const char*const* argv);
  virtual bool ProcessCommands(int argc, const char*const* argv);
  virtual void OnShutdown();
  bool OnCommand(const char* cmd, const char* val);
  static CmdType GetCmdType(const char* cmd);

 private:
  void OnControlEvent(ProtoSocket& theSocket, ProtoSocket::Event theEvent);
  MgenPayloadMgr                 mgenPayloadMgr;
  Mgen                           mgen;
  ProtoPipe                      control_pipe;
  bool                           control_remote;
  ProtoSocket                    socket;
  ProtoAddress                   dstAddr;
}; // end class mgenPayloadMgrApp

#endif //_MGENPAYLOADMGRAPP
