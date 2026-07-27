// ###########################################################################
// #
// # AlphaDotMatrix - Samsung 16L102DA4 dot-matrix alpha display emulation
// # Split from DeviceAlpha
// #
// ###########################################################################

#ifndef AlphaDotMatrixH
#define AlphaDotMatrixH

class AlphaDotMatrix {
protected:

	//Variables
	UINT8 fCharPos = 0;
	UINT8 fReset = 0;	
	UINT8 fNumDigits = 0;	
	UINT8 fRamCharSelect = 0;
	UINT8 fCustomByteCount = 0;
	UINT8 fDotCommaSetting = 0;	
	UINT8 fSel = 0;
	UINT8 fPrevSel = 0;
	UINT8 fMode = 0;
	UINT8 fIntensity = 0; 
	UINT8 fByteLatch = 0;

	//Booleans
	bool LatchFull = false;
	bool SelPulsed = false;
	bool CharCodeWriteIn = false;
	bool fSegOnOff = false;

	//Buffers
	UINT8 CharTable[256][6];
	UINT8 CharacterBuffer[16];
	UINT8 DotCommaBuffer[16];

	//Functions
	void __fastcall ProcessChar(UINT8 ch);
	void __fastcall ProcessInstruction(UINT8 ch);
	UINT8 __fastcall SwapBits(UINT8 ch);

public:

	
	//Functions
	void __fastcall WriteOutput0(UINT8 value);
	void __fastcall WriteOutput1(UINT8 value);	
	void __fastcall RunDotAlpha();
	void __fastcall Reset(void);

	//Outputs
	UINT8 __fastcall GetAlphaDotComma(char SegNum);
	UINT8 __fastcall GetAlphaDots(char CharNum, char ColumnNum);
	UINT8 __fastcall GetIntensity();

	//Constructor / Destructor
	AlphaDotMatrix();
	~AlphaDotMatrix();
};

#endif // AlphaDotMatrixH
