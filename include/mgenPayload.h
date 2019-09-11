#ifndef _MGEN_PAYLOAD
#define _MGEN_PAYLOAD

#include <protoDefs.h>  // for UINT16

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
        
		UINT16 GetPayloadLen() {return size;}
		void SetPayload(const char *);
		char *GetPayload();
		const char *GetRaw() {return payload;}
		void SetRaw(char *inStr, UINT16 inSize);
        
	private:
        unsigned int fromHex(char inHex);
		char toHex(int inNum);
        
	    UINT16 size;
		char *payload;
        
};  // end class MgenPayload

#endif //_MGEN_PAYLOAD
