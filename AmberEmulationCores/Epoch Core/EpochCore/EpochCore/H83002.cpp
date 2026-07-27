// ###########################################################################
// #
// # h83002 - H8/3002 CPU Emulation Core
// # Copyright (C) 2002-2010 Tony Friery [DialTone]
// #
// # ALL RIGHTS RESERVED
// #
// # Based on a version originally by The_Author and DynaChicken
// # for the ZiNc emulator, subsequently bug-fixed and ported to
// # MAME by R Belmont
// #
// ###########################################################################
#include "stdafx.h"
#include <string.h>
#include "h83002.h"
#include "stdlib.h"
#include <stdio.h>
#include <stdarg.h>

#define H8_SP	7
#define DASMADDRESSSTART 0
#define DASMOP1START 11
#define DASMOP2START 16
#define DASMOP3START 21
#define DASMOP4START 26
#define DASMOP5START 31
#define DASMMNEMONICSTART 39
#define DASMADDRESSMODESTART 48
#define DASMCYCLESSTART 65
#define DASMTOTALCYCLESSTART 69


// timing macros
// note: we assume a system 12 - type setup where external access is 3+1 states
// timing will be off somewhat for other configurations.

#define H8_IFETCH_TIMING(x)		h8_cyccnt += (x) * 4;
#define H8_BRANCH_TIMING(x)		h8_cyccnt += (x) * 4;
#define H8_STACK_TIMING(x)		h8_cyccnt += (x) * 4;
#define H8_BYTE_TIMING(x, adr)	if (adr >= 0xffff10) h8_cyccnt += (x * 3); else h8_cyccnt += (x * 4);
#define H8_WORD_TIMING(x, adr)	if (adr >= 0xffff10) h8_cyccnt += (x * 3); else h8_cyccnt += (x * 4);

static void H8IrqTrace(const char* fmt, ...)
{
	char buffer[512];
	va_list args;
	va_start(args, fmt);
	vsprintf_s(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	OutputDebugStringA(buffer);
}
#define H8_IOP_TIMING(x)		h8_cyccnt += (x);

#define DASM_IFETCH_TIMING(x)		dInCycles += (x) * 4;
#define DASM_BRANCH_TIMING(x)		dInCycles += (x) * 4;
#define DASM_STACK_TIMING(x)		dInCycles += (x) * 4;
#define DASM_BYTE_TIMING(x, adr)	if (adr >= 0xffff10) dInCycles += (x * 3); else dInCycles += (x * 4);
#define DASM_WORD_TIMING(x, adr)	if (adr >= 0xffff10) dInCycles += (x * 3); else dInCycles += (x * 4);
#define DASM_IOP_TIMING(x)			dInCycles += (x);


UINT8 H83002::h8_mem_read8(UINT32 address) {

	UINT8 ret;
	if (address > 0xffffff) {
		h8.err = 96;
	}

	ret = program_read_byte(address);
	return ret;

}

UINT16 H83002::h8_mem_read16(UINT32 address) {

	UINT16 ret;
	if (address > 0xffffff) {
		h8.err = 97;
	}
	ret = program_read_word(address);
	return ret;

}

void H83002::h8_mem_write8(UINT32 address, UINT8 val) {
	if (address > 0xffffff) {
		h8.err = 98;
	}
	program_write_byte(address, val);
}

void H83002::h8_mem_write16(UINT32 address, UINT16 val) {
	if (address > 0xffffff) {
		h8.err = 99;
	}
	program_write_word(address, val);
}

//Handling
void H83002::dasmCreateEntry() 
{
	dStrings[dCurrent] = (wchar_t *)malloc(DASMSTRINGLENGTH * sizeof(wchar_t));
	if (dStrings[dCurrent]) {		
		dCurrent++;
	}
	else {
		//ALLOCATION ERROR!!
	}
	
}
void H83002::dasmDestroyEntries()
{
	for (int i = 0; i < DASMMAXSTRINGS; i++)
	if (dStrings[i]) {
		free(dStrings[i]);
	}
}

//Output string functions
void H83002::setDasmOpcode(wchar_t* str)
{
	int len = lstrlenW(str);
	if (len > 0) {	
		for (int i = 0; i < len; i++) {
			dStrings[dCurrent][DASMMNEMONICSTART + i] = str[i];
		}
	}
}
void H83002::setDasmOpcodeBytes(UINT8 num, UINT16 op1, UINT16 op2, UINT16 op3, UINT16 op4, UINT16 op5)
{	
	wchar_t Number[9];

	_itow_s(op1, Number, sizeof(Number) / 2, 16);
	for (int i = 0; i < 4; i++) {
		dStrings[dCurrent][DASMOP1START + i] = Number[i];
	}

	if (num > 1)
	{
		_itow_s(op2, Number, sizeof(Number) / 2, 16);
		for (int i = 0; i < 4; i++) {
			dStrings[dCurrent][DASMOP2START + i] = Number[i];
		}
	}
	if (num > 2)
	{
		_itow_s(op3, Number, sizeof(Number) / 2, 16);
		for (int i = 0; i < 4; i++) {
			dStrings[dCurrent][DASMOP3START + i] = Number[i];
		}
	}
	if (num > 3)
	{
		_itow_s(op4, Number, sizeof(Number) / 2, 16);
		for (int i = 0; i < 4; i++) {
			dStrings[dCurrent][DASMOP4START + i] = Number[i];
		}
	}
	if (num > 4)
	{
		_itow_s(op5, Number, sizeof(Number) / 2, 16);
		for (int i = 0; i < 4; i++) {
			dStrings[dCurrent][DASMOP5START + i] = Number[i];
		}
	}
}

void H83002::setDasmAddress(UINT32 addr)
{
	wchar_t Number[9];
	
	_itow_s(addr, Number, sizeof(Number) / 2, 16);
	int len = lstrlenW(Number);

	for (int i = 0; i < 8; i++) {	
		dStrings[dCurrent][DASMADDRESSSTART + i] = L'0';
	}
	dStrings[dCurrent][DASMADDRESSSTART + 1] = L'x';
	int cnt = (len - 1);
	for (int i = 8; i > (8 - len); i--) {
		dStrings[dCurrent][DASMADDRESSSTART + i] = Number[cnt];
		cnt--;
	}
}

void H83002::setDasmAddressMode(wchar_t* str)
{
	int len = lstrlenW(str);
	if (len > 0) {
		for (int i = 0; i < len; i++) {
			if (str[i] != 0) {
				dStrings[dCurrent][DASMADDRESSMODESTART + i] = str[i];
			}
		}
	}
}

void H83002::setDasmInstructionCycles(UINT32 cycles)
{
	wchar_t Number[9];
	_itow_s(cycles, Number, sizeof(Number) / 2, 10);
	int len = lstrlenW(Number);
	for (int i = 0; i < len; i++) {
		if (Number[i] != 0) {
			dStrings[dCurrent][DASMCYCLESSTART + i] = Number[i];
		}
	}
}

void H83002::setDasmTotalCycles(UINT32 cycles)
{
	dCycles += cycles;
	wchar_t Number[16];
	_itow_s(dCycles, Number, sizeof(Number) / 2, 10);
	int len = lstrlenW(Number);
	for (int i = 0; i < len; i++) {
		dStrings[dCurrent][DASMTOTALCYCLESSTART + i] = Number[i];
	}
}

void H83002::dasmClearLine() {
	for (int i = 0; i < DASMSTRINGLENGTH; i++) {
		dStrings[dCurrent][i] = L' ';
	}
	dStrings[dCurrent][DASMSTRINGLENGTH - 1] = NULL;
}

//Options
void H83002::setDasmTermination(UINT8 term) 
{
	dTerminator = term;
}
void H83002::setDasmSize(UINT32 size)
{
	//Check not greater than available slots
	dSize = size;
	if (dSize > DASMMAXSTRINGS) {
		dSize = DASMMAXSTRINGS;
	}
}

void H83002::dasmInitialize(UINT8 Terminator, UINT32 Size) 
{
	ZeroMemory(dStrings, DASMMAXSTRINGS * sizeof(char*));
	//Set size
	setDasmSize(Size);
	//Set terminator option
	setDasmTermination(Terminator);
	//Reset Count
	dCurrent = 0;

	//Allocate all strings
	if (dSize) {
		for (UINT32 i = 0; (i < dSize); i++) {
			dasmCreateEntry();			
		}
	}

}

void H83002::dasmExecute(UINT32 address, UINT32 numIns, UINT32 totalCycles){
		
	/*

	dasmPC = (address & 0xffffff);
	dCycles = totalCycles;
	dCurrent = 0;

	for (UINT32 currentInstruction = 0; currentInstruction < numIns; currentInstruction++){

		dInCycles = 0;		

		opcode = cpu_readop16(dasmPC);
		dasmPC += 2;

		AH = ((opcode >> 12) & 0xf);
		AL = ((opcode >> 8) & 0xf);
		BH = ((opcode >> 4) & 0xf);
		BL = (opcode & 0xf);


		switch (AH)
		{
		case 0x0:
			// 0n nn
			dasmGroup0();
			break;
		case 0x1:
			// 1n nn
			dasmGroup1();
			break;
		case 0x2:
			// 2n nn
			// mov.b @aa:8,Rd //Timing Checked			
			DASM_IFETCH_TIMING(1);
			DASM_BYTE_TIMING(1, (0xffff00 | (opcode & 0xff)));
			dasmClearLine();
			setDasmOpcode(L"mov.b");				
			setDasmAddressMode(L"@aa:8,Rd");
			setDasmOpcodeBytes(1, opcode);			
			setDasmAddress(dasmPC);
			//Must Call Timing Macro's before this!
			setDasmInstructionCycles(dInCycles);
			setDasmTotalCycles(dCycles);		
			break;
		case 0x3:
			// 3n nn
			// mov.b Rs,@aa:8 //Timing Checked
			DASM_IFETCH_TIMING(1);
			DASM_BYTE_TIMING(1, (0xffff00 | (opcode & 0xff)));
			dasmClearLine();
			setDasmOpcode(L"mov.b");
			setDasmAddressMode(L"Rs, @aa:8");
			setDasmOpcodeBytes(1, opcode);
			setDasmAddress(dasmPC);
			//Must Call Timing Macro's before this!
			setDasmInstructionCycles(dInCycles);
			setDasmTotalCycles(dCycles);
			break;
		case 0x4:
			// 4n nn
			// branch @xx:8 //Timing Checked			
			DASM_IFETCH_TIMING(2);
			dasmClearLine();
			setDasmOpcode(L"branch");
			setDasmAddressMode(L"@xx:8");
			setDasmOpcodeBytes(1, opcode);
			setDasmAddress(dasmPC);
			//Must Call Timing Macro's before this!
			setDasmInstructionCycles(dInCycles);
			setDasmTotalCycles(dCycles);
			break;
		case 0x5:
			// 5n nn
			dasmGroup5();
			break;
		case 0x6:
			// 6n nn
			dasmGroup6();
			break;
		case 0x7:
			// 7n nn
			dasmGroup7();
			break;
		case 0x8:
			// 8n nn
			// add.b #xx:8, Rd //Timing Checked
			DASM_IFETCH_TIMING(1);
			dasmClearLine();
			setDasmOpcode(L"add.b");
			setDasmAddressMode(L"#xx:8, Rd");
			setDasmOpcodeBytes(1, opcode);
			setDasmAddress(dasmPC);
			//Must Call Timing Macro's before this!
			setDasmInstructionCycles(dInCycles);
			setDasmTotalCycles(dCycles);
			break;
		case 0x9:
			// 9n nn
			// addx.b #xx:8, Rd //Timing Checked			
			DASM_IFETCH_TIMING(1);
			dasmClearLine();
			setDasmOpcode(L"addx.b");
			setDasmAddressMode(L"#xx:8, Rd");
			setDasmOpcodeBytes(1, opcode);
			setDasmAddress(dasmPC);
			//Must Call Timing Macro's before this!
			setDasmInstructionCycles(dInCycles);
			setDasmTotalCycles(dCycles);
			break;
		case 0xa:
			// an nn
			// cmp.b #xx:8, Rd //Timing Checked
			DASM_IFETCH_TIMING(1);
			dasmClearLine();
			setDasmOpcode(L"cmp.b");
			setDasmAddressMode(L"#xx:8, Rd");
			setDasmOpcodeBytes(1, opcode);
			setDasmAddress(dasmPC);
			//Must Call Timing Macro's before this!
			setDasmInstructionCycles(dInCycles);
			setDasmTotalCycles(dCycles);
			break;
		case 0xb:
			// bn nn
			// subx.b #xx:8, Rd //Timing Checked
			DASM_IFETCH_TIMING(1);
			dasmClearLine();
			setDasmOpcode(L"sub.b");
			setDasmAddressMode(L"#xx:8, Rd");
			setDasmOpcodeBytes(1, opcode);
			setDasmAddress(dasmPC);
			//Must Call Timing Macro's before this!
			setDasmInstructionCycles(dInCycles);
			setDasmTotalCycles(dCycles);
			break;
		case 0xc:
			// cn nn
			// or.b #xx:8, Rd //Timing Checked
			DASM_IFETCH_TIMING(1);
			dasmClearLine();
			setDasmOpcode(L"or.b");
			setDasmAddressMode(L"#xx:8, Rd");
			setDasmOpcodeBytes(1, opcode);
			setDasmAddress(dasmPC);
			//Must Call Timing Macro's before this!
			setDasmInstructionCycles(dInCycles);
			setDasmTotalCycles(dCycles);
			break;
		case 0xd:
			// dn nn
			// xor.b #xx:8, Rd //Timing Checked
			DASM_IFETCH_TIMING(1);
			dasmClearLine();
			setDasmOpcode(L"xor.b");
			setDasmAddressMode(L"#xx:8, Rd");
			setDasmOpcodeBytes(1, opcode);
			setDasmAddress(dasmPC);
			//Must Call Timing Macro's before this!
			setDasmInstructionCycles(dInCycles);
			setDasmTotalCycles(dCycles);
			break;
		case 0xe:
			// en nn
			// and.b #xx:8, Rd //Timing Checked
			DASM_IFETCH_TIMING(1);
			dasmClearLine();
			setDasmOpcode(L"and.b");
			setDasmAddressMode(L"#xx:8, Rd");
			setDasmOpcodeBytes(1, opcode);
			setDasmAddress(dasmPC);
			//Must Call Timing Macro's before this!
			setDasmInstructionCycles(dInCycles);
			setDasmTotalCycles(dCycles);
			break;
		case 0xf:
			// fn nn
			// mov.b #xx:8, Rd //Timing Checked
			DASM_IFETCH_TIMING(1);
			dasmClearLine();
			setDasmOpcode(L"mov.b");
			setDasmAddressMode(L"#xx:8, Rd");
			setDasmOpcodeBytes(1, opcode);
			setDasmAddress(dasmPC);
			//Must Call Timing Macro's before this!
			setDasmInstructionCycles(dInCycles);
			setDasmTotalCycles(dCycles);
			break;
		}
		dCurrent++;
	}
	*/
}

void H83002::dasmGroup0()
{
	// Deal with all 0n nn opcodes
	/*
	switch (AL)
	{
	case 0x0:
		// nop
		DASM_IFETCH_TIMING(1);
		dasmClearLine();
		setDasmOpcode(L"nop");
		setDasmAddressMode(L"");
		setDasmOpcodeBytes(1, opcode);
		setDasmAddress(dasmPC);
		//Must Call Timing Macro's before this!
		setDasmInstructionCycles(dInCycles);
		setDasmTotalCycles(dCycles);
		break;
	case 0x1:
		// 01nx  where x should always be 0!
		if ((opcode & 0xf) != 0)
		{
			h8.h8err = 9;
			break;
		}

		switch ((opcode >> 4) & 0xf)
		{
			// 01f0 prefix
		case 0xf:
			opcode2 = h8_mem_read16(h8.pc);
			dasmPC += 2;

			if (!(opcode2 & 0x88))
			{
				switch ((opcode2 >> 8) & 0xff)
				{
				case 0x64:
					// or.l ERs,ERd //Timing Checked
					DASM_IFETCH_TIMING(2);
					dasmClearLine();
					setDasmOpcode(L"or.l");
					setDasmAddressMode(L"ERs,ERd");
					setDasmOpcodeBytes(2, opcode, opcode2);
					setDasmAddress(dasmPC);
					//Must Call Timing Macro's before this!
					setDasmInstructionCycles(dInCycles);
					setDasmTotalCycles(dCycles);
					break;
				case 0x65:
					// xor.l ERs,ERd //Timing Checked
					DASM_IFETCH_TIMING(2);
					dasmClearLine();
					setDasmOpcode(L"xor.l");
					setDasmAddressMode(L"ERs,ERd");
					setDasmOpcodeBytes(2, opcode, opcode2);
					setDasmAddress(dasmPC);
					//Must Call Timing Macro's before this!
					setDasmInstructionCycles(dInCycles);
					setDasmTotalCycles(dCycles);
					break;
				case 0x66:
					// and.l ERs,ERd //Timing Checked
					DASM_IFETCH_TIMING(2);
					dasmClearLine();
					setDasmOpcode(L"and.l");
					setDasmAddressMode(L"ERs,ERd");
					setDasmOpcodeBytes(2, opcode, opcode2);
					setDasmAddress(dasmPC);
					//Must Call Timing Macro's before this!
					setDasmInstructionCycles(dInCycles);
					setDasmTotalCycles(dCycles);
					break;
				default:
					h8.h8err = 10;
					break;
				}
			}
			else
			{
				// Invalid register values
				h8.h8err = 11;
			}

			break;
			// 0100 prefix
		case 0:
			opcode2 = h8_mem_read16(h8.pc);
			h8.pc += 2;

			switch ((opcode2 >> 8) & 0xff)
			{
			case 0x69:
				if ((opcode2 & 0x80) == 0x80)
				{
					// mov.l ERs,@ERd //Timing Checked
					DASM_WORD_TIMING(2, h8_getreg32((opcode2 >> 4) & 7));
					DASM_IFETCH_TIMING(2);
					dasmClearLine();
					setDasmOpcode(L"mov.l");
					setDasmAddressMode(L"ERs,@ERd");
					setDasmOpcodeBytes(2, opcode, opcode2);
					setDasmAddress(dasmPC);
					//Must Call Timing Macro's before this!
					setDasmInstructionCycles(dInCycles);
					setDasmTotalCycles(dCycles);
				}
				else
				{
					// mov.l @ERs,ERd //Timing Checked
					DASM_WORD_TIMING(2, h8_getreg32((opcode2 >> 4) & 7));
					DASM_IFETCH_TIMING(2);
					dasmClearLine();
					setDasmOpcode(L"mov.l");
					setDasmAddressMode(L"@ERs,ERd");
					setDasmOpcodeBytes(2, opcode, opcode2);
					setDasmAddress(dasmPC);
					//Must Call Timing Macro's before this!
					setDasmInstructionCycles(dInCycles);
					setDasmTotalCycles(dCycles);
				}
				break;
			case 0x6b:
				switch ((ext16 >> 4) & 0xf)
				{
				case 0x0:
					// mov.l @aa:16, ERd //Listed in docs but wasn't implemented (doesn't appear to be used though)
					address24 = h8_mem_read16(dasmPC);
					dasmPC += 2;
					if (address24 & 0x8000)
					{
						address24 |= 0xff0000;
					}					
					DASM_IFETCH_TIMING(3);
					DASM_WORD_TIMING(2, (address24 & 0xffff));
					dasmClearLine();
					setDasmOpcode(L"mov.l");
					setDasmAddressMode(L"@aa:16, ERd");
					setDasmOpcodeBytes(2, opcode, (address24 & 0xffff));
					setDasmAddress(dasmPC);
					//Must Call Timing Macro's before this!
					setDasmInstructionCycles(dInCycles);
					setDasmTotalCycles(dCycles);
					break;
				case 0x2:
					// mov.l @aa:24,ERd //Timing Checked
					address24 = h8_mem_read32(dasmPC);
					dasmPC += 4;
					DASM_IFETCH_TIMING(4);
					DASM_WORD_TIMING(2, address24);
					dasmClearLine();
					setDasmOpcode(L"mov.l");
					setDasmAddressMode(L"@aa:24,ERd");
					setDasmOpcodeBytes(3, opcode, address24 >> 16, address24 & 0xffff);
					setDasmAddress(dasmPC);
					//Must Call Timing Macro's before this!
					setDasmInstructionCycles(dInCycles);
					setDasmTotalCycles(dCycles);
					break;
				case 0x8:
					// mov.l ERs,@aa:16 //Timing Checked
					address24 = h8_mem_read16(dasmPC);
					dasmPC += 2;
					if (address24 & 0x8000)
					{
						address24 |= 0xff0000;
					}
					DASM_IFETCH_TIMING(3);
					DASM_WORD_TIMING(2, (address24 & 0xffff));
					dasmClearLine();
					setDasmOpcode(L"mov.l");
					setDasmAddressMode(L"ERs, @aa:16");
					setDasmOpcodeBytes(2, opcode, (address24 & 0xffff));
					setDasmAddress(dasmPC);
					//Must Call Timing Macro's before this!
					setDasmInstructionCycles(dInCycles);
					setDasmTotalCycles(dCycles);
					break;
				case 0xa:
					// mov.l ERs,@aa:24 //Timing Checked
					address24 = h8_mem_read32(dasmPC);
					dasmPC += 4;
					DASM_IFETCH_TIMING(4);
					DASM_WORD_TIMING(2, address24);
					dasmClearLine();
					setDasmOpcode(L"mov.l");
					setDasmAddressMode(L"ERs, @aa:24");
					setDasmOpcodeBytes(3, opcode, address24 >> 16, address24 & 0xffff);
					setDasmAddress(dasmPC);
					//Must Call Timing Macro's before this!
					setDasmInstructionCycles(dInCycles);
					setDasmTotalCycles(dCycles);
					break;
				default:
					h8.h8err = 12;
					break;
				}
				break;
			case 0x6d:
				if (opcode2 & 0x80)
				{
					// mov.l rs, @-erd //Timing Checked
					dstreg = (opcode2 >> 4) & 7;
					address24 = h8_getreg32(dstreg) & 0xffffff;
					DASM_IFETCH_TIMING(2);
					DASM_WORD_TIMING(2, address24);
					DASM_IOP_TIMING(2);
					dasmClearLine();
					setDasmOpcode(L"mov.l");
					setDasmAddressMode(L"rs, @-erd");
					setDasmOpcodeBytes(2, opcode, opcode2);
					setDasmAddress(dasmPC);
					//Must Call Timing Macro's before this!
					setDasmInstructionCycles(dInCycles);
					setDasmTotalCycles(dCycles);
				}
				else
				{
					// mov.l @ers+, rd //Timing Checked
					srcreg = (opcode2 >> 4) & 7;
					address24 = h8_getreg32(srcreg) & 0xffffff;
					DASM_IFETCH_TIMING(2);
					DASM_WORD_TIMING(2, address24);
					DASM_IOP_TIMING(2);
					dasmClearLine();
					setDasmOpcode(L"mov.l");
					setDasmAddressMode(L"@ers+, rd");
					setDasmOpcodeBytes(2, opcode, opcode2);
					setDasmAddress(dasmPC);
					//Must Call Timing Macro's before this!
					setDasmInstructionCycles(dInCycles);
					setDasmTotalCycles(dCycles);
				}				
				break;
			case 0x6f:
				// mov.l @(displ16 + Rs), rd //Timing Checked
				sdata16 = h8_mem_read16(dasmPC); // sign extend displacements !
				h8.pc += 2;
				address24 = (h8_getreg32((ext16 >> 4) & 7)) & 0xffffff;
				address24 += sdata16;
				DASM_IFETCH_TIMING(3);
				DASM_WORD_TIMING(2, address24);
				
				if (ext16 & 0x80)
				{
					// mov.l ERs, @(d:16, ERd)
					dasmClearLine();
					setDasmOpcode(L"mov.l");
					setDasmAddressMode(L"ERs, @(d:16, ERd)");
					setDasmOpcodeBytes(2, opcode, sdata16);
					setDasmAddress(dasmPC);
					//Must Call Timing Macro's before this!
					setDasmInstructionCycles(dInCycles);
					setDasmTotalCycles(dCycles);
				}
				else
				{
					// mov.l @(d:16, ERs), ERd
					dasmClearLine();
					setDasmOpcode(L"mov.l");
					setDasmAddressMode(L"@(d:16, ERs), ERd");
					setDasmOpcodeBytes(2, opcode, sdata16);
					setDasmAddress(dasmPC);
					//Must Call Timing Macro's before this!
					setDasmInstructionCycles(dInCycles);
					setDasmTotalCycles(dCycles);
				}				
				break;
			case 0x78:
				// prefix for
				// mov.l (@aa:x, rx), Rx
				srcreg = (opcode2 >> 4) & 7;
				// 6b20
				udata16 = h8_mem_read16(dasmPC);
				dasmPC += 2;
				address24 = h8_mem_read32(dasmPC);
				dasmPC += 4;
				address24 += h8_getreg32(srcreg);
				address24 &= 0xffffff;
				DASM_IFETCH_TIMING(5);
				DASM_WORD_TIMING(2, address24);
				if (opcode2 & 0x80) {
					dasmClearLine();
					setDasmOpcode(L"mov.l");
					setDasmAddressMode(L"ERs, @(d:24,ERd)");
					setDasmOpcodeBytes(5, opcode, opcode2, udata16, (address24 >> 16), (address24 & 0xffff));
					setDasmAddress(dasmPC);
				}
				else {
					dasmClearLine();
					setDasmOpcode(L"mov.l");
					setDasmAddressMode(L"@(d:24,ERs), ERd");
					setDasmOpcodeBytes(5, opcode, opcode2, udata16, (address24 >> 16), (address24 & 0xffff));
					setDasmAddress(dasmPC);
				}
				//Must Call Timing Macro's before this!
				setDasmInstructionCycles(dInCycles);
				setDasmTotalCycles(dCycles);
				break;
			default:
				h8.h8err = 14;
				break;
			}
			break;
		case 0xc:
			// mulxs
			opcode2 = h8_mem_read16(dasmPC);
			dasmPC += 2;

			if (((opcode2 >> 8) & 0xf) == 0)
			{
				// mulxs.b //Timing Checked
				DASM_IFETCH_TIMING(2);
				DASM_IOP_TIMING(12);
				dasmClearLine();
				setDasmOpcode(L"mulxs.b");
				setDasmAddressMode(L"");
				setDasmOpcodeBytes(2, opcode, opcode2);
				setDasmAddress(dasmPC);
				//Must Call Timing Macro's before this!
				setDasmInstructionCycles(dInCycles);
				setDasmTotalCycles(dCycles);
			}
			else if (((ext16 >> 8) & 0xf) == 2)
			{
				// mulxs.w //Timing Checked
				DASM_IFETCH_TIMING(2);
				DASM_IOP_TIMING(20);
				setDasmOpcode(L"mulxs.w");
				setDasmAddressMode(L"");
				setDasmOpcodeBytes(2, opcode, opcode2);
				setDasmAddress(dasmPC);
				//Must Call Timing Macro's before this!
				setDasmInstructionCycles(dInCycles);
				setDasmTotalCycles(dCycles);
			}
			else
			{
				// illop
				h8.h8err = 15;
			}
			break;
		case 0xd:
			// divxs - This may be buggy, esp. flags
			opcode2 = h8_mem_read16(dasmPC);
			dasmPC += 2;

			if (((opcode2 >> 8) & 0xf) == 3)
			{
				// DIVXS.W Rs, ERd  (ssss0ddd) //Timing Checked
				// ERd / Rs -> ERd
				// ERd = ER0 to ER7
				// Rs = R0 to R7, E0 to E7
				if ((opcode2 & 0x80) != 0)
				{
					// Invalid register for destination
					h8.h8err = 16;
				}
				else
				{
					DASM_IFETCH_TIMING(2);
					DASM_IOP_TIMING(20);
					dasmClearLine();
					setDasmOpcode(L"divxs.w");
					setDasmAddressMode(L"");
					setDasmOpcodeBytes(2, opcode, opcode2);
					setDasmAddress(dasmPC);
					//Must Call Timing Macro's before this!
					setDasmInstructionCycles(dInCycles);
					setDasmTotalCycles(dCycles);
				}
			}
			else if (((ext16 >> 8) & 0xf) == 1)
			{
				// DIVXS.B Rs, Rd (ssssdddd) //Timing Checked
				// Rd / Rs -> Rd
				// Rd = R0 to R7, E0 to E7
				// Rs = R0L to R7L, R0H to R7H
				DASM_IFETCH_TIMING(2);
				DASM_IOP_TIMING(12);
				dasmClearLine();
				setDasmOpcode(L"divxs.b");
				setDasmAddressMode(L"");
				setDasmOpcodeBytes(2, opcode, opcode2);
				setDasmAddress(dasmPC);
				//Must Call Timing Macro's before this!
				setDasmInstructionCycles(dInCycles);
				setDasmTotalCycles(dCycles);
			}
			else
			{
				h8.h8err = 17;
			}
			break;
		case 4: // LDC/STC

			//*** this section is incomplete, what is needed for epoch is here and working but be aware ***
			//*** there are a number of missing STC/LDC opcodes h8err has been set to catch ***
			ext16 = h8_mem_read16(h8.pc);
			h8.pc += 2;

			if (ext16 & 0x80)
			{
				switch ((ext16 >> 8) & 0xf)
				{
					// STC
				case 0x9: // stc .w rx, @rx
					h8.h8err = 18;
					break;
				case 0xb: // stc .w rx, @xx
					h8.h8err = 19;
					break;
				case 0xd: // stc.w CCR,@ERd //Timing Checked
					srcreg = (opcode2 >> 4) & 7;					
					address24 = h8_getreg32(srcreg) & 0xffffff;					
					H8_IFETCH_TIMING(2);
					H8_WORD_TIMING(1, address24);
					H8_IOP_TIMING(2);
					dasmClearLine();
					setDasmOpcode(L"stc.w");
					setDasmAddressMode(L"CCR, @ERd");
					setDasmOpcodeBytes(2, opcode, opcode2);
					setDasmAddress(dasmPC);
					//Must Call Timing Macro's before this!
					setDasmInstructionCycles(dInCycles);
					setDasmTotalCycles(dCycles);
					break;
				case 0xf: // stc .w @(displ16 + Rs), rd
					h8.h8err = 20;
					break;
				default:
					h8.h8err = 21;
					break;
				}
			}
			else
			{
				switch ((ext16 >> 8) & 0xf)
				{   // LDC
				case 0x9: // ldc.w @ERs,CCR
					h8.h8err = 22;
					break;
				case 0xb: // ldc.w @aa:16,CCR
					h8.h8err = 23;
					break;
				case 0xd: // ldc.w @ERs+,CCR //Timing Checked
					srcreg = (ext16 >> 4) & 7;
					address24 = h8_getreg32(srcreg) & 0xffffff;
					h8_setreg32(srcreg, h8_getreg32(srcreg) + 2);
					udata16 = h8_mem_read16(address24);
					h8_set_ccr8((udata16 & 0xff));
					H8_IFETCH_TIMING(1);
					H8_BYTE_TIMING(1, address24);
					H8_IOP_TIMING(2);
					break;
				case 0xf: // ldc.w @(d:16,ERs),CCR
					h8.h8err = 24;
					break;
				default:
					h8.h8err = 25;
					break;
				}
			}
			break;
		default:
			h8.h8err = 26;
			break;
		}
		break;
	case 0x2:
		// stc ccr, rd //Timing Checked
		if (((opcode >> 4) & 0xf) == 0)
		{
			h8_setreg8(opcode & 0xf, h8_get_ccr());
			H8_IFETCH_TIMING(1);
		}
		else
		{
			h8.h8err = 27;
		}
		break;
	case 0x3:
		// ldc rd, ccr //Timing Checked
		if (((opcode >> 4) & 0xf) == 0)
		{
			udata8 = h8_getreg8(opcode & 0xf);
			h8_set_ccr8(udata8);
			H8_IFETCH_TIMING(1);
		}
		else
		{
			h8.h8err = 28;
		}
		break;
	case 0x4:
		// orc //Timing Checked
		udata8 = h8_or8(opcode & 0xff, h8_get_ccr());
		h8_set_ccr8(udata8);
		H8_IFETCH_TIMING(1);
		break;
	case 0x5:
		// xorc //Timing Checked
		udata8 = h8_xor8(opcode & 0xff, h8_get_ccr());
		h8_set_ccr8(udata8);
		H8_IFETCH_TIMING(1);
		break;
	case 0x6:
		// andc //Timing Checked
		udata8 = h8_and8(opcode & 0xff, h8_get_ccr());
		h8_set_ccr8(udata8);
		H8_IFETCH_TIMING(1)
			break;
	case 0x7:
		// ldc //Timing Checked
		udata8 = opcode & 0xff;
		h8_mov8(udata8);
		h8_set_ccr8(udata8);
		H8_IFETCH_TIMING(1)
			break;
	case 0x8:
		// add.b rx, ry //Timing Checked
		dstreg = opcode & 0xf;
		udata8 = h8_add8(h8_getreg8((opcode >> 4) & 0xf), h8_getreg8(dstreg));
		h8_setreg8(dstreg, udata8);
		H8_IFETCH_TIMING(1)
			break;
	case 0x9:
		// add.w rx, ry //Timing Checked
		dstreg = opcode & 0xf;
		udata16 = h8_add16(h8_getreg16((opcode >> 4) & 0xf), h8_getreg16(dstreg));
		h8_setreg16(dstreg, udata16);
		H8_IFETCH_TIMING(1)
			break;
	case 0xa:
		if (opcode & 0x80) {
			// Add.l Rs,ERs
			if (opcode & 0x8) {
				h8.h8err = 29;
			}
			else {
				dstreg = opcode & 0x7;
				udata32 = h8_add32(h8_getreg32((opcode >> 4) & 0x7), h8_getreg32(dstreg));
				h8_setreg32(dstreg, udata32);
				H8_IFETCH_TIMING(1)
			}
		}
		else {
			// inc.b
			if (opcode & 0xf0) {
				h8.h8err = 30;
			}
			else {
				dstreg = opcode & 0xf;
				udata8 = h8_inc8(h8_getreg8(dstreg));
				h8_setreg8(dstreg, udata8);
				H8_IFETCH_TIMING(1);
			}
		}
		break;
	case 0xb:
		switch ((opcode >> 4) & 0xf)
		{
		case 0:
			if (opcode & 0x8)
			{
				h8.h8err = 31;
			}
			else
			{ //ADDS #1,ERd
				dstreg = opcode & 7;
				udata32 = h8_getreg32(dstreg) + 1;
				h8_setreg32(dstreg, udata32);
				H8_IFETCH_TIMING(1)
			}
			break;
		case 5://inc.w #1, Rd
			dstreg = opcode & 0xf;
			udata16 = h8_inc16(h8_getreg16(dstreg), 1);
			h8_setreg16(dstreg, udata16);
			H8_IFETCH_TIMING(1);
			break;
		case 7://inc.l #1, ERd
			dstreg = opcode & 0x7;
			udata32 = h8_inc32(h8_getreg32(dstreg), 1);
			h8_setreg32(dstreg, udata32);
			H8_IFETCH_TIMING(1);
			break;
		case 8:
			if (opcode & 0x8)
			{
				h8.h8err = 32;
			}
			else
			{ //ADDS #2,ERd
				dstreg = opcode & 7;
				udata32 = h8_getreg32(dstreg) + 2;
				h8_setreg32(dstreg, udata32);
				H8_IFETCH_TIMING(1);
			}
			break;
		case 9:
			if (opcode & 0x8)
			{
				h8.h8err = 33;
			}
			else
			{ //ADDS #4,ERd
				dstreg = opcode & 7;
				udata32 = h8_getreg32(dstreg) + 4;
				h8_setreg32(dstreg, udata32);
				H8_IFETCH_TIMING(1);
			}
			break;
		case 0xd: //inc.w #2, Rd
			dstreg = opcode & 0xf;
			udata16 = h8_inc16(h8_getreg16(dstreg), 2);
			h8_setreg16(dstreg, udata16);
			H8_IFETCH_TIMING(1);
			break;
		case 0xf: //inc.l #2, ERd
			if (opcode & 0x8) {
				h8.h8err = 34;
			}
			else {
				dstreg = opcode & 0x7;
				udata32 = h8_inc32(h8_getreg32(dstreg), 2);
				h8_setreg32(dstreg, udata32);
				H8_IFETCH_TIMING(1);
			}
			break;
		default:
			h8.h8err = 34;
			break;
		}
		break;
	case 0xc:
		// mov.b Rs, Rd //Timing Checked
		dstreg = opcode & 0xf;
		udata8 = h8_mov8(h8_getreg8((opcode >> 4) & 0xf));
		h8_setreg8(dstreg, udata8);
		H8_IFETCH_TIMING(1);
		break;
	case 0xd:
		// mov.w Rs, Rd //Timing Checked
		dstreg = opcode & 0xf;
		udata16 = h8_mov16(h8_getreg16((opcode >> 4) & 0xf));
		h8_setreg16(dstreg, udata16);
		H8_IFETCH_TIMING(1);
		break;
	case 0xe:
		// Addx //Timing Checked
		dstreg = opcode & 0xf;
		udata8 = h8_addx8(h8_getreg8((opcode >> 4) & 0xf), h8_getreg8(dstreg));
		h8_setreg8(dstreg, udata8);
		H8_IFETCH_TIMING(1);
		break;
	case 0xf:
		if (opcode & 0x80)
		{
			if (opcode & 8)
			{
				h8.h8err = 35;
			}
			else
			{	//MOV.L ERs,ERd //Timing Checked
				dstreg = opcode & 0x7;
				udata32 = h8_mov32(h8_getreg32((opcode >> 4) & 0x7));
				h8_setreg32(dstreg, udata32);
				H8_IFETCH_TIMING(1);
			}
		}
		else
		{
			// DAA MISSING
			h8.h8err = 36;
		}
		break;
	default:
		h8.h8err = 37;
		break;
	}
	*/
}
void H83002::dasmGroup1()
{

}
void H83002::dasmGroup2()
{

}
void H83002::dasmGroup3()
{

}
void H83002::dasmGroup4()
{

}
void H83002::dasmGroup5()
{

}
void H83002::dasmGroup6()
{

}
void H83002::dasmGroup7()
{

}

void H83002::h8_GenException(UINT8 vectornr)
{
	UINT32 oldPC = h8.pc & 0x00ffffff;
	UINT32 oldSP = h8_getreg32(H8_SP);
	UINT8 oldCCR = h8_get_ccr();
	UINT32 vectorAddress = ((UINT32)vectornr * 4) & 0x00ffffff;
	UINT32 vectorRaw = h8_mem_read32(vectorAddress);
	H8IrqTrace("H8 EXCEPTION vector=%u vectorAddr=%06X raw=%08X oldPC=%06X oldSP=%08X oldCCR=%02X EPINTSTT=%02X EPINTENB=%02X ISR=%02X\n",
		vectornr, vectorAddress, vectorRaw, oldPC, oldSP, oldCCR, h8.per_regs[EPINTSTT], h8.per_regs[EPINTENB], h8.per_regs[ISR]);

	// push PC and CCR on stack
	// extended mode stack push!
	h8_setreg32(H8_SP, h8_getreg32(H8_SP) - 4);
	h8_mem_write32(h8_getreg32(H8_SP), (h8.pc & 0xffffff) | (h8_get_ccr() << 24));

	// generate address from vector
	h8_set_ccr8(h8_get_ccr() | 0x80);

	if (h8.h8uiflag == 0)
	{
		h8_set_ccr8(h8_get_ccr() | 0x40);
	}

	h8.pc = vectorRaw & 0xffffff;	
	H8IrqTrace("H8 EXCEPTION ENTERED vector=%u newPC=%06X newSP=%08X CCR=%02X\n", vectornr, h8.pc & 0x00ffffff, h8_getreg32(H8_SP), h8_get_ccr());
	
	//IRQ priority decision time
	if (vectornr >= 20){
		//Internal IRQ
		H8_IOP_TIMING(1);
	} else {
		//External IRQ
		H8_IOP_TIMING(2);
	}
	//Save PC + CC
	H8_STACK_TIMING(2);
	//Vector Fetch
	H8_IFETCH_TIMING(2);
	//Internal Process
	H8_IOP_TIMING(4);

	//IRQ RESPONSE TIME AS PER H83002 Datasheet
	//					OnChip  8bit2st	8bit3st	16bit2st 16bit3st
	//IRQ Priority Time		2*		2*		2*		2*		2*		*Only 1 state for internal IRQs (IRQ >= 20)
	//Max States b4 EOI!	1 - 23	1 - 27	1 - 31*	1 - 23	1 - 25*	*States will increase if wait states are inserted in external memory access
	//Save PC+CCR			4		8		12*		4		6*		*States will increase if wait states are inserted in external memory access
	//Vector Fetch			4		8		12*		4		6*		*States will increase if wait states are inserted in external memory access
	//Inst Prefetch			4		8		12*		4		6*		*States will increase if wait states are inserted in external memory access
	//Internal Process		4		4		4		4		4		process after IRQ accepted and process after prefetch
	//* Instruction Prefetch after IRQ accepted and prefetch of first instruction
	//! Maximum Number of States before end of current instruction

}

int H83002::h8_get_priority_level(UINT8 mapNum)
{
	int res = 0;

	switch(mapNum)	{
	case 1: // IRQ0
		if (h8.per_regs[IPRA] & 0x80)
		{
			res = 1;
		}
		break;
	case 2: // IRQ1
		if (h8.per_regs[IPRA] & 0x40)
		{
			res = 1;
		}
		break;
	case 3: // IRQ2
	case 4: // IRQ3
		if (h8.per_regs[IPRA] & 0x20)
		{
			res = 1;
		}
		break;
	case 5: // IRQ4
	case 6: // IRQ5
		if (h8.per_regs[IPRA] & 0x10)
		{
			res = 1;
		}
		break;
	case 7: // WDT + Refresh Controller
	case 8:
		if (h8.per_regs[IPRA] & 0x8)
		{
			res = 1;
		}
		break;
	case 9: // ITU Channel0
	case 10: // ITU Channel0
	case 11: // ITU Channel0
		if (h8.per_regs[IPRA] & 0x4)
		{
			res = 1;
		}
		break;
	case 12: // ITU CHannel1
	case 13: // ITU CHannel1
	case 14: // ITU CHannel1
		if (h8.per_regs[IPRA] & 0x2)
		{
			res = 1;
		}
		break;
	case 15: // ITU Channel2
	case 16: // ITU Channel2
	case 17: // ITU Channel2
		if (h8.per_regs[IPRA] & 0x1)
		{
			res = 1;
		}
		break;
	case 18: // ITU Channel3
	case 19: // ITU Channel3
	case 20: // ITU Channel3
		if (h8.per_regs[IPRB] & 0x80)
		{
			res = 1;
		}
		break;
	case 21: // ITU Channel4
	case 22: // ITU Channel4
	case 23: // ITU Channel4
		if (h8.per_regs[IPRB] & 0x40)
		{
			res = 1;
		}
		break;
	case 24: // DMAC
	case 25: // DMAC
	case 26: // DMAC
	case 27: // DMAC
		if (h8.per_regs[IPRB] & 0x20)
		{
			res = 1;
		}
		break;
	//case : // Reserved Bit
	//break;
	case 28: // SCI channel0
	case 29: // SCI channel0
	case 30: // SCI channel0
	case 31: // SCI channel0
		if (h8.per_regs[IPRB] & 0x8)
		{
			res = 1;
		}
break;
	case 32: // SCI channel1
	case 33: // SCI channel1
	case 34: // SCI channel1
	case 35: // SCI channel1
		if (h8.per_regs[IPRB] & 0x4)
		{
			res = 1;
		}
		break;
	case 36: // ADI
		if (h8.per_regs[IPRB] & 0x2)
		{
			res = 1;
		}
		break;
	}

	return res;
}

int H83002::h8_get_real_irq_Value(int mapNum) {

	switch (mapNum) {
	case 0: return 7;	//NMI
	case 1: return 12;	//IRQ0
	case 2: return 13;	//IRQ1
	case 3: return 14;	//IRQ2
	case 4: return 15;	//IRQ3
	case 5: return 16;	//IRQ4
	case 6: return 17;	//IRQ5
	case 7: return 20;	//WDT
	case 8: return 21;	//REFRESH
	case 9: return 24;	//ITU0	
	case 10: return 25;	//ITU0
	case 11: return 26;	//ITU0
	case 12: return 28;	//ITU1
	case 13: return 29;	//ITU1
	case 14: return 30;	//ITU1
	case 15: return 32;	//ITU2
	case 16: return 33;	//ITU2
	case 17: return 34;	//ITU2
	case 18: return 36;	//ITU3
	case 19: return 37;	//ITU3	
	case 20: return 38;	//ITU3
	case 21: return 40;	//ITU4
	case 22: return 41;	//ITU4
	case 23: return 42;	//ITU4
	case 24: return 44;	//DMAC
	case 25: return 45;	//DMAC
	case 26: return 46;	//DMAC
	case 27: return 47;	//DMAC
	case 28: return 52;	//SCI0
	case 29: return 53;	//SCI0
	case 30: return 54;	//SCI0
	case 31: return 55;	//SCI0
	case 32: return 56;	//SCI1
	case 33: return 57;	//SCI1
	case 34: return 58;	//SCI1
	case 35: return 59;	//SCI1
	case 36: return 60;	//AD
	default: return -1; //Invalid
	}
}

void H83002::h8_get_irq_map(void)
{
	//WARNING - These mapped numbers do not match the H8 CPU values, the IRQ_Map is used by the emulation and the indexes have been changed to
	//			remove the number of unused IRQ values. The real value can be obtained using    real value = h8_get_real_irq_Value(map number);


	ZeroMemory(IRQ_Map, 37);

	// A/D
	//IRQ_Map[36] = 1

	// SCI-1
	if ((h8.per_regs[SSR1] & TEND) && (h8.per_regs[SCR1] & TEIE)) IRQ_Map[35] = 1;					//Transmit End 1
	if ((h8.per_regs[SSR1] & TDRE) && (h8.per_regs[SCR1] & TIE)) IRQ_Map[34] = 1;					//Transmit Data Empty 1
	if ((h8.per_regs[SSR1] & RDRF) && (h8.per_regs[SCR1] & RIE)) IRQ_Map[33] = 1;					//Receive Data Full 1
	if ((h8.per_regs[SSR1] & (ORER | FER | PER)) && (h8.per_regs[SCR1] & RIE)) IRQ_Map[32] = 1;		//Receive Error 1

	// SCI-0
	if ((h8.per_regs[SSR0] & TEND) && (h8.per_regs[SCR0] & TEIE)) IRQ_Map[31] = 1;					//Transmit End 0
	if ((h8.per_regs[SSR0] & TDRE) && (h8.per_regs[SCR0] & TIE)) IRQ_Map[30] = 1;					//Transmit Data Empty 0
	if ((h8.per_regs[SSR0] & RDRF) && (h8.per_regs[SCR0] & RIE)) IRQ_Map[29] = 1;					//Receive Data Full 0
	if ((h8.per_regs[SSR0] & (ORER | FER | PER)) && (h8.per_regs[SCR0] & RIE)) IRQ_Map[28] = 1;		//Receive Error 0

	// DMAC
	//IRQ_Map[27] = 1
	//IRQ_Map[26] = 1
	//IRQ_Map[25] = 1
	//IRQ_Map[24] = 1

	// ITU-4
	if ((h8.per_regs[TSR4] & OVF) && (h8.per_regs[TIER4] & OVIE)) IRQ_Map[23] = 1;		//Overflow 4
	if ((h8.per_regs[TSR4] & IMFB) && (h8.per_regs[TIER4] & IMIEB)) IRQ_Map[22] = 1;	//Match B 4
	if ((h8.per_regs[TSR4] & IMFA) && (h8.per_regs[TIER4] & IMIEA)) IRQ_Map[21] = 1;	//Match A 4

	// ITU-3
	if ((h8.per_regs[TSR3] & OVF) && (h8.per_regs[TIER3] & OVIE)) IRQ_Map[20] = 1;		//Overflow 3
	if ((h8.per_regs[TSR3] & IMFB) && (h8.per_regs[TIER3] & IMIEB)) IRQ_Map[19] = 1;	//Match B 3
	if ((h8.per_regs[TSR3] & IMFA) && (h8.per_regs[TIER3] & IMIEA)) IRQ_Map[18] = 1;	//Match A 3

	// ITU-2
	if ((h8.per_regs[TSR2] & OVF) && (h8.per_regs[TIER2] & OVIE)) IRQ_Map[17] = 1;		//Overflow 2
	if ((h8.per_regs[TSR2] & IMFB) && (h8.per_regs[TIER2] & IMIEB)) IRQ_Map[16] = 1;	//Match B 2 
	if ((h8.per_regs[TSR2] & IMFA) && (h8.per_regs[TIER2] & IMIEA)) IRQ_Map[15] = 1;	//Match A 2

	// ITU-1
	if ((h8.per_regs[TSR1] & OVF) && (h8.per_regs[TIER1] & OVIE)) IRQ_Map[14] = 1;		//Overflow 1
	if ((h8.per_regs[TSR1] & IMFB) && (h8.per_regs[TIER1] & IMIEB)) IRQ_Map[13] = 1;	//Match B 1
	if ((h8.per_regs[TSR1] & IMFA) && (h8.per_regs[TIER1] & IMIEA)) IRQ_Map[12] = 1;	//Match A 1

	// ITU-0
	if ((h8.per_regs[TSR0] & OVF) && (h8.per_regs[TIER0] & OVIE)) IRQ_Map[11] = 1;		//Overflow 0 
	if ((h8.per_regs[TSR0] & IMFB) && (h8.per_regs[TIER0] & IMIEB)) IRQ_Map[10] = 1;	//Match B 0
	if ((h8.per_regs[TSR0] & IMFA) && (h8.per_regs[TIER0] & IMIEA)) IRQ_Map[9] = 1;		//Match A 0
	
	// REFRESH
	//IRQ_Map[21] = 1;
	
	// WDT
	//IRQ_Map[20] = 1;	

	// EXTERNAL
	h8_epoch_irq_update();

	if ((h8.per_regs[ISR] & IRQ5BIT) && (h8.per_regs[IER] & IRQ5BIT)) IRQ_Map[6] = 1; // IRQ 5
	if ((h8.per_regs[ISR] & IRQ4BIT) && (h8.per_regs[IER] & IRQ4BIT)) IRQ_Map[5] = 1; // IRQ 4
	if ((h8.per_regs[ISR] & IRQ3BIT) && (h8.per_regs[IER] & IRQ3BIT)) IRQ_Map[4] = 1; // IRQ 3
	if ((h8.per_regs[ISR] & IRQ2BIT) && (h8.per_regs[IER] & IRQ2BIT)) IRQ_Map[3] = 1; // IRQ 2
	if ((h8.per_regs[ISR] & IRQ1BIT) && (h8.per_regs[IER] & IRQ1BIT)) IRQ_Map[2] = 1; // IRQ 1
	if ((h8.per_regs[ISR] & IRQ0BIT) && (h8.per_regs[IER] & IRQ0BIT)) IRQ_Map[1] = 1; // IRQ 0

	//NMI
	//IRQ_Map[0] = 1;


}

//Epoch Interrupt Controller
void H83002::h8_epoch_irq_update() {
	
	// Recompute Epoch-derived IRQ lines from live Epoch IRQ state.
	h8.per_regs[ISR] &= (UINT8)~(IRQ4BIT | IRQ5BIT);

	//Input Uses IRQ 4
	if ((h8.per_regs[EPINTENB] & INTINPUT ) && (h8.per_regs[EPINTSTT] & INTINPUT )) h8.per_regs[ISR] |= IRQ4BIT;

	//Everything Else IRQ 5
	if ((h8.per_regs[EPINTENB] & INTAUDIO ) && (h8.per_regs[EPINTSTT] & INTAUDIO )) 
		h8.per_regs[ISR] |= IRQ5BIT;
	if ((h8.per_regs[EPINTENB] & INTI2C   ) && (h8.per_regs[EPINTSTT] & INTI2C   )) 
		h8.per_regs[ISR] |= IRQ5BIT;
	if ((h8.per_regs[EPINTENB] & INTSYNC  ) && (h8.per_regs[EPINTSTT] & INTSYNC  )) 
		h8.per_regs[ISR] |= IRQ5BIT;
	if ((h8.per_regs[EPINTENB] & INTFUNCSW) && (h8.per_regs[EPINTSTT] & INTFUNCSW)) 
		h8.per_regs[ISR] |= IRQ5BIT;
	if ((h8.per_regs[EPINTENB] & INTRFRSH ) && (h8.per_regs[EPINTSTT] & INTRFRSH )) 
		h8.per_regs[ISR] |= IRQ5BIT;
	if ((h8.per_regs[EPINTENB] & INTFRAME ) && (h8.per_regs[EPINTSTT] & INTFRAME )) 
		h8.per_regs[ISR] |= IRQ5BIT;
	if ((h8.per_regs[EPINTENB] & INTMATRIX) && (h8.per_regs[EPINTSTT] & INTMATRIX)) 
		h8.per_regs[ISR] |= IRQ5BIT;

}


void H83002::h8_check_irqs(void)
{
	int AllowedLevel = -1;

	if (h8.h8iflag == 0)
	{
		AllowedLevel = 0;
	}
	else
	{
		if ((h8.per_regs[SYSCR] & 0x08) == 0)
		{
			if (h8.h8uiflag == 0)
			{
				AllowedLevel = 1;
			}
		}
	}
	
	if (AllowedLevel >= 0){
		
		UINT8 sourceH = 0xff, sourceL = 0xff, Level;
		
		h8_get_irq_map();

		for (int i = 36; i >= 0; i--) //Process from 36 to 0 so the last detected overwrites any previous
		{
			if (IRQ_Map[i]) 
			{
				// Get Priority Level for this IRQ
				Level = h8_get_priority_level(i);
				//Check if Allowed
				if (Level >= AllowedLevel) 
				{
					if (Level == 0) 
					{
						sourceL = h8_get_real_irq_Value(i);
					}
					else  if (Level == 1) 
					{
						sourceH = h8_get_real_irq_Value(i);
					}
				}
			}
		}

		if (sourceH != 0xff) //Process High Level Interrupt
		{
			//Clear the ISR if Matching IRQ is carried out (external IRQs only)
			switch (sourceH) {
			case IRQ5VAL: h8.per_regs[ISR] &= ~(IRQ5BIT); break;
			case IRQ4VAL: h8.per_regs[ISR] &= ~(IRQ4BIT); break;
			case IRQ3VAL: h8.per_regs[ISR] &= ~(IRQ3BIT); break;
			case IRQ2VAL: h8.per_regs[ISR] &= ~(IRQ2BIT); break;
			case IRQ1VAL: h8.per_regs[ISR] &= ~(IRQ1BIT); break;
			case IRQ0VAL: h8.per_regs[ISR] &= ~(IRQ0BIT); break;
			}
			//Do the Interrupt
			h8_GenException(sourceH);
		}
		else if (sourceL != 0xff) { //Process Low Level Interrupt

			//Clear the ISR if Matching IRQ is carried out (external IRQs only)
			switch (sourceL) {
			case IRQ5VAL: h8.per_regs[ISR] &= ~(IRQ5BIT); break;
			case IRQ4VAL: h8.per_regs[ISR] &= ~(IRQ4BIT); break;
			case IRQ3VAL: h8.per_regs[ISR] &= ~(IRQ3BIT); break;
			case IRQ2VAL: h8.per_regs[ISR] &= ~(IRQ2BIT); break;
			case IRQ1VAL: h8.per_regs[ISR] &= ~(IRQ1BIT); break;
			case IRQ0VAL: h8.per_regs[ISR] &= ~(IRQ0BIT); break;
			}
			//Do the Interrupt
			h8_GenException(sourceL);
		}

	}

}

UINT8 H83002::h8_get_ccr(void)
{
	h8.ccr = 0;

	if (h8.h8nflag)  h8.ccr |= NFLAG;
	if (h8.h8zflag)  h8.ccr |= ZFLAG;
	if (h8.h8vflag)  h8.ccr |= VFLAG;
	if (h8.h8cflag)  h8.ccr |= CFLAG;
	if (h8.h8uflag)  h8.ccr |= UFLAG;
	if (h8.h8hflag)  h8.ccr |= HFLAG;
	if (h8.h8uiflag) h8.ccr |= UIFLAG;
	if (h8.h8iflag)  h8.ccr |= IFLAG;

	return h8.ccr;
}


void H83002::h8_set_ccr8(UINT8 data)
{
	h8.ccr = data;

	h8.h8nflag = 0;
	h8.h8zflag = 0;
	h8.h8vflag = 0;
	h8.h8cflag = 0;
	h8.h8hflag = 0;
	h8.h8uflag = 0;
	h8.h8uiflag = 0;
	h8.h8iflag = 0;

	if (h8.ccr & NFLAG)  h8.h8nflag  = 1;
	if (h8.ccr & ZFLAG)  h8.h8zflag  = 1;
	if (h8.ccr & VFLAG)  h8.h8vflag  = 1;
	if (h8.ccr & CFLAG)  h8.h8cflag  = 1;
	if (h8.ccr & HFLAG)  h8.h8hflag  = 1;
	if (h8.ccr & UFLAG)  h8.h8uflag  = 1;
	if (h8.ccr & UIFLAG) h8.h8uiflag = 1;
	if (h8.ccr & IFLAG)  h8.h8iflag  = 1;

}



UINT16 H83002::h8_getreg16(UINT8 reg)
{
	if (reg > 7)
	{
		return h8.regs[reg - 8] >> 16;
	}
	else
	{
		return h8.regs[reg];
	}
}

void H83002::h8_setreg16(UINT8 reg, UINT16 data)
{
	if (reg > 7)
	{
		h8.regs[reg - 8] &= 0xffff;
		h8.regs[reg - 8] |= data << 16;
	}
	else
	{
		h8.regs[reg] &= 0xffff0000;
		h8.regs[reg] |= data;
	}
}

UINT8 H83002::h8_getreg8(UINT8 reg)
{
	if (reg > 7)
	{
		return h8.regs[reg - 8];
	}
	else
	{
		return h8.regs[reg] >> 8;
	}
}

void H83002::h8_setreg8(UINT8 reg, UINT8 data)
{
	if (reg > 7)
	{
		h8.regs[reg - 8] &= 0xffffff00;
		h8.regs[reg - 8] |= data;
	}
	else
	{
		h8.regs[reg] &= 0xffff00ff;
		h8.regs[reg] |= data << 8;
	}
}

UINT32 H83002::h8_getreg32(UINT8 reg)
{
	if (reg > 7)
	{
		h8.err = -1;
		return 0;
	}
	else {
		return h8.regs[reg];
	}
}

void H83002::h8_setreg32(UINT8 reg, UINT32 data)
{
	if (reg > 7)
	{
		h8.err = -2;		
	}
	else {
		h8.regs[reg] = data;
	}
	
}

void H83002::h8_onstateload(void)
{
	h8_set_ccr8(h8.ccr);
}

void H83002::reset()
{
	//Reset CPU

	//Zero all memory
	ZeroMemory(&h8, sizeof(h8));
	h8_sleeping = 0;
	h8_suppress_irq_once = 0;
	
	//Set i in CCR
	h8.h8iflag = 1; 
	//Clear Error Flag
	h8.err = 0;
	//Set Program Counter
	h8.pc = h8_mem_read32(0) & 0xffffff;
	
	//Reset peripherals	
	h8_dmac_reset();	//Direct Memory Access Controller
	h8_itu_reset();		//Integrated Timer Unit
	h8_tpc_reset();		//Timing Pattern Controller
	h8_wdt_reset();		//Watch Dog Timer
	h8_rc_reset();		//Refresh Controller
	h8_sci_reset();		//Serial Communications Interface	
	h8_io_reset();		//IO Ports
	h8_adc_reset();		//AD Converter
	h8_bc_reset();		//Bus Controller
	h8_sys_reg_reset();	//System Registers	
	h8_ic_reset();		//Interrupt Controller
	  
}












