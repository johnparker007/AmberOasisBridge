// ###########################################################################
// #
// # AlphaStarburst - Starburst / segmented alpha display emulation
// # Split from DeviceAlpha
// #
// ###########################################################################

#ifndef AlphaStarburstH
#define AlphaStarburstH

class AlphaStarburst {
protected:
	UINT8 fClock = 0;
	UINT8 fPrevClock = 0;
	UINT8 fCharPos = 0;
	UINT8 fReset = 0;
	UINT8 fChar = 0;
	UINT8 fClocks = 0;
	UINT8 fNumDigits = 0;

public:
	int Chars[16];
	bool Enabled = 0;
	char Intensity = 0;
	UINT8 DisplayChanged = 0;
	UINT8 IntensityChanged = 0;
	char OutChars[16];

	void __fastcall DoChar(UINT8 ch);
	void __fastcall Enable(UINT8 enable);
	void __fastcall Reset(void);
	int __fastcall GetAlphaSegments(char SegNum);

	UINT8 __fastcall GetAlphaCharacter(char SegNum);
	UINT8 __fastcall GetArduinoCharacter(char SegNum);
	UINT8 __fastcall GetAlphaDotComma(char SegNum);
	UINT8 __fastcall GetAlphaChar(char num);

	AlphaStarburst();
	~AlphaStarburst();
};

#endif // AlphaStarburstH
