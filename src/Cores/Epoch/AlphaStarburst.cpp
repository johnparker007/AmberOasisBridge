// ###########################################################################
// #
// # AlphaStarburst - Starburst / segmented alpha display emulation
// # Split from DeviceAlpha
// #
// ###########################################################################

#include "stdafx.h"
#include "AlphaStarburst.h"

void __fastcall AlphaStarburst::DoChar(UINT8 ch) {
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

void __fastcall AlphaStarburst::Enable(UINT8 enable) {
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

void __fastcall AlphaStarburst::Reset(void) {
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

AlphaStarburst::~AlphaStarburst() {
}

UINT8 AlphaStarburst::GetAlphaDotComma(char SegNum) {
	if ((SegNum < 0) || (SegNum >= 16)) {
		return 0;
	}
	UINT8 ret;
	ret = (Chars[(UINT8)SegNum] >> 8);
	return ret;
}

UINT8 AlphaStarburst::GetAlphaChar(char num) {
	UINT8 ret;
	ret = GetArduinoCharacter(num);
	return ret;
}

int AlphaStarburst::GetAlphaSegments(char SegNum) {

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
	case 37: ret = 30617;	break;//%
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

UINT8 AlphaStarburst::GetArduinoCharacter(char SegNum) {

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

UINT8 AlphaStarburst::GetAlphaCharacter(char SegNum) {

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

AlphaStarburst::AlphaStarburst() {
	Reset();
	for (int loop = 0; loop < 16; loop++) {
		Chars[loop] = 0;
		OutChars[loop] = 0;
	}
}
