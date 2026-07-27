#ifndef DEVICEEDCUNIT
#define DEVICEEDCUNIT

//#include "LoadSaveCompressDLLClass.h"

#define EDCBUFFERSIZE 256

class EDCUNIT {
public:

	void __fastcall		Write(UINT8 ByteIn);
	//void __fastcall		Reset(LoadSaveCompressDLLClass* LSCIn);

	char* __fastcall	getEDCString();

	void SaveState();
	void LoadState();

	EDCUNIT();
	~EDCUNIT();

private:

	char Buf[EDCBUFFERSIZE];
	
	UINT16 Length = 0;
	UINT16 SaveLength = 0;
	UINT8 Checksum = 0, lastCheck = 0, Mode = 0;
	

	//LoadSaveCompressDLLClass* LSC = NULL;

};

#endif // DEVICEEDCUNIT