// ###########################################################################
// #
// # DeviceAlpha - Handler for Alpha Display Hardware
// # Copyright (C) 2002-2012 Tony Friery [DialTone]
// #
// # ALL RIGHTS RESERVED
// #
// ###########################################################################

#ifndef DeviceAlphaH
#define DeviceAlphaH

//Updated to include Samsung 16L102DA4 Alpha

class DeviceAlpha {
	protected:

		UINT8 				fClock = 0;					// Clock Line
		UINT8 				fPrevClock = 0;				// Clock Line
		UINT8				fCharPos = 0;				// Character Position
		UINT8				fReset = 0;
		UINT8				fChar = 0;					// Current Character being Built
		UINT8				fClocks = 0;				// Number of Clock state Changes
		UINT8   			fNumDigits = 0;				// Number of Digits on Display
		//16L102DA4 Alpha
		UINT8 				fClock2 = 0;				// Clock Line
		UINT8 				fPrevClock2 = 0;			// Clock Line
		UINT8				fCharPos2 = 0;				// Character Position
		UINT8				fReset2 = 0;
		UINT8				fChar2 = 0;					// Current Character being Built
		UINT8				fClocks2 = 0;				// Number of Clock state Changes
		UINT8   			fNumDigits2 = 0;			// Number of Digits on Display

		UINT8				fCommand = 0;				//Current Command Byte
		UINT8				fEnterChars = 0;
		UINT8				fEnterDots = 0;				//
		UINT8				fRamCharSelect = 0;			//Selects Which Ram Character to write to
		UINT8				fByteCount = 0;				//		
		UINT8				fDotComma[16];				
		UINT8				fSegOnOff = 0;
		UINT8				fPrevSel = 0;
		UINT8				CharTable[256][6];
		UINT8				fEnterUDRAM = 0;
	public:

		int					Chars[16];         		    // Display Buffer
		UINT8				Chars2[16];         	    // Display Buffer
		bool    			Enabled = 0;				// Enabled or Disabled
		char    			Intensity = 0;				// Brightness Level
		char    			Intensity2 = 0;				// Brightness Level
		UINT8   			DisplayChanged = 0;
		UINT8   			IntensityChanged = 0;
		char				OutChars[16];

		void __fastcall WriteSel(UINT8 sel);
		void __fastcall	DoChar(UINT8 ch);
		void __fastcall	Enable(UINT8 enable);
		void __fastcall	Reset(void);
		void __fastcall	Reset2(void);
		int __fastcall GetAlphaSegments(char SegNum);		
		UINT8 __fastcall GetAlphaCharacter(char SegNum);
		UINT8 __fastcall GetArduinoCharacter(char SegNum);
		UINT8 __fastcall GetAlphaDotComma(char SegNum);	
		UINT8 __fastcall GetAlphaDDotComma(char SegNum);
		UINT8 __fastcall GetAlphaChar(char num);
		UINT8 __fastcall GetAlphaDots(char CharNum, char ColumnNum);
		void __fastcall	WriteCommand(UINT8 ch);
		void __fastcall	WriteCharacter(UINT8 ch);

		DeviceAlpha();
		~DeviceAlpha();


};

#endif // DeviceAlphaH
