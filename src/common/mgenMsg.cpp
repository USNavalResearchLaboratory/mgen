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
#include "mgenGlobals.h"
#include "mgenMsg.h"
#include "mgen.h"

#include <string.h>
#include <time.h>
MgenMsg::MgenMsg()
  : msg_len(0), mgen_msg_len(0),
    version(VERSION), flags(0),  
    packet_header_len(0), flow_id(0), 
    seq_num(0), latitude(0),longitude(0),altitude(0),
    gps_status(INVALID_GPS),payload_type(USER_DATA),
    payload_len(0), payload_data(NULL),
    protocol(INVALID_PROTOCOL),
    msg_error(ERROR_NONE),
    compute_crc(true)
{

}
MgenMsg::~MgenMsg()
{
}

/*
MgenMsg& MgenMsg::operator=(const MgenMsg& x)
{
  msg_len = x.msg_len;
  mgen_msg_len = x.mgen_msg_len;
  version = x.version;
  flags = x.flags;
  packet_header_len = x.packet_header_len;
  flow_id = x.flow_id;
  seq_num = x.seq_num;
  SetTxTime(x.tx_time);
  SetDstAddr(x.dst_addr);
  SetSrcAddr(x.src_addr);
  SetHostAddr(x.host_addr);
  latitude = x.latitude;
  longitude = x.longitude;
  altitude = x.altitude;
  gps_status = x.gps_status;
  payload_type = x.payload_type;
  payload_len = x.payload_len;
  payload_data = x.payload_data;  // note the pointer here is externally managed
  protocol = x.protocol;
  msg_error = x.msg_error;
  compute_crc = x.compute_crc;
  return *this;
}
*/

UINT16 MgenMsg::Pack(UINT32* alignedBuffer, UINT16 bufferLen, bool includeChecksum, UINT32& tx_checksum)
{
    char* buffer = (char*)alignedBuffer;
    UINT16 len = 0;
    
    // If these assertions fail, the code base needs some reworking
    // for compiler with different size settings (or set compiler
    // settings so these values work)
    ASSERT(sizeof(INT8) == 1);
    ASSERT(sizeof(INT16) == 2);
    ASSERT(sizeof(INT32) == 4);
    
    UINT16 msgLen = bufferLen;

    UINT16 temp16 = htons(msg_len);
    memcpy(buffer+len,&temp16,sizeof(UINT16));
    len += sizeof(UINT16);
    
    // version
    buffer[len++] = (char)version;
    
    // flags
    buffer[len++] = (char)flags;
    
    // flow_id
    UINT32 temp32;
    temp32 = htonl(flow_id);
    memcpy(buffer+len, &temp32, sizeof(INT32)); 
    len += sizeof(INT32);
    
    // seq_num
    temp32 = htonl(seq_num);
    memcpy(buffer+len, &temp32, sizeof(INT32));
    len += sizeof(INT32);
    
    // tx_time(seconds)
    temp32 = htonl(tx_time.tv_sec);
    memcpy(buffer+len, &temp32, sizeof(INT32));
    len += sizeof(INT32);
    // tx_time(microseconds)
    temp32 = htonl(tx_time.tv_usec);
    memcpy(buffer+len, &temp32, sizeof(INT32));
    len += sizeof(INT32);
    
    // dst_port
    temp16 = htons(dst_addr.GetPort());
    memcpy(buffer+len, &temp16, sizeof(INT16));
    len += sizeof(INT16);
    // dst_addr fields
    AddressType addrType;
    switch (dst_addr.GetType())
    {
    case ProtoAddress::IPv4:
      addrType = IPv4;
      break;
    case ProtoAddress::IPv6:
      addrType = IPv6;
      break;
#ifdef SIMULATE
    case ProtoAddress::SIM:
      addrType = SIM;
      break;
#endif // SIMULATE
    default:
      DMSG(0, "MgenMsg::Pack() Error: unsupported address type\n");
      return 0;
    }
    UINT8 addrLen = dst_addr.GetLength();
    // dst_addr(type) 
    buffer[len++] = (char)addrType;
    // dst_addr(len)
    buffer[len++] = (char)addrLen;    
    // dst_addr(addr)
    memcpy(buffer+len, dst_addr.GetRawHostAddress(), addrLen);
    len += addrLen; 
    
    
    // The fields below are optional and only
    // packed if the msg_len permits
    
    // host_addr fields (not yet supported - TBD)
    
    switch (host_addr.GetType())
    {
    case ProtoAddress::IPv4:
      addrType = IPv4;
      break;
    case ProtoAddress::IPv6:
      addrType = IPv6;
      break;
#ifdef SIMULATE
    case ProtoAddress::SIM:
      addrType = SIM;
      break;
#endif // SIMULATE
    default:
      addrType = INVALID_ADDRESS; 
    }
    if (host_addr.IsValid())
      addrLen = host_addr.GetLength();
    else
      addrLen = 0;

    // Is there room for the host_port and host_addr?
    if (msgLen >= (len + addrLen + 4))
    {
        // host_port
        if (host_addr.IsValid())
          temp16 = htons(host_addr.GetPort());
        else
          temp16 = 0;
        memcpy(buffer+len, &temp16, sizeof(INT16));
        len += sizeof(INT16);
        // host_addr(type)
        buffer[len++] = (char)addrType;
        // host_addr(len)
        buffer[len++] = (char)addrLen;
        // host_addr(addr)
        if (addrLen)
          memcpy(buffer+len, host_addr.GetRawHostAddress(), addrLen);
        len += addrLen;
    }
    else
    {
        if (msgLen < len)
        {
            DMSG(0, "MgenMsg::Pack() Error: minimum MGEN message size not met\n");
            return 0;
        }
        memset(buffer+len, 0, msgLen-len);  
        packet_header_len = (UINT16)len;      
        return msgLen;
    } 
    // GPS position information
    // Is there room for the GPS info?
    if (msgLen >= (len + 13))
    {
        // latitude
        temp32 = htonl((UINT32)((latitude + 180.0)*60000.0));
        memcpy(buffer+len, &temp32, sizeof(INT32));
        len += sizeof(INT32);
        // longitude
        temp32 = htonl((UINT32)((longitude + 180.0)*60000.0));
        memcpy(buffer+len, &temp32, sizeof(INT32));
        len += sizeof(INT32);
        // altitude
        temp32 = htonl(altitude);
        memcpy(buffer+len, &temp32, sizeof(INT32));
        len += sizeof(INT32);
        // status
        buffer[len++] = (char) gps_status;
    }
    else
    {
        memset(buffer+len, 0, msgLen-len);
        packet_header_len = (UINT16)len;      
        return msgLen;    
    }   
    
    if (msgLen >= (len+1))
    {
        buffer[len++] = (UINT8)payload_type;
    }
    else
    {
        packet_header_len = (UINT16)len;      
        return msgLen;
    }
    if (msgLen >= (len+2))
    {
        UINT16 tmp16 = htons(payload_len);
        memcpy(buffer+len, &tmp16, 2);
        len += 2;
    }
    else
    {
        memset(buffer+len, 0, msgLen - len);
        packet_header_len = (UINT16)len;      
        return msgLen;
    }
    packet_header_len = (UINT16)len;  // everything _before_ the payload
    if ((NULL != payload_data) && (msgLen >= (len+payload_len)))
    {
        // Copy whatever there is room for
        memcpy(buffer+len, payload_data, payload_len);
        len += payload_len;
    }
    else
    {
        memset(buffer+len-2, 0, 2);// zero payload_len
    }
    // Zero-fill rest of buffer
    if (msgLen > len)
    {
#ifdef RANDOM_FILL
        if (msgLen >= (2+len)) 
        { 
            //2 or more bytes filler
            memset(buffer+len,0,2);
            srand(time(NULL));
            for (int fill=len+2;fill < msgLen;fill++)
                memset(buffer+fill,rand(),1);
        } //1 byte difference
        else if (msgLen != len) 
        {
            memset(buffer+len,0,1);			  
        }
#else
        memset(buffer+len, 0, msgLen - len);
#endif //RANDOM_FILL
    }
    if (includeChecksum)
    {
        if (msgLen > (len + 4)) 
        {
            buffer[FLAGS_OFFSET] |= CHECKSUM;  // set "CHECKSUM" flag
            SetFlag(CHECKSUM);
        } 
        else
        {
            PLOG(PL_ERROR,"MgenMsg::PACK() no room for checksum\n");
        }
        if (FlagIsSet(LAST_BUFFER)) 
          ComputeCRC32(tx_checksum,(UINT8*)buffer,msgLen - 4);
        else
          ComputeCRC32(tx_checksum,(UINT8*)buffer,msgLen);
        
        ClearFlag(LAST_BUFFER);
    }
    return msgLen;
}  // end MgenMsg::Pack()

bool MgenMsg::Unpack(UINT32* alignedBuffer, UINT16 bufferLen, bool forceChecksum, bool log_data)
{
    // init optional fields
    host_addr.Invalidate();
    gps_status = INVALID_GPS;
    
    unsigned int len = 0;
    
    if (bufferLen < MIN_SIZE) 
	{
        DMSG(0, "MgenMsg::Unpack() error: INT16 message\n");
        msg_error = ERROR_LENGTH;
        return false;
	}
    const char* buffer = (const char*)alignedBuffer;
    UINT16 temp16;
    memcpy(&temp16,buffer+len,sizeof(INT16));
    msg_len = ntohs(temp16);
    len += sizeof (INT16);
    
    // version
    version = (UINT8)buffer[len++];
    if (version != VERSION)
	{
        DMSG(0, "MgenMsg::Unpack() error: bad protocol version: %d\n", version);
        msg_error = ERROR_VERSION;
        //exit(0);
        return false;   
	}
    
    // flags
    flags = (UINT8)buffer[len++];   
    
    // flow_id
    UINT32 temp32;
    memcpy(&temp32, buffer+len, sizeof(INT32));
    flow_id = ntohl(temp32);
    len += sizeof(INT32);
    
    // seq_num 
    memcpy(&temp32, buffer+len, sizeof(INT32));
    seq_num = ntohl(temp32);
    len += sizeof(INT32);
    
    // tx_sec(seconds)
    memcpy(&temp32, buffer+len, sizeof(INT32));
    tx_time.tv_sec = ntohl(temp32);
    len += sizeof(INT32);
    // tx_usec(microseconds)
    memcpy(&temp32, buffer+len, sizeof(INT32));
    tx_time.tv_usec = ntohl(temp32);
    len += sizeof(INT32);
    
    // dst_port
    memcpy(&temp16, buffer+len, sizeof(INT16));
    UINT16 dstPort = ntohs(temp16);
    len += sizeof(INT16);
    
    // dst_addr(type)
    ProtoAddress::Type addrType;
    switch (buffer[len++])
	{
    case IPv4:
	  addrType = ProtoAddress::IPv4;
	  break;
    case IPv6:
	  addrType = ProtoAddress::IPv6;
	  break;
#ifdef SIMULATE
    case SIM:
	  addrType = ProtoAddress::SIM;
	  break;
#endif // SIMULATE
    default:
	  DMSG(0, "MgenMsg::Unpack() unsupported address type\n");
	  msg_error = ERROR_DSTADDR;
	  return false;   
	}
    // dst_addr(len)
    unsigned int addrLen = (UINT8)buffer[len++];
    // dst_addr(addr)
    dst_addr.SetRawHostAddress(addrType, buffer+len, addrLen); 
    dst_addr.SetPort(dstPort);
    len += addrLen;
    // host_addr fields
    if ((len+4) <= bufferLen)
	{
        // host_port
        memcpy(&temp16, buffer+len, sizeof(INT16));
        UINT16 hostPort = ntohs(temp16);
        len += sizeof(INT16);
        
        // host_addr(type)
        switch (buffer[len++])
	    {
        case IPv4:
	      addrType = ProtoAddress::IPv4;
	      break;
        case IPv6:
	      addrType = ProtoAddress::IPv6;
	      break;
        default:
	      addrType = ProtoAddress::INVALID;
	      //DMSG(0, "MgenMsg::Unpack() unsupported host_addr type\n");
	      //return false;  
	      break; 
	    }
        // host_addr(len)
        addrLen = (UINT8)buffer[len++];
        // host_addr(addr)
        if ((len+addrLen) <= bufferLen)
	    {
            if (ProtoAddress::INVALID != addrType)
            {
                host_addr.SetRawHostAddress(addrType, buffer+len, addrLen);
                host_addr.SetPort(hostPort);
            }
            len += addrLen; 
	    }
        else
	    {
            // Not enough room
            packet_header_len = len;
            return true;
	    }
	}
    else
	{
        // Not enough room
        packet_header_len = len;
        return true;   
	}  
    
    // GPS position information
    if ((len+13) <= bufferLen)
	{
        // latitude
        memcpy(&temp32, buffer+len, sizeof(INT32));
        latitude = ((double)ntohl(temp32))/60000.0 - 180.0;
        len += sizeof(INT32);
        // longitude
        memcpy(&temp32, buffer+len, sizeof(INT32));
        longitude = ((double)ntohl(temp32))/60000.0 - 180.0;
        len += sizeof(INT32);
        // altitude 
        memcpy(&temp32, buffer+len, sizeof(INT32));
        altitude = ntohl(temp32);
        len += sizeof(INT32);
        // status
        gps_status = (GPSStatus)buffer[len++];
	}
    else
    {
        packet_header_len = len;  
        return true;
    }
    // "payload_type" field
    if ((len+1) <= bufferLen)
	{
        payload_type = (PayloadType)buffer[len++];
	}
    else
	{
        packet_header_len = len;
        return true;
	}
    // "payload_len" and "payload_data"
    if ((len+2) <= bufferLen)
    {
        memcpy(&temp16, buffer+len, sizeof(INT16));
        payload_len = ntohs(temp16);
        len += 2;
        packet_header_len = (UINT16)len;  
        if ((0 != payload_len) && ((len+payload_len) <= bufferLen))
        {
            payload_data = alignedBuffer + len/4;
            len += payload_len;
        }
        else
        {
            payload_len = 0;
            payload_data = NULL;
        }
    }
    return true;
}  // end MgenMsg::Unpack()

bool MgenMsg::WriteChecksum(UINT32& tx_checksum,UINT8* buffer,UINT32 bufferLen)
{
    if (bufferLen >= 4) 
    {
        UINT32 checksumPosition = bufferLen - 4;
        tx_checksum = (tx_checksum ^ CRC32_XOROT);
        
        DMSG(2,"Wrote: %lu at: %lu\n",tx_checksum,checksumPosition);
        tx_checksum = htonl(tx_checksum); 
        
        memcpy(buffer+checksumPosition,&tx_checksum,4);
        
        return true;
    }
    else 
    {
        DMSG(0, "MgenMsg::WriteChecksum() no room for checksum\n");  
        return false;
    }
    
} // MgenMsg::WriteChecksum()

void MgenMsg::ComputeCRC32(UINT32& checksum,
                           const UINT8* buffer, 
                           UINT32               bufferLength)
{
    static int totNumBytes;
    
    if (checksum == 0) {
        checksum = CRC32_XINIT;
        totNumBytes = 0;
    }
    
    totNumBytes += bufferLength;
    DMSG(2,"Calcing %d\n",totNumBytes); 
    
    for (UINT32 i = 0; i < bufferLength; i++)
      checksum = CRC32_TABLE[(checksum ^ *buffer++) & 0xFFL] ^ (checksum >> 8);
    
}  // end MgenMsg::ComputeCRC()

UINT32 MgenMsg::ComputeCRC32(const UINT8* buffer, 
                             UINT32               bufferLength)
{
    UINT32 result = CRC32_XINIT;
    for (UINT32 i = 0; i < bufferLength; i++)
      result = CRC32_TABLE[(result ^ *buffer++) & 0xFFL] ^ (result >> 8);
    // return XOR out value 
    return (result ^ CRC32_XOROT);
}  // end MgenMsg::ComputeCRC()

const UINT32 MgenMsg::CRC32_XINIT = 0xFFFFFFFFL; // initial value
const UINT32 MgenMsg::CRC32_XOROT = 0xFFFFFFFFL; // final xor value 

/*****************************************************************/
/*                                                               */
/* CRC LOOKUP TABLE                                              */
/* ================                                              */
/* The following CRC lookup table was generated automagically    */
/* by the Rocksoft^tm Model CRC Algorithm Table Generation       */
/* Program V1.0 using the following model parameters:            */
/*                                                               */
/*    Width   : 4 bytes.                                         */
/*    Poly    : 0x04C11DB7L                                      */
/*    Reverse : TRUE.                                            */
/*                                                               */
/* For more information on the Rocksoft^tm Model CRC Algorithm,  */
/* see the document titled "A Painless Guide to CRC Error        */
/* Detection Algorithms" by Ross Williams                        */
/* (ross@guest.adelaide.edu.au.). This document is likely to be  */
/* in the FTP archive "ftp.adelaide.edu.au/pub/rocksoft".        */
/*                                                               */
/*****************************************************************/

const UINT32 MgenMsg::CRC32_TABLE[256] =
{
 0x00000000L, 0x77073096L, 0xEE0E612CL, 0x990951BAL,
 0x076DC419L, 0x706AF48FL, 0xE963A535L, 0x9E6495A3L,
 0x0EDB8832L, 0x79DCB8A4L, 0xE0D5E91EL, 0x97D2D988L,
 0x09B64C2BL, 0x7EB17CBDL, 0xE7B82D07L, 0x90BF1D91L,
 0x1DB71064L, 0x6AB020F2L, 0xF3B97148L, 0x84BE41DEL,
 0x1ADAD47DL, 0x6DDDE4EBL, 0xF4D4B551L, 0x83D385C7L,
 0x136C9856L, 0x646BA8C0L, 0xFD62F97AL, 0x8A65C9ECL,
 0x14015C4FL, 0x63066CD9L, 0xFA0F3D63L, 0x8D080DF5L,
 0x3B6E20C8L, 0x4C69105EL, 0xD56041E4L, 0xA2677172L,
 0x3C03E4D1L, 0x4B04D447L, 0xD20D85FDL, 0xA50AB56BL,
 0x35B5A8FAL, 0x42B2986CL, 0xDBBBC9D6L, 0xACBCF940L,
 0x32D86CE3L, 0x45DF5C75L, 0xDCD60DCFL, 0xABD13D59L,
 0x26D930ACL, 0x51DE003AL, 0xC8D75180L, 0xBFD06116L,
 0x21B4F4B5L, 0x56B3C423L, 0xCFBA9599L, 0xB8BDA50FL,
 0x2802B89EL, 0x5F058808L, 0xC60CD9B2L, 0xB10BE924L,
 0x2F6F7C87L, 0x58684C11L, 0xC1611DABL, 0xB6662D3DL,
 0x76DC4190L, 0x01DB7106L, 0x98D220BCL, 0xEFD5102AL,
 0x71B18589L, 0x06B6B51FL, 0x9FBFE4A5L, 0xE8B8D433L,
 0x7807C9A2L, 0x0F00F934L, 0x9609A88EL, 0xE10E9818L,
 0x7F6A0DBBL, 0x086D3D2DL, 0x91646C97L, 0xE6635C01L,
 0x6B6B51F4L, 0x1C6C6162L, 0x856530D8L, 0xF262004EL,
 0x6C0695EDL, 0x1B01A57BL, 0x8208F4C1L, 0xF50FC457L,
 0x65B0D9C6L, 0x12B7E950L, 0x8BBEB8EAL, 0xFCB9887CL,
 0x62DD1DDFL, 0x15DA2D49L, 0x8CD37CF3L, 0xFBD44C65L,
 0x4DB26158L, 0x3AB551CEL, 0xA3BC0074L, 0xD4BB30E2L,
 0x4ADFA541L, 0x3DD895D7L, 0xA4D1C46DL, 0xD3D6F4FBL,
 0x4369E96AL, 0x346ED9FCL, 0xAD678846L, 0xDA60B8D0L,
 0x44042D73L, 0x33031DE5L, 0xAA0A4C5FL, 0xDD0D7CC9L,
 0x5005713CL, 0x270241AAL, 0xBE0B1010L, 0xC90C2086L,
 0x5768B525L, 0x206F85B3L, 0xB966D409L, 0xCE61E49FL,
 0x5EDEF90EL, 0x29D9C998L, 0xB0D09822L, 0xC7D7A8B4L,
 0x59B33D17L, 0x2EB40D81L, 0xB7BD5C3BL, 0xC0BA6CADL,
 0xEDB88320L, 0x9ABFB3B6L, 0x03B6E20CL, 0x74B1D29AL,
 0xEAD54739L, 0x9DD277AFL, 0x04DB2615L, 0x73DC1683L,
 0xE3630B12L, 0x94643B84L, 0x0D6D6A3EL, 0x7A6A5AA8L,
 0xE40ECF0BL, 0x9309FF9DL, 0x0A00AE27L, 0x7D079EB1L,
 0xF00F9344L, 0x8708A3D2L, 0x1E01F268L, 0x6906C2FEL,
 0xF762575DL, 0x806567CBL, 0x196C3671L, 0x6E6B06E7L,
 0xFED41B76L, 0x89D32BE0L, 0x10DA7A5AL, 0x67DD4ACCL,
 0xF9B9DF6FL, 0x8EBEEFF9L, 0x17B7BE43L, 0x60B08ED5L,
 0xD6D6A3E8L, 0xA1D1937EL, 0x38D8C2C4L, 0x4FDFF252L,
 0xD1BB67F1L, 0xA6BC5767L, 0x3FB506DDL, 0x48B2364BL,
 0xD80D2BDAL, 0xAF0A1B4CL, 0x36034AF6L, 0x41047A60L,
 0xDF60EFC3L, 0xA867DF55L, 0x316E8EEFL, 0x4669BE79L,
 0xCB61B38CL, 0xBC66831AL, 0x256FD2A0L, 0x5268E236L,
 0xCC0C7795L, 0xBB0B4703L, 0x220216B9L, 0x5505262FL,
 0xC5BA3BBEL, 0xB2BD0B28L, 0x2BB45A92L, 0x5CB36A04L,
 0xC2D7FFA7L, 0xB5D0CF31L, 0x2CD99E8BL, 0x5BDEAE1DL,
 0x9B64C2B0L, 0xEC63F226L, 0x756AA39CL, 0x026D930AL,
 0x9C0906A9L, 0xEB0E363FL, 0x72076785L, 0x05005713L,
 0x95BF4A82L, 0xE2B87A14L, 0x7BB12BAEL, 0x0CB61B38L,
 0x92D28E9BL, 0xE5D5BE0DL, 0x7CDCEFB7L, 0x0BDBDF21L,
 0x86D3D2D4L, 0xF1D4E242L, 0x68DDB3F8L, 0x1FDA836EL,
 0x81BE16CDL, 0xF6B9265BL, 0x6FB077E1L, 0x18B74777L,
 0x88085AE6L, 0xFF0F6A70L, 0x66063BCAL, 0x11010B5CL,
 0x8F659EFFL, 0xF862AE69L, 0x616BFFD3L, 0x166CCF45L,
 0xA00AE278L, 0xD70DD2EEL, 0x4E048354L, 0x3903B3C2L,
 0xA7672661L, 0xD06016F7L, 0x4969474DL, 0x3E6E77DBL,
 0xAED16A4AL, 0xD9D65ADCL, 0x40DF0B66L, 0x37D83BF0L,
 0xA9BCAE53L, 0xDEBB9EC5L, 0x47B2CF7FL, 0x30B5FFE9L,
 0xBDBDF21CL, 0xCABAC28AL, 0x53B39330L, 0x24B4A3A6L,
 0xBAD03605L, 0xCDD70693L, 0x54DE5729L, 0x23D967BFL,
 0xB3667A2EL, 0xC4614AB8L, 0x5D681B02L, 0x2A6F2B94L,
 0xB40BBE37L, 0xC30C8EA1L, 0x5A05DF1BL, 0x2D02EF8DL
};  // end MgenMsg::CRC32_TABLE



bool MgenMsg::LogRecvError(FILE*                    logFile,
                           bool                     logBinary,
                           bool                     localTime,
                           bool                     flush,
                           const struct timeval&    theTime)
{
    if (logBinary)
    {
        char header[128];
        unsigned int index = 0;
        // set "eventType"
        header[index++] = (char)RERR_EVENT;
        // zero "reserved" field
        header[index++] = '\0';
        // set "recordLength" (Note we don't save payload for SEND events)
        UINT16 recordLength = 12 + src_addr.GetLength() + sizeof(INT32);
        UINT16 temp16 = htons(recordLength);
        memcpy(header+index, &temp16, sizeof(INT16));
        index += sizeof(INT16);
        // set "eventTime" fields
        UINT32 temp32 = htonl(theTime.tv_sec);
        memcpy(header+index, &temp32, sizeof(INT32));
        index += sizeof(INT32);
        temp32 = htonl(theTime.tv_usec);
        memcpy(header+index, &temp32, sizeof(INT32));
        index += sizeof(INT32);
        // set "srcPort"
        temp16 = htons(src_addr.GetPort());
        memcpy(header+index, &temp16, sizeof(INT16));
        index += sizeof(INT16);
        // set "src_addrType"
        switch(src_addr.GetType())
        {
            case ProtoAddress::IPv4:
                header[index++] = IPv4;
                break;
            case ProtoAddress::IPv6:
                header[index++] = IPv6;
                break;
            default:
                DMSG(0, "MgenMsg::LogRecvError() unknown src_addr type\n");
                header[index++] = INVALID_ADDRESS;
                break;
        }
        // set "src_addrLen"
        unsigned int addrLen = src_addr.GetLength();
        header[index++] = (char)addrLen;
        // set "src_addr"
        memcpy(header+index, src_addr.GetRawHostAddress(), addrLen);
        index += addrLen;
        
        // set "errorCode"
        INT32 errorCode = (INT32)msg_error;
        errorCode = ntohl(errorCode);
        memcpy(header+index, &errorCode, sizeof(INT32));
        index += sizeof(INT32);
        
        // write record
  
	if (fwrite(header, 1, index, logFile) < index)
        {
            DMSG(0, "MgenMsg::LogRecvError() fwrite() error: %s\n", GetErrorString());
            return false;   
        }
    }
    else
    {
        const char* errorString = "";
        switch (msg_error)
        {
            case ERROR_NONE:
                errorString = "none";
                break;
            case ERROR_VERSION:
                errorString = "version";
                break;
            case ERROR_CHECKSUM:
                errorString = "checksum";
                break;
            case ERROR_LENGTH:
                errorString = "length";
                break;
            case ERROR_DSTADDR:
                errorString = "dstAddr";
                break;
        }
        Mgen::LogTimestamp(logFile, theTime, localTime);
        Mgen::Log(logFile, "RERR type>%s src>%s/%hu\n", errorString, 
                           src_addr.GetHostString(), src_addr.GetPort());
    }
    if (flush)  fflush(logFile);
    return true;
}  // end MgenMsg::LogRecvError()

// ljt why not set addr in msg? and time?
bool MgenMsg::LogTcpConnectionEvent(FILE*                     logFile,
                                    bool                      logBinary,
                                    bool                      localTime,
                                    bool                      flush,
                                    LogEventType              eventType,
                                    bool                      isClient,
                                    const struct timeval&     theTime)
{

    SetProtocol(TCP);

    ProtoAddress addr = GetDstAddr(); // ljt clean this up
    
    if (logBinary)
    {
        char header[128];
        unsigned int index = 0;
        // set "eventType"
        header[index++] = (char)eventType;
        // put protocol in the "reserved" field
        header[index++] = (char)protocol;
        UINT16 recordLength = 0;
        
        // header + addr + dstPort + flowid?
        recordLength = 12 + addr.GetLength() + 2 + sizeof(INT32);
        if (host_addr.IsValid()) 
        {
            recordLength += host_addr.GetLength() + 4;
        }
        UINT16 temp16 = htons(recordLength);
        memcpy(header+index, &temp16, sizeof(INT16));
        index += sizeof(INT16);
        
        // set "eventTime" fields
        UINT32 temp32 = htonl(theTime.tv_sec);
        memcpy(header+index, &temp32, sizeof(INT32));
        index += sizeof(INT32);
        
        temp32 = htonl(theTime.tv_usec);
        memcpy(header+index, &temp32, sizeof(INT32));
        index += sizeof(INT32);
        
        // set "port"
        temp16 = htons(addr.GetPort());
        memcpy(header+index, &temp16, sizeof(INT16));
        index += sizeof(INT16);
        
        // set "addrType"
        switch(addr.GetType())
        {
        case ProtoAddress::IPv4:
          header[index++] = IPv4;
          break;
        case ProtoAddress::IPv6:
          header[index++] = IPv6;
          break;
        default:
          DMSG(0, "MgenMsg::LogTcpConnectionEvent() unknown srcAddr type\n");
          header[index++] = INVALID_ADDRESS;
          break;
        }
        // set "addrLen"
        unsigned int addrLen = addr.GetLength();
        header[index++] = (char)addrLen;
        // set "addr"
        memcpy(header+index, addr.GetRawHostAddress(), addrLen);
        index += addrLen;
        // set "dstPort"
        temp16 = htons(src_addr.GetPort());
        memcpy(header+index,&temp16,sizeof(INT16));
        index += sizeof(INT16);
        
        // set "flowId"
        // ConvertBinaryLog uses flowID to determine whether 
        // it is a client or server event in some cases
        INT32 flowID = (INT32)flow_id; 
        flowID = htonl(flowID);
        memcpy(header+index,&flowID,sizeof(INT32));
        index += sizeof(INT32);
        
        if (host_addr.IsValid())
        {
            //set "hostAddr"
            temp16 = htons(host_addr.GetPort());
            memcpy(header+index,&temp16,sizeof(INT16));
            index += sizeof(INT16);
            
            //set "hostAddrType"
            switch(host_addr.GetType())
            {
            case ProtoAddress::IPv4:
              header[index++] = IPv4;
              break;
            case ProtoAddress::IPv6:
              header[index++] = IPv6;
              break;
            default:
              DMSG(0, "MgenMsg::LogTcpConnectionEvent() unknown hostAddr type\n");
              header[index++] = INVALID_ADDRESS;
              break;
            }
            
            // set "hostAddrLen"
            addrLen = host_addr.GetLength();
            header[index++] = (char)addrLen;
            // set "host_addr"
            memcpy(header+index,host_addr.GetRawHostAddress(),addrLen);
            index += addrLen;	   	  
            
        }
        // write record
        if (fwrite(header,1,index,logFile) < index)
        {
            DMSG(0,"MgenMsg::LogTcpConnection() fwrite() error: %s\n",GetErrorString());
            return false;
        }            
        
    }
    else 
    {
        Mgen::LogTimestamp(logFile, theTime, localTime);
        switch (eventType)
        {
            case ACCEPT_EVENT:
            {
                Mgen::Log(logFile,"ACCEPT src>%s/%hu dstPort>%hu", 
                        addr.GetHostString(),addr.GetPort(),src_addr.GetPort());
                break;
            }
            case ON_EVENT:
            {
                Mgen::Log(logFile,"ON flow>%lu srcPort>%hu dst>%s/%hu ",
                        flow_id,src_addr.GetPort(),addr.GetHostString(),addr.GetPort());
                break;
            }
            case CONNECT_EVENT:
            {
                Mgen::Log(logFile,"CONNECT flow>%lu srcPort>%hu dst>%s/%hu ",
                        flow_id,src_addr.GetPort(),addr.GetHostString(),addr.GetPort());
                break;
            }
            case DISCONNECT_EVENT:
            {
                if (isClient)
                    Mgen::Log(logFile,"DISCONNECT flow>%lu srcPort>%hu dst>%s/%hu ",
                            flow_id,src_addr.GetPort(),addr.GetHostString(),addr.GetPort());
                else
                    Mgen::Log(logFile,"DISCONNECT src>%s/%hu dstPort>%hu ",
                            addr.GetHostString(),addr.GetPort(),src_addr.GetPort());
                break;
            }
            case RECONNECT_EVENT:
            {
                if (isClient)
                    Mgen::Log(logFile,"RECONNECT flow>%lu srcPort>%hu dst>%s/%hu ",
                            flow_id,src_addr.GetPort(),addr.GetHostString(),addr.GetPort());
                else
                    Mgen::Log(logFile,"RECONNECT src>%s/%hu dstPort>%hu ",
                            addr.GetHostString(),addr.GetPort(),src_addr.GetPort());
                break;
                
            }
            case SHUTDOWN_EVENT:
            {
                if (isClient)
                    Mgen::Log(logFile,"SHUTDOWN flow>%lu srcPort>%hu dst>%s/%hu ",
                            flow_id,src_addr.GetPort(),addr.GetHostString(),addr.GetPort());
                else
                    Mgen::Log(logFile,"SHUTDOWN src>%s/%hu dstPort>%hu",
                            addr.GetHostString(),addr.GetPort(),src_addr.GetPort());
                break;
            }
            case OFF_EVENT:
            {
                if (isClient)
                    Mgen::Log(logFile,"OFF flow>%lu srcPort>%u dst>%s/%hu ",
                            flow_id,src_addr.GetPort(),addr.GetHostString(),addr.GetPort());
                else
                    Mgen::Log(logFile,"OFF src>%s/%hu dstPort>%hu ",
                            addr.GetHostString(),addr.GetPort(),src_addr.GetPort());
                break;
            }
            default:
            {
                DMSG(0,"Mgen::LogTcpConnectionEvent() error: invalid event type\n");
                ASSERT(0);
                break;
            }
        } // end switch (eventType)
        if (host_addr.IsValid())
        {
            Mgen::Log(logFile, " host>%s/%hu\n", host_addr.GetHostString(), 
                      host_addr.GetPort());      
        }
        else
        {
            Mgen::Log(logFile, "\n");
        }
    }
    
    if (flush) fflush(logFile);
    return true;
    
}  // end MgenMsg::LogTcpConnectionEvent()

bool MgenMsg::LogRecvEvent(FILE*                    logFile,
                           bool                     logBinary, 
                           bool                     localTime,
                           bool                     logRx,
			               bool                     logData,
			               bool                     logGpsData,
                           UINT32*                  alignedMsgBuffer,
                           bool                     flush,
                           int                      ttl,  // will be >= 0 when known
                           const struct timeval&    theTime)
{	      

    if (logBinary)
    {        
        char header[128];
        unsigned int index = 0;
        // set "eventType"
        header[index++] = (char)RECV_EVENT;
        // put protocol in the "reserved" field
        header[index++] = (char)protocol;
        // set "recordLength" (Note we only save payload for RECV events)
        UINT16 recordLength = 0;
        recordLength = 12 + src_addr.GetLength() + packet_header_len + payload_len;      
        UINT16 temp16 = htons(recordLength);
        memcpy(header+index, &temp16, sizeof(INT16));
        index += sizeof(INT16);
        // set "eventTime" fields
        UINT32 temp32 = htonl(theTime.tv_sec);
        memcpy(header+index, &temp32, sizeof(INT32));
        index += sizeof(INT32);        
        temp32 = htonl(theTime.tv_usec);
        memcpy(header+index, &temp32, sizeof(INT32));
        index += sizeof(INT32);
        
        // set "srcPort"
        temp16 = htons(src_addr.GetPort());
        memcpy(header+index, &temp16, sizeof(INT16));
        index += sizeof(INT16);
        
        // set "src_addrType"
        switch(src_addr.GetType())
        {
            case ProtoAddress::IPv4:
              header[index++] = IPv4;
              break;
            case ProtoAddress::IPv6:
              header[index++] = IPv6;
              break;
            default:
              DMSG(0, "MgenMsg::LogRecvEvent() unknown src_addr type\n");
              header[index++] = INVALID_ADDRESS;
              break;
        }
        
        // set "src_addrLen"
        unsigned int addrLen = src_addr.GetLength();
        header[index++] = (char)addrLen;
        
        // set "src_addr"
        memcpy(header+index, src_addr.GetRawHostAddress(), addrLen);
        index += addrLen;
        
        // write record header
        if (fwrite(header, 1, index, logFile) < index)
        {
            DMSG(0, "MgenMsg::LogRecvEvent() fwrite() error: %s\n", GetErrorString());
            return false;   
        }
        
        // write message content as remainder of record
        UINT16 msgLength = recordLength - index + 4;
        
        char* msgBuffer = (char*)alignedMsgBuffer;
        // Clear CHECKSUM flag for binary logging
        msgBuffer[FLAGS_OFFSET] &= ~CHECKSUM;
        
        // Set CHECKSUM_ERROR flag
        if (FlagIsSet(MgenMsg::CHECKSUM_ERROR)) 
      	{
            msgBuffer[FLAGS_OFFSET] |= CHECKSUM_ERROR;
      	}
        
        if (fwrite(msgBuffer, 1, msgLength, logFile) < msgLength)
        {
            DMSG(0, "MgenMsg::LogRecvEvent() fwrite() error: %s\n", GetErrorString());
            return false;
        }
    }
    else
    {
        if (logRx)
        {
            Mgen::LogTimestamp(logFile, theTime, localTime);
            Mgen::Log(logFile,"RECV proto>%s flow>%lu seq>%lu src>%s/%hu ",
                      MgenEvent::GetStringFromProtocol(protocol),
                      flow_id, seq_num, src_addr.GetHostString(), 
                      src_addr.GetPort());
            Mgen::Log(logFile, "dst>%s/%hu sent>",  dst_addr.GetHostString(), dst_addr.GetPort());
            Mgen::LogTimestamp(logFile, tx_time, localTime);
            Mgen::Log(logFile,"size>%u ", msg_len);
            // Here we are looking at the message's host_addr
            if (host_addr.IsValid())
            {
                Mgen::Log(logFile, "host>%s/%hu ", host_addr.GetHostString(), host_addr.GetPort());      
            }
            
            if (ttl >= 0)
                Mgen::Log(logFile, "ttl>%d ", ttl);

            // (TBD) only output GPS info if INVALID_GPS != gps_status
            const char* statusString  = NULL;
            switch (gps_status)
            {
                case INVALID_GPS:
                  statusString = "INVALID";
                  break;
                case STALE:
                  statusString = "STALE";
                  break;
                case CURRENT:
                  statusString = "CURRENT";
                  break; 
                default:
                  DMSG(0, "MgenMsg::LogRecvEvent() invalid GPS status\n");
                  Mgen::Log(logFile, "\n");
                  return false;
            } // end switch (gps_status)
	        if (logGpsData)
	            Mgen::Log(logFile, "gps>%s,%f,%f,%ld ", statusString,
		                            latitude, longitude, altitude);

            if (payload_len && logData)
            {
                if (USER_DATA == payload_type)
                {
                    Mgen::Log(logFile, "data>%hu:", payload_len);
                    char* payloadString = MgenPayload::GetPayloadString((const char*)payload_data, payload_len);    
                    Mgen::Log(logFile, "%s ", payloadString);    
                    delete[] payloadString;
                }
            } 

            if (FlagIsSet(MgenMsg::CONTINUES)) 
            {
                Mgen::Log(logFile,"flags>0x%02x ",(flags & MgenMsg::CONTINUES));
            }
            if (FlagIsSet(MgenMsg::END_OF_MSG))
            {
                Mgen::Log(logFile,"flags>0x%02x ",(flags & MgenMsg::END_OF_MSG));
            }		  
            if (FlagIsSet(MgenMsg::CHECKSUM_ERROR)) // We had a checksum error
            {
                SetFlag(MgenMsg::CHECKSUM_ERROR);
                Mgen::Log(logFile,"flags>0x%02x ",(flags & MgenMsg::CHECKSUM_ERROR));
            }
            Mgen::Log(logFile, "\n");
        }  // end if log_rx
        
        if ((MGEN_DATA == payload_type) && (payload_len > 0))
        {
            // For now, look for an analytic "REPORT" ... we will generalize the "type" field more later
            unsigned int bufferLen = payload_len;
            UINT32* bufferPtr = payload_data;
            while (bufferLen > 0)
            {
                if ((UINT8)MgenDataItem::GetItemType(bufferPtr) > 0x0f)
                {
                    MgenAnalytic::Report report;
                    if (report.InitFromBuffer(bufferPtr, bufferLen))
                    {
                        report.Log(logFile, tx_time, theTime, localTime, &src_addr); 
                        UINT8 reportLen = report.GetLength();
                        if (0 == reportLen) break; // truncated report
                        bufferLen -= reportLen;
                        bufferPtr += (reportLen/sizeof(UINT32));
                    }
                    else
                    {
                        PLOG(PL_ERROR, "MgenMsg::LogRecvEvent() warning: received invalid REPORT payload\n");
                        break;
                    }
                }
                else
                {
                    MgenDataItem item(bufferPtr, bufferLen);
                    UINT16 itemLen = item.GetLength();
                    if (0 == itemLen) break; // truncated item
                    bufferLen -= itemLen;
                    bufferPtr += itemLen / sizeof(UINT32);
                }
            }
        }
    }  // end if/else logBinary

    if (flush) {fflush(logFile);}
    
    return true;
}  // end MgenMsg::LogRecvEvent()

bool MgenMsg::LogSendEvent(FILE*    logFile, 
                           bool     logBinary, 
                           bool     localTime,
                           UINT32*  alignedMsgBuffer,
                           bool     flush,
                           const struct timeval& theTime)
{
    if (logBinary)
    {
        char header[128];
        unsigned int index = 0;
        // set "eventType"
        header[index++] = (char)SEND_EVENT;
        // put protocol in the "reserved" field
        header[index++] = (char)protocol;
        // set "recordLength" (Note we don't save payload for SEND events)
        UINT16 recordLength = 0;
        recordLength = 12 + dst_addr.GetLength() + packet_header_len;
        if (host_addr.IsValid()) 
        {
            recordLength += host_addr.GetLength() + 4;
        }	    
        
        if (protocol == TCP)
        {
            recordLength += sizeof(INT32);
        }
        UINT16 temp16 = htons(recordLength);
        memcpy(header+index, &temp16, sizeof(INT16));
        index += sizeof(INT16);
        
        // the tx_time in the txBuffer is used as the
	// SEND message event time.  For TCP messages
	// this needs to be the time of the first
	// TCP section sent.

        // Write mgen_msg_size since we are relying
        // on mgen_len for segment size in recv
        // processing...
        if (protocol == TCP)
        {
            UINT32 temp32 = htonl(mgen_msg_len);
            memcpy(header+index,&temp32,sizeof(UINT32));
            index += sizeof(INT32);
        }
        
        // write record header
        if (fwrite(header, 1, index, logFile) < index)
        {
            DMSG(0, "MgenMsg::LogSendEvent() fwrite() error: %s\n", GetErrorString());
            return false;   
        }
        
        // write message content as remainder of record
        UINT16 msgLength = recordLength - index + 4;
        
        // Clear CHECKSUM flag for binary logging
        char* msgBuffer = (char*)alignedMsgBuffer;
        msgBuffer[FLAGS_OFFSET] &= ~CHECKSUM;
        if (fwrite(msgBuffer, 1, msgLength, logFile) < msgLength)
        {
            DMSG(0, "MgenMsg::LogSendEvent() fwrite() error: %s\n", GetErrorString());
            return false;
        }
        
    }
    else
    {
        Mgen::LogTimestamp(logFile, theTime, localTime); 
        Mgen::Log(logFile,"SEND proto>%s flow>%lu seq>%lu srcPort>%hu dst>%s/%hu",
                  MgenEvent::GetStringFromProtocol(protocol),
                  flow_id, 
                  seq_num, 	// jm:  seq_num uninitialized here - at least can be...
                  src_addr.GetPort(),
                  dst_addr.GetHostString(), dst_addr.GetPort());
        if (protocol == TCP)
        {
            Mgen::Log(logFile," size>%lu ",mgen_msg_len);
        }
        else
        {
            Mgen::Log(logFile," size>%u ",msg_len);
        }
        
        if (host_addr.IsValid())
        {
            Mgen::Log(logFile, "host>%s/%hu\n", host_addr.GetHostString(), 
                      host_addr.GetPort());      
        }
        else
        {
            Mgen::Log(logFile, "\n");
        }
    }
    if (flush) fflush(logFile);
    return true;
}  // end MgenMsg::LogSendEvent()

void MgenMsg::LogDrecEvent(LogEventType eventType, const DrecEvent *event, UINT16 portNumber,Mgen& mgen)
{
    FILE* logFile;
    logFile = mgen.GetLogFile();
    // Get current system time
    if (mgen.GetOffsetPending()) return;  // Don't log "offset" pre-processed events
    if (NULL == logFile) return;
    struct timeval eventTime;
    ProtoSystemTime(eventTime);
    bool localTime = mgen.GetLocalTime();
    if (mgen.GetLogBinary())
    {
        char buffer[128];
        unsigned int index = 0;
        
        // set "type" field
        buffer[index++] = (char)eventType;
        
        // zero "reserved" field
        buffer[index++] = 0;
        
        // skip "recordLength" field for moment
        index += 2;
        
        // Fill in "eventTime" field
        unsigned int temp32 = htonl(eventTime.tv_sec);
        memcpy(buffer+index, &temp32, sizeof(INT32));
        index += sizeof(INT32);
        temp32 = htonl(eventTime.tv_usec);
        memcpy(buffer+index, &temp32, sizeof(INT32));
        index += sizeof(INT32);
        
        switch(eventType)
        {
            case LISTEN_EVENT:
            case IGNORE_EVENT:
            {
                // set "protocol" field
                buffer[index++] = (char)event->GetProtocol();
                // zero second "reserved" field
                buffer[index++] = 0;
                // set "portNumber" field
                UINT16 temp16 = htons(portNumber);
                memcpy(buffer+index, &temp16, sizeof(INT16));
                index += sizeof(INT16);
                break;
            } 
            case JOIN_EVENT:
            case LEAVE_EVENT:
            {   
                // set "groupPort" field
                UINT16 temp16 = portNumber;
                memcpy(buffer+index, &temp16, sizeof(INT16));
                index += sizeof(INT16);
                const ProtoAddress& addr = event->GetGroupAddress();
                // set "groupAddrType" field
                switch (addr.GetType())
                {
                    case ProtoAddress::IPv4:
                        buffer[index++] = (char)MgenMsg::IPv4;   
                        break;
                    case ProtoAddress::IPv6:
                        buffer[index++] = (char)MgenMsg::IPv6;
                        break;
                    default:
                        DMSG(0, "Mgen::LogDrecEvent(JOIN/LEAVE) error: invalid address type\n");
                        return;
                }
                // set "groupAddrLen" field
                UINT8 len = (UINT8)addr.GetLength();
                buffer[index++] = len;
                // set "groupAddr" field
                memcpy(buffer+index, addr.GetRawHostAddress(), len);
                index += len;
                const char* interfaceName = event->GetInterface();
                len = interfaceName ? strlen(interfaceName) : 0;
                // set "ifaceNameLen" field
                buffer[index++] = len;
                // set "ifaceName" field
                memcpy(buffer+index, interfaceName, len);
                index += len;
                break; 
            }
            default:
                DMSG(0, "Mgen::LogDrecEvent() error: invalid event type\n");
                ASSERT(0);
                break;
        }  // end switch(eventType)
        
        // Set the "recordLength" field
        const UINT16 RECORD_LENGTH_OFFSET = 2;
        UINT16 temp16 = htons(index - (RECORD_LENGTH_OFFSET+2));
        memcpy(buffer+RECORD_LENGTH_OFFSET, &temp16, sizeof(INT16));
        
        // Write record to log file
        if (fwrite(buffer, 1, index, logFile) < index)
            DMSG(0, "Mgen::LogDrecEvent() fwrite() error: %s\n", GetErrorString()); 
    }
    else
    {
        switch (eventType)
        {
            case LISTEN_EVENT:
               Mgen::LogTimestamp(logFile, eventTime, localTime);
               Mgen::Log(logFile, "LISTEN proto>%s port>%hu\n",
                                        MgenBaseEvent::GetStringFromProtocol(event->GetProtocol()),
                                        portNumber);
                break;
            case IGNORE_EVENT:
                Mgen::LogTimestamp(logFile, eventTime, localTime);
               Mgen::Log(logFile, "IGNORE proto>%s port>%hu\n",
                                        MgenBaseEvent::GetStringFromProtocol(event->GetProtocol()),
                                        portNumber);
                break;
            case JOIN_EVENT:
            {
                Mgen::LogTimestamp(logFile, eventTime, localTime);
                Mgen::Log(logFile, "JOIN group>%s", event->GetGroupAddress().GetHostString());
                // If source is valid, it means it is SSM, print source as well
                if ( event->GetSourceAddress().IsValid() )
                {
                     const char* source;

                     source = event->GetSourceAddress().GetHostString();
                     if (source)
                     {
                         Mgen::Log(logFile, " source>%s", source );
                     }
                }

                const char* iface = event->GetInterface();
                if (iface) Mgen::Log(logFile, " interface>%s", iface);
                if (portNumber)
                  Mgen::Log(logFile, " port>%hu\n", portNumber);
                else
                    Mgen::Log(logFile, "\n");
                break;
            }
            case LEAVE_EVENT:
            {
                Mgen::LogTimestamp(logFile, eventTime, localTime);
                Mgen::Log(logFile, "LEAVE group>%s",
                                        event->GetGroupAddress().GetHostString());
                // If source is valid, it means it is SSM, print source as well
                if ( event->GetSourceAddress().IsValid() )
                {
                    const char* source;

                    source = event->GetSourceAddress().GetHostString();

                    if (source)
                    {
                        Mgen::Log(logFile, " source>%s", source );
                    }
                }

                const char* iface = event->GetInterface();
                if (iface) Mgen::Log(logFile, " interface>%s", iface);
                if (portNumber)
                  Mgen::Log(logFile, " port>%hu\n", portNumber);
                else
                    Mgen::Log(logFile, "\n");
                break;
            }
            default:
                DMSG(0, "Mgen::LogDrecEvent() error: invalid event type\n");
                ASSERT(0);
                break;   
        }  // end switch(eventType)       
    }  // end if/else (log_binary)
    if (mgen.GetLogFlush()) fflush(logFile);

} // end MgenMsg::LogDrecEvent

bool MgenMsg::ConvertBinaryLog(const char* path, Mgen& mgen)
{
    FILE* logFile = mgen.GetLogFile();
    bool localTime = mgen.GetLocalTime();
    bool log_flush = mgen.GetLogFlush();
    bool log_rx = mgen.GetLogRx();
    bool log_data = mgen.GetLogData();
    bool log_gps_data = mgen.GetLogGpsData();

    if (NULL == logFile) return false;
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        DMSG(0, "Mgen::ConvertFile() fopen() Error: %s\n", GetErrorString());   
        return false;    
    }
    const unsigned int BINARY_RECORD_MAX = 1024;  // maximum record size
    UINT32 alignedBuffer[BINARY_RECORD_MAX/4];
    char* buffer = (char*)alignedBuffer;
    const char* eventName;
    // Read ASCII binary log header line
    // The first four characters should be "mgen"
    if (fread(buffer, 1, 4, file) < 4)
    {
        DMSG(0, "Mgen::ConvertBinaryLog() fread() error: %s\n", GetErrorString());
        fclose(file);
        return false;
    }
    if (strncmp("mgen", buffer, 4) != 0)
    {
        DMSG(0, "Mgen::ConvertBinaryLog() error: invalid mgen log file\n");
        fclose(file);
        return false;
    }
    
    // Read remainder of header line, including terminating NULL
    unsigned int index = 3;
    while ('\0' != buffer[index])
    {
        index++;
        if (fread(buffer+index, 1, 1, file) < 1)
        {
            DMSG(0, "Mgen::ConvertBinaryLog() fread() error: %s\n", GetErrorString());
            fclose(file);
            return false;
        }
    }
    
    // Confirm log file "version" and "type"
    char* ptr = strstr(buffer, "version=");
    if (ptr)
    {
        // Just look at major version number for moment
        int version;
        if (1 == sscanf(ptr, "version=%d", &version))
        {
            if (version != 4 && version != 5)
            {
                DMSG(0, "Mgen::ConvertBinaryLog() invalid log file version\n"); 
                fclose(file);
                return false;   
            }            
        }
        else
        {
            DMSG(0, "Mgen::ConvertBinaryLog() error finding log \"version\" value\n"); 
            fclose(file);
            return false;  
        }                
    }
    else
    {
        DMSG(0, "Mgen::ConvertBinaryLog() error finding log \"version\" label\n"); 
        fclose(file);
        return false;  
    }
    ptr = strstr(ptr, "type=");
    if (ptr)
    {
        char fileType[128];
        if (1 == sscanf(ptr, "type=%s", fileType))
        {
            if (strcmp(fileType, "binary_log"))
            {
                DMSG(0, "Mgen::ConvertBinaryLog() invalid log file type\n"); 
                fclose(file);
                return false; 
            }
        }
        else
        {
            DMSG(0, "Mgen::ConvertBinaryLog() error finding log \"type\" value\n"); 
            fclose(file);
            return false; 
        }
    }
    else
    {
        DMSG(0, "Mgen::ConvertBinaryLog() error finding log \"type\" label\n"); 
        fclose(file);
        return false;
    }    
    
    // Now loop, reading in records one by one, and output text format
    while (1)
    {
        // 1) Read record header (4 bytes)
        char header[4];
        if (fread(header, 1, 4, file) < 4)
        {
            if (feof(file))
            {
                break;
            }
            else
            {
                DMSG(0, "Mgen::ConvertBinaryLog() fread() error: %s\n", GetErrorString());
                fclose(file);
                return false;
            }   
        }
        LogEventType eventType = (LogEventType)header[0];
        Protocol theProtocol = (Protocol)header[1];
        UINT16 recordLength;
        memcpy(&recordLength, header+2, sizeof(INT16));
        recordLength = ntohs(recordLength);
        if (recordLength > BINARY_RECORD_MAX)
        {
            DMSG(0, "Mgen::ConvertBinaryLog() record len:%hu exceeds maximum length\n", recordLength);
            fclose(file);
            return false;  
        }
        // 2) Read the entire record
        if (fread(buffer, 1, recordLength, file) < recordLength)
        {
            DMSG(0, "Mgen::ConvertBinaryLog() fread() error: %s\n", GetErrorString());
            fclose(file);
            return false;
        }
        // 3) Convert the record to text
        unsigned int index = 0;
        switch (eventType)
        {
            case RECV_EVENT:
              {
                  // get "eventTime"
                  struct timeval eventTime;
                  UINT32 temp32;
                  memcpy(&temp32, buffer+index, sizeof(INT32));
                  eventTime.tv_sec = ntohl(temp32);
                  index += sizeof(INT32);
                  memcpy(&temp32, buffer+index, sizeof(INT32));
                  eventTime.tv_usec = ntohl(temp32);
                  index += sizeof(INT32);
                  // get "srcPort"
                  UINT16 temp16;
                  memcpy(&temp16, buffer+index, sizeof(INT16));
                  UINT16 srcPort = ntohs(temp16);
                  index += sizeof(INT16);
                  // get "srcAddrType"
                  ProtoAddress::Type addrType;
                  switch (buffer[index++])
                  {
                    case MgenMsg::IPv4:
                        addrType = ProtoAddress::IPv4;
                        break;
                    case MgenMsg::IPv6:
                        addrType = ProtoAddress::IPv6;
                        break;
                    default:
                        DMSG(0, "Mgen::ConvertBinaryLog() unknown source address type:%d\n",
                                buffer[index-1]);
                        fclose(file);
                        return false;   
                  }
                  // get "srcAddrLen"
                  unsigned int addrLen = (unsigned int)buffer[index++];
                  ProtoAddress srcAddr;
                  // get "srcAddr"
                  srcAddr.SetRawHostAddress(addrType, buffer+index, addrLen);
                  index += addrLen;
                  srcAddr.SetPort(srcPort);

                  // The remainder of the record corresponds to the message content
                  MgenMsg msg;
                  msg.SetProtocol(theProtocol);
                  msg.SetSrcAddr(srcAddr);
                  msg.SetTxTime(eventTime);
                  ASSERT(0 == index%4)
                  msg.Unpack(alignedBuffer+index/4, recordLength - index, false, log_data);
                  msg.LogRecvEvent(logFile, false, localTime, log_rx, log_data, log_gps_data, NULL, -1, log_flush,eventTime);
                  break;
              }
            case SEND_EVENT:
              {
                  MgenMsg msg;
                  // get tcp mgen_msg_len
                  if (theProtocol == TCP)
                  {
                      UINT32 temp32;
                      memcpy(&temp32,buffer+index,sizeof(INT32));
                      msg.SetMgenMsgLen(ntohl(temp32));
                      index += sizeof(UINT32);

                  }
                  msg.SetProtocol(theProtocol);
                  ASSERT(0 == index%4)
                  msg.Unpack(alignedBuffer+index/4, recordLength, false, log_data);
                  msg.LogSendEvent(logFile,false, localTime, NULL, log_flush, msg.tx_time);
                  break;
              }
            case LISTEN_EVENT:
            case IGNORE_EVENT:
              {
                  eventName = (LISTEN_EVENT == eventType) ? "LISTEN" : "IGNORE";
                  unsigned int index = 0;
                  // get "eventTime"
                  struct timeval eventTime;
                  UINT32 temp32;
                  memcpy(&temp32, buffer+index, sizeof(INT32));
                  eventTime.tv_sec = ntohl(temp32);
                  index += sizeof(INT32);
                  memcpy(&temp32, buffer+index, sizeof(INT32));
                  eventTime.tv_usec = ntohl(temp32);
                  index += sizeof(INT32);
                  // get "protocol"
                  const char* protoName = 
                    MgenBaseEvent::GetStringFromProtocol((Protocol)buffer[index++]);
                  // skip "reserved" field
                  index++;
                  // get "portNumber"
                  UINT16 temp16;
                  memcpy(&temp16, buffer+index, sizeof(INT16));
                  UINT16 portNumber = ntohs(temp16);
                  // Output text log format
                  Mgen::LogTimestamp(logFile, eventTime, localTime);
                  Mgen::Log(logFile, "%s proto>%s port>%hu\n",
                            eventName, protoName, portNumber);
                  break;   
              }
            case JOIN_EVENT:
            case LEAVE_EVENT:
              {
                  unsigned int index = 0;
                  // get "eventTime"
                  struct timeval eventTime;
                  UINT32 temp32;
                  memcpy(&temp32, buffer+index, sizeof(INT32));
                  eventTime.tv_sec = ntohl(temp32);
                  index += sizeof(INT32);
                  memcpy(&temp32, buffer+index, sizeof(INT32));
                  eventTime.tv_usec = ntohl(temp32);
                  index += sizeof(INT32);
                  // get "groupPort"
                  UINT16 temp16;
                  memcpy(&temp16, buffer+index, sizeof(INT16));
                  UINT16 groupPort = ntohs(temp16);
                  index += sizeof(INT16);
                  // get "groupAddrType"
                  ProtoAddress::Type addrType;
                  switch (buffer[index++])
                  {
                  case MgenMsg::IPv4:
                    addrType = ProtoAddress::IPv4;
                    break;
                  case MgenMsg::IPv6:
                    addrType = ProtoAddress::IPv6;
                    break;
                  default:
                    DMSG(0, "Mgen::ConvertBinaryLog() unknown source address type\n");
                    fclose(file);
                    return false;   
                  }
                  // get "groupAddrLen"
                  unsigned int addrLen = (unsigned int)buffer[index++];
                  ProtoAddress groupAddr;
                  // get "groupAddr"
                  groupAddr.SetRawHostAddress(addrType, buffer+index, addrLen);
                  index += addrLen;
                  // get "ifaceNameLen"
                  unsigned int ifaceNameLen = buffer[index++];
                  char ifaceName[128];
                  memcpy(ifaceName, buffer+index, ifaceNameLen);
                  ifaceName[ifaceNameLen] = '\0';
                  // Output text log format
                  eventName = (JOIN_EVENT == eventType) ? "JOIN" : "LEAVE";
                  Mgen::LogTimestamp(logFile, eventTime, localTime);
                  Mgen::Log(logFile, "%s group>%s", eventName, groupAddr.GetHostString());
                  if (ifaceNameLen) Mgen::Log(logFile, " interface>%s", ifaceName);
                  if (groupPort)
                    Mgen::Log(logFile, " port>%hu\n", groupPort);
                  else
                    Mgen::Log(logFile, "\n");
                  break;
              }
            case START_EVENT:
            case STOP_EVENT:
              {
                  eventName = (START_EVENT == eventType) ? "START" : "STOP";
                  unsigned int index = 0;
                  // get "eventTime"
                  struct timeval eventTime;
                  UINT32 temp32;
                  memcpy(&temp32, buffer+index, sizeof(INT32));
                  eventTime.tv_sec = ntohl(temp32);
                  index += sizeof(INT32);
                  memcpy(&temp32, buffer+index, sizeof(INT32));
                  eventTime.tv_usec = ntohl(temp32);
                  index += sizeof(INT32);
                  Mgen::LogTimestamp(logFile, eventTime, localTime);
                  Mgen::Log(logFile, "%s\n", eventName);
                  break;
              }
            case ON_EVENT:
            case ACCEPT_EVENT:
            case CONNECT_EVENT:
            case DISCONNECT_EVENT:
            case OFF_EVENT:
            case SHUTDOWN_EVENT:
	    case RECONNECT_EVENT:
              {
                  // get "eventTime"
                  struct timeval eventTime;
                  UINT32 temp32;
                  memcpy(&temp32, buffer+index, sizeof(INT32));
                  eventTime.tv_sec = ntohl(temp32);
                  index += sizeof(INT32);
                  memcpy(&temp32, buffer+index, sizeof(INT32));
                  eventTime.tv_usec = ntohl(temp32);
                  index += sizeof(INT32);

                  // get "srcPort"
                  UINT16 temp16;
                  memcpy(&temp16, buffer+index, sizeof(INT16));
                  UINT16 port = ntohs(temp16);
                  index += sizeof(INT16);

                  // get "srcAddrType"
                  ProtoAddress::Type addrType;
                  switch (buffer[index++])
                  {
                  case MgenMsg::IPv4:
                    addrType = ProtoAddress::IPv4;
                    break;
                  case MgenMsg::IPv6:
                    addrType = ProtoAddress::IPv6;
                    break;
                  default:
                    DMSG(0, "Mgen::ConvertBinaryLog() unknown source address type:%d\n",
                         buffer[index-1]);
                    fclose(file);
                    return false;   
                  }
                  // get "srcAddrLen"
                  unsigned int addrLen = (unsigned int)buffer[index++];
                  ProtoAddress addr;
                  // get "srcAddr"
                  addr.SetRawHostAddress(addrType, buffer+index, addrLen);
                  index += addrLen;
                  addr.SetPort(port);
                  // get "dstPort"
                  memcpy(&temp16,buffer+index,sizeof(INT16));
                  UINT16 dstPort = ntohs(temp16);
                  index += sizeof(INT16);              

                  // get "flow_id" (it might not exist - its' how we
                  // are differentiating between clients and servers
                  // for now)
                  UINT32 flow_id = 0;
                  memcpy(&temp32,buffer+index,sizeof(UINT32));
                  flow_id = ntohl(temp32);
                  index += sizeof(UINT32);	    

                  ProtoAddress hostAddr;
                  // get "hostPort"
                  if ((index+4) <= recordLength)
                  {

                      memcpy(&temp16, buffer+index, sizeof(INT16));
                      UINT16 hostPort = ntohs(temp16);
                      index += sizeof(INT16);
                      // get "hostAddrType"
                      switch (buffer[index++])
                      {
                      case MgenMsg::IPv4:
                        addrType = ProtoAddress::IPv4;
                        break;
                      case MgenMsg::IPv6:
                        addrType = ProtoAddress::IPv6;
                        break;
                      default:
                        addrType = ProtoAddress::INVALID;
                        break;
                      }
                      // get "hostAddrLen"
                      addrLen = (unsigned int)buffer[index++];

                      if (index+addrLen <= recordLength)
                      {
                          if (ProtoAddress::INVALID != addrType && addrLen)
                          {
                              // get "hostAddr"
                              hostAddr.SetRawHostAddress(addrType, buffer+index, addrLen);
                              index += addrLen;
                              hostAddr.SetPort(hostPort);
                          }
                      }
                  }
                  // Let's just keep it verbose and clear...
                  switch (eventType) 
                  {
                      case ON_EVENT:
                            Mgen::LogTimestamp(logFile, eventTime, localTime);
                            Mgen::Log(logFile, "ON flow>%lu srcPort>%hu dst>%s/%hu",
                                        flow_id,dstPort,addr.GetHostString(),addr.GetPort());
                            break;
                      case ACCEPT_EVENT:
                            Mgen::LogTimestamp(logFile, eventTime, localTime);
                            Mgen::Log(logFile, "ACCEPT src>%s/%hu dstPort>%hu",
                                        addr.GetHostString(),addr.GetPort(),dstPort);
                            break;
                      case CONNECT_EVENT:
                            Mgen::LogTimestamp(logFile, eventTime, localTime);
                            Mgen::Log(logFile, "CONNECT flow>%lu srcPort>%hu dst>%s/%hu",
                                        flow_id,dstPort,addr.GetHostString(),addr.GetPort());
                            break;
                      case DISCONNECT_EVENT:
                            Mgen::LogTimestamp(logFile, eventTime, localTime);
                            if (flow_id)
                                Mgen::Log(logFile, "DISCONNECT flow>%lu dst>%s/%hu srcPort>%hu",
                                            flow_id,addr.GetHostString(),addr.GetPort(),dstPort);
                            else
                                Mgen::Log(logFile, "DISCONNECT src>%s/%hu dstPort>%hu",
                                            addr.GetHostString(),addr.GetPort(), dstPort);
                            break;
			    
		      case RECONNECT_EVENT:
			// We will only have client side connect events but log
			// both in the event this changes.
			    Mgen::LogTimestamp(logFile, eventTime, localTime);
                            if (flow_id)
                                Mgen::Log(logFile, "RECONNECT flow>%lu dst>%s/%hu srcPort>%hu",
                                            flow_id,addr.GetHostString(),addr.GetPort(),dstPort);
                            else
                                Mgen::Log(logFile, "RECONNECT src>%s/%hu dstPort>%hu",
                                            addr.GetHostString(),addr.GetPort(), dstPort);
                            break;

                      case SHUTDOWN_EVENT:
                            Mgen::LogTimestamp(logFile, eventTime, localTime);
                            if (flow_id)
                                Mgen::Log(logFile, "SHUTDOWN flow>%lu dst>%s/%hu srcPort>%hu",
                                            flow_id,addr.GetHostString(),addr.GetPort(), dstPort);
                            else
                                Mgen::Log(logFile, "SHUTDOWN src>%s/%hu dstPort>%hu",
                                            addr.GetHostString(),addr.GetPort(), dstPort);
                            break;

                      case OFF_EVENT:
                            Mgen::LogTimestamp(logFile, eventTime, localTime);
                            if (flow_id)
                                Mgen::Log(logFile, "OFF flow>%lu srcPort>%hu dst>%s/%hu",
                                            flow_id,dstPort, addr.GetHostString(),addr.GetPort());
                            else
                                Mgen::Log(logFile, "OFF src>%s/%hu dstPort>%hu",
                                            addr.GetHostString(),addr.GetPort(),dstPort);
                            break;
                      default:
                            DMSG(0,"Mgen::ConvertBinaryLog Invalid event type.\n");
                  }
                  if (hostAddr.IsValid()) 		
                        Mgen::Log(logFile,"host>%s/%hu", hostAddr.GetHostString(),hostAddr.GetPort());	   	  
                  Mgen::Log(logFile,"\n");
                  break;
              }
            default:
              DMSG(0, "Mgen::ConvertBinaryLog() invalid event type\n");
              fclose(file);
              return false;
        }  // end switch(eventType)
    }  // end while(1)
    fclose(file);
    return true;
}  // end MgenMsg::ConvertBinaryLog()

