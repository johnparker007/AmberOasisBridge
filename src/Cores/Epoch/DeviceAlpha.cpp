// ###########################################################################
// #
// # DeviceAlpha - Handler for Alpha Display Hardware
// # Copyright (C) 2002-2012 Tony Friery [DialTone]
// #
// # ALL RIGHTS RESERVED
// #
// ###########################################################################

#include "stdafx.h"
#include "DeviceAlpha.h"

UINT8 __fastcall DeviceAlpha::GetAlphaDots(char CharNum, char ColumnNum) {

	// Bounds protect the renderer against corrupt or transient cursor/data values.
	if ((CharNum < 0) || (CharNum >= 16) || (ColumnNum < 0) || (ColumnNum >= 5)) {
		return 0;
	}

	return CharTable[Chars2[(UINT8)CharNum]][(UINT8)ColumnNum];

}


UINT8 __fastcall DeviceAlpha::ReverseByte(UINT8 ch) {

	UINT8 Swapped = 0;
	if (ch & 0x01) Swapped |= 0x80;
	if (ch & 0x02) Swapped |= 0x40;
	if (ch & 0x04) Swapped |= 0x20;
	if (ch & 0x08) Swapped |= 0x10;
	if (ch & 0x10) Swapped |= 0x08;
	if (ch & 0x20) Swapped |= 0x04;
	if (ch & 0x40) Swapped |= 0x02;
	if (ch & 0x80) Swapped |= 0x01;
	return Swapped;

}

void __fastcall DeviceAlpha::WriteSel(UINT8 sel) {

	// SDK EPOCHMAP.doc shows FE1001 bit 0 is the alpha inter-character
	// strobe/select line. It is not a display-controller reset and must not
	// clear the Samsung command-entry state, otherwise ordinary data bytes can
	// be reinterpreted as position/length commands between characters.
	fPrevSel = sel;

}

void __fastcall DeviceAlpha::WriteOutput0(UINT8 value) {

	// SDK EPOCHMAP.doc:
	//   FE1000 bit 0 = alpha serial data
	//   FE1000 bit 1 = alpha data send/clock
	// The Samsung 16L102DA4 receives serial data LSB-first. Assemble one byte
	// from bit 0 on the rising edge of bit 1, then feed the completed byte to
	// the Samsung command decoder.
	const UINT8 oldClock = fOut0Prev & 0x02;
	const UINT8 newClock = value & 0x02;
	const UINT8 dataBit = value & 0x01;

	if ((!oldClock) && newClock) {
		ShiftSerialBit(dataBit);
	}

	fOut0Prev = value;

}

void __fastcall DeviceAlpha::WriteOutput1(UINT8 value) {

	// SDK EPOCHMAP.doc:
	//   FE1001 bit 0 = alpha inter-character strobe
	//   FE1001 bit 1 = alpha reset line
	// Keep the strobe as state only. It must not clear fEnterChars/fEnterDots.
	const UINT8 oldReset = fOut1Prev & 0x02;
	const UINT8 newReset = value & 0x02;

	WriteSel(value & 0x01);

	// Treat reset as asserted on a rising edge. If later testing shows the
	// hardware line is active-low in the game output latch, invert this edge.
	if ((!oldReset) && newReset) {
		Reset2();
		fShiftByte = 0;
		fShiftCount = 0;
	}

	fOut1Prev = value;

}

void __fastcall DeviceAlpha::ShiftSerialBit(UINT8 bit) {

	if (bit) {
		fShiftByte |= (1 << fShiftCount);
	}

	fShiftCount++;

	if (fShiftCount >= 8) {
		ProcessCommandByte(fShiftByte);
		fShiftByte = 0;
		fShiftCount = 0;
	}

}

void __fastcall DeviceAlpha::WriteCommand(UINT8 ch) {

	// Backwards-compatible immediate-byte entry point. Older emulator code fed
	// this with a packed physical byte and expected bit reversal here. The SDK
	// driven FE1000/FE1001 path now assembles the true logical byte and calls
	// ProcessCommandByte() directly.
	ProcessCommandByte(ReverseByte(ch));

}

void __fastcall DeviceAlpha::ProcessCommandByte(UINT8 commandByte) {

	fCommand = commandByte;

	if (fEnterChars) {
		if (fNumDigits2 == 0 || fNumDigits2 > 16) {
			fNumDigits2 = 16;
		}

		if (fCharPos2 >= fNumDigits2) {
			fCharPos2 = 0;
		}

		Chars2[fCharPos2] = fCommand;
		//Auto Increment Pos
		fCharPos2++;
		if (fCharPos2 >= fNumDigits2) {
			fCharPos2 = 0;
		}
		return;
	}
	else if (fEnterDots) {
		if (fNumDigits2 == 0 || fNumDigits2 > 16) {
			fNumDigits2 = 16;
		}

		if (fCharPos2 >= fNumDigits2) {
			fCharPos2 = 0;
		}

		//Other Byte, bit 0 is DP on off, bit 1 is comma on off
		fDotComma[fCharPos2] = (fCommand & 0x3);
		//Auto Increment fCharPos
		fCharPos2++;
		if (fCharPos2 >= fNumDigits2) {
			fCharPos2 = 0;
		}
		return;
	}
	else if (fEnterUDRAM) {
		//Other Bytes, Lowest 7 Bits Define 1 vertical line of character, MSB unused
		//Protect Array. CharTable is rendered as 5 columns, so avoid column 5 overwrite.
		if ((fRamCharSelect < 256) && (fByteCount < 5)) {
			//Set Ram
			CharTable[fRamCharSelect][fByteCount] = (fCommand & 0x7f);
		}
		//Increment Byte Count
		fByteCount++;
		return;
	}

	switch (fCommand & 0xf0) {
	case 0x10://Set Display Position (1 Byte)
		//Set Position
		fCharPos2 = (fCommand & 0xf);
		//Set Enter Chars. Make entry modes explicit/mutually exclusive.
		fEnterChars = 1;
		fEnterDots = 0;
		fEnterUDRAM = 0;
		fByteCount = 0;
		break;
	case 0x20://User Definable Font (6 bytes)
		//First Byte Sets RAM Char Select
		fRamCharSelect = (fCommand & 0x7);
		//Set Enter Ram Flag. Make entry modes explicit/mutually exclusive.
		fEnterChars = 0;
		fEnterDots = 0;
		fEnterUDRAM = 1;
		fByteCount = 0;
		break;
	case 0x30://Comma And/Or Decimal Point On/Off (Multi Bytes)
		//First Byte Set Position
		fCharPos2 = (fCommand & 0xf);
		fEnterChars = 0;
		fEnterDots = 1;
		fEnterUDRAM = 0;
		fByteCount = 0;
		break;
	case 0x50://Luminence Control (Dimming) (1 Byte)
		//Set Intensity
		Intensity2 = (fCommand & 0x7);
		break;
	case 0x60://Digit Length Set (1 Byte)
		//Set Number of Digits
		switch (fCommand & 7) {
		case 0: fNumDigits2 = 16; break;
		case 1: fNumDigits2 = 9;  break;
		case 2: fNumDigits2 = 10; break;
		case 3: fNumDigits2 = 11; break;
		case 4: fNumDigits2 = 12; break;
		case 5: fNumDigits2 = 13; break;
		case 6: fNumDigits2 = 14; break;
		case 7: fNumDigits2 = 15; break;
		}
		if (fCharPos2 >= fNumDigits2) {
			fCharPos2 = 0;
		}
		break;
	case 0x70://All Segments On/Off (1 Byte)
		//Set Segments On/Off
		fSegOnOff = (fCommand & 0x3);
		break;
	case 0xb0://Unlisted, set to zero
		if (fCommand & 0xf) {
			char somethingunknown = 1;
		}
		break;
	default:
		break;
	}

}

void __fastcall DeviceAlpha::DoChar(UINT8 ch) {
	UINT8 command;
	UINT8 x = ch;
	bool update = false;

	if (Enabled) {
		command = x & 0xe0;

		switch (command) {
		case 0xE0:  // Set Duty Period
			x = x & 0x1f;
			if (Intensity != x) {
				Intensity = x;
				IntensityChanged++;
			}
			break;
		case 0xA0:  // Set RAM Position
			fCharPos = x & 0x0f;
			break;
		case 0x80:      // Test Mode
			break;
		case 0xC0:      // Number of digits
			fNumDigits = x & 0x1F;
			if (fNumDigits == 0)
				fNumDigits = 16;

			break;
		default:
			ch = ch & 0x3f;

			if (!ch)
				return;
			if ((ch == '.') || (ch == ',')) {
				if (!(Chars[fCharPos] & 0xff00))
					update = true;

				Chars[fCharPos] |= ch << 8;
			}
			else {
				fCharPos = (fCharPos + 1) & 15;

				if ((Chars[fCharPos] & 0xff) != ch)
					update = true;

				Chars[fCharPos] = ch;
			}
			break;
		}

		if (update)
			DisplayChanged++;
	}
}



void __fastcall DeviceAlpha::Enable(UINT8 enable) {
	if (enable) {
		Enabled = true;
	}
	else {
		Enabled = false;
		Reset();
		DisplayChanged++;
		IntensityChanged++;
	}
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	DeviceAlpha::Reset
//
//  PURPOSE:	(POR) Reset the emulated Alphanumeric Hardware
//
//  INPUTS:		void
//
//  OUTPUT:		void
//
///////////////////////////////////////////////////////////////////////
void __fastcall DeviceAlpha::Reset(void) {
	fClock = 0;		// Clear Clock bit
	fCharPos = 15;		// Start cursor on RHS
	fClocks = 0;		// Reset Clock Count
	fNumDigits = 16;		// Reset Digit Count
	Intensity = 0;		// Reset Brightness
	fChar = 0;        // Reset Current Character Byte	

	// Reset display RAM
	for (UINT8 count = 0; count < 16; count++) {
		Chars[count] = 0x20;
	}
}

void __fastcall DeviceAlpha::Reset2(void) {

	fEnterChars = 0;		// Reset Enter Characters Flag
	fPrevSel = 0;		// Reset Previous Sel Pin
	fClock2 = 0;		// Clear Clock bit
	fCharPos2 = 0;		// Start cursor
	fClocks2 = 0;		// Reset Clock Count
	fNumDigits2 = 16;		// Reset Digit Count
	Intensity2 = 0;		// Reset Brightness
	fChar2 = 0;        // Reset Current Character Byte
	fEnterDots = 0;		// Reset Enter Dots Flag
	fEnterUDRAM = 0;		// Reset Enter UD Ram flag
	fByteCount = 0;		// Reset UDRAM byte counter
	fCommand = 0;
	fSegOnOff = 0;
	fPrevSel = 0;
	fOut0Prev = 0;
	fOut1Prev = 0;
	fShiftByte = 0;
	fShiftCount = 0;

	// Reset display RAM
	for (UINT8 count = 0; count < 16; count++) {
		fDotComma[count] = 0;
		Chars2[count] = 0x20;
	}

}



///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	DeviceAlpha::~DeviceAlpha
//
//  PURPOSE:	Destructor for DeviceAlpha Class
//
//  INPUTS:		(none)
//
//  OUTPUT:		(none)
//
///////////////////////////////////////////////////////////////////////
DeviceAlpha::~DeviceAlpha() {

}


UINT8 DeviceAlpha::GetAlphaDotComma(char SegNum) {
	if ((SegNum < 0) || (SegNum >= 16)) {
		return 0;
	}
	UINT8 ret;
	ret = (Chars[(UINT8)SegNum] >> 8);
	return ret;
}
UINT8 DeviceAlpha::GetAlphaDDotComma(char SegNum) {
	if ((SegNum < 0) || (SegNum >= 16)) {
		return 0;
	}
	UINT8 ret;
	ret = (fDotComma[(UINT8)SegNum]);
	return ret;
}
UINT8 DeviceAlpha::GetAlphaChar(char num) {
	UINT8 ret;
	ret = GetArduinoCharacter(num);
	return ret;
}
int DeviceAlpha::GetAlphaSegments(char SegNum) {

	char StandardisedChar;
	int ret = 0;

	StandardisedChar = GetAlphaCharacter(SegNum);
	if (StandardisedChar != 31) {
		StandardisedChar |= 0;
	}
	//Updated for 16 segment display (MPU4 is 14)
	switch (StandardisedChar) {
	case 0: ret = 20607;	break;//@
	case 1: ret = 17615;	break;//A
	case 2: ret = 5439;		break;//B
	case 3: ret = 243;		break;//C
	case 4: ret = 4415;		break;//D
	case 5: ret = 16627;	break;//E
	case 6: ret = 16579;	break;//F
	case 7: ret = 1275;		break;//G
	case 8: ret = 17612;	break;//H
	case 9: ret = 4403;		break;//I
	case 10: ret = 124;		break;//J
	case 11: ret = 19136;	break;//K
	case 12: ret = 240;		break;//L
	case 13: ret = 33484;	break;//M
	case 14: ret = 35020;	break;//N
	case 15: ret = 255;		break;//O
	case 16: ret = 17607;	break;//P
	case 17: ret = 2303;	break;//Q
	case 18: ret = 19655;	break;//R
	case 19: ret = 17595;	break;//S
	case 20: ret = 4355;	break;//T
	case 21: ret = 252;		break;//U
	case 22: ret = 8896;	break;//V
	case 23: ret = 10444;	break;//W
	case 24: ret = 43520;	break;//X
	case 25: ret = 37376;	break;//Y
	case 26: ret = 8755;	break;//Z
	case 27: ret = 225;		break;// SQUARE OPEN BRACKET
	case 28: ret = 34816;	break;// BACKSLASH
	case 29: ret = 30;		break;// SQUARE CLOSED BRACKET
	case 30: ret = 10240;	break;//^
	case 31: ret = 48;		break;//_
	case 32: ret = 0;		break;// 
	case 33: ret = 33057;	break;//!
	case 34: ret = 384;		break;//"
	case 35: ret = 21820;	break;//#
	case 36: ret = 4539;	break;//$
	case 37: ret = 30489;	break;//%
	case 38: ret = 51577;	break;//&
	case 39: ret = 512;		break;//'
	case 40: ret = 2560;	break;//<
	case 41: ret = 40960;	break;//>
	case 42: ret = 65280;	break;//*
	case 43: ret = 21760;	break;//+
	case 44: ret = 0;		break;//;
	case 45: ret = 17408;	break;//-
	case 46: ret = 0;		break;//.
	case 47: ret = 8704;	break;///
	case 48: ret = 8959;	break;//0
	case 49: ret = 4352;	break;//1
	case 50: ret = 17527;	break;//2
	case 51: ret = 17471;	break;//3
	case 52: ret = 17548;	break;//4
	case 53: ret = 17595;	break;//5
	case 54: ret = 17659;	break;//6
	case 55: ret = 15;		break;//7
	case 56: ret = 17663;	break;//8
	case 57: ret = 17599;	break;//9
	case 58: ret = 33;		break;//=
	case 59: ret = 8193;	break;//;
	case 60: ret = 17456;	break;//==
	case 61: ret = 17456;	break;//=
	case 62: ret = 786;		break;//!!
	case 63: ret = 5127;	break;//?
	}
	return ret;

}
UINT8 DeviceAlpha::GetArduinoCharacter(char SegNum) {

	char StandardisedChar;
	UINT8 ret = 0;

	StandardisedChar = GetAlphaCharacter(SegNum);
	if (StandardisedChar != 31) {
		StandardisedChar |= 0;
	}
	//Updated for 16 segment display (MPU4 is 14)
	switch (StandardisedChar) {
	case 0: ret = 0x40;	break;//@
	case 1: ret = 0x41;	break;//A
	case 2: ret = 0x42;	break;//B
	case 3: ret = 0x43;	break;//C
	case 4: ret = 0x44;		break;//D
	case 5: ret = 0x45;		break;//E
	case 6: ret = 0x46;		break;//F
	case 7: ret = 0x47;		break;//G
	case 8: ret = 0x48;		break;//H
	case 9: ret = 0x49;		break;//I
	case 10: ret = 0x4a;	break;//J
	case 11: ret = 0x4b;	break;//K
	case 12: ret = 0x4c;	break;//L
	case 13: ret = 0x4d;	break;//M
	case 14: ret = 0x4e;	break;//N
	case 15: ret = 0x4f;	break;//O
	case 16: ret = 0x50;	break;//P
	case 17: ret = 0x51;	break;//Q
	case 18: ret = 0x52;	break;//R
	case 19: ret = 0x53;	break;//S
	case 20: ret = 0x54;	break;//T
	case 21: ret = 0x55;	break;//U
	case 22: ret = 0x56;	break;//V
	case 23: ret = 0x57;	break;//W
	case 24: ret = 0x58;	break;//X
	case 25: ret = 0x59;	break;//Y
	case 26: ret = 0x5a;	break;//Z
	case 27: ret = 0x3c;	break;// SQUARE OPEN BRACKET
	case 28: ret = 0x2f;	break;// BACKSLASH
	case 29: ret = 0x3e;	break;// SQUARE CLOSED BRACKET
	case 30: ret = 0x5e;	break;//^
	case 31: ret = 0x5f;	break;//_
	case 32: ret = 0x20;	break;// 
	case 33: ret = 0x21;	break;//!
	case 34: ret = 0x22;	break;//"
	case 35: ret = 0x23;	break;//#
	case 36: ret = 0x24;	break;//$
	case 37: ret = 0x25;	break;//%
	case 38: ret = 0x26;	break;//&
	case 39: ret = 0x27;	break;//'
	case 40: ret = 0x28;	break;//<
	case 41: ret = 0x29;	break;//>
	case 42: ret = 0x2a;	break;//*
	case 43: ret = 0x2b;	break;//+
	case 44: ret = 0x2c;	break;//;
	case 45: ret = 0x2d;	break;//-
	case 46: ret = 0x2e;	break;//.
	case 47: ret = 0x2f;	break;///
	case 48: ret = 0x30;	break;//0
	case 49: ret = 0x31;	break;//1
	case 50: ret = 0x32;	break;//2
	case 51: ret = 0x33;	break;//3
	case 52: ret = 0x34;	break;//4
	case 53: ret = 0x35;	break;//5
	case 54: ret = 0x36;	break;//6
	case 55: ret = 0x37;	break;//7
	case 56: ret = 0x38;	break;//8
	case 57: ret = 0x39;	break;//9
	case 58: ret = 0x3D;	break;//=
	case 59: ret = 0x20;	break;//;
	case 60: ret = 0x3D;	break;//==
	case 61: ret = 0x20;	break;//=
	case 62: ret = 0x21;	break;//!!
	case 63: ret = 0x3f;	break;//?
	}
	return ret;
}
UINT8 DeviceAlpha::GetAlphaCharacter(char SegNum) {

	UINT8 AlphaConvert = 0;
	UINT8 UseChar;

	if ((SegNum < 0) || (SegNum >= 16)) {
		return 32;
	}

	UseChar = (Chars[(UINT8)SegNum] & 0x3f);

	switch (UseChar) {
	case 0:;
	case 1:;
	case 2:;
	case 3:;
	case 4:;
	case 5:;
	case 6:;
	case 7:;
	case 8:;
	case 9:;
	case 10:;
	case 11:;
	case 12:;
	case 13:;
	case 14:;
	case 15:;
	case 16:;
	case 17:;
	case 18:;
	case 19:;
	case 20:;
	case 21:;
	case 22:;
	case 23:;
	case 24:;
	case 25:;
	case 26: AlphaConvert = (UseChar); break;
	case 27: AlphaConvert = 32; break;
	case 28: AlphaConvert = 28; break;
	case 29: AlphaConvert = 32; break;
	case 30: AlphaConvert = 32; break;
	case 31: AlphaConvert = 31; break;
	case 32: AlphaConvert = 32; break;
	case 33: AlphaConvert = 32; break;
	case 34: AlphaConvert = 34; break;
	case 35: AlphaConvert = 35; break;
	case 36: AlphaConvert = 36; break;
	case 37: AlphaConvert = 37; break;
	case 38: AlphaConvert = 38; break;
	case 39: AlphaConvert = 39; break;
	case 40: AlphaConvert = 40; break;
	case 41: AlphaConvert = 41; break;
	case 42: AlphaConvert = 42; break;
	case 43: AlphaConvert = 43; break;
	case 44: AlphaConvert = 44; break;
	case 45: AlphaConvert = 45; break;
	case 46: AlphaConvert = 46; break;
	case 47: AlphaConvert = 47; break;
	case 48:;
	case 49:;
	case 50:;
	case 51:;
	case 52:;
	case 53:;
	case 54:;
	case 55:;
	case 56:
	case 57: AlphaConvert = (UseChar); break;
	case 58: AlphaConvert = 57; break;
	case 59: AlphaConvert = 58; break;
	case 60: AlphaConvert = 59; break;
	case 61: AlphaConvert = 60; break;
	case 62: AlphaConvert = 61; break;
	case 63: AlphaConvert = 62; break;
	}

	return AlphaConvert;
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	DeviceAlpha::DeviceAlpha
//
//  PURPOSE:	Constructor for DeviceAlpha Class
//
//  INPUTS:		(none)
//
//  OUTPUT:		(none)
//
///////////////////////////////////////////////////////////////////////
#define D6 64
#define D5 32
#define D4 16
#define D3 8
#define D2 4
#define D1 2
#define D0 1

DeviceAlpha::DeviceAlpha() {
	Reset();
	int loop, loop2;
	for (loop = 0; loop < 128; loop++) {
		for (loop2 = 0; loop2 < 5; loop2++) {
			CharTable[loop][loop2] = 0;
		}
	}
	for (loop = 128; loop < 256; loop++) {
		//Mark unknown as ?
		CharTable[loop][0] = (D1);
		CharTable[loop][1] = (D0);
		CharTable[loop][2] = (D6 | D4 | D0);
		CharTable[loop][3] = (D3 | D0);
		CharTable[loop][4] = (D2 | D1);

	}

	for (loop = 0; loop < 16; loop++) {
		Chars[loop] = 0;
		Chars2[loop] = 0;
		OutChars[loop] = 0;
		fDotComma[loop] = 0;
	}
	Reset2();

	//First 8 are User Defined RAM

	// Greater Or Equal
	CharTable[8][0] = (D6 | D4);
	CharTable[8][1] = (D6 | D4 | D3);
	CharTable[8][2] = (D6 | D4 | D2);
	CharTable[8][3] = (D6 | D4 | D1);
	CharTable[8][4] = (D6 | D4 | D0);
	// Less Than Equal
	CharTable[9][0] = (D6 | D4 | D0);
	CharTable[9][1] = (D6 | D4 | D1);
	CharTable[9][2] = (D6 | D4 | D2);
	CharTable[9][3] = (D6 | D4 | D3);
	CharTable[9][4] = (D6 | D4);
	// Not Equal
	CharTable[10][0] = (D2 | D4);
	CharTable[10][1] = (D2 | D4 | D1);
	CharTable[10][2] = (D2 | D4 | D3);
	CharTable[10][3] = (D2 | D4 | D5);
	CharTable[10][4] = (D2 | D4);
	// Unsure
	CharTable[11][0] = (D2 | D4 | D0);
	CharTable[11][1] = (D2 | D4);
	CharTable[11][2] = (D2 | D4);
	CharTable[11][3] = (D2 | D4);
	CharTable[11][4] = (D2 | D4 | D6);
	// Boolean OR
	CharTable[12][0] = 0;
	CharTable[12][1] = 127;
	CharTable[12][2] = 0;
	CharTable[12][3] = 127;
	CharTable[12][4] = 0;
	// Triple Horizontal Lines
	CharTable[13][0] = (D5 | D3 | D1);
	CharTable[13][1] = (D5 | D3 | D1);
	CharTable[13][2] = (D5 | D3 | D1);
	CharTable[13][3] = (D5 | D3 | D1);
	CharTable[13][4] = (D5 | D3 | D1);
	// Upside down T
	CharTable[14][0] = (D6);
	CharTable[14][1] = (D6);
	CharTable[14][2] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[14][3] = (D6);
	CharTable[14][4] = (D6);
	// X with line
	CharTable[15][0] = (D0 | D2 | D6);
	CharTable[15][1] = (D0 | D3 | D5);
	CharTable[15][2] = (D0 | D4);
	CharTable[15][3] = (D0 | D3 | D5);
	CharTable[15][4] = (D0 | D2 | D6);

	// Alpha
	CharTable[16][0] = (D5 | D4 | D3);
	CharTable[16][1] = (D6 | D2);
	CharTable[16][2] = (D6 | D2);
	CharTable[16][3] = (D5 | D4 | D3);
	CharTable[16][4] = (D6 | D2);
	// Beta
	CharTable[17][0] = (D6);
	CharTable[17][1] = (D6 | D5 | D4 | D3 | D2 | D1);
	CharTable[17][2] = (D5 | D2 | D0);
	CharTable[17][3] = (D5 | D2 | D0);
	CharTable[17][4] = (D3 | D4 | D1);
	// Rotated L
	CharTable[18][0] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[18][1] = (D0);
	CharTable[18][2] = (D0);
	CharTable[18][3] = (D0);
	CharTable[18][4] = (D0);
	// Phi
	CharTable[19][0] = (D2);
	CharTable[19][1] = (D6 | D5 | D4 | D3 | D2);
	CharTable[19][2] = (D2);
	CharTable[19][3] = (D6 | D5 | D4 | D3 | D2);
	CharTable[19][4] = (D6 | D2);
	// Epsilon
	CharTable[20][0] = (D6 | D0);
	CharTable[20][1] = (D6 | D5 | D1 | D0);
	CharTable[20][2] = (D6 | D4 | D2 | D0);
	CharTable[20][3] = (D6 | D3 | D0);
	CharTable[20][4] = (D6 | D5 | D1 | D0);
	// Theta
	CharTable[21][0] = (D5 | D4 | D3);
	CharTable[21][1] = (D6 | D2);
	CharTable[21][2] = (D6 | D2);
	CharTable[21][3] = (D6 | D4 | D3 | D2);
	CharTable[21][4] = (D2);
	// Micro
	CharTable[22][0] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[22][1] = (D2 | D1 | D0);
	CharTable[22][2] = (D3);
	CharTable[22][3] = (D3);
	CharTable[22][4] = (D2 | D1 | D0);
	// Time?
	CharTable[23][0] = (D2);
	CharTable[23][1] = (D2);
	CharTable[23][2] = (D5 | D4 | D3 | D2);
	CharTable[23][3] = (D6 | D2);
	CharTable[23][4] = (D6 | D2);
	// Dont know
	CharTable[24][0] = (D6 | D3 | D0);
	CharTable[24][1] = (D6 | D4 | D2 | D0);
	CharTable[24][2] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[24][3] = (D6 | D4 | D2 | D0);
	CharTable[24][4] = (D6 | D3 | D0);
	// Dont Know
	CharTable[25][0] = (D5 | D4 | D3 | D2 | D1);
	CharTable[25][1] = (D6 | D3 | D0);
	CharTable[25][2] = (D6 | D3 | D0);
	CharTable[25][3] = (D6 | D3 | D0);
	CharTable[25][4] = (D5 | D4 | D3 | D2 | D1);
	// Omega
	CharTable[26][0] = (D6 | D4 | D3 | D2 | D1);
	CharTable[26][1] = (D6 | D5 | D0);
	CharTable[26][2] = (D0);
	CharTable[26][3] = (D6 | D5 | D0);
	CharTable[26][4] = (D6 | D4 | D3 | D2 | D1);
	// Dont Know
	CharTable[27][0] = (D5 | D4 | D3 | D1);
	CharTable[27][1] = (D6 | D2 | D0);
	CharTable[27][2] = (D6 | D2 | D0);
	CharTable[27][3] = (D6 | D2 | D0);
	CharTable[27][4] = (D5 | D4 | D3);
	// Infinity
	CharTable[28][0] = (D4 | D3 | D2);
	CharTable[28][1] = (D5 | D1);
	CharTable[28][2] = (D4 | D3 | D2);
	CharTable[28][3] = (D5 | D1);
	CharTable[28][4] = (D4 | D3 | D2);
	// Dont Know
	CharTable[29][0] = (D3 | D2);
	CharTable[29][1] = (D5 | D1);
	CharTable[29][2] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[29][3] = (D5 | D1);
	CharTable[29][4] = (D3 | D2);
	// Dont Know
	CharTable[30][0] = (D6 | D4 | D0);
	CharTable[30][1] = (D6 | D4 | D1);
	CharTable[30][2] = (D6 | D4 | D2);
	CharTable[30][3] = (D6 | D4 | D3);
	CharTable[30][4] = (D6 | D4);
	// // Dont Know
	CharTable[31][0] = (D6 | D5 | D4 | D3 | D2 | D1);
	CharTable[31][1] = (D0);
	CharTable[31][2] = (D0);
	CharTable[31][3] = (D0);
	CharTable[31][4] = (D0);

	//Space
	//Leave Blank
	// !	
	CharTable[33][2] = (D6 | D3 | D2 | D1 | D0);
	// "	
	CharTable[34][1] = (D3 | D2 | D1 | D0);
	CharTable[34][3] = (D3 | D2 | D1 | D0);
	// #
	CharTable[35][0] = (D4 | D2);
	CharTable[35][1] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[35][2] = (D4 | D2);
	CharTable[35][3] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[35][4] = (D4 | D2);
	// $
	CharTable[36][0] = (D5 | D2);
	CharTable[36][1] = (D5 | D3 | D1);
	CharTable[36][2] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[36][3] = (D5 | D3 | D1);
	CharTable[36][4] = (D4 | D1);
	// %
	CharTable[37][0] = (D5 | D1 | D0);
	CharTable[37][1] = (D4 | D1 | D0);
	CharTable[37][2] = (D3);
	CharTable[37][3] = (D6 | D5 | D2);
	CharTable[37][4] = (D6 | D5 | D1);
	// &
	CharTable[38][0] = (D5 | D4 | D2 | D1);
	CharTable[38][1] = (D6 | D3 | D0);
	CharTable[38][2] = (D6 | D4 | D2 | D0);
	CharTable[38][3] = (D5 | D1);
	CharTable[38][4] = (D6 | D4 | D3);
	// '	
	CharTable[39][1] = (D2 | D0);
	CharTable[39][2] = (D1 | D0);
	// (	
	CharTable[40][2] = (D4 | D3 | D2);
	CharTable[40][3] = (D5 | D1);
	CharTable[40][4] = (D6 | D0);
	// )
	CharTable[41][0] = (D6 | D0);
	CharTable[41][1] = (D5 | D1);
	CharTable[41][2] = (D4 | D3 | D2);
	// *
	CharTable[42][0] = (D5 | D1);
	CharTable[42][1] = (D4 | D2);
	CharTable[42][2] = (D3);
	CharTable[42][3] = (D4 | D2);
	CharTable[42][4] = (D5 | D1);
	// +
	CharTable[43][0] = (D3);
	CharTable[43][1] = (D3);
	CharTable[43][2] = (D5 | D4 | D3 | D2 | D1);
	CharTable[43][3] = (D3);
	CharTable[43][4] = (D3);
	// ,	
	CharTable[44][1] = (D6 | D4);
	CharTable[44][2] = (D6 | D5);
	// -
	CharTable[45][0] = (D3);
	CharTable[45][1] = (D3);
	CharTable[45][2] = (D3);
	CharTable[45][3] = (D3);
	CharTable[45][4] = (D3);
	// .	
	CharTable[46][1] = (D6 | D5);
	CharTable[46][2] = (D6 | D5);
	// /
	CharTable[47][0] = (D5);
	CharTable[47][1] = (D4);
	CharTable[47][2] = (D3);
	CharTable[47][3] = (D2);
	CharTable[47][4] = (D1);

	//0
	CharTable[48][0] = (D5 | D4 | D3 | D2 | D1);
	CharTable[48][1] = (D6 | D4 | D0);
	CharTable[48][2] = (D6 | D3 | D0);
	CharTable[48][3] = (D6 | D2 | D0);
	CharTable[48][4] = (D5 | D4 | D3 | D2 | D1);
	//1
	CharTable[49][1] = (D6 | D1);
	CharTable[49][2] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[49][3] = (D6);
	//2
	CharTable[50][0] = (D6 | D1);
	CharTable[50][1] = (D6 | D5 | D0);
	CharTable[50][2] = (D6 | D4 | D0);
	CharTable[50][3] = (D6 | D3 | D0);
	CharTable[50][4] = (D6 | D2 | D1);
	//3
	CharTable[51][0] = (D5 | D0);
	CharTable[51][1] = (D6 | D0);
	CharTable[51][2] = (D6 | D2 | D0);
	CharTable[51][3] = (D6 | D3 | D1 | D0);
	CharTable[51][4] = (D5 | D4 | D0);
	//4
	CharTable[52][0] = (D4 | D3);
	CharTable[52][1] = (D4 | D2);
	CharTable[52][2] = (D4 | D1);
	CharTable[52][3] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[52][4] = (D4);
	//5
	CharTable[53][0] = (D5 | D2 | D1 | D0);
	CharTable[53][1] = (D6 | D2 | D0);
	CharTable[53][2] = (D6 | D2 | D0);
	CharTable[53][3] = (D6 | D2 | D0);
	CharTable[53][4] = (D5 | D4 | D3 | D0);
	//6
	CharTable[54][0] = (D5 | D4 | D3 | D2);
	CharTable[54][1] = (D6 | D3 | D1);
	CharTable[54][2] = (D6 | D3 | D0);
	CharTable[54][3] = (D6 | D3 | D0);
	CharTable[54][4] = (D5 | D4);
	//7
	CharTable[55][0] = (D0);
	CharTable[55][1] = (D6 | D5 | D4 | D0);
	CharTable[55][2] = (D3 | D0);
	CharTable[55][3] = (D2 | D0);
	CharTable[55][4] = (D1 | D0);
	//8
	CharTable[56][0] = (D5 | D4 | D2 | D1);
	CharTable[56][1] = (D6 | D3 | D0);
	CharTable[56][2] = (D6 | D3 | D0);
	CharTable[56][3] = (D6 | D3 | D0);
	CharTable[56][4] = (D5 | D4 | D2 | D1);
	//9
	CharTable[57][0] = (D2 | D1);
	CharTable[57][1] = (D6 | D3 | D0);
	CharTable[57][2] = (D6 | D3 | D0);
	CharTable[57][3] = (D5 | D3 | D0);
	CharTable[57][4] = (D4 | D3 | D2 | D1);
	//	:
	CharTable[58][1] = (D5 | D4 | D2 | D1);
	CharTable[58][2] = (D5 | D4 | D2 | D1);
	// ;	
	CharTable[59][1] = (D6 | D4 | D3 | D1 | D0);
	CharTable[59][2] = (D5 | D4 | D3 | D1 | D0);
	// <
	CharTable[60][1] = (D3);
	CharTable[60][2] = (D4 | D2);
	CharTable[60][3] = (D5 | D1);
	CharTable[60][4] = (D6 | D0);
	// =
	CharTable[61][0] = (D4 | D2);
	CharTable[61][1] = (D4 | D2);
	CharTable[61][2] = (D4 | D2);
	CharTable[61][3] = (D4 | D2);
	CharTable[61][4] = (D4 | D2);
	// >
	CharTable[62][0] = (D6 | D0);
	CharTable[62][1] = (D5 | D1);
	CharTable[62][2] = (D4 | D2);
	CharTable[62][3] = (D3);
	// ?
	CharTable[63][0] = (D1);
	CharTable[63][1] = (D0);
	CharTable[63][2] = (D6 | D4 | D0);
	CharTable[63][3] = (D3 | D0);
	CharTable[63][4] = (D2 | D1);


	//@
	CharTable[64][0] = (D5 | D4 | D3 | D2 | D1);
	CharTable[64][1] = (D6 | D0);
	CharTable[64][2] = (D6 | D4 | D3 | D2 | D0);
	CharTable[64][3] = (D6 | D4 | D2 | D0);
	CharTable[64][4] = (D4 | D3 | D2 | D1);
	//A
	CharTable[65][0] = (D6 | D5 | D4 | D3 | D2);
	CharTable[65][1] = (D4 | D1);
	CharTable[65][2] = (D4 | D0);
	CharTable[65][3] = (D4 | D1);
	CharTable[65][4] = (D6 | D5 | D4 | D3 | D2);
	//B
	CharTable[66][0] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[66][1] = (D6 | D3 | D0);
	CharTable[66][2] = (D6 | D3 | D0);
	CharTable[66][3] = (D6 | D3 | D0);
	CharTable[66][4] = (D5 | D4 | D2 | D1);
	//C
	CharTable[67][0] = (D5 | D4 | D3 | D2 | D1);
	CharTable[67][1] = (D6 | D0);
	CharTable[67][2] = (D6 | D0);
	CharTable[67][3] = (D6 | D0);
	CharTable[67][4] = (D5 | D1);
	//D
	CharTable[68][0] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[68][1] = (D6 | D0);
	CharTable[68][2] = (D6 | D0);
	CharTable[68][3] = (D5 | D1);
	CharTable[68][4] = (D4 | D3 | D2);
	//E
	CharTable[69][0] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[69][1] = (D6 | D3 | D0);
	CharTable[69][2] = (D6 | D3 | D0);
	CharTable[69][3] = (D6 | D3 | D0);
	CharTable[69][4] = (D6 | D0);
	//F
	CharTable[70][0] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[70][1] = (D3 | D0);
	CharTable[70][2] = (D3 | D0);
	CharTable[70][3] = (D3 | D0);
	CharTable[70][4] = (D0);
	//G
	CharTable[71][0] = (D5 | D4 | D3 | D2 | D1);
	CharTable[71][1] = (D6 | D0);
	CharTable[71][2] = (D6 | D3 | D0);
	CharTable[71][3] = (D6 | D3 | D0);
	CharTable[71][4] = (D6 | D5 | D4 | D3 | D1);
	//H
	CharTable[72][0] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[72][1] = (D3);
	CharTable[72][2] = (D3);
	CharTable[72][3] = (D3);
	CharTable[72][4] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	//I	
	CharTable[73][1] = (D6 | D0);
	CharTable[73][2] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[73][3] = (D6 | D0);
	//J
	CharTable[74][0] = (D5);
	CharTable[74][1] = (D6);
	CharTable[74][2] = (D6 | D0);
	CharTable[74][3] = (D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[74][4] = (D0);
	//K
	CharTable[75][0] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[75][1] = (D3);
	CharTable[75][2] = (D4 | D2);
	CharTable[75][3] = (D5 | D1);
	CharTable[75][4] = (D6 | D0);
	//L
	CharTable[76][0] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[76][1] = (D6);
	CharTable[76][2] = (D6);
	CharTable[76][3] = (D6);
	CharTable[76][4] = (D6);
	//M
	CharTable[77][0] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[77][1] = (D1);
	CharTable[77][2] = (D3 | D2);
	CharTable[77][3] = (D1);
	CharTable[77][4] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	//N
	CharTable[78][0] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[78][1] = (D2);
	CharTable[78][2] = (D3);
	CharTable[78][3] = (D4);
	CharTable[78][4] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	//O
	CharTable[79][0] = (D5 | D4 | D3 | D2 | D1);
	CharTable[79][1] = (D6 | D0);
	CharTable[79][2] = (D6 | D0);
	CharTable[79][3] = (D6 | D0);
	CharTable[79][4] = (D5 | D4 | D3 | D2 | D1);

	//P
	CharTable[80][0] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[80][1] = (D3 | D0);
	CharTable[80][2] = (D3 | D0);
	CharTable[80][3] = (D3 | D0);
	CharTable[80][4] = (D2 | D1);
	//Q
	CharTable[81][0] = (D5 | D4 | D3 | D2 | D1);
	CharTable[81][1] = (D6 | D0);
	CharTable[81][2] = (D6 | D4 | D0);
	CharTable[81][3] = (D5 | D0);
	CharTable[81][4] = (D6 | D4 | D3 | D2 | D1);
	//R
	CharTable[82][0] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[82][1] = (D3 | D0);
	CharTable[82][2] = (D4 | D3 | D0);
	CharTable[82][3] = (D5 | D3 | D0);
	CharTable[82][4] = (D6 | D2 | D1);
	//S
	CharTable[83][0] = (D6 | D2 | D1);
	CharTable[83][1] = (D6 | D3 | D0);
	CharTable[83][2] = (D6 | D3 | D0);
	CharTable[83][3] = (D6 | D3 | D0);
	CharTable[83][4] = (D5 | D4 | D0);
	//T
	CharTable[84][0] = (D0);
	CharTable[84][1] = (D0);
	CharTable[84][2] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[84][3] = (D0);
	CharTable[84][4] = (D0);
	//U
	CharTable[85][0] = (D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[85][1] = (D6);
	CharTable[85][2] = (D6);
	CharTable[85][3] = (D6);
	CharTable[85][4] = (D5 | D4 | D3 | D2 | D1 | D0);
	//V
	CharTable[86][0] = (D4 | D3 | D2 | D1 | D0);
	CharTable[86][1] = (D5);
	CharTable[86][2] = (D6);
	CharTable[86][3] = (D5);
	CharTable[86][4] = (D4 | D3 | D2 | D1 | D0);
	//W
	CharTable[87][0] = (D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[87][1] = (D6);
	CharTable[87][2] = (D5 | D4 | D3);
	CharTable[87][3] = (D6);
	CharTable[87][4] = (D5 | D4 | D3 | D2 | D1 | D0);
	//X
	CharTable[88][0] = (D6 | D5 | D1 | D0);
	CharTable[88][1] = (D4 | D2);
	CharTable[88][2] = (D3);
	CharTable[88][3] = (D4 | D2);
	CharTable[88][4] = (D6 | D5 | D1 | D0);
	//Y
	CharTable[89][0] = (D2 | D1 | D0);
	CharTable[89][1] = (D3);
	CharTable[89][2] = (D6 | D5 | D4);
	CharTable[89][3] = (D3);
	CharTable[89][4] = (D2 | D1 | D0);
	//Z
	CharTable[90][0] = (D6 | D5 | D0);
	CharTable[90][1] = (D6 | D4 | D0);
	CharTable[90][2] = (D6 | D3 | D0);
	CharTable[90][3] = (D6 | D2 | D0);
	CharTable[90][4] = (D6 | D1 | D0);
	//[	
	CharTable[91][1] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[91][2] = (D6 | D0);
	CharTable[91][3] = (D6 | D0);
	//BACKSLASH	
	CharTable[92][0] = (D1);
	CharTable[92][1] = (D2);
	CharTable[92][2] = (D3);
	CharTable[92][3] = (D4);
	CharTable[92][4] = (D5);
	//]	
	CharTable[93][1] = (D6 | D0);
	CharTable[93][2] = (D6 | D0);
	CharTable[93][3] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	//^
	CharTable[94][0] = (D2);
	CharTable[94][1] = (D1);
	CharTable[94][2] = (D0);
	CharTable[94][3] = (D1);
	CharTable[94][4] = (D2);
	//_
	CharTable[95][0] = (D6);
	CharTable[95][1] = (D6);
	CharTable[95][2] = (D6);
	CharTable[95][3] = (D6);
	CharTable[95][4] = (D6);

	//'	
	CharTable[96][1] = (D1 | D0);
	CharTable[96][2] = (D2 | D0);
	//a
	CharTable[97][0] = (D5);
	CharTable[97][1] = (D6 | D4 | D2);
	CharTable[97][2] = (D6 | D4 | D2);
	CharTable[97][3] = (D6 | D4 | D2);
	CharTable[97][4] = (D6 | D5 | D4 | D3);
	//b
	CharTable[98][0] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[98][1] = (D6 | D2);
	CharTable[98][2] = (D6 | D2);
	CharTable[98][3] = (D6 | D2);
	CharTable[98][4] = (D5 | D4 | D3);
	//c
	CharTable[99][0] = (D5 | D4 | D3);
	CharTable[99][1] = (D6 | D2);
	CharTable[99][2] = (D6 | D2);
	CharTable[99][3] = (D6 | D2);
	CharTable[99][4] = (D6 | D2);
	//d
	CharTable[100][0] = (D5 | D4 | D3);
	CharTable[100][1] = (D6 | D2);
	CharTable[100][2] = (D6 | D2);
	CharTable[100][3] = (D6 | D2);
	CharTable[100][4] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	//e
	CharTable[101][0] = (D5 | D4 | D3);
	CharTable[101][1] = (D6 | D4 | D2);
	CharTable[101][2] = (D6 | D4 | D2);
	CharTable[101][3] = (D6 | D4 | D2);
	CharTable[101][4] = (D4 | D3);
	//f
	CharTable[102][1] = (D2);
	CharTable[102][2] = (D6 | D5 | D4 | D3 | D2 | D1);
	CharTable[102][3] = (D2 | D0);
	CharTable[102][4] = (D0);
	//g
	CharTable[103][0] = (D3);
	CharTable[103][1] = (D6 | D4 | D2);
	CharTable[103][2] = (D6 | D4 | D2);
	CharTable[103][3] = (D6 | D4 | D2);
	CharTable[103][4] = (D5 | D4 | D3 | D2);
	//h
	CharTable[104][0] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[104][1] = (D3);
	CharTable[104][2] = (D2);
	CharTable[104][3] = (D2);
	CharTable[104][4] = (D6 | D5 | D4 | D3);
	//i
	CharTable[105][1] = (D6 | D2);
	CharTable[105][2] = (D6 | D5 | D4 | D3 | D2 | D0);
	CharTable[105][3] = (D6);
	//j
	CharTable[106][0] = (D5);
	CharTable[106][1] = (D6);
	CharTable[106][2] = (D6 | D0);
	CharTable[106][3] = (D5 | D4 | D3 | D2 | D1 | D0);
	//k	
	CharTable[107][1] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[107][2] = (D4);
	CharTable[107][3] = (D5 | D3);
	CharTable[107][4] = (D6 | D2);
	//l	
	CharTable[108][1] = (D6 | D0);
	CharTable[108][2] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[108][3] = (D6);
	//m
	CharTable[109][0] = (D6 | D5 | D4 | D3 | D2);
	CharTable[109][1] = (D2);
	CharTable[109][2] = (D5 | D4 | D3);
	CharTable[109][3] = (D2);
	CharTable[109][4] = (D6 | D5 | D4 | D3);
	//n
	CharTable[110][0] = (D6 | D5 | D4 | D3 | D2);
	CharTable[110][1] = (D3);
	CharTable[110][2] = (D2);
	CharTable[110][3] = (D2);
	CharTable[110][4] = (D6 | D5 | D4 | D3);
	//o
	CharTable[111][0] = (D5 | D4 | D3);
	CharTable[111][1] = (D6 | D2);
	CharTable[111][2] = (D6 | D2);
	CharTable[111][3] = (D6 | D2);
	CharTable[111][4] = (D5 | D4 | D3);


	//p
	CharTable[112][0] = (D6 | D5 | D4 | D3 | D2);
	CharTable[112][1] = (D4 | D2);
	CharTable[112][2] = (D4 | D2);
	CharTable[112][3] = (D4 | D2);
	CharTable[112][4] = (D3);
	//q
	CharTable[113][0] = (D3);
	CharTable[113][1] = (D4 | D2);
	CharTable[113][2] = (D4 | D2);;
	CharTable[113][3] = (D4 | D2);;
	CharTable[113][4] = (D6 | D5 | D4 | D3 | D2);
	//r
	CharTable[114][0] = (D6 | D5 | D4 | D3 | D2);
	CharTable[114][1] = (D3);
	CharTable[114][2] = (D2);
	CharTable[114][3] = (D2);
	CharTable[114][4] = (D3);
	//s
	CharTable[115][0] = (D6 | D3);
	CharTable[115][1] = (D6 | D4 | D2);
	CharTable[115][2] = (D6 | D4 | D2);
	CharTable[115][3] = (D6 | D4 | D2);
	CharTable[115][4] = (D5 | D2);
	//t
	CharTable[116][0] = (D2);
	CharTable[116][1] = (D2);
	CharTable[116][2] = (D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[116][3] = (D6 | D2);
	CharTable[116][4] = (D5 | D2);
	//u
	CharTable[117][0] = (D5 | D4 | D3 | D2);
	CharTable[117][1] = (D6);
	CharTable[117][2] = (D6);
	CharTable[117][3] = (D5);
	CharTable[117][4] = (D6 | D5 | D4 | D3 | D2);
	//v
	CharTable[118][0] = (D4 | D3 | D2);
	CharTable[118][1] = (D5);
	CharTable[118][2] = (D6);
	CharTable[118][3] = (D5);
	CharTable[118][4] = (D4 | D3 | D2);
	//w
	CharTable[119][0] = (D5 | D4 | D3 | D2);
	CharTable[119][1] = (D6);
	CharTable[119][2] = (D5 | D4);
	CharTable[119][3] = (D6);
	CharTable[119][4] = (D5 | D4 | D3 | D2);
	//x
	CharTable[120][0] = (D6 | D2);
	CharTable[120][1] = (D5 | D3);
	CharTable[120][2] = (D4);
	CharTable[120][3] = (D5 | D3);
	CharTable[120][4] = (D6 | D2);
	//y
	CharTable[121][0] = (D3 | D2);
	CharTable[121][1] = (D6 | D4);
	CharTable[121][2] = (D6 | D4);
	CharTable[121][3] = (D6 | D4);
	CharTable[121][4] = (D5 | D4 | D3 | D2);
	//z
	CharTable[122][0] = (D6 | D2 | D1 | D0);
	CharTable[122][1] = (D6 | D5 | D2);
	CharTable[122][2] = (D6 | D4 | D2);
	CharTable[122][3] = (D6 | D3 | D2);
	CharTable[122][4] = (D6 | D2);
	//{
	CharTable[123][0] = (D3);
	CharTable[123][1] = (D5 | D4 | D2 | D1);
	CharTable[123][2] = (D6 | D0);
	CharTable[123][3] = (D6 | D0);
	//|	
	CharTable[124][2] = (D6 | D5 | D4 | D2 | D1 | D0);

	//}
	CharTable[125][1] = (D6 | D0);
	CharTable[125][2] = (D6 | D0);
	CharTable[125][3] = (D5 | D4 | D2 | D1);
	CharTable[125][4] = (D3);
	//~	
	CharTable[126][0] = (D1);
	CharTable[126][1] = (D0);
	CharTable[126][2] = (D1 | D0);
	CharTable[126][3] = (D1);
	CharTable[126][4] = (D0);

	//Chequer board
	CharTable[127][0] = (D6 | D4 | D2 | D0);
	CharTable[127][1] = (D5 | D3 | D1);
	CharTable[127][2] = (D6 | D4 | D2 | D0);
	CharTable[127][3] = (D5 | D3 | D1);
	CharTable[127][4] = (D6 | D4 | D2 | D0);

	// 
	CharTable[156][0] = (D6 | D3);
	CharTable[156][1] = (D6 | D5 | D4 | D3 | D2 | D1);
	CharTable[156][2] = (D6 | D3 | D0);
	CharTable[156][3] = (D6 | D3 | D0);
	CharTable[156][4] = (D6 | D3 | D1);

	//Space
	/*
	CharTable[9][0] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[9][1] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[9][2] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[9][3] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	CharTable[9][4] = (D6 | D5 | D4 | D3 | D2 | D1 | D0);
	*/



}