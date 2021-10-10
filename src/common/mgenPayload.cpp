#include "mgenPayload.h"
#include "protoDebug.h"
#include <string.h>
#include <ctype.h>

MgenPayload::MgenPayload() 
 : payload_len(0), buffer_len(0), payload_buffer(NULL)
{
}

MgenPayload::~MgenPayload() 
{
    if (NULL != payload_buffer)
    {
        delete[] payload_buffer;
        payload_buffer = NULL;
        buffer_len = 0;
    }
}

/**
 * Take a hex encoded string and translate it to binary
 */
bool MgenPayload::SetPayloadString(const char* inStr) 
{
	if (inStr == NULL) 
    {
        payload_len = 0;
        return true;
    }
	payload_len = (strlen(inStr) / 2) + (strlen(inStr) % 2);
    if (payload_len > buffer_len)
    {
	    // reallocate payload_buffer
	    if (NULL != payload_buffer) delete[] payload_buffer;
        unsigned words = payload_len / sizeof(UINT32);
        if (0 != (payload_len % 4)) words++;
        if (NULL == (payload_buffer = new UINT32[words]))
        {
            PLOG(PL_ERROR, "MgenPayload::SetPayloadString() new payload_buffer[] error: %s\n", GetErrorString());
            payload_len = buffer_len = 0;
            return false;
        }
    }   
	// every input character represents 4 bits.
    UINT16 numBytes = payload_len;
    const char* ptr1 = inStr;
    char* ptr2 = (char*)payload_buffer;
    for (unsigned int i = 0; i < numBytes; i++)
    {
        *ptr2 = fromHex(*ptr1++) << 4;
        *ptr2++ |= fromHex(*ptr1++);
    }
    return true;
}  // end MgenPayload::SetPayload()

char* MgenPayload::GetPayloadString(const char* payloadBytes, UINT16 payloadLen) 
{
    // Returns hex string representation of payload
    unsigned int textLen = payloadLen*2;
    char* text = new char[textLen + 1];
    if (NULL == text) return NULL;
    const char* bufPtr = payloadBytes;
    char* textPtr = text;
    for (unsigned int i = 0; i < payloadLen; i++)
    {
        *textPtr++ = toHex((*bufPtr >> 4) & 0x0f);
        *textPtr++ = toHex(*bufPtr & 0x0f);
        bufPtr++;
    }
    text[textLen] = '\0';
    return text;
}  // end MgenPayload::GetPayloadString()
    

/**
 * Copy a binary into the payload
 */
bool MgenPayload::SetPayloadBytes(char* data, UINT16 size) 
{
    if ((NULL == data) || (0 == size))
    {
        payload_len = 0;
        return true;
    }
    if (size > buffer_len)
    {
	    // clear the old payload
	    if (NULL != payload_buffer) delete[] payload_buffer;
        unsigned words = payload_len / sizeof(UINT32);
        if (0 != (payload_len % 4)) words++;
        if (NULL == (payload_buffer = new UINT32[words]))
        {
            PLOG(PL_ERROR, "MgenPayload::SetPayloadBytes() new payload_buffer[] error: %s\n", GetErrorString());
            payload_len = buffer_len = 0;
            return false;
        }
    }  
	memcpy(payload_buffer, data, size);
    payload_len = size;
    return true;
}  // end MgenPayload::SetPayloadBytes()

bool MgenPayload::Allocate(UINT16 size)
{
    if (size > buffer_len)
    {
        // clear the old payload
	    if (NULL != payload_buffer) delete[] payload_buffer;
        unsigned words = size/sizeof(UINT32);
        if (0 != (size % sizeof(UINT32))) words++;
        if (NULL == (payload_buffer = new UINT32[words]))
        {
            PLOG(PL_ERROR, "MgenPayload::Allocate() new payload_buffer[] error: %s\n", GetErrorString());
            payload_len = buffer_len = 0;
            return false;
        }
        buffer_len = size;
    }  
    return true;
}  // end MgenPayload::Allocate()


/**
 * Converts from hex to int
 */
char MgenPayload::fromHex(char inHex) 
{
    switch (toupper(inHex))
    {
        case '0':
            return 0;
        case '1':
            return 1;
        case '2':
            return 2;
        case '3':
            return 3;
        case '4':
            return 4;
        case '5':
            return 5;
        case '6':
            return 6;
        case '7':
            return 7;
        case '8':
            return 8;
        case '9':
            return 9;
        case 'A':
            return 10;
        case 'B':
            return 11;
        case 'C':
            return 12;
        case 'D':
            return 13;
        case 'E':
            return 14;
        case 'F':
            return 15;
        default:
            return 0;
    }
}  // end MgenPayload::fromHex()
 

/**
 * Converts from int to hex
 */
char MgenPayload::toHex(char inNum) 
{
    switch (inNum)
    {
        case 0:
            return '0';
        case 1:
            return '1';
        case 2:
            return '2';
        case 3:
            return '3';
        case 4:
            return '4';
        case 5:
            return '5';
        case 6:
            return '6';
        case 7:
            return '7';
        case 8:
            return '8';
        case 9:
            return '9';
        case 10:
            return 'A';
        case 11:
            return 'B';
        case 12:
            return 'C';
        case 13:
            return 'D';
        case 14:
            return 'E';
        case 15:
            return 'F';
        default:
            return '?';
    }
}  // end MgenPayload::toHex() 


MgenDataItem::MgenDataItem(UINT32*        bufferPtr, 
                           unsigned int   bufferBytes, 
                           bool           freeOnDestruct)
 : ProtoPkt(bufferPtr, bufferBytes, freeOnDestruct)
{
    InitFromBuffer();
}

MgenDataItem::~MgenDataItem()
{
}

bool MgenDataItem::InitFromBuffer(UINT32*         bufferPtr, 
                                  unsigned int    numBytes, 
                                  bool            freeOnDestruct)
{
    if (NULL != bufferPtr) 
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    else
        ProtoPkt::SetLength(0);
    if (GetBufferLength() >= OFFSET_LEN)
    {
        UINT8 minLength = GetItemLength();
        if (ProtoPkt::InitFromBuffer(minLength))
        {
            return true;
        }
        else
        {
            PLOG(PL_ERROR, "MgenDataItem::InitFromBuffer() error: invalid buffer size! (size:%d minLength:%d)\n",
                    GetBufferLength(), minLength);
            SetType(DATA_ITEM_INVALID);
        }
    }
    if (NULL != bufferPtr) DetachBuffer();
    return false;
}  // end MgenDataItem::InitFromBuffer()

bool MgenDataItem::InitIntoBuffer(Type          itemType,
                                  UINT32*       bufferPtr, 
                                  unsigned int  bufferBytes, 
                                  bool          freeOnDestruct)
{
    unsigned int minLength = OFFSET_LEN + 1;
    if (NULL != bufferPtr)
    {
        if (bufferBytes < minLength)
            return false;
        else
            AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
    }
    else if (GetBufferLength() < minLength) 
    {
        return false;
    }
    memset((char*)AccessBuffer(), 0, minLength);
    SetType(itemType);
    SetItemLength(minLength);  // will be updated as other fields are set
    SetLength(minLength);        // will be updated as other fields are set
    return true;
}  // end MgenDataItem::InitIntoBuffer()

bool MgenFlowCommand:: SetStatus(UINT32 flowId, Status status)
{
    // To maintain 32-bit alignment, length of command MUST satisfy
    // len = OFFSET_LEN + 2 + N*4 for N = 0, 1, 2, ...
    // where maxFlowId = 16 + N*32 
    UINT32 N = (2*flowId > 16) ? (2*flowId - 16 - 1)/32 + 1: 0;
    UINT32 lengthNeeded = OFFSET_BITS + 2 + N*4;
    if (lengthNeeded > GetBufferLength())
    {
        PLOG(PL_ERROR, "MgenFlowCommand:: SetStatus() error: insufficient buffer space for given flowId\n");
        return false;
    }
    else if (lengthNeeded > GetLength())
    {
        char* ptr = (char*)AccessBuffer();
        UINT32 oldLength = GetLength() - OFFSET_BITS;
        UINT32 oldOffsetHi = OFFSET_BITS + oldLength/2;
        UINT32 newLength = lengthNeeded - OFFSET_BITS;
        UINT32 newOffsetHi = OFFSET_BITS + newLength/2;
        memmove(ptr+newOffsetHi, ptr+oldOffsetHi, oldLength/2);
        // Zero bits in tail of old 'lo' and old 'hi'
        memset(ptr+oldOffsetHi, 0, newOffsetHi - oldOffsetHi);
        memset(ptr+newOffsetHi+oldLength/2, 0, newOffsetHi - oldOffsetHi);
        SetItemLength(lengthNeeded);
        SetLength(lengthNeeded);
    }
    SetType(DATA_ITEM_FLOW_CMD);
    flowId -= 1;  // to use as an index
    // First, set or clear 'lo' part
    char* mask = (char*)AccessBuffer(OFFSET_BITS);
    if (0 != (status & 0x01))
        mask[flowId >> 3] |=  (0x80 >> (flowId & 0x07));
    else
        mask[flowId >> 3] &= ~(0x80 >> (flowId & 0x07));
    // Then, set or clear 'hi' part
    mask += (GetLength() - OFFSET_BITS) / 2;
    if (0 != (status & 0x02))
        mask[flowId >> 3] |=  (0x80 >> (flowId & 0x07));
    else
        mask[flowId >> 3] &= ~(0x80 >> (flowId & 0x07));
    return true;
}  // end  MgenFlowCommand:: SetStatus()

UINT32 MgenFlowCommand:: GetMaxFlowId() const
{
   UINT32 len = GetLength();
   if (len > OFFSET_BITS)
       return (8 * (len - OFFSET_BITS) / 2);
   else
       return 0;
}  // end MgenFlowCommand:: GetMaxFlowId()

MgenFlowCommand::Status MgenFlowCommand:: GetStatus(UINT32 flowId) const
{
    // To maintain 32-bit alignment, length of command MUST satisfy
    // len = OFFSET_LEN + 2 + N*4 for N = 0, 1, 2, ...
    // where maxFlowId = 16 + N*32 
    UINT32 N = (2*flowId > 16) ? (2*flowId - 16 - 1)/32 + 1 : 0;
    UINT32 lengthNeeded = OFFSET_BITS + 2 + N*4;
    if (lengthNeeded > GetLength()) return FLOW_UNCHANGED;
    int status = 0;
    flowId -= 1;
    // First, test the 'lo' part
    const char* mask = (char*)GetBuffer(OFFSET_BITS);
    if (0 != (mask[(flowId >> 3)] & (0x80 >> (flowId & 0x07))))
        status = 0x01;
    // Then, test the 'hi' part
    mask += (GetLength() - OFFSET_BITS) / 2;
    if (0 != (mask[(flowId >> 3)] & (0x80 >> (flowId & 0x07))))
        status |= 0x02;
    return (Status)status;
}  // end MgenFlowCommand:: GetStatus()
       
