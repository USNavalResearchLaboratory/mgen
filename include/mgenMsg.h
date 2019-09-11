/*********************************************************************
 *
 * AUTHORIZATION TO USE AND DISTRIBUTE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: 
 *
 * (1) source code distributions retain this paragraph in its entirety, 
 *  
 * (2) distributions including binary code include this paragraph in
 *     its entirety in the documentation or other materials provided 
 *     with the distribution, and 
 *
 * (3) all advertising materials mentioning features or use of this 
 *     software display the following acknowledgment:
 * 
 *      "This product includes software written and developed 
 *       by Brian Adamson and Joe Macker of the Naval Research 
 *       Laboratory (NRL)." 
 *         
 *  The name of NRL, the name(s) of NRL  employee(s), or any entity
 *  of the United States Government may not be used to endorse or
 *  promote  products derived from this software, nor does the 
 *  inclusion of the NRL written and developed software  directly or
 *  indirectly suggest NRL or United States  Government endorsement
 *  of this product.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 ********************************************************************/
 
#ifndef _MGEN_MESSAGE
#define _MGEN_MESSAGE
      
#include "protokit.h"
#include "mgenGlobals.h"
#include "mgenPayload.h"
#include <stdio.h>  // for FILE*


// (TBD) rework MgenMsg class into more optimized form
class Mgen;
class DrecEvent;
/**
 * @class MgenMsg
 *
 * @brief Maintains state for an mgen message (e.g. flow id, sequence 
 * number, message length etc.).  Has logging facility for
 * sent/received messages and errors and functions to compute
 * message checksums.
 */

class MgenMsg
{
    friend class MgenTcpTransport; // for msg_len & mgen_msg_len

  public:
    
    enum {VERSION = 2};
    
    
    enum Error
    {
      ERROR_NONE = 0,
      ERROR_VERSION,
      ERROR_CHECKSUM,
      ERROR_LENGTH,
      ERROR_DSTADDR
    };
    
    enum AddressType 
    {
      INVALID_ADDRESS = 0, 
      IPv4            = 1, 
      IPv6            = 2
#ifdef SIMULATE
      ,SIM             = 3
#endif // SIMULATE
    };
    
    enum GPSStatus 
    {
      INVALID_GPS     = 0,
      STALE           = 1, 
      CURRENT         = 2
    };
    
    enum Flag 
    {
	  CLEAR       = 0x00,   // no flag
	  CONTINUES   = 0x01,   // message is a fragment
	  END_OF_MSG  = 0x02,   // end of message
	  CHECKSUM    = 0x04,   // message includes checksum
	  LAST_BUFFER = 0x08,   // last buffer in fragment
	  CHECKSUM_ERROR = 0x10 // checksum error on message
    };
    
    MgenMsg();
	~MgenMsg();
	MgenMsg& operator=(const MgenMsg&);
    UINT16 Pack(char* buffer, UINT16 bufferLen, bool includeChecksum, UINT32& tx_checksum);  
    
    bool Unpack(const char* buffer, UINT16 bufferLen,bool forceChecksum,bool log_data);
	static bool WriteChecksum(UINT32&   tx_checksum,
                              UINT8*    buffer,
                              UINT32    buflen);
    
    bool FlagIsSet(MgenMsg::Flag theFlag) {return (0 != (flags & theFlag));}  
    UINT16 GetMsgLen() const {return msg_len;}
	unsigned int   GetMgenMsgLen() const {return mgen_msg_len;}
	UINT32 GetFlowId() const {return flow_id;}
    unsigned int GetSeqNum() const {return seq_num;}

    const ProtoAddress& GetDstAddr() const {return dst_addr;}
    const ProtoAddress& GetHostAddr() const {return host_addr;}
    MgenMsg::Error GetError() {return msg_error;}
	void ClearError() {msg_error = ERROR_NONE;}
    
    // (TBD) enforce minimum message len
	void SetProtocol(Protocol theProtocol) {protocol = theProtocol;}
	Protocol GetProtocol() {return protocol;};
    void SetVersion(UINT8 value) {version = value;}
    void SetFlag(MgenMsg::Flag theFlag) {flags |= theFlag;}
    void ClearFlag(MgenMsg::Flag theFlag) {if (FlagIsSet(theFlag)) flags ^= theFlag;}
    void SetMsgLen(UINT16 msgLen) {msg_len = msgLen;}       
    void SetMgenMsgLen(unsigned int mgenMsgLen) {mgen_msg_len = mgenMsgLen;}
    void SetFlowId(UINT32 flowId) {flow_id = flowId;}
    void SetSeqNum(UINT32 seqNum) {seq_num = seqNum;}
    void SetTxTime(const struct timeval& txTime) {tx_time = txTime;}
    const struct timeval& GetTxTime() {return tx_time;}
    void SetDstAddr(const ProtoAddress& dstAddr) {dst_addr = dstAddr;}
    void SetSrcAddr(const ProtoAddress& srcAddr) {src_addr = srcAddr;}
    ProtoAddress& GetSrcAddr() {return src_addr;}
    void SetHostAddr(const ProtoAddress& hostAddr) {host_addr = hostAddr;}
    void SetGPSLatitude(double value) {latitude = value;}
    void SetGPSLongitude(double value) {longitude = value;}
    void SetGPSAltitude(INT32 value) {altitude = value;}
    void SetGPSStatus(GPSStatus status) {gps_status = status;}
    void SetMpPayload(const char* buffer, unsigned short len)
    {mp_payload = buffer; mp_payload_len = len;}
	void SetPayload(char *buffer);
	const char *GetPayload() const;
	const char *GetPayloadRaw();
	void SetPayloadRaw(char *inBuffer,UINT16 inLen);
	UINT16 GetPayloadLen() const;
    void SetError(MgenMsg::Error error) {msg_error = error;};
    void SetChecksumError() {msg_error = ERROR_CHECKSUM;};
	bool ComputeCRC() {return compute_crc;}
	void ComputeCRC(bool theFlag) {compute_crc = theFlag;}
    // For these, "msgBuffer" is a packed message buffer
    bool LogRecvEvent(FILE*                 logFile, 
                      bool                  logBinary,
                      bool                  local_time,
		      bool                  log_data,
		      bool                  log_gps_data,
                      char*                 msgBuffer,
                      bool                  flush,
                      const struct timeval& theTime);
	bool LogSendEvent(FILE*                 logFile, 
                      bool                  logBinary, 
                      bool                  local_time,
                      char*                 msgBuffer,
                      bool                  flush,
                      const struct timeval& theTime);
    bool LogTcpConnectionEvent(FILE*        logFile, 
                               bool                  logBinary,
                               bool                  local_time,
                               bool                  flush,
                               LogEventType          eventType, 
                               bool                  isClient,
                               const struct timeval& theTime);
    
    bool LogRecvError(FILE*                    logFile,
                      bool                     logBinary, 
                      bool                     local_time,
                      bool                     flush,
                      const struct timeval& theTime);

    void LogDrecEvent(LogEventType eventType, 
                      const DrecEvent *event, 
                      UINT16 portNumber,
                      Mgen& mgen);
    bool ConvertBinaryLog(const char* path,Mgen& mgen);
    
    static void ComputeCRC32(UINT32& checksum,
                             const UINT8* buffer, 
                             UINT32               buflen);
    
    static const UINT32 CRC32_XOROT;

  protected:
    UINT16  msg_len;
	unsigned int    mgen_msg_len;
    
  private:
    
    static UINT32 ComputeCRC32(const UINT8* buffer, 
                               UINT32               buflen);
    static const UINT32 CRC32_XINIT;
    static const UINT32 CRC32_TABLE[256];
    
    UINT8   version;
    UINT8   flags; 
    UINT16  packet_header_len;
    UINT32   flow_id; 
    UINT32   seq_num; 
    struct timeval  tx_time;
    ProtoAddress    dst_addr;
    ProtoAddress    src_addr;
    ProtoAddress    host_addr;
    
    double          latitude;
    double          longitude;
    INT32           altitude;
    GPSStatus       gps_status;
    UINT16  reserved;
	MgenPayload	    *payload;
    const char*     mp_payload;
    unsigned short  mp_payload_len;
    Protocol        protocol;
    Error           msg_error;
	bool            compute_crc;
    
    enum {FLAGS_OFFSET = 3};
};  // end class MgenMsg    

#endif // _MGEN_MESSAGE
