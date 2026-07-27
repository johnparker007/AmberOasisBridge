// ###########################################################################
// #
// # AlphaDotMatrix - Samsung 16L102DA4 dot-matrix alpha display emulation
// # Split from DeviceAlpha
// #
// ###########################################################################

#include "stdafx.h"
#include "AlphaDotMatrix.h"

#define D6 64
#define D5 32
#define D4 16
#define D3 8
#define D2 4
#define D1 2
#define D0 1

#define MODE_NORMAL 0
#define MODE_CUSTOM_CHAR 1
#define MODE_DOT_COMMA 2

UINT8 __fastcall AlphaDotMatrix::GetAlphaDots(char CharNum, char ColumnNum) {

	// Bounds protect the renderer against corrupt or transient cursor/data values.
	if ((CharNum < 0) || (CharNum >= 16) || (ColumnNum < 0) || (ColumnNum >= 5)) {
		return 0;
	}

	if (fSegOnOff)
	{
		return CharTable[CharacterBuffer[(UINT8)CharNum]][(UINT8)ColumnNum];
	}
	else
	{
		return 0x0;
	}	
}

UINT8 __fastcall AlphaDotMatrix::GetIntensity()
{	
	return fIntensity;	
}

void __fastcall AlphaDotMatrix::WriteOutput0(UINT8 value) {
	// The Samsung 16L102DA4 receives data LSB-first. Shift one bit on
	// the rising edge of bit 1.
	
	fByteLatch = SwapBits(value);
	LatchFull = true;
}

void __fastcall AlphaDotMatrix::WriteOutput1(UINT8 value) {

	// Epoch SDK mapping:
	//   FE1001 bit 0 = alpha inter-character strobe / select
	//   FE1001 bit 1 = alpha reset line

	fSel = (value & 1);
	fReset = (value & 2);

	if ((fPrevSel != 0) && (fSel == 0))
	{
		SelPulsed = true;
		CharCodeWriteIn = false;
	}
	else
	{
		SelPulsed = false;
	}

	fPrevSel = (value & 1);

	if (fReset)
	{
		//Reset();
	}

	//Dot Matrix Alphanumeric Display
	
}


void __fastcall AlphaDotMatrix::RunDotAlpha()
{
	if (fSel == 0)
	{
		if (LatchFull)
		{
			if (SelPulsed)
			{
				ProcessInstruction(fByteLatch);
				SelPulsed = false;
			}
			else if (CharCodeWriteIn)
			{
				ProcessChar(fByteLatch);
			}
			else
			{
				ProcessInstruction(fByteLatch);
			}
			LatchFull = false;			
		}
	}

}

UINT8 __fastcall AlphaDotMatrix::SwapBits(UINT8 ch) {

	// Backwards-compatible immediate byte path. Older emulator code passed a
	// physically packed LSB-first byte here, so preserve the historical bit swap.
	UINT8 Swapped = 0;
	if (ch & 0x1) Swapped |= 0x80;
	if (ch & 0x2) Swapped |= 0x40;
	if (ch & 0x4) Swapped |= 0x20;
	if (ch & 0x8) Swapped |= 0x10;
	if (ch & 0x10) Swapped |= 0x8;
	if (ch & 0x20) Swapped |= 0x4;
	if (ch & 0x40) Swapped |= 0x2;
	if (ch & 0x80) Swapped |= 0x1;
	return Swapped;
}

void __fastcall AlphaDotMatrix::ProcessChar(UINT8 ch)
{	
	//Normal Character Entry Mode
	
	//Protect Bad fCharPos
	if (fCharPos >= fNumDigits) {
		fCharPos = 0;
	}

	//Set Character
 	CharacterBuffer[fCharPos] = (ch);
	DotCommaBuffer[fCharPos] = fDotCommaSetting;

	//Auto Increment Pos
	fCharPos++;
	if (fCharPos >= fNumDigits) {
		fCharPos = 0;
	}	
}

void __fastcall AlphaDotMatrix::ProcessInstruction(UINT8 ch)
{
	if (fMode == MODE_CUSTOM_CHAR)
	{
		//Other Bytes, Lowest 7 Bits Define 1 vertical line of character, MSB unused
		//Protect Array. CharTable is rendered as 5 columns, so avoid column 5 overwrite.
		if ((fRamCharSelect < 256) && (fCustomByteCount < 5)) {
			//Set Ram
			CharTable[fRamCharSelect][fCustomByteCount] = (ch & 0x7f);
		}
		//Increment Byte Count
		fCustomByteCount++;
		if (fCustomByteCount == 5)
		{
			fMode = MODE_NORMAL;
		}
		return;
	}
	else if (fMode == MODE_DOT_COMMA)
	{
		fDotCommaSetting = (ch & 3);
		DotCommaBuffer[fCharPos] = fDotCommaSetting;
		fMode = MODE_NORMAL;

		return;
	}

	switch (ch & 0xf0) {
	case 0x10://Set Display Position (1 Byte)
		//Set Position
		fCharPos = (ch & 0xf);
		fDotCommaSetting = 0;
		//Set Enter Chars. Make entry modes explicit/mutually exclusive.
		CharCodeWriteIn = true;
		return;
	case 0x20://User Definable Font (6 bytes)		
		//First Byte Sets RAM Char Select
		fRamCharSelect = (ch & 0x7);
		//Set Enter Ram Flag. Make entry modes explicit/mutually exclusive.	
		fMode = MODE_CUSTOM_CHAR;
		fCustomByteCount = 0;
		fDotCommaSetting = 0;		
		return;
	case 0x30://Comma And/Or Decimal Point On/Off (Multi Bytes)		
		//First Byte Set Position		
		fCharPos = (ch & 0xf);
		fMode = MODE_DOT_COMMA;
		return;
	case 0x50://Luminence Control (Dimming) (1 Byte)
		//Set Intensity
		fIntensity = (ch & 0x7);
		fDotCommaSetting = 0;
		return;
	case 0x60://Digit Length Set (1 Byte)
		//Set Number of Digits
		switch (ch & 7) {
		case 0: fNumDigits = 16; break;
		case 1: fNumDigits = 9;  break;
		case 2: fNumDigits = 10; break;
		case 3: fNumDigits = 11; break;
		case 4: fNumDigits = 12; break;
		case 5: fNumDigits = 13; break;
		case 6: fNumDigits = 14; break;
		case 7: fNumDigits = 15; break;
		}
		if (fCharPos >= fNumDigits) {
			fCharPos = 0;
		}
		fDotCommaSetting = 0;
		return;
	case 0x70://All Segments On/Off (1 Byte)
		//Set Segments On/Off
		switch (ch & 3) {
		case 1:
			fSegOnOff = false;
			break;
		case 2:
		case 3:
			fSegOnOff = true;
			break;
		}
		
		fDotCommaSetting = 0;
		return;	
	}	
}

void __fastcall AlphaDotMatrix::Reset(void) {

	fCharPos = 0;		// Start cursor
	fNumDigits = 16;		// Reset Digit Count
	fIntensity = 0;		// Reset Brightness
	fCustomByteCount = 0;		// Reset UDRAM byte counter
	fDotCommaSetting = 0;
	fMode = 0;	
	fReset = 0;	
	fRamCharSelect = 0;	
	fSel = 0;
	fPrevSel = 0;	
	fByteLatch = 0;
	fSegOnOff = true;

	// Reset display RAM
	for (UINT8 count = 0; count < 16; count++) {
		DotCommaBuffer[count] = 0;
		CharacterBuffer[count] = 0x20;
	}
}

AlphaDotMatrix::~AlphaDotMatrix() {
}

UINT8 AlphaDotMatrix::GetAlphaDotComma(char SegNum) {
	if ((SegNum < 0) || (SegNum >= 16)) {
		return 0;
	}
	UINT8 ret;
	ret = (DotCommaBuffer[(UINT8)SegNum]);
	return ret;
}

AlphaDotMatrix::AlphaDotMatrix() {
	for (int loop = 0; loop < 128; loop++) {
		for (int loop2 = 0; loop2 < 5; loop2++) {
			CharTable[loop][loop2] = 0;
		}
	}
	for (int loop = 128; loop < 256; loop++) {
		// Mark unknown as ?
		CharTable[loop][0] = (D1);
		CharTable[loop][1] = (D0);
		CharTable[loop][2] = (D6 | D4 | D0);
		CharTable[loop][3] = (D3 | D0);
		CharTable[loop][4] = (D2 | D1);
	}

	for (int loop = 0; loop < 16; loop++) {
		CharacterBuffer[loop] = 0;
		DotCommaBuffer[loop] = 0;
	}

	Reset();

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

	// £
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
