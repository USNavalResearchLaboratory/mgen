#include "mgenSimSinkTransport.h"
#include "mgen.h"

MgenSimSinkTransport::MgenSimSinkTransport(Mgen& theMgen,
                                           Protocol theProtocol,
                                           UINT16        thePort,
                                           const ProtoAddress&   theDstAddress)
  : MgenSinkTransport(theMgen,theProtocol,thePort,theDstAddress),
    msg_sink(NULL)
{
}

MgenSimSinkTransport::~MgenSimSinkTransport()
{

}
MgenSimSinkTransport::MgenSimSinkTransport(Mgen& theMgen,
                                     Protocol theProtocol)
  : MgenSinkTransport(theMgen,theProtocol)
{
}

bool MgenSimSinkTransport::SendMessage(MgenMsg& theMsg,const ProtoAddress& dst_addr,char* txBuffer)
{
  UINT32 txChecksum = 0;
  unsigned int len = 0;
  theMsg.SetFlag(MgenMsg::LAST_BUFFER);

  len = theMsg.Pack(txBuffer,theMsg.GetMsgLen(),mgen.GetChecksumEnable(),txChecksum);

  if (len == 0)
    return false; // no room

  if (mgen.GetChecksumEnable() && theMsg.FlagIsSet(MgenMsg::CHECKSUM))
    theMsg.WriteChecksum(txChecksum,(unsigned char*)txBuffer,(UINT32)len);

  if (!msg_sink->HandleMessage(txBuffer,theMsg.GetMsgLen(),theMsg.GetSrcAddr()))
    return false;

  LogEvent(SEND_EVENT,&theMsg,theMsg.GetTxTime(),txBuffer);
  return true;
    
} // end MgenSimSinkTransport::SendMessage
