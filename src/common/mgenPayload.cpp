#include "mgenPayload.h"
#include <string.h>
#include <ctype.h>

MgenPayload::MgenPayload() {
	payload = 0;
	size = 0;
}

MgenPayload::~MgenPayload() {
	if (payload != 0) delete [] payload;
}

/**
 * Take a hex encoded string and translate it to binary
 */
void MgenPayload::SetPayload(const char *inStr) {
	if (inStr == NULL) return;
	size = (strlen(inStr) / 2) + (strlen(inStr) % 2);
	//clear the old payload
	if (payload != NULL) delete payload;
	unsigned int cur=0;
	int j = 0;
	//new buffer
	char *tmp=new char[size];
	//every character = 4bits.
    int inLen = strlen(inStr);
	for (int i=0;i < inLen; i++) {
		cur <<= 4;
		cur += fromHex(*(inStr+i));
		//every second char needs the byte to be recorded
		if (((strlen(inStr) - i) % 2) == 1) {
			memset(tmp+j,cur,1);
			cur = 0;
			j++;
		}
	}
	payload = tmp;
}

char *MgenPayload::GetPayload() {
	char *res = new char[(size * 2) + 1];
	unsigned int j;
	memset(res,0,(size * 2) + 1);
	char *tmp = new char[1];
	for (int i=0;i<size;i++) {
		memcpy(tmp,payload+i,1);
		//left than right shift to get the first 4 bits in the byte
		j=(unsigned int)(*tmp);
		j <<= 24;
		j >>= 28;
		res[i*2] = toHex(j);
		//get the next 4 bits
		memcpy(tmp,payload+i,1);
		j=(unsigned int)(*tmp);
		j <<= 28;
		j >>= 28;
		res[(i*2) + 1] = toHex(j);
	}
	delete [] tmp;
	return res;
}

/**
 * Converts from hex to int
 */
unsigned int MgenPayload::fromHex(char inHex) {
	unsigned int res = 0;
	char hex = toupper(inHex);
	if (hex == '0') res = 0;
	else if (hex == '1') res = 1;
	else if (hex == '2') res = 2;
	else if (hex == '3') res = 3;
	else if (hex == '4') res = 4;
	else if (hex == '5') res = 5;
	else if (hex == '6') res = 6;
	else if (hex == '7') res = 7;
	else if (hex == '8') res = 8;
	else if (hex == '9') res = 9;
	else if (hex == 'A') res = 10;
	else if (hex == 'B') res = 11;
	else if (hex == 'C') res = 12;
	else if (hex == 'D') res = 13;
	else if (hex == 'E') res = 14;
	else if (hex == 'F') res = 15;
	return res;
}

/**
 * Converts from int to hex
 */
char MgenPayload::toHex(int inNum) {
	char res;
	if (inNum == 0) res = '0';
	else if (inNum == 1) res = '1';
	else if (inNum == 2) res = '2';
	else if (inNum == 3) res = '3';
	else if (inNum == 4) res = '4';
	else if (inNum == 5) res = '5';
	else if (inNum == 6) res = '6';
	else if (inNum == 7) res = '7';
	else if (inNum == 8) res = '8';
	else if (inNum == 9) res = '9';
	else if (inNum == 10) res = 'A';
	else if (inNum == 11) res = 'B';
	else if (inNum == 12) res = 'C';
	else if (inNum == 13) res = 'D';
	else if (inNum == 14) res = 'E';
	else if (inNum == 15) res = 'F';
	else res = '0';
	return res;
}

/**
 * Copy a binary into the payload
 */
void MgenPayload::SetRaw(char *inStr, UINT16 inSize) 
{
	if (inStr == NULL) return;
	if (inSize == 0) {		
		if (payload != NULL) delete [] payload;
		payload=NULL;
		return;
	}
	if (payload != NULL) delete [] payload;
	payload = new char[inSize];
	memcpy(payload,inStr,inSize);
	size=inSize;
}  // end MgenPayload::SetRaw()

