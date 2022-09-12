#ifndef _MGEN_PAYLOAD
#define _MGEN_PAYLOAD

#include "protoDefs.h"  // for UINT16
#include "protoPkt.h"

/**
 * @class MgenPayload
 *
 * @brief Contains functions to set and get mgen message payload.
 */
class MgenPayload 
{
	public:
	   MgenPayload();
		~MgenPayload();
        
		UINT16 GetLength() 
            {return payload_len;}
		
        bool SetPayloadString(const char* text);
        // Returns a newly allocated string representation of payload
		static char* GetPayloadString(const char* payloadBytes, UINT16 payloadLen);
        
		bool SetPayloadBytes(char* data, UINT16 size);
		const char* GetPayloadBytes() const
            {return (const char*)payload_buffer;}
		UINT32* AccessPayloadBuffer() 
            {return payload_buffer;}
        void SetLength(UINT16 length)
            {payload_len = length;}
        
        bool Allocate(UINT16 size);
        
	private:
        static char fromHex(char inHex);
		static char toHex(char inNum);
        
	    UINT16   payload_len;
        UINT16   buffer_len;
		UINT32*  payload_buffer;
        
};  // end class MgenPayload

// MGEN_DATA item format (note type > 0x0f are MgenAnalytic::Report items
// 
//       0                   1                   2                   3
//       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |      type     |      len      |       ...                     |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
//      |                                                               |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

class MgenDataItem : public ProtoPkt
{
    public:
        MgenDataItem(UINT32*        bufferPtr = NULL, 
                     unsigned int   bufferBytes = 0, 
                     bool           freeOnDestruct = false);
        ~MgenDataItem();
                
        enum Type
        {
            DATA_ITEM_INVALID = 0,
            DATA_ITEM_FLOW_CMD
            // Note type > 0x0f are MgenAnalytic::Report items
        };
            
        // Use these to parse   
        static Type GetItemType(UINT32* bufferPtr)
            {return ((Type)(((UINT8*)bufferPtr)[0]));}
         
        bool InitFromBuffer(UINT32*         bufferPtr = NULL, 
                            unsigned int    numBytes = 0, 
                            bool            freeOnDestruct = false); 
        Type GetType() const
            {return (Type)GetUINT8(OFFSET_TYPE);}    
        UINT8 GetItemLength() const
            {return GetUINT8(OFFSET_LEN);}
        
        // Use these to build a report (MUST call set dst/src/flowId in  
        // order first. All other fields may be set any time afterward)
        bool InitIntoBuffer(Type            type = DATA_ITEM_INVALID,
                            UINT32*         bufferPtr = NULL, 
                            unsigned int    bufferBytes = 0, 
                            bool            freeOnDestruct = false);
        void SetType(Type type)
            {SetUINT8(OFFSET_TYPE, (UINT8)type);}
        void SetItemLength(UINT8 len)
            {SetUINT8(OFFSET_LEN, len);}
        
    protected:
        enum 
        {
            OFFSET_TYPE = 0,
            OFFSET_LEN = OFFSET_TYPE + 1
        };
};  // end class MgenDataItem

class MgenFlowCommand : public MgenDataItem
{
    public:
        MgenFlowCommand(UINT32*        bufferPtr = NULL, 
                        unsigned int   bufferBytes = 0, 
                        bool           freeOnDestruct = false)
        : MgenDataItem(bufferPtr, bufferBytes, freeOnDestruct) {}
        ~MgenFlowCommand() {}
        
         // This is used for remote control of flow status
        enum Status
        {
            FLOW_UNCHANGED  = 0,
            FLOW_SUSPEND    = 1,
            FLOW_RESUME     = 2,
            FLOW_RESET      = 3
        };
            
        bool SetStatus(UINT32 flowId, Status status);
        bool IsSet() const
            {return (GetLength() > OFFSET_BITS);}
        
        UINT32 GetMaxFlowId() const;
        
        Status GetStatus(UINT32 flowId) const;

        char* AccessBitmask(unsigned int offset)
            {return (char*)AccessBuffer(OFFSET_BITS + offset);}
        UINT8 GetMaskLen() const
            {return (GetItemLength() - OFFSET_BITS);}
       
    private:
        enum {OFFSET_BITS = OFFSET_LEN + 1};
       
       
};  // end class MgenFlowCommand()

#endif //_MGEN_PAYLOAD
