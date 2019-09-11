#ifndef _MGEN_SIM_SINK_TRANSPORT
#define _MGEN_SIM_SINK_TRANSPORT
#include "mgenTransport.h"
/**
 * @class MgenSimSinkTransport
 *
 * @brief Mgen sink transport for use in NS simulation environment.
 */
class MgenSimSinkTransport : public MgenSinkTransport
{
  public:
    MgenSimSinkTransport(Mgen& mgen,Protocol theProtocol);
    MgenSimSinkTransport(Mgen& mgen,Protocol theProtocol, UINT16 thePort, const ProtoAddress& theDstAddress);
    ~MgenSimSinkTransport();

    bool SendMessage(MgenMsg& theMsg,const ProtoAddress& dst_addr,char* txBuffer);
    void SetSink(class ProtoMessageSink* theSink) {msg_sink = theSink;}
    class ProtoMessageSink* msg_sink;

}; // end class MgenTransport::MgenSimSinkTransport
    
#endif // _MGEN_SIM_SINK_TRANSPORT
