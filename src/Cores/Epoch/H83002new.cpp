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
#include "h83002.h"
#include <stdio.h>
#define H8_SP 7


UINT16 H83002::getNextOpcode() {
	UINT16 opcode = cpu_readop16(h8.pc);
	//Increment program counter
	h8.pc += 2;
	return opcode;
}


void H83002::h8_timing_init() {

	UINT8 busWidth = h8.per_regs[ABWCR];
	UINT8 stateControl = h8.per_regs[ASTCR];
	UINT8 insertedWaitStates = 0;//This seems to be OK for epoch?

	switch (busWidth) {
	case 0x0://16 bit access (all areas)
		switch (stateControl) {
		case 0x0://2 state access
			fetchValue = 2;
			branchValue = 2;
			stackValue = 2;
			byteValue = 2;
			wordValue = 2;
			break;
		case 0xff://3 state access
			fetchValue = 3;
			branchValue = 3;
			stackValue = 3;
			byteValue = 3;
			wordValue = 3;
			break;
		default://2 | 3 state access (per area) not accounted for!
			h8.err = 106;
			break;
		}
		break;
	case 0xff://8 bit access (all areas)
		switch (stateControl) {
		case 0x0://2 state access
			fetchValue = 4;
			branchValue = 4;
			stackValue = 4;
			byteValue = 2;
			wordValue = 4;
			break;
		case 0xff://3 state access
			fetchValue = 6;
			branchValue = 6;
			stackValue = 6;
			byteValue = 3;
			wordValue = 6;
			break;
		default://2 | 3 state access (per area) not accounted for!
			h8.err = 107;
			break;
		}
		break;
	default://8 | 16 bit access (per area) not accounted for!
		h8.err = 108;
		break;
	}
}

void H83002::h8_timing_fetch(UINT8 Access, UINT8 fetch)
{
	switch (Access) {
	case ON_CHIP_MEMORY:
		h8_cyccnt += (2 * fetch);
		break;
	case ON_CHIP_MODULE:
		h8_cyccnt += (fetchValue * fetch);
		break;
	case MEMORY_AREA:
		h8_cyccnt += (fetchValue * fetch); //+ insertedWaitStates);
		break;
	default:
		h8.err = 109;
	}
}
void H83002::h8_timing_branch(UINT8 Access, UINT8 branch)
{
	switch (Access) {
	case ON_CHIP_MEMORY:
		h8_cyccnt += (2 * branch);
		break;
	case ON_CHIP_MODULE:
		h8_cyccnt += (branchValue * branch);
		break;
	case MEMORY_AREA:
		h8_cyccnt += (branchValue * branch); //+ insertedWaitStates);
		break;
	default:
		h8.err = 109;
	}
}
void H83002::h8_timing_stack(UINT8 Access, UINT8 stack)
{
	switch (Access) {
	case ON_CHIP_MEMORY:
		h8_cyccnt += (2 * stack);
		break;
	case ON_CHIP_MODULE:
		h8_cyccnt += (stackValue * stack);
		break;
	case MEMORY_AREA:
		h8_cyccnt += (stackValue * stack); //+ insertedWaitStates);
		break;
	default:
		h8.err = 109;
	}
}
void H83002::h8_timing_byte(UINT8 Access, UINT8 byte)
{
	switch (Access) {
	case ON_CHIP_MEMORY:
		h8_cyccnt += (2 * byte);
		break;
	case ON_CHIP_MODULE:
		h8_cyccnt += (byteValue * byte);
		break;
	case MEMORY_AREA:
		h8_cyccnt += (byteValue * byte); //+ insertedWaitStates);
		break;
	default:
		h8.err = 109;
	}
}
void H83002::h8_timing_word(UINT8 Access, UINT8 word)
{
	switch (Access) {
	case ON_CHIP_MEMORY:
		h8_cyccnt += (2 * word);
		break;
	case ON_CHIP_MODULE:
		h8_cyccnt += (wordValue * word);
		break;
	case MEMORY_AREA:
		h8_cyccnt += (wordValue * word); //+ insertedWaitStates);
		break;
	default:
		h8.err = 109;
	}
}

void H83002::h8_timing_internal(UINT8 iop)
{
	h8_cyccnt += (iop);
}

int H83002::executeNew(UINT32 totalCycles)
{
	//Reset Cycles
	h8_cyccnt = 0;

	// Check interrupts unless the previous instruction was a CCR-control
	// instruction. The H8/300H suppresses interrupt acceptance for one
	// instruction after LDC/ANDC/ORC/XORC-style CCR changes.
	if (h8_suppress_irq_once) {
		h8_suppress_irq_once = 0;
	}
	else {
		h8_check_irqs();
	}

	// If SLEEP was executed and no interrupt/exception was taken above, the
	// CPU remains halted. Return a small non-zero cycle count so the board
	// scheduler does not report a false zero-cycle/missing-opcode error.
	if (h8_sleeping && h8_cyccnt == 0) {
		h8_timing_internal(1);
		return h8_cyccnt;
	}
	if (h8_cyccnt != 0) {
		h8_sleeping = 0;
	}

	//Store Previous Program Counter
	h8.ppc = h8.pc;
	//Get current opcode
	UINT16 opcode = getNextOpcode();

	//Variables
	UINT8 AH = ((opcode >> 12) & 0xf);
	UINT8 srcreg, dstreg, udata8;
	INT8 sdata8;

	switch (AH)
	{
	case 0x0:
		// 0n nn
		h8_group0new(opcode);
		break;
	case 0x1:
		// 1n nn
		h8_group1new(opcode);
		break;
	case 0x2:
		// 2n nn
		// mov.b @aa:8, Rd //Timing Checked
		h8_timing_fetch(AccessType, 1);
		dstreg = (opcode >> 8) & 0xf;
		udata8 = h8_mem_read8((0xffff00 | (opcode & 0xff)));
		h8_timing_byte(AccessType, 1);
		h8_mov8(udata8); // flags calculation
		h8_setreg8(dstreg, udata8);
		break;
	case 0x3:
		// 3n nn
		// mov.b Rs,@aa:8 //Timing Checked
		h8_timing_fetch(AccessType, 1);
		srcreg = (opcode >> 8) & 0xf;
		udata8 = h8_getreg8(srcreg);
		h8_mov8(udata8); // flags calculation, dont care about others
		h8_mem_write8(0xffff00 | (opcode & 0xff), udata8);
		h8_timing_byte(AccessType, 1);
		break;
	case 0x4:
		// 4n nn
		// branch @xx:8 //Timing Checked
		h8_timing_fetch(AccessType, 2);
		sdata8 = (opcode & 0xff);
		if (h8_branch((opcode >> 8) & 0xf))
		{
			h8.pc = (h8.pc + sdata8) & 0x00ffffff;
		}
		break;
	case 0x5:
		// 5n nn
		h8_group5new(opcode);
		break;
	case 0x6:
		// 6n nn
		h8_group6new(opcode);
		break;
	case 0x7:
		// 7n nn
		h8_group7new(opcode);
		break;
	case 0x8:
		// 8n nn
		// add.b #xx:8, Rd //Timing Checked
		h8_timing_fetch(AccessType, 1);
		dstreg = (opcode >> 8) & 0xf;
		udata8 = h8_add8(opcode & 0xff, h8_getreg8(dstreg));
		h8_setreg8(dstreg, udata8);
		break;
	case 0x9:
		// 9n nn
		// addx.b #xx:8, Rd //Timing Checked
		h8_timing_fetch(AccessType, 1);
		dstreg = (opcode >> 8) & 0xf;
		udata8 = h8_addx8(opcode & 0xff, h8_getreg8(dstreg));
		h8_setreg8(dstreg, udata8);
		break;
	case 0xa:
		// an nn
		// cmp.b #xx:8, Rd //Timing Checked
		h8_timing_fetch(AccessType, 1);
		dstreg = (opcode >> 8) & 0xf;
		h8_cmp8(opcode & 0xff, h8_getreg8(dstreg));
		break;
	case 0xb:
		// bn nn
		// subx.b #xx:8, Rd //Timing Checked
		h8_timing_fetch(AccessType, 1);
		dstreg = (opcode >> 8) & 0xf;
		udata8 = h8_subx8(opcode & 0xff, h8_getreg8(dstreg));
		h8_setreg8(dstreg, udata8);
		break;
	case 0xc:
		// cn nn
		// or.b #xx:8, Rd //Timing Checked
		h8_timing_fetch(AccessType, 1);
		dstreg = (opcode >> 8) & 0xf;
		udata8 = h8_or8(opcode & 0xff, h8_getreg8(dstreg));
		h8_setreg8(dstreg, udata8);
		break;
	case 0xd:
		// dn nn
		// xor.b #xx:8, Rd //Timing Checked
		h8_timing_fetch(AccessType, 1);
		dstreg = (opcode >> 8) & 0xf;
		udata8 = h8_xor8(opcode & 0xff, h8_getreg8(dstreg));
		h8_setreg8(dstreg, udata8);
		break;
	case 0xe:
		// en nn
		// and.b #xx:8, Rd //Timing Checked
		h8_timing_fetch(AccessType, 1);
		dstreg = (opcode >> 8) & 0xf;
		udata8 = h8_and8(opcode & 0xff, h8_getreg8(dstreg));
		h8_setreg8(dstreg, udata8);
		break;
	case 0xf:
		// fn nn
		// mov.b #xx:8, Rd //Timing Checked
		h8_timing_fetch(AccessType, 1);
		dstreg = (opcode >> 8) & 0xf;
		udata8 = h8_mov8(opcode & 0xff);
		h8_setreg8(dstreg, udata8);
		break;
	}

	//Wait for stack pointer to be set before potentially firing stack errors
	if (h8.SPSFlag == 0) {
		if (h8.regs[7] != 0) {
			h8.StackVal = h8.regs[7];
			h8.SPSFlag = 1; //Stack pointer set
		}
	}
	else if (h8.regs[7] < 0xfe121c) {
		h8.err = 1; //Stack pointer out of Lower bounds + Stack Overflow
	}
	else if (h8.regs[7] >= 0xff0000) {
		h8.err = 2; //Stack pointer out of Upper Bounds
	}
	else if (h8.regs[7] > h8.StackVal) {
		h8.err = 3; //Stack Underflow
	}

	if ((h8.pc > 0x80000) && (h8.pc < 0xfe121c)) {
		h8.err = 4; //CPU Program Counter out of bounds.
	}

	if ((h8.h8cflag > 1) || (h8.h8cflag < 0)) {
		h8.err = 5;
	}
	if ((h8.h8vflag > 1) || (h8.h8vflag < 0)) {
		h8.err = 6;
	}
	if ((h8.h8zflag > 1) || (h8.h8zflag < 0)) {
		h8.err = 7;
	}
	if ((h8.h8nflag > 1) || (h8.h8nflag < 0)) {
		h8.err = 8;
	}
	if ((h8.h8uflag > 1) || (h8.h8uflag < 0)) {
		h8.err = 9;
	}
	if ((h8.h8hflag > 1) || (h8.h8hflag < 0)) {
		h8.err = 10;
	}
	if ((h8.h8uiflag > 1) || (h8.h8uiflag < 0)) {
		h8.err = 11;
	}
	if ((h8.h8iflag > 1) || (h8.h8iflag < 0)) {
		h8.err = 12;
	}

	if (h8_cyccnt == 0) {
		char buf[256];
		sprintf_s(buf,
			"H8 ZERO CYCLES: PPC=%06X PC=%06X OPCODE=%04X ERR=%u CCR=%02X ER0=%08X ER1=%08X ER2=%08X ER3=%08X ER7=%08X\n",
			h8.ppc & 0x00ffffff,
			h8.pc & 0x00ffffff,
			opcode,
			h8.err,
			h8_get_ccr(),
			h8.regs[0],
			h8.regs[1],
			h8.regs[2],
			h8.regs[3],
			h8.regs[7]);
		OutputDebugStringA(buf);
	}

	return h8_cyccnt;
}

void H83002::h8_group0new(UINT16 opcode) {

	UINT8 udata8, dstreg;
	UINT16 udata16;

	// Deal with all 0n nn opcodes	
	switch ((opcode >> 8) & 0xf)
	{
	case 0x0:
		//NOP
		h8_timing_fetch(AccessType, 1);
		break;
	case 0x1:
		//Table 2
		h8_table2(opcode);
		break;
	case 0x2:
		//STC.b CCR, Rd
		if (((opcode >> 4) & 0xf) == 0)
		{
			h8_timing_fetch(AccessType, 1);
			h8_setreg8(opcode & 0xf, h8_get_ccr());
		}
		else
		{
			h8.err = 13;
		}
		break;
	case 0x3:
		//LDC.b Rs, CCR
		if (((opcode >> 4) & 0xf) == 0)
		{
			h8_timing_fetch(AccessType, 1);
			udata8 = h8_getreg8(opcode & 0xf);
			h8_set_ccr8(udata8);
			h8_suppress_irq_once = 1;
		}
		else
		{
			h8.err = 14;
		}
		break;
	case 0x4:
		//ORC.b #xx:8, CCR
		h8_timing_fetch(AccessType, 1);
		udata8 = h8_or8(opcode & 0xff, h8_get_ccr());
		h8_set_ccr8(udata8);
		h8_suppress_irq_once = 1;
		break;
	case 0x5:
		//XORC.b #xx:8, CCR
		h8_timing_fetch(AccessType, 1);
		udata8 = h8_xor8(opcode & 0xff, h8_get_ccr());
		h8_set_ccr8(udata8);
		h8_suppress_irq_once = 1;
		break;
	case 0x6:
		//ANDC.b #xx:8, CCR
		h8_timing_fetch(AccessType, 1);
		udata8 = h8_and8(opcode & 0xff, h8_get_ccr());
		h8_set_ccr8(udata8);
		h8_suppress_irq_once = 1;
		break;
	case 0x7:
		//LDC.b #xx:8, CCR
		h8_timing_fetch(AccessType, 1);
		udata8 = opcode & 0xff;
		h8_set_ccr8(udata8);
		h8_suppress_irq_once = 1;
		break;
	case 0x8:
		//ADD.b Rs, Rd
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata8 = h8_add8(h8_getreg8((opcode >> 4) & 0xf), h8_getreg8(dstreg));
		h8_setreg8(dstreg, udata8);
		break;
	case 0x9:
		//ADD.w Rs, Rd
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata16 = h8_add16(h8_getreg16((opcode >> 4) & 0xf), h8_getreg16(dstreg));
		h8_setreg16(dstreg, udata16);
		break;
	case 0xa:
		//Table 2
		h8_table2(opcode);
		break;
	case 0xb:
		//Table 2
		h8_table2(opcode);
		break;
	case 0xc:
		//MOV.b Rs, Rd
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata8 = h8_mov8(h8_getreg8((opcode >> 4) & 0xf));
		h8_setreg8(dstreg, udata8);
		break;
	case 0xd:
		//MOV.w Rs, Rd		
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata16 = h8_mov16(h8_getreg16((opcode >> 4) & 0xf));
		h8_setreg16(dstreg, udata16);
		break;
	case 0xe:
		//ADDX.b Rs, Rd
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata8 = h8_addx8(h8_getreg8((opcode >> 4) & 0xf), h8_getreg8(dstreg));
		h8_setreg8(dstreg, udata8);
		break;
	case 0xf:
		//Table 2
		h8_table2(opcode);
		break;

	}
}
void H83002::h8_group1new(UINT16 opcode) {

	UINT8 dstreg, udata8;
	UINT16 udata16;

	switch ((opcode >> 8) & 0xf)
	{
	case 0x0:
		//Table 2
		h8_table2(opcode);
		break;
	case 0x1:
		//Table 2
		h8_table2(opcode);
		break;
	case 0x2:
		//Table 2
		h8_table2(opcode);
		break;
	case 0x3:
		//Table 2
		h8_table2(opcode);
		break;
	case 0x4:
		//OR.b Rs, Rd
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata8 = h8_or8(h8_getreg8((opcode >> 4) & 0xf), h8_getreg8(dstreg));
		h8_setreg8(dstreg, udata8);
		break;
	case 0x5:
		//XOR.b Rs, Rd
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata8 = h8_xor8(h8_getreg8((opcode >> 4) & 0xf), h8_getreg8(dstreg));
		h8_setreg8(dstreg, udata8);
		break;
	case 0x6:
		//AND.b Rs, Rd
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata8 = h8_and8(h8_getreg8((opcode >> 4) & 0xf), h8_getreg8(dstreg));
		h8_setreg8(dstreg, udata8);
		break;
	case 0x7:
		//Table 2
		h8_table2(opcode);
		break;
	case 0x8:
		//SUB.b Rs, Rd
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata8 = h8_sub8(h8_getreg8((opcode >> 4) & 0xf), h8_getreg8(dstreg));
		h8_setreg8(dstreg, udata8);
		break;
	case 0x9:
		//SUB.w	Rs, Rd
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata16 = h8_sub16(h8_getreg16((opcode >> 4) & 0xf), h8_getreg16(dstreg));
		h8_setreg16(dstreg, udata16);
		break;
	case 0xa:
		//Table 2	
		h8_table2(opcode);
		break;
	case 0xb:
		//Table 2
		h8_table2(opcode);
		break;
	case 0xc:
		//CMP.b Rs, Rd
		h8_timing_fetch(AccessType, 1);
		h8_cmp8(h8_getreg8((opcode >> 4) & 0xf), h8_getreg8(opcode & 0xf));
		break;
	case 0xd:
		//CMP.w Rs, Rd	
		h8_timing_fetch(AccessType, 1);
		h8_cmp16(h8_getreg16((opcode >> 4) & 0xf), h8_getreg16(opcode & 0xf));
		break;
	case 0xe:
		//SUBX.b Rs, Rd
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata8 = h8_subx8(h8_getreg8((opcode >> 4) & 0xf), h8_getreg8(dstreg));
		h8_setreg8(dstreg, udata8);
		break;
	case 0xf:
		//Table 2
		h8_table2(opcode);
		break;
	default:
		h8.err = 15;
	}

}
void H83002::h8_group5new(UINT16 opcode) {

	UINT8 udata8;
	UINT16 udata16;
	UINT32 udata32, address24;
	INT8 sdata8;
	INT16 sdata16;

	switch ((opcode >> 8) & 0xf)
	{
	case 0x0:
		//MULXU.b Rs,Rd
		h8_timing_fetch(AccessType, 1);
		udata8 = h8_getreg8((opcode >> 4) & 0xf);
		udata16 = h8_getreg16(opcode & 0xf);
		udata16 &= 0xff;
		udata16 = udata16 * udata8;
		// no flags modified!
		h8_setreg16(opcode & 0xf, udata16);
		h8_timing_internal(12);
		break;
	case 0x1:
		//DIVXU.b Rs, Rd
		h8_timing_fetch(AccessType, 1);
		udata8 = h8_getreg8((opcode >> 4) & 0xf);
		udata16 = h8_getreg16(opcode & 0xf);
		udata16 = h8_divxu8(udata16, udata8);
		h8_setreg16(opcode & 0xf, udata16);
		h8_timing_internal(12);
		break;
	case 0x2:
		//MULXU.w Rs,ERd
		h8_timing_fetch(AccessType, 1);
		udata16 = h8_getreg16((opcode >> 4) & 0xf);
		udata32 = h8_getreg32(opcode & 0x7);
		udata32 &= 0xffff;
		udata32 = udata32 * udata16;
		// no flags modified!
		h8_setreg32(opcode & 0x7, udata32);
		h8_timing_internal(20);
		break;
	case 0x3:
		//DIVXU.w Rs, ERd
		h8_timing_fetch(AccessType, 1);
		udata16 = h8_getreg16((opcode >> 4) & 0xf);
		udata32 = h8_getreg32(opcode & 0x7);
		udata32 = h8_divxu16(udata32, udata16);
		h8_setreg32(opcode & 0x7, udata32);
		h8_timing_internal(20);
		break;
	case 0x4:
		//RTS
		if (opcode == 0x5470)
		{
			// rts
			h8_timing_fetch(AccessType, 2);
			udata32 = h8_mem_read32(h8_getreg32(H8_SP));
			h8_timing_stack(AccessType, 2);
			h8_setreg32(H8_SP, h8_getreg32(H8_SP) + 4);
			// extended mode
			h8.pc = (udata32 & 0xffffff);
			h8_timing_internal(2);
		}
		else
		{
			// No other perms are valid
			h8.err = 16;
		}
		break;
	case 0x5:
		//BSR d:8
		h8_timing_fetch(AccessType, 2);
		sdata8 = opcode & 0xff;
		// extended mode stack push!
		h8_setreg32(H8_SP, h8_getreg32(H8_SP) - 4);
		h8_mem_write32(h8_getreg32(H8_SP), (h8.pc & 0xffffff));
		h8_timing_stack(AccessType, 2);
		h8.pc = (h8.pc + sdata8) & 0x00ffffff;
		break;
	case 0x6:
		//RTE
		if (opcode == 0x5670)
		{
			h8_timing_fetch(AccessType, 2);
			udata32 = h8_mem_read32(h8_getreg32(H8_SP));
			h8_timing_stack(AccessType, 2);
			h8_setreg32(H8_SP, h8_getreg32(H8_SP) + 4);

			// extended mode restore PC
			h8.pc = udata32 & 0xffffff;
			// restore CCR			
			h8_set_ccr8((UINT8)(udata32 >> 24));
			h8_timing_internal(2);
		}
		else {
			// No other valid perms
			h8.err = 1;
		}
		break;
	case 0x7:
		// TRAPA #x:2 uses vectors 8..11 in H8/300H advanced mode.
		h8_timing_fetch(AccessType, 1);
		h8_GenException(8 + ((opcode >> 4) & 0x03));
		break;
	case 0x8:
		//Table 2
		h8_table2(opcode);
		break;
	case 0x9:
		//JMP @erd
		h8_timing_fetch(AccessType, 2);
		address24 = h8_getreg32((opcode >> 4) & 7);
		address24 &= 0xffffff;
		h8.pc = address24;
		break;
	case 0xa:
		//JMP @aa:24
		h8_timing_fetch(AccessType, 2);
		address24 = h8_mem_read32(h8.pc - 2);
		address24 &= 0xffffff;
		h8.pc = address24;
		h8_timing_internal(2);
		break;
	case 0xb:
		// JMP @@aa:8. In advanced mode the 8-bit memory-indirect
		// branch table is in low memory (0x000000..0x0000ff), not
		// the 0xffff00 short absolute data page.
		h8_timing_fetch(AccessType, 2);
		address24 = h8_mem_read32(opcode & 0x00ff);
		h8_timing_branch(AccessType, 2);
		h8.pc = address24 & 0xffffff;
		h8_timing_internal(2);
		break;
	case 0xc:
		//BSR d:16
		if (opcode & 0xff)
		{
			h8.err = 18;
		}
		else
		{
			h8_timing_fetch(AccessType, 2);
			sdata16 = h8_mem_read16(h8.pc);
			h8_setreg32(H8_SP, h8_getreg32(H8_SP) - 4);
			h8_mem_write32(h8_getreg32(H8_SP), (h8.pc + 2) & 0xffffff);
			h8_timing_stack(AccessType, 2);
			h8.pc += sdata16 + 2;
			h8.pc &= 0xffffff;
			h8_timing_internal(2);


			// Address should be even
			if (h8.pc & 1)
			{
				h8.err = 19;
			}
		}
		break;
	case 0xd:
		//JSR @reg
		h8_timing_fetch(AccessType, 2);
		address24 = h8_getreg32((opcode >> 4) & 7);
		address24 &= 0xffffff;
		// extended mode stack push!
		h8_setreg32(H8_SP, h8_getreg32(H8_SP) - 4);
		h8_mem_write32(h8_getreg32(H8_SP), (h8.pc & 0xffffff));
		h8_timing_stack(AccessType, 2);
		h8.pc = address24;
		break;
	case 0xe:
		//JSR @aa:24
		h8_timing_fetch(AccessType, 2);
		address24 = h8_mem_read32(h8.pc - 2);
		address24 &= 0xffffff;
		// extended mode stack push!
		h8_setreg32(H8_SP, h8_getreg32(H8_SP) - 4);
		h8_mem_write32(h8_getreg32(H8_SP), (h8.pc + 2) & 0xffffff);
		h8_timing_stack(AccessType, 2);
		h8.pc = address24;
		h8_timing_internal(2);
		break;
	case 0xf:
		// JSR @@aa:8. Advanced-mode memory-indirect branch table is
		// in low memory (0x000000..0x0000ff).
		h8_timing_fetch(AccessType, 2);
		address24 = h8_mem_read32(opcode & 0x00ff) & 0xffffff;
		h8_timing_branch(AccessType, 2);
		// extended mode stack push!
		h8_setreg32(H8_SP, h8_getreg32(H8_SP) - 4);
		h8_mem_write32(h8_getreg32(H8_SP), (h8.pc & 0xffffff));
		h8_timing_stack(AccessType, 2);
		h8.pc = address24;
		break;
	default:
		h8.err = 20;
	}

}
void H83002::h8_group6new(UINT16 opcode) {


	UINT8 bitnr, srcreg, dstreg, udata8;
	UINT16 udata16;
	UINT32 address24;
	INT16 sdata16;

	switch ((opcode >> 8) & 0xf)
	{
	case 0x0:
		//BSET Rn, Rd
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata8 = h8_getreg8(dstreg);
		bitnr = h8_getreg8((opcode >> 4) & 0xf) & 0x7;
		udata8 = h8_bset8(bitnr, udata8);
		h8_setreg8(dstreg, udata8);
		break;
	case 0x1:
		//BNOT Rn, Rd
		dstreg = opcode & 0xf;
		udata8 = h8_getreg8(dstreg);
		bitnr = h8_getreg8((opcode >> 4) & 0xf) & 0x7;
		udata8 = h8_bnot8(bitnr, udata8);
		h8_setreg8(dstreg, udata8);
		h8_timing_fetch(AccessType, 1);
		break;
	case 0x2:
		//BCLR Rn, Rd
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata8 = h8_getreg8(dstreg);
		bitnr = h8_getreg8((opcode >> 4) & 0xf) & 0x7;
		udata8 = h8_bclr8(bitnr, udata8);
		h8_setreg8(dstreg, udata8);
		break;
	case 0x3:
		//BTST Rn, Rd
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata8 = h8_getreg8(dstreg);
		bitnr = h8_getreg8((opcode >> 4) & 0xf) & 0x7;
		h8_btst8(bitnr, udata8);
		break;
	case 0x4:
		//OR.w Rs,Rd
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata16 = h8_getreg16(dstreg);
		udata16 = h8_or16(h8_getreg16((opcode >> 4) & 0xf), udata16);
		h8_setreg16(dstreg, udata16);
		break;
	case 0x5:
		//XOR.w Rs,Rd
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata16 = h8_getreg16(dstreg);
		udata16 = h8_xor16(h8_getreg16((opcode >> 4) & 0xf), udata16);
		h8_setreg16(dstreg, udata16);
		break;
	case 0x6:
		//AND.w Rs,Rd
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata16 = h8_getreg16(dstreg);
		udata16 = h8_and16(h8_getreg16((opcode >> 4) & 0xf), udata16);
		h8_setreg16(dstreg, udata16);
		break;
	case 0x7:
		if ((opcode & 0x80) == 0) {
			//BST #xx:3,Rd
			h8_timing_fetch(AccessType, 1);
			udata8 = h8_getreg8(opcode & 0xf);
			udata8 = h8_bst8((opcode >> 4) & 0x7, udata8);
			h8_setreg8(opcode & 0xf, udata8);
		}
		else {
			//BIST #xx:3,Rd
			h8_timing_fetch(AccessType, 1);
			udata8 = h8_getreg8(opcode & 0xf);
			udata8 = h8_bist8((opcode >> 4) & 0x7, udata8);
			h8_setreg8(opcode & 0xf, udata8);
		}
		break;
	case 0x8:
		//MOV
		if (opcode & 0x80)
		{
			// mov.b Rs,@ERd
			h8_timing_fetch(AccessType, 1);
			udata8 = h8_getreg8(opcode & 0xf);
			address24 = h8_getreg32((opcode >> 4) & 0x7) & 0xffffff;
			h8_mov8(udata8);
			h8_mem_write8(address24, udata8);
			h8_timing_byte(AccessType, 1);
		}
		else
		{
			// mov.b @ERs,Rd
			h8_timing_fetch(AccessType, 1);
			address24 = h8_getreg32((opcode >> 4) & 0x7) & 0xffffff;
			udata8 = h8_mem_read8(address24);
			h8_timing_byte(AccessType, 1);
			h8_mov8(udata8);
			h8_setreg8(opcode & 0xf, udata8);
		}
		break;
	case 0x9:
		//MOV
		if (opcode & 0x80)
		{
			// mov.w Rs,@ERd
			h8_timing_fetch(AccessType, 1);
			address24 = h8_getreg32((opcode >> 4) & 0x7) & 0xffffff;
			udata16 = h8_getreg16(opcode & 0xf);
			h8_mov16(udata16);
			h8_mem_write16(address24, udata16);
			h8_timing_word(AccessType, 1);
		}
		else
		{
			// mov.w @ERs,Rd
			h8_timing_fetch(AccessType, 1);
			address24 = h8_getreg32((opcode >> 4) & 0x7) & 0xffffff;
			udata16 = h8_mem_read16(address24);
			h8_timing_word(AccessType, 1);
			h8_mov16(udata16);
			h8_setreg16(opcode & 0xf, udata16);
		}
		break;
	case 0xa:
		//MOV
		switch ((opcode >> 4) & 0xf)
		{
		case 0x0:
			// mov.b @aa:16,Rd
			h8_timing_fetch(AccessType, 2);
			address24 = (UINT32)getNextOpcode();
			if (address24 & 0x8000)
			{
				address24 |= 0xff0000;
			}

			udata8 = h8_mem_read8(address24);
			h8_timing_byte(AccessType, 1);
			h8_mov8(udata8); // flags only
			h8_setreg8(opcode & 0xf, udata8);
			break;
		case 0x2:
			// mov.b @aa:24,Rd
			h8_timing_fetch(AccessType, 3);
			address24 = h8_mem_read32(h8.pc) & 0xffffff;
			h8.pc += 4;
			udata8 = h8_mem_read8(address24);
			h8_timing_byte(AccessType, 1);
			h8_mov8(udata8); // flags only
			h8_setreg8(opcode & 0xf, udata8);
			break;
		case 0x4:
			// MOVFPE @aa:16,Rd. On H8/300H this is byte-only and,
			// for on-chip memory/register targets, is functionally the
			// same as MOV.B @aa:16,Rd.
			h8_timing_fetch(AccessType, 2);
			address24 = (UINT32)getNextOpcode();
			if (address24 & 0x8000)
			{
				address24 |= 0xff0000;
			}
			udata8 = h8_mem_read8(address24 & 0x00ffffff);
			h8_timing_byte(AccessType, 1);
			h8_mov8(udata8);
			h8_setreg8(opcode & 0xf, udata8);
			break;
		case 0x8:
			// mov.b Rs,@aa:16
			h8_timing_fetch(AccessType, 2);
			address24 = (UINT32)getNextOpcode();
			if (address24 & 0x8000)
			{
				address24 |= 0xff0000;
			}
			udata8 = h8_getreg8(opcode & 0xf);
			h8_mov8(udata8); // flags only
			h8_mem_write8(address24, udata8);
			h8_timing_byte(AccessType, 1);
			break;
		case 0xa:
			// mov.b Rs,@aa:24 //Unknown write!
			h8_timing_fetch(AccessType, 3);
			address24 = h8_mem_read32(h8.pc) & 0xffffff;
			h8.pc += 4;
			udata8 = h8_getreg8(opcode & 0xf);
			h8_mov8(udata8); // flags only
			h8_mem_write8(address24, udata8);
			h8_timing_byte(AccessType, 1);
			break;
		case 0xc:
			// MOVTPE Rs,@aa:16. Byte-only; for on-chip memory/register
			// destinations this is functionally MOV.B Rs,@aa:16.
			h8_timing_fetch(AccessType, 2);
			address24 = (UINT32)getNextOpcode();
			if (address24 & 0x8000)
			{
				address24 |= 0xff0000;
			}
			udata8 = h8_getreg8(opcode & 0xf);
			h8_mov8(udata8);
			h8_mem_write8(address24 & 0x00ffffff, udata8);
			h8_timing_byte(AccessType, 1);
			break;
		default:
			h8.err = 21;
			break;
		}
		break;
	case 0xb:
		//MOV
		switch ((opcode >> 4) & 0xf)
		{
		case 0x0:
			// mov.w @aa:16,Rd
			h8_timing_fetch(AccessType, 2);
			address24 = (UINT32)getNextOpcode();
			if (address24 & 0x8000)
			{
				address24 |= 0xff0000;
			}
			udata16 = h8_mem_read16(address24);
			h8_timing_word(AccessType, 1);
			h8_mov16(udata16); // flags only
			h8_setreg16(opcode & 0xf, udata16);
			break;
		case 0x2:
			// mov.w @aa:24, Rd
			h8_timing_fetch(AccessType, 3);
			address24 = h8_mem_read32(h8.pc) & 0xffffff;
			h8.pc += 4;
			udata16 = h8_mem_read16(address24);
			h8_timing_word(AccessType, 1);
			h8_mov16(udata16); // flags only
			h8_setreg16(opcode & 0xf, udata16);
			break;
		case 0x8:
			// mov.w Rs, @aa:16
			h8_timing_fetch(AccessType, 2);
			address24 = (UINT32)getNextOpcode();
			if (address24 & 0x8000)
			{
				address24 |= 0xff0000;
			}
			udata16 = h8_getreg16(opcode & 0xf);
			h8_mov16(udata16); // flags only
			h8_mem_write16(address24, udata16);
			h8_timing_word(AccessType, 1);
			break;
		case 0xa:
			// mov.w Rs, @aa:24
			h8_timing_fetch(AccessType, 3);
			address24 = h8_mem_read32(h8.pc) & 0xffffff;
			h8.pc += 4;
			udata16 = h8_getreg16(opcode & 0xf);
			h8_mov16(udata16); // flags only
			h8_mem_write16(address24, udata16);
			h8_timing_word(AccessType, 1);
			break;
		default:
			h8.err = 22;
			break;
		}
		break;
	case 0xc:
		//MOV
		if (opcode & 0x80)
		{
			// mov.b Rs, @-ERd
			h8_timing_fetch(AccessType, 1);
			dstreg = (opcode >> 4) & 0x7;
			h8_setreg32(dstreg, h8_getreg32(dstreg) - 1);
			address24 = h8_getreg32(dstreg) & 0xffffff;
			udata8 = h8_getreg8(opcode & 0xf);
			h8_mem_write8(address24, udata8);
			h8_timing_byte(AccessType, 1);
			h8_timing_internal(2);
		}
		else
		{
			// mov.b @ERs+, Rd
			h8_timing_fetch(AccessType, 1);
			srcreg = (opcode >> 4) & 0x7;
			address24 = h8_getreg32(srcreg) & 0xffffff;
			h8_setreg32(srcreg, h8_getreg32(srcreg) + 1);
			udata8 = h8_mem_read8(address24);
			h8_timing_byte(AccessType, 1);
			h8_setreg8(opcode & 0xf, udata8);
			h8_timing_internal(2);
		}
		h8_mov8(udata8);
		break;
	case 0xd:
		//MOV
		if (opcode & 0x80)
		{
			// mov.w Rs, @ERd
			h8_timing_fetch(AccessType, 1);
			dstreg = (opcode >> 4) & 0x7;
			h8_setreg32(dstreg, h8_getreg32(dstreg) - 2);
			address24 = h8_getreg32(dstreg) & 0xffffff;
			udata16 = h8_getreg16(opcode & 0xf);
			h8_mem_write16(address24, udata16);
			h8_timing_word(AccessType, 1);
			h8_timing_internal(2);
		}
		else
		{
			// mov.w @ERs+, Rd
			h8_timing_fetch(AccessType, 1);
			srcreg = (opcode >> 4) & 0x7;
			address24 = h8_getreg32(srcreg) & 0xffffff;
			h8_setreg32(srcreg, h8_getreg32(srcreg) + 2);
			udata16 = h8_mem_read16(address24);
			h8_timing_word(AccessType, 1);
			h8_setreg16(opcode & 0xf, udata16);
			h8_timing_internal(2);
		}
		h8_mov16(udata16);
		break;
	case 0xe:
		//MOV
		// TODO: Check sign extend
		h8_timing_fetch(AccessType, 2);
		sdata16 = getNextOpcode();
		address24 = h8_getreg32((opcode >> 4) & 0x7);
		address24 += sdata16;
		address24 &= 0xffffff;

		if (opcode & 0x80)
		{
			// mov.b Rs,@(d:16,ERd)
			udata8 = h8_getreg8(opcode & 0xf);
			h8_mem_write8(address24, udata8);
			h8_timing_byte(AccessType, 1);
		}
		else
		{
			// mov.b @(d:16,ERs),Rd
			udata8 = h8_mem_read8(address24);
			h8_timing_byte(AccessType, 1);
			h8_setreg8(opcode & 0xf, udata8);
		}
		h8_mov8(udata8);

		break;
	case 0xf:
		//MOV		
		h8_timing_fetch(AccessType, 2);

		// Get Signed 16 bit Displacement
		sdata16 = getNextOpcode();
		//
		address24 = h8_getreg32((opcode >> 4) & 0x7);
		address24 += sdata16;
		address24 &= 0xffffff;

		if (opcode & 0x80)
		{
			// mov.w Rs,@(d:16,ERd)			
			udata16 = h8_getreg16(opcode & 0xf);
			h8_mem_write16(address24, udata16);
			h8_timing_word(AccessType, 1);
		}
		else
		{
			// mov.w @(d:16,ERs),Rd
			udata16 = h8_mem_read16(address24);
			h8_timing_word(AccessType, 1);
			h8_setreg16(opcode & 0xf, udata16);
		}
		h8_mov16(udata16);
		break;
	default:
		h8.err = 23;
	}

}
void H83002::h8_group7new(UINT16 opcode) {

	UINT16 opcode2, udata16;
	UINT8 dstreg, bitnr, udata8;
	UINT32 udata32;

	switch ((opcode >> 8) & 0xf)
	{
	case 0x0:
		//BSET #xx:3,Rd
		if ((opcode & 0x80) != 0)
		{
			h8.err = 24;
		}
		else
		{
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0xf;
			udata8 = h8_getreg8(dstreg);
			bitnr = (opcode >> 4) & 7;
			udata8 = h8_bset8(bitnr, udata8);
			h8_setreg8(dstreg, udata8);
		}
		break;
	case 0x1:
		//BNOT #xx:3,Rd
		if ((opcode & 0x80) != 0)
		{
			h8.err = 25;
		}
		else
		{
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0xf;
			udata8 = h8_getreg8(dstreg);
			bitnr = (opcode >> 4) & 7;
			udata8 = h8_bnot8(bitnr, udata8);
			h8_setreg8(dstreg, udata8);
		}
		break;
	case 0x2:
		//BCLR #xx:3,Rd
		if ((opcode & 0x80) != 0)
		{
			h8.err = 26;
		}
		else
		{
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0xf;
			udata8 = h8_getreg8(dstreg);
			bitnr = (opcode >> 4) & 7;
			udata8 = h8_bclr8(bitnr, udata8);
			h8_setreg8(dstreg, udata8);
		}
		break;
	case 0x3:
		//BTST #xx:3,Rd
		if ((opcode & 0x80) != 0)
		{
			h8.err = 27;
		}
		else
		{
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0xf;
			udata8 = h8_getreg8(dstreg);
			bitnr = (opcode >> 4) & 7;
			h8_btst8(bitnr, udata8);
		}
		break;
	case 0x4:
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata8 = h8_getreg8(dstreg);
		bitnr = (opcode >> 4) & 7;
		if ((opcode & 0x80) == 0) {
			//BOR #xx:3,Rd
			h8_bor8(bitnr, udata8);
		}
		else {
			//BIOR #xx:3, Rd
			h8_bior8(bitnr, udata8);
		}
		break;
	case 0x5:
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata8 = h8_getreg8(dstreg);
		bitnr = (opcode >> 4) & 7;
		if ((opcode & 0x80) == 0) {
			//BXOR #xx:3,Rd
			h8_bxor8(bitnr, udata8);
		}
		else {
			//BIXOR #xx:3,Rd
			h8_bixor8(bitnr, udata8);
		}
		break;
	case 0x6:
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata8 = h8_getreg8(dstreg);
		bitnr = (opcode >> 4) & 7;
		if ((opcode & 0x80) == 0) {
			//BAND #xx:3,Rd
			h8_band8(bitnr, udata8);
		}
		else {
			//BIAND #xx:3,Rd
			h8_biand8(bitnr, udata8);
		}
		break;
	case 0x7:
		h8_timing_fetch(AccessType, 1);
		dstreg = opcode & 0xf;
		udata8 = h8_getreg8(dstreg);
		bitnr = (opcode >> 4) & 7;
		if ((opcode & 0x80) == 0) {
			//BLD #xx:3,Rd
			h8_bld8(bitnr, udata8);
		}
		else {
			//BILD #xx:3,Rd
			h8_bild8(bitnr, udata8);
		}
		break;
	case 0x8:
		//MOV
		h8_timing_fetch(AccessType, 4);
		udata16 = getNextOpcode();
		udata32 = h8_mem_read32(h8.pc) & 0xffffff;
		h8.pc += 4;

		if (opcode & 0x8f)
		{
			h8.err = 34;
			break;
		}

		if (((udata16 >> 8) & 0xff) == 0x6a)
		{
			// mov.b register indirect w/ displacement
			if (((udata16 >> 4) & 0xf) == 0xa)
			{
				// mov.b Rs,@(d:24,ERd)
				udata8 = h8_getreg8(udata16 & 0xf);
				h8_mov8(udata8); // update flags !
				udata32 += h8_getreg32((opcode >> 4) & 7);
				h8_mem_write8((udata32 & 0xffffff), udata8);
				h8_timing_byte(AccessType, 1);
			}
			else if (((udata16 >> 4) & 0xf) == 0x2)
			{
				// mov.b @(d:24,ERs),Rd
				udata32 += h8_getreg32((opcode >> 4) & 7);
				udata8 = h8_mem_read8((udata32 & 0xffffff));
				h8_timing_byte(AccessType, 1);
				h8_mov8(udata8); // update flags !
				h8_setreg8(udata16 & 0xf, udata8);
			}
			else
			{
				h8.err = 33;
			}
		}
		else if (((udata16 >> 8) & 0xff) == 0x6b)
		{
			// mov.w register indirect w/ displacement
			if (((udata16 >> 4) & 0xf) == 0xa)
			{
				// mov.w Rs,@(d:24,ERd)
				udata16 = h8_getreg16(udata16 & 0xf);
				h8_mov16(udata16); // update flags !
				udata32 += h8_getreg32((opcode >> 4) & 7);
				h8_mem_write16((udata32 & 0xffffff), udata16);
				h8_timing_word(AccessType, 1);
			}
			else if (((udata16 >> 4) & 0xf) == 0x2)
			{
				// mov.w @(d:24,ERs),Rd
				UINT8 dstreg = (udata16 & 0xf);
				udata32 += h8_getreg32((opcode >> 4) & 7);
				udata16 = h8_mem_read16(udata32 & 0xffffff);
				h8_timing_word(AccessType, 1);
				h8_mov16(udata16); // update flags !
				h8_setreg16(dstreg, udata16);
			}
			else
			{
				h8.err = 32;
			}
		}
		else
		{
			h8.err = 31;
		}
		break;
	case 0x9:
		//Table 2
		h8_table2(opcode);
		break;
	case 0xa:
		//Table 2
		h8_table2(opcode);
		break;
	case 0xb:
		//EEPMOV
		h8.pc += 2; //2nd opcode is unused
		h8_timing_fetch(AccessType, 2);
		if ((opcode & 0xff) == 0xd4)
		{
			// eepmov.w
			// TODO: Check for illegal EEPMOV format

			UINT16 cnt = h8_getreg16(0x4);	// R4

			while (cnt > 0)
			{
				// @ER5 -> @ER6
				udata8 = h8_mem_read8(h8.regs[0x5] & 0xffffff);
				h8_timing_byte(AccessType, 1);
				h8_mem_write8(h8.regs[0x6] & 0xffffff, udata8);
				h8_timing_byte(AccessType, 1);
				h8.regs[5]++;
				h8.regs[6]++;
				cnt--;
			}

			h8_timing_byte(AccessType, 2);

			// Record resulting count as none remaining
			h8_setreg16(0x4, 0);

		}
		else if ((opcode & 0xff) == 0x5c)
		{
			// eepmov.b

			// TODO: Check for illegal EEPMOV format

			UINT8 cnt = h8_getreg8(0xc);		// R4L

			while (cnt > 0)
			{
				// @ER5 -> @ER6
				udata8 = h8_mem_read8(h8.regs[0x5] & 0xffffff);
				h8_timing_byte(AccessType, 1);
				h8_mem_write8(h8.regs[6] & 0xffffff, udata8);
				h8_timing_byte(AccessType, 1);
				h8.regs[5]++;
				h8.regs[6]++;
				cnt--;
			}

			h8_timing_byte(AccessType, 2);

			// Record resulting count as none remaining
			h8_setreg8(0xc, 0);

		}
		else
		{
			h8.err = 30;
		}
		break;
	case 0xc:
		//Table 3
		opcode2 = getNextOpcode();
		h8_table3(opcode, opcode2);
		break;
	case 0xd:
		//Table 3
		opcode2 = getNextOpcode();
		h8_table3(opcode, opcode2);
		break;
	case 0xe:
		//Table 3
		opcode2 = getNextOpcode();
		h8_table3(opcode, opcode2);
		break;
	case 0xf:
		//Table 3
		opcode2 = getNextOpcode();
		h8_table3(opcode, opcode2);
		break;
	default:
		h8.err = 29;
	}

}

void H83002::h8_table2(UINT16 opcode) {

	UINT8 AHAL = (opcode >> 8);
	UINT8 BH = ((opcode >> 4) & 0xf);
	UINT8 dstreg, srcreg, udata8, CH;
	UINT16 opcode2, udata16, dst16;
	UINT32 address24, udata32, dst32;
	INT32 sdata32;
	INT16 sdata16;
	INT8 sdata8;

	switch (AHAL) {
	case 0x01:
		switch (BH) {
		case 0x00:
			//MOV
			opcode2 = getNextOpcode();
			switch ((opcode2 >> 8) & 0xff)
			{
			case 0x69:
				h8_timing_fetch(AccessType, 2);
				if ((opcode2 & 0x80) == 0x80)
				{
					// mov.l ERs,@ERd //Timing Checked
					udata32 = h8_mov32(h8_getreg32(opcode2 & 7));
					h8_mem_write32(h8_getreg32((opcode2 >> 4) & 7), udata32);
					h8_timing_word(AccessType, 2);
				}
				else
				{
					// mov.l @ERs,ERd //Timing Checked
					udata32 = h8_mem_read32(h8_getreg32((opcode2 >> 4) & 7));
					h8_timing_word(AccessType, 2);
					h8_mov32(udata32);
					h8_setreg32(opcode2 & 7, udata32);
				}
				break;
			case 0x6b:
				switch ((opcode2 >> 4) & 0xf)
				{
				case 0x0:
					h8_timing_fetch(AccessType, 3);
					// mov.l @aa:16, ERd //Listed in docs but wasn't implemented (doesn't appear to be used though)
					address24 = getNextOpcode();
					if (address24 & 0x8000)
					{
						address24 |= 0xff0000;
					}
					udata32 = h8_mem_read32(address24);
					h8_timing_word(AccessType, 2);
					h8_mov32(udata32); // flags only
					h8_setreg32(opcode2 & 0x7, udata32);

					break;
				case 0x2:
					h8_timing_fetch(AccessType, 4);
					// mov.l @aa:24,ERd //Timing Checked
					address24 = h8_mem_read32(h8.pc) & 0x00ffffff;
					h8.pc += 4;
					udata32 = h8_mem_read32(address24);
					h8_timing_word(AccessType, 2);
					h8_mov32(udata32); // flags only
					h8_setreg32(opcode2 & 0x7, udata32);
					break;
				case 0x8:
					h8_timing_fetch(AccessType, 3);
					// mov.l ERs,@aa:16 //Timing Checked
					address24 = h8_mem_read16(h8.pc);
					h8.pc += 2;
					if (address24 & 0x8000)
					{
						address24 |= 0xff0000;
					}
					udata32 = h8_getreg32(opcode2 & 0x7);
					h8_mov32(udata32); // flags only
					h8_mem_write32(address24, udata32);
					h8_timing_word(AccessType, 2);
					break;
				case 0xa:
					h8_timing_fetch(AccessType, 4);
					// mov.l ERs,@aa:24 //Timing Checked
					address24 = h8_mem_read32(h8.pc) & 0x00ffffff;
					h8.pc += 4;
					udata32 = h8_getreg32(opcode2 & 0x7);
					h8_mov32(udata32); // flags only
					h8_mem_write32(address24, udata32);
					h8_timing_word(AccessType, 2);
					break;
				default:
					h8.err = 28;
					break;
				}
				break;
			case 0x6d:
				h8_timing_fetch(AccessType, 2);
				if (opcode2 & 0x80)
				{
					// mov.l rs, @-erd //Timing Checked
					dstreg = (opcode2 >> 4) & 7;

					h8_setreg32(dstreg, h8_getreg32(dstreg) - 4);
					address24 = h8_getreg32(dstreg) & 0xffffff;

					udata32 = h8_getreg32(opcode2 & 0x7);
					h8_mem_write32(address24, udata32);
					h8_timing_word(AccessType, 2);
					h8_timing_internal(2);
				}
				else
				{
					// mov.l @ers+, rd //Timing Checked
					srcreg = (opcode2 >> 4) & 7;

					address24 = h8_getreg32(srcreg) & 0xffffff;
					h8_setreg32(srcreg, h8_getreg32(srcreg) + 4);

					udata32 = h8_mem_read32(address24);
					h8_timing_word(AccessType, 2);
					h8_setreg32(opcode2 & 0x7, udata32);
					h8_timing_internal(2);
				}

				h8_mov32(udata32);
				break;
			case 0x6f:
				h8_timing_fetch(AccessType, 3);
				// mov.l @(displ16 + Rs), rd //Timing Checked
				sdata16 = getNextOpcode(); // sign extend displacements !								
				address24 = (h8_getreg32((opcode2 >> 4) & 7)) & 0xffffff;
				address24 += sdata16;

				if (opcode2 & 0x80)
				{
					// mov.l ERs, @(d:16, ERd)
					udata32 = h8_getreg32(opcode2 & 0x7);
					h8_mem_write32(address24, udata32);
					h8_timing_word(AccessType, 2);
				}
				else
				{
					// mov.l @(d:16, ERs), ERd
					udata32 = h8_mem_read32(address24);
					h8_timing_word(AccessType, 2);
					h8_setreg32(opcode2 & 0x7, udata32);
				}
				h8_mov32(udata32);
				break;
			case 0x78:
				h8_timing_fetch(AccessType, 5);
				// prefix for
				// mov.l (@aa:x, rx), Rx
				srcreg = (opcode2 >> 4) & 7;

				// 6b20
				udata16 = getNextOpcode();
				dstreg = udata16 & 7;

				address24 = h8_mem_read32(h8.pc) & 0x00ffffff;
				h8.pc += 4;

				address24 += h8_getreg32(srcreg);
				address24 &= 0xffffff;

				if ((opcode2 & 0x80) && ((udata16 & ~7) == 0x6ba0))
				{
					udata32 = h8_getreg32(dstreg);
					h8_mem_write32(address24, udata32);
					h8_timing_word(AccessType, 2);
					h8_mov32(udata32);
				}
				else if ((!(opcode2 & 0x80)) && ((udata16 & ~7) == 0x6b20))
				{
					udata32 = h8_mem_read32(address24);
					h8_timing_word(AccessType, 2);
					h8_setreg32(dstreg, udata32);
					h8_mov32(udata32);
				}
				else
				{
					h8.err = 35;
				}
				break;
			default:
				h8.err = 36;
				break;
			}
			break;
		case 0x4:
			//LDC/STC
			opcode2 = getNextOpcode();
			CH = ((opcode2 >> 12) & 0xf);
			if (CH == 6) {
				//0x01406
				h8_table3(opcode, opcode2);
			}
			else if (CH == 7) {
				// LDC.W @(d:24,ERs),CCR / STC.W CCR,@(d:24,ERd)
				// Format: 0140 78r0 6B20/6BA0 0000dddd
				udata16 = getNextOpcode();
				srcreg = (opcode2 >> 4) & 0x7;
				udata32 = h8_mem_read32(h8.pc) & 0x00ffffff;
				h8.pc += 4;
				address24 = (h8_getreg32(srcreg) + udata32) & 0x00ffffff;
				h8_timing_fetch(AccessType, 5);
				if (udata16 == 0x6ba0) {
					h8_mem_write16(address24, (UINT16)h8_get_ccr());
					h8_timing_word(AccessType, 1);
					h8_timing_internal(2);
				}
				else if (udata16 == 0x6b20) {
					udata16 = h8_mem_read16(address24);
					h8_set_ccr8((UINT8)(udata16 & 0xff));
					h8_suppress_irq_once = 1;
					h8_timing_word(AccessType, 1);
					h8_timing_internal(2);
				}
				else {
					h8.err = 38;
				}
			}
			break;
		case 0x8:
			// SLEEP is valid only as 0x0180 in this group.
			// It stops instruction execution until an interrupt/exception.
			if (opcode & 0x000f) {
				h8.err = 45;
				break;
			}
			h8_timing_fetch(AccessType, 1);
			h8_sleeping = 1;
			break;
		case 0xc:
			//Table 3 0x01C
			opcode2 = getNextOpcode();
			h8_table3(opcode, opcode2);
			break;
		case 0xd:
			//Table 3 0x01D
			opcode2 = getNextOpcode();
			h8_table3(opcode, opcode2);
			break;
		case 0xf:
			//Table 3 0x01F
			opcode2 = getNextOpcode();
			h8_table3(opcode, opcode2);
			break;
		default:
			//Error
			h8.err = 45;
		}
		break;
	case 0x0A:
		switch (BH) {
		case 0x0:
			//INC			
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0xf;
			udata8 = h8_inc8(h8_getreg8(dstreg));
			h8_setreg8(dstreg, udata8);
			break;
		case 0x8://ADD.l Rs,ERd
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0x7;
			udata32 = h8_add32(h8_getreg32((opcode >> 4) & 0x7), h8_getreg32(dstreg));
			h8_setreg32(dstreg, udata32);
			break;
		default:
			//Error
			h8.err = 101;
		}
		break;
	case 0x0B:
		switch (BH) {
		case 0x0:
			//ADDS #1,ERd
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 7;
			udata32 = h8_getreg32(dstreg) + 1;
			h8_setreg32(dstreg, udata32);
			break;
		case 0x5:
			//INC.w #1, Rd
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0xf;
			udata16 = h8_inc16(h8_getreg16(dstreg), 1);
			h8_setreg16(dstreg, udata16);
			break;
		case 0x7:
			//INC.l #1, ERd
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0x7;
			udata32 = h8_inc32(h8_getreg32(dstreg), 1);
			h8_setreg32(dstreg, udata32);
			break;
		case 0x8:
			//ADDS #2,ERd
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 7;
			udata32 = h8_getreg32(dstreg) + 2;
			h8_setreg32(dstreg, udata32);
			break;
		case 0x9:
			//ADDS #4,ERd
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 7;
			udata32 = h8_getreg32(dstreg) + 4;
			h8_setreg32(dstreg, udata32);
			break;
		case 0xd:
			//INC.w #2, Rd
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0xf;
			udata16 = h8_inc16(h8_getreg16(dstreg), 2);
			h8_setreg16(dstreg, udata16);
			break;
		case 0xf:
			//INC.l #2, ERd			
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0x7;
			udata32 = h8_inc32(h8_getreg32(dstreg), 2);
			h8_setreg32(dstreg, udata32);
			break;
		default:
			//ERROR
			h8.err = 46;
		}
		break;
	case 0x0F:
		switch (BH) {
		case 0x0:
			// DAA.b Rd
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0x0f;
			udata8 = h8_daa8(h8_getreg8(dstreg));
			h8_setreg8(dstreg, udata8);
			break;
		case 0x8:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			//MOV.l ERs,ERd //Timing Checked
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0x7;
			udata32 = h8_mov32(h8_getreg32((opcode >> 4) & 0x7));
			h8_setreg32(dstreg, udata32);
			break;
		default:
			//
			h8.err = 48;
		}
		break;
	case 0x10:
		switch (BH) {
		case 0x0:
			// SHLL.b Rd
			h8_timing_fetch(AccessType, 1);
			udata8 = h8_getreg8(opcode & 0xf);
			udata8 = h8_shll8(udata8);
			h8_setreg8(opcode & 0xf, udata8);
			break;
		case 0x1:
			// SHLL.w Rd
			h8_timing_fetch(AccessType, 1);
			udata16 = h8_getreg16(opcode & 0xf);
			udata16 = h8_shll16(udata16);
			h8_setreg16(opcode & 0xf, udata16);
			break;
		case 0x3:
			// SHLL.l ERd
			h8_timing_fetch(AccessType, 1);
			udata32 = h8_getreg32(opcode & 0x7);
			udata32 = h8_shll32(udata32);
			h8_setreg32(opcode & 0x7, udata32);
			break;
		case 0x8:
			// SHAL.b Rd
			h8_timing_fetch(AccessType, 1);
			udata8 = h8_getreg8(opcode & 0xf);
			udata8 = h8_shal8(udata8);
			h8_setreg8(opcode & 0xf, udata8);
			break;
		case 0x9:
			// SHAL.w Rd
			h8_timing_fetch(AccessType, 1);
			udata16 = h8_getreg16(opcode & 0xf);
			udata16 = h8_shal16(udata16);
			h8_setreg16(opcode & 0xf, udata16);
			break;
		case 0xb:
			// SHAL.l ERd
			h8_timing_fetch(AccessType, 1);
			udata32 = h8_getreg32(opcode & 0x7);
			udata32 = h8_shal32(udata32);
			h8_setreg32(opcode & 0x7, udata32);
			break;
		default:
			//
			h8.err = 49;
		}
		break;
	case 0x11:
		switch (BH) {
		case 0x0:
			// SHLR.b rx
			h8_timing_fetch(AccessType, 1);
			udata8 = h8_getreg8(opcode & 0xf);
			udata8 = h8_shlr8(udata8);
			h8_setreg8(opcode & 0xf, udata8);
			break;
		case 0x1:
			// SHLR.w rx
			h8_timing_fetch(AccessType, 1);
			udata16 = h8_getreg16(opcode & 0xf);
			udata16 = h8_shlr16(udata16);
			h8_setreg16(opcode & 0xf, udata16);
			break;
		case 0x3:// SHLR.l rx
			h8_timing_fetch(AccessType, 1);
			udata32 = h8_getreg32(opcode & 0x7);
			udata32 = h8_shlr32(udata32);
			h8_setreg32(opcode & 0x7, udata32);
			break;
		case 0x8:// SHAR.b rx
			h8_timing_fetch(AccessType, 1);
			udata8 = h8_getreg8(opcode & 0xf);
			udata8 = h8_shar8(udata8);
			h8_setreg8(opcode & 0xf, udata8);
			break;
		case 0x9:
			// SHAR.w rx
			h8_timing_fetch(AccessType, 1);
			udata16 = h8_getreg16(opcode & 0xf);
			udata16 = h8_shar16(udata16);
			h8_setreg16(opcode & 0xf, udata16);
			break;
		case 0xb:
			// SHAR.l rx
			h8_timing_fetch(AccessType, 1);
			udata32 = h8_getreg32(opcode & 0x7);
			udata32 = h8_shar32(udata32);
			h8_setreg32(opcode & 0x7, udata32);
			break;
		default:
			//
			h8.err = 50;
		}
		break;
	case 0x12:
		switch (BH) {
		case 0x0:
			// rotxl.b Rx
			h8_timing_fetch(AccessType, 1);
			udata8 = h8_getreg8(opcode & 0xf);
			udata8 = h8_rotxl8(udata8);
			h8_setreg8(opcode & 0xf, udata8);
			break;
		case 0x1:
			// rotxl.w Rx
			h8_timing_fetch(AccessType, 1);
			udata16 = h8_getreg16(opcode & 0xf);
			udata16 = h8_rotxl16(udata16);
			h8_setreg16(opcode & 0xf, udata16);
			break;
		case 0x3:
			// rotxl.l Rx
			udata32 = h8_getreg32(opcode & 0x7);
			h8_timing_fetch(AccessType, 1);
			udata32 = h8_rotxl32(udata32);
			// MAME has this as & 0xf but that must be a mi
			// 
			// 
			// ?
			h8_setreg32(opcode & 0x7, udata32);
			break;
		case 0x8:
			// rotl.b Rx
			h8_timing_fetch(AccessType, 1);
			udata8 = h8_getreg8(opcode & 0xf);
			udata8 = h8_rotl8(udata8);
			h8_setreg8(opcode & 0xf, udata8);
			break;
		case 0x9:
			// rotl.w Rx
			h8_timing_fetch(AccessType, 1);
			udata16 = h8_getreg16(opcode & 0xf);
			udata16 = h8_rotl16(udata16);
			h8_setreg16(opcode & 0xf, udata16);
			break;
		case 0xb:
			// rotl.l Rx
			h8_timing_fetch(AccessType, 1);
			udata32 = h8_getreg32(opcode & 0x7);
			udata32 = h8_rotl32(udata32);
			h8_setreg32(opcode & 0x7, udata32);
			break;
		default:
			h8.err = 51;
			break;
		}
		break;
	case 0x13:
		switch (BH) {
		case 0x0:
			// rotxr.b Rx
			h8_timing_fetch(AccessType, 1);
			udata8 = h8_getreg8(opcode & 0xf);
			udata8 = h8_rotxr8(udata8);
			h8_setreg8(opcode & 0xf, udata8);
			break;
		case 0x1:
			// rotxr.w Rx
			h8_timing_fetch(AccessType, 1);
			udata16 = h8_getreg16(opcode & 0xf);
			udata16 = h8_rotxr16(udata16);
			h8_setreg16(opcode & 0xf, udata16);
			break;
		case 0x3:
			// rotxr.l ERx
			h8_timing_fetch(AccessType, 1);
			udata32 = h8_getreg32(opcode & 0x7);
			udata32 = h8_rotxr32(udata32);
			h8_setreg32(opcode & 0x7, udata32);
			break;
		case 0x8:
			// rotr.b Rx
			h8_timing_fetch(AccessType, 1);
			udata8 = h8_getreg8(opcode & 0xf);
			udata8 = h8_rotr8(udata8);
			h8_setreg8(opcode & 0xf, udata8);
			break;
		case 0x9:
			// rotr.w Rx
			h8_timing_fetch(AccessType, 1);
			udata16 = h8_getreg16(opcode & 0xf);
			udata16 = h8_rotr16(udata16);
			h8_setreg16(opcode & 0xf, udata16);
			break;
		case 0xb:
			// rotr.l ERx
			h8_timing_fetch(AccessType, 1);
			udata32 = h8_getreg32(opcode & 0x7);
			udata32 = h8_rotr32(udata32);
			h8_setreg32(opcode & 0x7, udata32);
			break;
		default:
			h8.err = 52;
			break;
		}
		break;
	case 0x17:
		switch (BH) {
		case 0x0:
			// not.b Rx
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0xf;
			udata8 = h8_not8(h8_getreg8(dstreg));
			h8_setreg8(dstreg, udata8);
			break;
		case 0x1:
			// not.w Rx
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0xf;
			udata16 = h8_not16(h8_getreg16(dstreg));
			h8_setreg16(dstreg, udata16);
			break;
		case 0x3:
			// not.l ERx
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0x7;
			udata32 = h8_not32(h8_getreg32(dstreg));
			h8_setreg32(dstreg, udata32);
			break;
		case 0x5:
			// extu.w Rx
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0xf;
			udata16 = h8_getreg16(dstreg) & 0xff;
			h8_setreg16(dstreg, udata16);
			h8.h8nflag = 0;
			h8.h8vflag = 0;
			h8.h8zflag = ((udata16 == 0) ? 1 : 0);
			break;
		case 0x7:
			// extu.l Rx
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0x7;
			udata32 = h8_getreg32(dstreg) & 0xffff;
			h8_setreg32(dstreg, udata32);
			h8.h8nflag = 0;
			h8.h8vflag = 0;
			h8.h8zflag = ((udata32 == 0) ? 1 : 0);
			break;
		case 0x8:
			// neg.b Rx
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0xf;
			sdata8 = h8_neg8(h8_getreg8(dstreg));
			h8_setreg8(dstreg, sdata8);
			break;
		case 0x9:
			// neg.w Rx
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0xf;
			sdata16 = h8_neg16(h8_getreg16(dstreg));
			h8_setreg16(dstreg, sdata16);
			break;
		case 0xb:
			// neg.l ERx
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0x7;
			sdata32 = h8_neg32(h8_getreg32(dstreg));
			h8_setreg32(dstreg, sdata32);
			break;
		case 0xd:
			// exts.w
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0xf;
			udata16 = h8_getreg16(dstreg) & 0xff;

			if (udata16 & 0x80)
			{
				udata16 |= 0xff00;
			}
			h8_setreg16(dstreg, udata16);
			h8.h8vflag = 0;
			h8.h8nflag = (udata16 & 0x8000) ? 1 : 0;
			h8.h8zflag = (udata16) ? 0 : 1;
			break;
		case 0xf:
			// exts.l Rx
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0x7;
			udata32 = h8_getreg32(dstreg) & 0xffff;
			if (udata32 & 0x8000)
			{
				udata32 |= 0xffff0000;
			}
			h8_setreg32(dstreg, udata32);
			h8.h8vflag = 0;
			h8.h8nflag = (udata32 & 0x80000000) ? 1 : 0;
			h8.h8zflag = (udata32) ? 0 : 1;
			break;
		default:
			//
			h8.err = 53;
		}
		break;
	case 0x1a:
		switch (BH) {
		case 0x0:
			//DEC.b Rd
			h8_timing_fetch(AccessType, 1);
			udata8 = h8_getreg8(opcode & 0xf);
			udata8 = h8_dec8(udata8);
			h8_setreg8(opcode & 0xf, udata8);
			break;
		case 0x8://SUB.l ERs,ERd			
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0x7;
			udata32 = h8_sub32(h8_getreg32((opcode >> 4) & 0x7), h8_getreg32(dstreg));
			h8_setreg32(dstreg, udata32);
			break;
		default:
			//ERROR
			h8.err = 54;
		}
		break;
	case 0x1b:
		switch (BH) {
		case 0:	// subs.l #1, rN (decrement without touching flags)
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0x7;
			udata32 = h8_getreg32(dstreg);
			udata32--;
			h8_setreg32(dstreg, udata32);
			break;
		case 5:	// dec.w #1, rN
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0xf;
			udata16 = h8_dec16(h8_getreg16(dstreg));
			h8_setreg16(dstreg, udata16);
			break;
		case 7:	// dec.l #1, rN
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0x7;
			udata32 = h8_dec32(h8_getreg32(dstreg));
			h8_setreg32(dstreg, udata32);
			break;
		case 8:	// subs.l #2,rN
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 7;
			udata32 = h8_getreg32(dstreg);
			udata32 -= 2;
			h8_setreg32(dstreg, udata32);
			break;
		case 9: 	// subs.l #4, rN
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 7;
			udata32 = h8_getreg32(dstreg);
			udata32 -= 4;
			h8_setreg32(dstreg, udata32);
			break;
		case 0xd:	// dec.w #2, rN
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0xf;
			udata16 = h8_dec16(h8_getreg16(dstreg));
			if (h8.h8vflag)
			{
				udata16 = h8_dec16(udata16);
				h8.h8vflag = 1;
			}
			else
			{
				udata16 = h8_dec16(udata16);
			}
			h8_setreg16(dstreg, udata16);
			break;
		case 0x0f:	// dec.l #2, rN
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0x7;
			udata32 = h8_dec32(h8_getreg32(dstreg));
			if (h8.h8vflag)
			{
				udata32 = h8_dec32(udata32);
				h8.h8vflag = 1;
			}
			else
			{
				udata32 = h8_dec32(udata32);
			}
			h8_setreg32(dstreg, udata32);
			break;
		default:
			//
			h8.err = 55;
		}
		break;
	case 0x1f:
		switch (BH) {
		case 0x0:
			// DAS.b Rd
			h8_timing_fetch(AccessType, 1);
			dstreg = opcode & 0x0f;
			udata8 = h8_das8(h8_getreg8(dstreg));
			h8_setreg8(dstreg, udata8);
			break;
		case 0x8: //CMP.l ERs, ERd
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			h8_timing_fetch(AccessType, 1);
			h8_cmp32(h8_getreg32((opcode >> 4) & 0x7), h8_getreg32(opcode & 0x7));
			break;
		default:
			//
			h8.err = 57;
		}
		break;
	case 0x58:
		//Branches: 16 bit signed offset
		if (opcode & 0xf)
		{
			h8.err = 58;
		}
		else
		{
			h8_timing_fetch(AccessType, 2);
			sdata16 = getNextOpcode();

			if (h8_branch((opcode >> 4) & 0xf))
			{
				h8.pc += sdata16;
				h8.pc &= 0xffffff;
			}

			h8_timing_internal(2);

			// Note: Branch destination should be even
			if (h8.pc & 1)
			{
				h8.err = 59;
			}
		}
		break;
	case 0x79:
		udata16 = getNextOpcode();
		h8_timing_fetch(AccessType, 2);
		dstreg = opcode & 0xf;
		dst16 = h8_getreg16(dstreg);
		switch (BH) {
		case 0:
			// mov.w #xx:16,Rd
			dst16 = h8_mov16(udata16);
			h8_setreg16(dstreg, dst16);
			break;
		case 1:
			// add.w #xx:16,Rd
			dst16 = h8_add16(udata16, dst16);
			h8_setreg16(dstreg, dst16);
			break;
		case 2:
			// cmp.w #xx:16,Rd						
			h8_cmp16(udata16, dst16);
			break;
		case 3:
			// sub.w #xx:16,Rd
			dst16 = h8_sub16(udata16, dst16);
			h8_setreg16(dstreg, dst16);
			break;
		case 4:
			// or.w #xx:16,Rd
			dst16 = h8_or16(udata16, dst16);
			h8_setreg16(dstreg, dst16);
			break;
		case 5:
			// xor.w #xx:16,Rd
			dst16 = h8_xor16(udata16, dst16);
			h8_setreg16(dstreg, dst16);
			break;
		case 6:
			// and.w #xx:16,Rd
			dst16 = h8_and16(udata16, dst16);
			h8_setreg16(dstreg, dst16);
			break;
		default:
			//ERROR
			h8.err = 60;
		}
		break;
	case 0x7a:

		udata32 = h8_mem_read32(h8.pc);
		h8_timing_fetch(AccessType, 3);
		dstreg = opcode & 0x7;
		h8.pc += 4;
		dst32 = h8_getreg32(dstreg);

		switch (BH) {
		case 0:
			// mov.l #aa:32,ERd
			dst32 = h8_mov32(udata32);
			h8_setreg32(dstreg, dst32);
			break;
		case 1:
			// add.l #aa:32,ERd
			dst32 = h8_add32(udata32, dst32);
			h8_setreg32(dstreg, dst32);
			break;
		case 2:
			// cmp.l #aa:32,ERd
			h8_cmp32(udata32, dst32);
			break;
		case 3:
			// sub.l #aa:32,ERd
			dst32 = h8_sub32(udata32, dst32);
			h8_setreg32(dstreg, dst32);
			break;
		case 4:
			// or.l #aa:32,ERd
			dst32 = h8_or32(udata32, dst32);
			h8_setreg32(dstreg, dst32);
			break;
		case 5:
			// xor.l #aa:32,ERd
			dst32 = h8_xor32(udata32, dst32);
			h8_setreg32(dstreg, dst32);
			break;
		case 6:
			// and.l #aa:32,ERd
			dst32 = h8_and32(udata32, dst32);
			h8_setreg32(dstreg, dst32);
			break;
		default:
			//ERROR
			h8.err = 61;
		}
		break;
	default:
		h8.err = 62;
		break;
	}
}

void H83002::h8_table3(UINT16 opcode, UINT16 opcode2)
{
	UINT32 opcodes = (((UINT32)opcode << 4) | (opcode2 >> 12));

	UINT8 CH = ((opcode2 >> 12) & 0xf);
	UINT8 CL = ((opcode2 >> 8) & 0xf);
	UINT8 DH = ((opcode2 >> 4) & 0xf);

	UINT8 srcreg, udata8, bitmask;
	UINT16 udata16;
	UINT32 address24, udata32;
	INT32 sdata32;
	INT16 sdata16;
	INT8 sdata8;


	switch (opcodes) {
	case 0x01406:
		switch (CL) {
		case 0x9:
			// LDC.W @ERs,CCR / STC.W CCR,@ERd
			h8_timing_fetch(AccessType, 2);
			srcreg = DH & 0x7;
			address24 = h8_getreg32(srcreg) & 0x00ffffff;
			if (DH & 0x8) {
				// STC.W CCR,@ERd
				h8_mem_write16(address24, (UINT16)h8_get_ccr());
			}
			else {
				// LDC.W @ERs,CCR
				udata16 = h8_mem_read16(address24);
				h8_set_ccr8((UINT8)(udata16 & 0xff));
				h8_suppress_irq_once = 1;
			}
			h8_timing_word(AccessType, 1);
			h8_timing_internal(2);
			break;
		case 0xb:
			// LDC/STC absolute CCR forms. DH bit 3 selects STC; DH low bits
			// select absolute size: 0=@aa:16, 2=@aa:24.
			if ((DH & 0x7) == 0x0) {
				h8_timing_fetch(AccessType, 3);
				address24 = (UINT32)getNextOpcode();
				if (address24 & 0x8000)
					address24 |= 0xff0000;
			}
			else if ((DH & 0x7) == 0x2) {
				h8_timing_fetch(AccessType, 4);
				address24 = h8_mem_read32(h8.pc) & 0x00ffffff;
				h8.pc += 4;
			}
			else {
				h8.err = 69;
				break;
			}
			if (DH & 0x8) {
				// STC.W CCR,@aa
				h8_mem_write16(address24 & 0x00ffffff, (UINT16)h8_get_ccr());
			}
			else {
				// LDC.W @aa,CCR
				udata16 = h8_mem_read16(address24 & 0x00ffffff);
				h8_set_ccr8((UINT8)(udata16 & 0xff));
				h8_suppress_irq_once = 1;
			}
			h8_timing_word(AccessType, 1);
			h8_timing_internal(2);
			break;
		case 0xd:
			if (DH & 0x8) {
				//STC.w CCR, @-ERd
				h8_timing_fetch(AccessType, 2);
				srcreg = (opcode2 >> 4) & 7;
				h8_setreg32(srcreg, h8_getreg32(srcreg) - 2);
				address24 = h8_getreg32(srcreg) & 0xffffff;
				h8_mem_write16(address24, h8_get_ccr());
				h8_timing_word(AccessType, 1);
				h8_timing_internal(2);
			}
			else {
				//LDC.w @ERs+, CCR
				h8_timing_fetch(AccessType, 2);
				srcreg = (opcode2 >> 4) & 7;
				address24 = h8_getreg32(srcreg) & 0xffffff;
				h8_setreg32(srcreg, h8_getreg32(srcreg) + 2);
				udata16 = h8_mem_read16(address24);
				h8_set_ccr8((udata16 & 0xff));
				h8_suppress_irq_once = 1;
				h8_timing_word(AccessType, 1);
				h8_timing_internal(2);
			}
			break;
		case 0xf:
			// LDC.W @(d:16,ERs),CCR / STC.W CCR,@(d:16,ERd)
			h8_timing_fetch(AccessType, 3);
			srcreg = DH & 0x7;
			sdata16 = (INT16)getNextOpcode();
			address24 = (h8_getreg32(srcreg) + sdata16) & 0x00ffffff;
			if (DH & 0x8) {
				h8_mem_write16(address24, (UINT16)h8_get_ccr());
			}
			else {
				udata16 = h8_mem_read16(address24);
				h8_set_ccr8((UINT8)(udata16 & 0xff));
				h8_suppress_irq_once = 1;
			}
			h8_timing_word(AccessType, 1);
			h8_timing_internal(2);
			break;
		default:
			h8.err = 69;
		}
		return;
	case 0x01C05:
		switch (CL) {
		case 0x0:
			//MULXS.b Rs, Rd
			h8_timing_fetch(AccessType, 2);
			sdata16 = h8_getreg16(opcode2 & 0x0f);
			sdata8 = h8_getreg8((opcode2 >> 4) & 0xf);
			sdata16 = h8_mulxs8(sdata8, (sdata16 & 0xff));
			h8_setreg16(opcode2 & 0x0f, sdata16);
			h8_timing_internal(12);
			break;
		case 0x2:
			//MULXS.w Rs, Erd
			h8_timing_fetch(AccessType, 2);
			sdata32 = h8_getreg32(opcode2 & 0x7);
			sdata16 = h8_getreg16((opcode2 >> 4) & 0xf);
			sdata32 = h8_mulxs16(sdata16, sdata32);
			h8_setreg32(opcode2 & 0x7, sdata32);
			h8_timing_internal(20);
			break;
		default:
			h8.err = 70;
		}
		return;
	case 0x01D05:
		switch (CL) {
		case 0x1:
			//DIVXS.B Rs, Rd
			h8_timing_fetch(AccessType, 2);
			sdata16 = h8_getreg16(opcode2 & 0x0f);
			sdata8 = h8_getreg8((opcode2 >> 4) & 0x0f);
			sdata16 = h8_divxs8(sdata8, sdata16);
			h8_setreg16(opcode2 & 0x0f, sdata16);
			h8_timing_internal(12);
			break;
		case 0x3:
			//DIVXS.W Rs, ERd
			h8_timing_fetch(AccessType, 2);
			sdata32 = h8_getreg32(opcode2 & 0x7);
			sdata16 = h8_getreg16((opcode2 >> 4) & 0xf);
			sdata32 = h8_divxs16(sdata16, sdata32);
			h8_setreg32(opcode2 & 0x7, sdata32);
			h8_timing_internal(20);
			break;
		default:
			h8.err = 71;
		}
		return;
	case 0x01F06:
		switch (CL) {
		case 0x4:
			//OR.l ERs,ERd //Timing Checked
			h8_timing_fetch(AccessType, 2);
			udata32 = h8_or32(h8_getreg32((opcode2 >> 4) & 0x7), h8_getreg32(opcode2 & 0x7));
			h8_setreg32(opcode2 & 0x7, udata32);
			break;
		case 0x5:
			//XOR.l ERs,ERd //Timing Checked
			h8_timing_fetch(AccessType, 2);
			udata32 = h8_xor32(h8_getreg32((opcode2 >> 4) & 0x7), h8_getreg32(opcode2 & 0x7));
			h8_setreg32(opcode2 & 0x7, udata32);
			break;
		case 0x6:
			//AND.l ERs,ERd //Timing Checked
			h8_timing_fetch(AccessType, 2);
			udata32 = h8_and32(h8_getreg32((opcode2 >> 4) & 0x7), h8_getreg32(opcode2 & 0x7));
			h8_setreg32(opcode2 & 0x7, udata32);
			break;
		default:
			h8.err = 72;
		}
		return;
	}
	h8_timing_fetch(AccessType, 2);

	switch (opcodes & 0xff0ff) {//Mask BH (Register Field)
	case 0x7C006:
		address24 = h8_getreg32((opcode >> 4) & 0x7) & 0x00ffffff;
		udata8 = h8_mem_read8(address24);
		switch (CL) {
		case 0x3:
			//BTST.b Rn, @ERd				
			udata8 >>= (h8_getreg8(DH) & 7);
			udata8 &= 0x1;
			h8.h8zflag = udata8 ? 0 : 1;
			h8_timing_byte(AccessType, 1);
			break;
		default:
			h8.err = 73;
		}
		return;
	case 0x7C007:
		address24 = h8_getreg32((opcode >> 4) & 0x7) & 0x00ffffff;
		udata8 = h8_mem_read8(address24);
		switch (CL) {
		case 0x3:
			//BTST.b #xx:3, @ERd
			udata8 >>= (DH & 7);
			udata8 &= 0x1;
			h8.h8zflag = udata8 ? 0 : 1;
			h8_timing_byte(AccessType, 1);
			break;
		case 0x4:
			if (DH & 0x8) {
				//BIOR.b #xx:3, @ERd
				udata8 >>= (DH & 7);
				udata8 &= 0x1;
				h8.h8cflag |= (udata8 ^ 1);
				h8_timing_byte(AccessType, 1);
			}
			else {
				//BOR.b #xx:3, @ERd
				udata8 >>= (DH & 7);
				udata8 &= 0x1;
				h8.h8cflag |= udata8;
				h8_timing_byte(AccessType, 1);
			}
			break;
		case 0x5:
			if (DH & 0x8) {
				//BIXOR.b #xx:3,@ERd
				udata8 >>= (DH & 7);
				udata8 &= 0x1;
				h8.h8cflag ^= (udata8 ^ 1);
				h8_timing_byte(AccessType, 1);
			}
			else {
				//BXOR.b #xx:3,@ERd
				udata8 >>= (DH & 7);
				udata8 &= 0x1;
				h8.h8cflag ^= udata8;
				h8_timing_byte(AccessType, 1);
			}
			break;
		case 0x6:
			if (DH & 0x8) {
				//BIAND.b #xx:3,@ERd
				udata8 >>= (DH & 7);
				udata8 &= 0x1;
				h8.h8cflag &= (udata8 ^ 1);
				h8_timing_byte(AccessType, 1);
			}
			else {
				//BAND.b #xx:3,@ERd
				udata8 >>= (DH & 7);
				udata8 &= 0x1;
				h8.h8cflag &= udata8;
				h8_timing_byte(AccessType, 1);
			}
			break;
		case 0x7:
			if (DH & 0x8) {
				//BILD.b #xx:3,@ERd
				udata8 >>= (DH & 7);
				udata8 &= 0x1;
				h8.h8cflag = (udata8 ^ 1);
				h8_timing_byte(AccessType, 1);
			}
			else {
				//BLD.b #xx:3,@ERd
				udata8 >>= (DH & 7);
				udata8 &= 0x1;
				h8.h8cflag = udata8;
				h8_timing_byte(AccessType, 1);
			}
			break;
		default:
			h8.err = 74;
		}
		return;
	case 0x7D006:
		address24 = h8_getreg32((opcode >> 4) & 0x7) & 0x00ffffff;
		udata8 = h8_mem_read8(address24);
		switch (CL) {
		case 0x0:
			//BSET.b Rn, @ERd
			h8_timing_byte(AccessType, 1);
			udata8 = h8_bset8((h8_getreg8(DH) & 7), udata8);
			h8_mem_write8(address24, udata8);
			h8_timing_byte(AccessType, 1);
			break;
		case 0x1:
			//BNOT.b Rn, @ERd
			h8_timing_byte(AccessType, 1);
			bitmask = ((1 << (h8_getreg8(DH) & 7)));
			if (udata8 & bitmask) {
				//Clear bit
				udata8 &= (UINT8)(~bitmask);
			}
			else {
				//Set bit
				udata8 |= (bitmask);
			}
			h8_mem_write8(address24, udata8);
			h8_timing_byte(AccessType, 1);
			break;
		case 0x2:
			//BCLR.b Rn, @ERd
			h8_timing_byte(AccessType, 1);
			udata8 = h8_bclr8((h8_getreg8(DH) & 7), udata8);
			h8_mem_write8(address24, udata8);
			h8_timing_byte(AccessType, 1);
			break;
		case 0x7:
			if (DH & 0x8) {
				//BIST.b #xx:3,@ERd
				bitmask = (1 << (DH & 7));
				if (h8.h8cflag) {
					//Clear Bit
					udata8 &= (UINT8)(~bitmask);
				}
				else {
					//Set Bit
					udata8 |= (bitmask);
				}
				h8_mem_write8(address24, udata8);
				h8_timing_byte(AccessType, 1);
			}
			else {
				//BST.b #xx:3,@ERd
				bitmask = (1 << (DH & 7));
				if (h8.h8cflag) {
					//Set Bit
					udata8 |= (bitmask);
				}
				else {
					//Clear Bit
					udata8 &= (UINT8)(~bitmask);
				}
				h8_mem_write8(address24, udata8);
				h8_timing_byte(AccessType, 1);
			}
			break;
		default:
			h8.err = 75;
		}
		return;
	case 0x7D007:
		address24 = h8_getreg32((opcode >> 4) & 0x7) & 0x00ffffff;
		udata8 = h8_mem_read8(address24);
		switch (CL) {
		case 0x0:
			//BSET.b #xx:3,@ERd	
			h8_timing_byte(AccessType, 1);
			udata8 = h8_bset8((DH & 7), udata8);
			h8_mem_write8(address24, udata8);
			h8_timing_byte(AccessType, 1);
			break;
		case 0x1:
			//BNOT.b #xx:3,@ERd
			h8_timing_byte(AccessType, 1);
			bitmask = ((1 << (DH & 7)));
			if (udata8 & bitmask) {
				//Clear bit
				udata8 &= (UINT8)(~bitmask);
			}
			else {
				//Set bit
				udata8 |= (bitmask);
			}
			h8_mem_write8(address24, udata8);
			h8_timing_byte(AccessType, 1);
			break;
		case 0x2:
			//BCLR.b #xx:3,@ERd
			h8_timing_byte(AccessType, 1);
			udata8 = h8_bclr8(DH & 7, udata8);
			h8_mem_write8(address24, udata8);
			h8_timing_byte(AccessType, 1);
			break;
		default:
			h8.err = 76;
		}
		return;
	}

	// Absolute @aa:8 bit operations use the low byte of the first opcode as
	// the address in the on-chip peripheral page. Do not reuse the @ERd
	// address calculated above.
	address24 = 0xffff00 | (opcode & 0xff);
	udata8 = h8_mem_read8(address24);

	switch (opcodes & 0xff00f) {//Mask BH & BL (Absolute Address)
	case 0x7E006:
		switch (CL) {
		case 0x3:
			//BTST.b Rn,@aa:8					
			udata8 >>= (h8_getreg8(DH) & 7);
			udata8 &= 0x1;
			h8.h8zflag = udata8 ? 0 : 1;
			h8_timing_byte(AccessType, 1);
			break;
		default:
			h8.err = 77;
		}
		break;
	case 0x7E007:
		switch (CL) {
		case 0x3:
			//BTST.b #xx:3,@aa:8										
			udata8 >>= (DH & 7);
			udata8 &= 0x1;
			h8.h8zflag = udata8 ? 0 : 1;
			h8_timing_byte(AccessType, 1);
			break;
		case 0x4:
			if (DH & 0x8) {
				//BIOR.b #xx:3,@aa:8						
				udata8 >>= (DH & 7);
				udata8 &= 0x1;
				h8.h8cflag |= (udata8 ^ 1);
				h8_timing_byte(AccessType, 1);
			}
			else {
				//BOR.b #xx:3,@aa:8											
				udata8 >>= (DH & 7);
				udata8 &= 0x1;
				h8.h8cflag |= udata8;
				h8_timing_byte(AccessType, 1);
			}
			break;
		case 0x5:
			if (DH & 0x8) {
				//BIXOR.b #xx:3,@aa:8							
				udata8 >>= (DH & 7);
				udata8 &= 0x1;
				h8.h8cflag ^= (udata8 ^ 1);
				h8_timing_byte(AccessType, 1);
			}
			else {
				//BXOR.b #xx:3,@aa:8												
				udata8 >>= (DH & 7);
				udata8 &= 0x1;
				h8.h8cflag ^= udata8;
				h8_timing_byte(AccessType, 1);
			}
			break;
		case 0x6:
			if (DH & 0x8) {
				//BIAND.b #xx:3,@aa:8												
				udata8 >>= (DH & 7);
				udata8 &= 0x1;
				h8.h8cflag &= (udata8 ^ 1);
				h8_timing_byte(AccessType, 1);
			}
			else {
				//BAND.b #xx:3,@aa:8												
				udata8 >>= (DH & 7);
				udata8 &= 0x1;
				h8.h8cflag &= udata8;
				h8_timing_byte(AccessType, 1);
			}
			break;
		case 0x7:
			if (DH & 0x8) {
				//BILD.b #xx:3,@aa:8												
				udata8 >>= (DH & 7);
				udata8 &= 0x1;
				h8.h8cflag = (udata8 ^ 1);
				h8_timing_byte(AccessType, 1);
			}
			else {
				//BLD.b #xx:3,@aa:8						
				udata8 >>= (DH & 7);
				udata8 &= 0x1;
				h8.h8cflag = udata8;
				h8_timing_byte(AccessType, 1);
			}
			break;
		default:
			h8.err = 78;
		}
		break;
	case 0x7F006:
		switch (CL) {
		case 0x0:
			//BSET.b Rn, @aa:8	
			h8_timing_byte(AccessType, 1);
			udata8 = h8_bset8((h8_getreg8(DH) & 7), udata8);
			h8_mem_write8(address24, udata8);
			h8_timing_byte(AccessType, 1);
			break;
		case 0x1:
			//BNOT.b Rn, @aa:8		
			h8_timing_byte(AccessType, 1);
			bitmask = ((1 << (h8_getreg8(DH) & 7)));
			if (udata8 & bitmask) {
				//Clear bit
				udata8 &= (UINT8)(~bitmask);
			}
			else {
				//Set bit
				udata8 |= (bitmask);
			}
			h8_mem_write8(address24, udata8);
			h8_timing_byte(AccessType, 1);
			break;
		case 0x2:
			//BCLR.b Rn, @aa:8		
			h8_timing_byte(AccessType, 1);
			udata8 = h8_bclr8((h8_getreg8(DH) & 7), udata8);
			h8_mem_write8(address24, udata8);
			h8_timing_byte(AccessType, 1);
			break;
		case 0x3:
			// BTST.b Rn, @aa:8
			h8_timing_byte(AccessType, 1);
			bitmask = (1 << (h8_getreg8(DH) & 7));
			h8.h8zflag = (udata8 & bitmask) ? 0 : 1;
			h8_timing_byte(AccessType, 1);
			break;
		case 0x7:
			if (DH & 0x8) {
				//BIST.b #xx:3, @aa:8		
				h8_timing_byte(AccessType, 1);
				bitmask = (1 << (DH & 7));
				if (h8.h8cflag) {
					//Clear Bit
					udata8 &= (UINT8)(~bitmask);
				}
				else {
					//Set Bit
					udata8 |= (bitmask);
				}
				h8_mem_write8(address24, udata8);
				h8_timing_byte(AccessType, 1);
			}
			else {
				//BST.b #xx:3, @aa:8											
				h8_timing_byte(AccessType, 1);
				bitmask = (1 << (DH & 7));
				if (h8.h8cflag) {
					//Set Bit
					udata8 |= (bitmask);
				}
				else {
					//Clear Bit
					udata8 &= (UINT8)(~bitmask);
				}
				h8_mem_write8(address24, udata8);
				h8_timing_byte(AccessType, 1);
			}
			break;
		default:
			h8.err = 79;
		}
		break;
	case 0x7F007:
		switch (CL) {
		case 0x0:
			//BSET.b #xx:3, @aa:8				
			h8_timing_byte(AccessType, 1);
			udata8 = h8_bset8((DH & 7), udata8);
			h8_mem_write8(address24, udata8);
			h8_timing_byte(AccessType, 1);
			break;
		case 0x1:
			//BNOT.b #xx:3, @aa:8										
			h8_timing_byte(AccessType, 1);
			bitmask = ((1 << (DH & 7)));
			if (udata8 & bitmask) {
				//Clear bit
				udata8 &= (UINT8)(~bitmask);
			}
			else {
				//Set bit
				udata8 |= (bitmask);
			}

			h8_mem_write8(address24, udata8);
			h8_timing_byte(AccessType, 1); break;
		case 0x2:
			//BCLR.b #xx:3, @aa:8					
			h8_timing_byte(AccessType, 1);
			udata8 = h8_bclr8(DH & 7, udata8);
			h8_mem_write8(address24, udata8);
			h8_timing_byte(AccessType, 1);
			break;
		default:
			h8.err = 80;
		}
		break;
	default:
		//ERROR!
		h8.err = 81;
	}

}


// Reverified
inline UINT8 H83002::h8_add8(UINT8 src, UINT8 dst)//
{
	UINT16 res;
	res = (UINT16)src + (UINT16)dst;

	// H,N,Z,V,C modified
	h8.h8nflag = (res & 0x80) ? 1 : 0;
	h8.h8vflag = ((src ^ res) & (dst ^ res) & 0x80) ? 1 : 0;
	h8.h8cflag = (res & 0x100) ? 1 : 0;
	h8.h8zflag = (res & 0xff) ? 0 : 1;
	h8.h8hflag = ((src ^ dst ^ res) & 0x10) ? 1 : 0;

	return (UINT8)(res & 0xff);
}

// Reverified
inline UINT16 H83002::h8_add16(UINT16 src, UINT16 dst)//
{
	UINT32 res;
	res = (UINT32)src + (UINT32)dst;

	// H,N,Z,V,C modified
	h8.h8nflag = (res & 0x8000) ? 1 : 0;
	h8.h8vflag = ((src ^ res) & (dst ^ res) & 0x8000) ? 1 : 0;
	h8.h8cflag = (res & 0x10000) ? 1 : 0;
	h8.h8zflag = (res & 0xffff) ? 0 : 1;
	h8.h8hflag = ((src ^ dst ^ res) & 0x1000) ? 1 : 0;

	return (UINT16)(res & 0xffff);
}

// Reverified
inline UINT32 H83002::h8_add32(UINT32 src, UINT32 dst)//
{
	UINT64 res;
	res = (UINT64)src + (UINT64)dst;

	// H,N,Z,V,C modified
	h8.h8nflag = (res & 0x80000000) ? 1 : 0;
	h8.h8vflag = (((src ^ res) & (dst ^ res)) & 0x80000000) ? 1 : 0;
	h8.h8cflag = ((res) & (((UINT64)1) << 32)) ? 1 : 0;
	h8.h8zflag = (res & 0xffffffff) ? 0 : 1;
	h8.h8hflag = ((src ^ dst ^ res) & 0x10000000) ? 1 : 0;

	return (UINT32)(res & 0xffffffff);
}

// Reverified
inline UINT8 H83002::h8_addx8(UINT8 src, UINT8 dst)//
{
	UINT16 res;

	res = (UINT16)src + (UINT16)dst + (UINT16)h8.h8cflag;

	// H,N,Z,V,C modified
	h8.h8nflag = (res & 0x80) ? 1 : 0;
	h8.h8vflag = ((src ^ res) & (dst ^ res) & 0x80) ? 1 : 0;
	h8.h8cflag = (res >> 8) & 1;
	h8.h8hflag = ((src ^ dst ^ res) & 0x10) ? 1 : 0;
	h8.h8zflag = (res & 0xff) ? 0 : h8.h8zflag;

	return (UINT8)(res & 0xff);
}

// Reverified
inline UINT8 H83002::h8_and8(UINT8 src, UINT8 dst)//
{
	UINT8 res;
	res = src & dst;

	// N, V and Z modified
	h8.h8nflag = (res >> 7) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT16 H83002::h8_and16(UINT16 src, UINT16 dst)//
{
	UINT16 res;
	res = src & dst;

	// N, V and Z modified
	h8.h8nflag = (res >> 15) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT32 H83002::h8_and32(UINT32 src, UINT32 dst)//
{
	UINT32 res;
	res = src & dst;

	// N, V and Z modified
	h8.h8nflag = (res >> 31) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline void H83002::h8_band8(UINT8 src, UINT8 dst)//
{
	// does not affect result, res in C flag only
	UINT8 res;

	res = dst & (1 << (src & 0x7));
	h8.h8cflag &= (res ? 1 : 0);
}

// Reverified
inline UINT8 H83002::h8_bclr8(UINT8 src, UINT8 dst)//
{
	UINT8 res;
	res = dst & ~(1 << (src & 0x07));
	return res;
}

// Reverified
inline void H83002::h8_biand8(UINT8 src, UINT8 dst)//
{
	// does not affect result, res in C flag only
	UINT8 res;

	res = dst & (1 << (src & 0x7));
	h8.h8cflag &= (res ? 0 : 1);
}

// Reverified
inline void H83002::h8_bild8(UINT8 bit, UINT8 dst)
{
	// load bit to carry
	h8.h8cflag = (~dst >> (bit & 0x7)) & 1;
}

// Reverified
inline void H83002::h8_bior8(UINT8 src, UINT8 dst)
{
	// does not affect result, res in C flag only
	UINT8 res;

	res = dst & (1 << (src & 0x7));
	h8.h8cflag |= (res ? 0 : 1);
}

// Reverified
inline UINT8 H83002::h8_bist8(UINT8 src, UINT8 dst)
{
	UINT8 res;

	// store inverse of carry flag in bit position
	if (h8.h8cflag == 0)
	{
		res = dst | (1 << (src & 0x07));
	}
	else
	{
		res = dst & ~(1 << (src & 0x07)); // mask off
	}

	return res;
}

// Reverified
inline void H83002::h8_bixor8(UINT8 src, UINT8 dst)
{
	// does not affect result, res in C flag only
	UINT8 res;

	res = dst & (1 << (src & 0x7));
	h8.h8cflag ^= (res ? 0 : 1);
}

// Reverified
inline void H83002::h8_bld8(UINT8 bit, UINT8 dst)
{
	// load bit to carry
	h8.h8cflag = (dst >> (bit & 0x7)) & 1;
}

// Reverified
inline UINT8 H83002::h8_bnot8(UINT8 src, UINT8 dst)
{
	UINT8 res;

	// invert single bit, no effect on C flag
	res = dst ^ (1 << (src & 0x7));

	return res;
}

// Reverified
inline void H83002::h8_bor8(UINT8 src, UINT8 dst)
{
	// does not affect result, res in C flag only
	UINT8 res;

	res = dst & (1 << (src & 0x7));
	h8.h8cflag |= (res ? 1 : 0);
}

// Reverified
inline int H83002::h8_branch(UINT8 condition)
{
	// input: branch condition
	// output: 1 if condition met, 0 if not condition met

	switch (condition)
	{
	case 0: // bt  (Always)
		return 1;
		break;
	case 1: // bf  (Never)
		return 0;
		break;
	case 2: // bhi ((C | Z) == 0)
		if ((h8.h8cflag | h8.h8zflag) == 0)
		{
			return 1;
		}
		return 0;
		break;
	case 3: // bls ((C | Z) == 1)
		if ((h8.h8cflag | h8.h8zflag) == 1)
		{
			return 1;
		}
		return 0;
		break;
	case 4: // bcc C = 0
		if (h8.h8cflag == 0)
		{
			return 1;
		}
		return 0;
		break;
	case 5: // bcs C = 1
		if (h8.h8cflag == 1)
		{
			return 1;
		}
		return 0;
		break;
	case 6: // bne Z = 0
		if (h8.h8zflag == 0)
		{
			return 1;
		}
		return 0;
		break;
	case 7: // beq Z = 1
		if (h8.h8zflag == 1)
		{
			return 1;
		}
		return 0;
		break;
	case 8: // bvc V = 0
		if (h8.h8vflag == 0)
		{
			return 1;
		}
		return 0;
		break;
	case 9: // bvs V = 1
		if (h8.h8vflag == 1)
		{
			return 1;
		}
		return 0;
		break;
	case 0xa: // bpl N = 0
		if (h8.h8nflag == 0)
		{
			return 1;
		}
		return 0;
		break;
	case 0xb: // bmi N = 1
		if (h8.h8nflag == 1)
		{
			return 1;
		}
		return 0;
		break;
	case 0xc: // bge (N ^ V) = 0
		if ((h8.h8nflag) == 0) {
			if ((h8.h8vflag) == 0) {
				//Branch if Greater or Equal
				return 1;
			}
		}
		if (h8.h8nflag) {
			if (h8.h8vflag) {
				//Branch if Greater or Equal
				return 1;
			}
		}
		return 0;
		break;
	case 0xd: // blt (N ^ V) = 1
		if (h8.h8nflag) {
			if ((h8.h8vflag) == 0) {
				//Branch if Less than 
				return 1;
			}
		}
		if ((h8.h8nflag) == 0) {
			if (h8.h8vflag) {
				//Branch if Less than 
				return 1;
			}
		}
		return 0;
		break;
	case 0xe: // bgt should be (Z & (N ^ V)) = 0  NOT (Z | (N ^ V)) = 0 as in the docs?

		//if ((h8.h8zflag & (h8.h8nflag ^ h8.h8vflag)) == 0)
		//Assert Branch Logic	
		if ((h8.h8zflag) == 0) {
			if ((h8.h8nflag) == 0) {
				if ((h8.h8vflag) == 0) {
					//Branch if Greater Than
					return 1;
				}
			}
			if (h8.h8nflag) {
				if (h8.h8vflag) {
					//Branch if Greater Than
					return 1;
				}
			}
		}
		return 0;
		break;
	case 0xf: // ble (Z | (N ^ V)) = 1

		//Assert Branch Logic		
		if (h8.h8zflag) {
			//Branch if Less than or Equal
			return 1;
		}
		if (h8.h8nflag) {
			if ((h8.h8vflag) == 0) {
				//Branch if Less than or Equal
				return 1;
			}
		}
		if ((h8.h8nflag) == 0) {
			if (h8.h8vflag) {
				//Branch if Less than or Equal
				return 1;
			}
		}
		return 0;
		break;
	}

	return 0;
}

// Reverified
inline UINT8 H83002::h8_bset8(UINT8 src, UINT8 dst)
{
	UINT8 res;

	res = dst | (1 << (src & 0x7));
	return res;
}

// Reverified
inline UINT8 H83002::h8_bst8(UINT8 src, UINT8 dst)
{
	UINT8 res;

	// store carry flag in bit position
	if (h8.h8cflag == 1)
	{
		res = dst | (1 << (src & 0x7));
	}
	else
	{
		res = dst & ~(1 << (src & 0x7)); // mask off
	}

	return res;
}

// Reverified
inline void H83002::h8_btst8(UINT8 bit, UINT8 dst)
{
	// test single bit and update Z flag
	if ((dst & (1 << (bit & 0x7))) == 0)
	{
		h8.h8zflag = 1;
	}
	else
	{
		h8.h8zflag = 0;
	}
}

// Reverified
inline void H83002::h8_bxor8(UINT8 src, UINT8 dst)
{
	// does not affect result, res in C flag only
	UINT8 res;

	res = dst & (1 << (src & 0x7));
	h8.h8cflag ^= (res ? 1 : 0);
}

// Reverified
inline void H83002::h8_cmp8(UINT8 src, UINT8 dst)
{
	UINT16 res = (UINT16)dst - (UINT16)src;

	h8.h8cflag = (res & 0x100) ? 1 : 0;
	h8.h8vflag = ((dst ^ src) & (dst ^ res) & 0x80) ? 1 : 0;
	h8.h8zflag = ((res & 0xff) == 0) ? 1 : 0;
	h8.h8nflag = (res & 0x80) ? 1 : 0;
	h8.h8hflag = ((src ^ dst ^ res) & 0x10) ? 1 : 0;
}

// Reverified
inline void H83002::h8_cmp16(UINT16 src, UINT16 dst)
{
	UINT32 res = (UINT32)dst - (UINT32)src;

	h8.h8cflag = (res & 0x10000) ? 1 : 0;
	h8.h8vflag = ((dst ^ src) & (dst ^ res) & 0x8000) ? 1 : 0;
	h8.h8zflag = ((res & 0xffff) == 0) ? 1 : 0;
	h8.h8nflag = (res & 0x8000) ? 1 : 0;
	h8.h8hflag = ((src ^ dst ^ res) & 0x1000) ? 1 : 0;
}

// Reverified
inline void H83002::h8_cmp32(UINT32 src, UINT32 dst)
{
	UINT64 res = (UINT64)dst - (UINT64)src;

	h8.h8cflag = (res & 0x100000000ui64) ? 1 : 0;
	h8.h8vflag = ((dst ^ src) & (dst ^ res) & 0x80000000) ? 1 : 0;
	h8.h8zflag = ((res & 0xffffffff) == 0) ? 1 : 0;
	h8.h8nflag = (res & 0x80000000) ? 1 : 0;
	h8.h8hflag = ((src ^ dst ^ res) & 0x10000000) ? 1 : 0;
}

// Reverified
inline UINT8 H83002::h8_dec8(UINT8 src)
{
	UINT8 res;

	res = src - 1;

	// N, V and Z modified
	h8.h8nflag = (res >> 7) & 1;
	h8.h8vflag = (src == 0x80) ? 1 : 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT16 H83002::h8_dec16(UINT16 src)
{
	UINT16 res;

	res = src - 1;

	// N, V and Z modified
	h8.h8nflag = (res >> 15) & 1;
	h8.h8vflag = (src == 0x8000) ? 1 : 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT32 H83002::h8_dec32(UINT32 src)
{
	UINT32 res;

	res = src - 1;

	// N, V and Z modified
	h8.h8nflag = (res >> 31) & 1;
	h8.h8vflag = (src == 0x80000000) ? 1 : 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

inline UINT16 H83002::h8_divxs8(INT8 src, INT16 dst)
{
	INT16 quotient = 0;
	INT16 remainder = 0;
	UINT16 res = (UINT16)dst;

	// H8/300H: H/V/C are unchanged.  Z reports divisor zero;
	// N reports quotient sign when a division is actually performed.
	if (src == 0) {
		h8.h8zflag = 1;
		return res;
	}

	// The CPU does not raise an exception for overflow.  Avoid undefined
	// host C++ overflow while leaving the destination unchanged.
	if (dst == (INT16)0x8000 && src == -1) {
		h8.h8nflag = 0;
		h8.h8zflag = 0;
		return res;
	}

	quotient = dst / src;
	remainder = dst % src;

	h8.h8nflag = ((((INT16)dst < 0) ^ (src < 0)) ? 1 : 0);
	h8.h8zflag = 0;

	if (quotient < -128 || quotient > 127) {
		return res;
	}

	res = ((((UINT16)remainder) & 0xff) << 8) | (((UINT16)quotient) & 0xff);
	return res;
}

// Reverified
inline UINT32 H83002::h8_divxs16(INT16 src, INT32 dst)
{
	INT32 quotient = 0;
	INT32 remainder = 0;
	UINT32 res = (UINT32)dst;

	if (src == 0) {
		h8.h8zflag = 1;
		return res;
	}
	if (dst == (INT32)0x80000000 && src == -1) {
		h8.h8nflag = 0;
		h8.h8zflag = 0;
		return res;
	}

	quotient = dst / src;
	remainder = dst % src;

	h8.h8nflag = ((((INT32)dst < 0) ^ (src < 0)) ? 1 : 0);
	h8.h8zflag = 0;

	if (quotient < -32768 || quotient > 32767) {
		return res;
	}

	res = ((((UINT32)remainder) & 0xffff) << 16) | (((UINT32)quotient) & 0xffff);
	return res;
}

// Reverified
inline UINT16 H83002::h8_divxu8(UINT16 dst, UINT8 src)
{
	UINT16 quotient = 0;
	UINT16 remainder = 0;
	UINT16 res = dst;

	// H8/300H: H/V/C are unchanged.  Z reports divisor zero;
	// N reports bit 7 of the quotient after a valid division.
	if (src == 0) {
		h8.h8zflag = 1;
		return res;
	}

	quotient = dst / src;
	remainder = dst % src;

	h8.h8nflag = (quotient & 0x80) ? 1 : 0;
	h8.h8zflag = 0;

	if (quotient > 0xff) {
		return res;
	}

	res = ((remainder & 0xff) << 8) | (quotient & 0xff);
	return res;
}

// Reverified
inline UINT32 H83002::h8_divxu16(UINT32 dst, UINT16 src)
{
	UINT32 quotient = 0;
	UINT32 remainder = 0;
	UINT32 res = dst;

	if (src == 0) {
		h8.h8zflag = 1;
		return res;
	}

	quotient = dst / src;
	remainder = dst % src;

	h8.h8nflag = (quotient & 0x8000) ? 1 : 0;
	h8.h8zflag = 0;

	if (quotient > 0xffff) {
		return res;
	}

	res = ((remainder & 0xffff) << 16) | (quotient & 0xffff);
	return res;
}

inline UINT8 H83002::h8_daa8(UINT8 src)
{
	UINT16 res = src;
	UINT8 correction = 0;
	UINT8 oldC = h8.h8cflag ? 1 : 0;
	UINT8 oldH = h8.h8hflag ? 1 : 0;

	if (((src & 0x0f) > 0x09) || oldH)
		correction |= 0x06;
	if ((src > 0x99) || oldC)
		correction |= 0x60;

	res += correction;

	// H is documented as undefined (*) for DAA/DAS; keep it in range and
	// preserve the pre-adjust half-carry so code cannot observe a synthetic
	// helper artefact. V is also undefined; clear it deterministically.
	h8.h8cflag = (oldC || (res > 0xff)) ? 1 : 0;
	h8.h8hflag = oldH;
	h8.h8nflag = (res & 0x80) ? 1 : 0;
	h8.h8zflag = ((res & 0xff) == 0) ? 1 : 0;
	h8.h8vflag = 0;

	return (UINT8)(res & 0xff);
}

inline UINT8 H83002::h8_das8(UINT8 src)
{
	UINT16 res = src;
	UINT8 correction = 0;
	UINT8 oldC = h8.h8cflag ? 1 : 0;
	UINT8 oldH = h8.h8hflag ? 1 : 0;

	if (((src & 0x0f) > 0x09) || oldH)
		correction |= 0x06;
	if ((src > 0x99) || oldC)
		correction |= 0x60;

	res -= correction;

	h8.h8cflag = oldC;
	h8.h8hflag = oldH;
	h8.h8nflag = (res & 0x80) ? 1 : 0;
	h8.h8zflag = ((res & 0xff) == 0) ? 1 : 0;
	h8.h8vflag = 0;

	return (UINT8)(res & 0xff);
}


// Reverified
// Reverified
inline UINT8 H83002::h8_inc8(UINT8 src)
{
	UINT8 res;
	res = src + 1;

	// N, V and Z modified
	h8.h8nflag = (res >> 7) & 1;
	h8.h8vflag = (src == 0x7f) ? 1 : 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT16 H83002::h8_inc16(UINT16 src, UINT8 incVal)
{
	UINT16 res;
	res = src + incVal;

	// N, V and Z modified
	h8.h8nflag = (res >> 15) & 1;
	h8.h8zflag = (res == 0) ? 1 : 0;
	switch (incVal) {
	case 1:	h8.h8vflag = (src == 0x7fff) ? 1 : 0; break;
	case 2:	h8.h8vflag = ((src == 0x7ffe) || (src == 0x7fff)) ? 1 : 0; break;
	default: h8.err = 82;	break;
	}
	return res;
}

// Reverified
inline UINT32 H83002::h8_inc32(UINT32 src, UINT8 incVal)
{
	UINT32 res;
	res = src + incVal;

	// N, V and Z modified
	h8.h8nflag = (res >> 31) & 1;
	h8.h8zflag = (res == 0) ? 1 : 0;

	switch (incVal) {
	case 1: h8.h8vflag = (src == 0x7fffffff) ? 1 : 0; break;
	case 2:	h8.h8vflag = ((src == 0x7ffffffe) || (src == 0x7fffffff)) ? 1 : 0; break;
	default: h8.err = 83; break;
	}
	return res;
}

// Reverified
inline UINT8 H83002::h8_mov8(UINT8 src)
{
	// N, V and Z modified
	h8.h8nflag = (src >> 7) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (src == 0) ? 1 : 0;

	return src;
}

// Reverified
inline UINT16 H83002::h8_mov16(UINT16 src)
{
	// N, V and Z modified
	h8.h8nflag = (src >> 15) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (src == 0) ? 1 : 0;

	return src;
}

// Reverified
inline UINT32 H83002::h8_mov32(UINT32 src)
{
	// N, V and Z modified
	h8.h8nflag = (src >> 31) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (src == 0) ? 1 : 0;

	return src;
}

// Reverified
inline INT16 H83002::h8_mulxs8(INT8 src, INT8 dst)
{
	INT16 res;

	res = (INT16)src * dst;

	// N and Z modified
	h8.h8nflag = (res >> 15) & 1;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline INT32 H83002::h8_mulxs16(INT16 src, INT16 dst)
{
	INT32 res;

	res = (INT32)src * dst;

	// N and Z modified
	h8.h8nflag = (res >> 31) & 1;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline INT8 H83002::h8_neg8(INT8 src)
{
	UINT16 res = (UINT16)0 - (UINT8)src;
	UINT8 r = (UINT8)res;

	// NEG modifies H,N,Z,V,C as a subtraction from zero.
	h8.h8nflag = (r & 0x80) ? 1 : 0;
	h8.h8zflag = (r == 0) ? 1 : 0;
	h8.h8vflag = (((UINT8)src) == 0x80) ? 1 : 0;
	h8.h8cflag = (((UINT8)src) != 0) ? 1 : 0;
	h8.h8hflag = ((0 ^ (UINT8)src ^ r) & 0x10) ? 1 : 0;

	return (INT8)r;
}

// Reverified
inline INT16 H83002::h8_neg16(INT16 src)
{
	UINT32 res = (UINT32)0 - (UINT16)src;
	UINT16 r = (UINT16)res;

	h8.h8nflag = (r & 0x8000) ? 1 : 0;
	h8.h8zflag = (r == 0) ? 1 : 0;
	h8.h8vflag = (((UINT16)src) == 0x8000) ? 1 : 0;
	h8.h8cflag = (((UINT16)src) != 0) ? 1 : 0;
	h8.h8hflag = ((0 ^ (UINT16)src ^ r) & 0x1000) ? 1 : 0;

	return (INT16)r;
}

// Reverified
inline INT32 H83002::h8_neg32(INT32 src)
{
	UINT64 res = (UINT64)0 - (UINT32)src;
	UINT32 r = (UINT32)res;

	h8.h8nflag = (r & 0x80000000) ? 1 : 0;
	h8.h8zflag = (r == 0) ? 1 : 0;
	h8.h8vflag = (((UINT32)src) == 0x80000000) ? 1 : 0;
	h8.h8cflag = (((UINT32)src) != 0) ? 1 : 0;
	h8.h8hflag = ((0 ^ (UINT32)src ^ r) & 0x10000000) ? 1 : 0;

	return (INT32)r;
}

// Reverified
// Reverified
inline UINT8 H83002::h8_not8(UINT8 src)
{
	UINT8 res;

	res = ~src;

	// N and Z modified
	h8.h8nflag = (res >> 7) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT16 H83002::h8_not16(UINT16 src)
{
	UINT16 res;

	res = ~src;

	// N and Z modified
	h8.h8nflag = (res >> 15) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT32 H83002::h8_not32(UINT32 src)
{
	UINT32 res;

	res = ~src;

	// N and Z modified
	h8.h8nflag = (res >> 31) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT8 H83002::h8_or8(UINT8 src, UINT8 dst)
{
	UINT8 res;
	res = src | dst;

	// N, V and Z modified
	h8.h8nflag = (res >> 7) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT16 H83002::h8_or16(UINT16 src, UINT16 dst)
{
	UINT16 res;
	res = src | dst;

	// N, V and Z modified
	h8.h8nflag = (res >> 15) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT32 H83002::h8_or32(UINT32 src, UINT32 dst)
{
	UINT32 res;
	res = src | dst;


	// zflag
	h8.h8nflag = (res >> 31) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT8 H83002::h8_rotl8(UINT8 src)
{
	UINT8 res;

	// rotate
	res = (src << 1) & 0xfe;
	h8.h8cflag = (src >> 7) & 1;
	res |= (h8.h8cflag & 1);

	// N, V and Z modified
	h8.h8nflag = (res >> 7) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT16 H83002::h8_rotl16(UINT16 src)
{
	UINT16 res;

	// rotate
	res = (src << 1) & 0xfffe;
	h8.h8cflag = (src >> 15) & 1;
	res |= (h8.h8cflag & 1);

	// N, V and Z modified
	h8.h8nflag = (res >> 15) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT32 H83002::h8_rotl32(UINT32 src)
{
	UINT32 res;

	// rotate
	res = (src << 1) & 0xfffffffe;
	h8.h8cflag = (src >> 31) & 1;
	res |= (h8.h8cflag & 1);

	// N, V and Z modified
	h8.h8nflag = (res >> 31) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT8 H83002::h8_rotr8(UINT8 src)
{
	UINT8 res;

	// rotate
	res = (src >> 1) & 0x7f;
	h8.h8cflag = src & 1;

	if (h8.h8cflag)
	{
		res |= 0x80; // put cflag in upper bit
	}

	// N, V and Z modified
	h8.h8nflag = (res >> 7) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT16 H83002::h8_rotr16(UINT16 src)
{
	UINT16 res;

	// rotate
	res = (src >> 1) & 0x7fff;
	h8.h8cflag = src & 1;

	if (h8.h8cflag)
	{
		res |= 0x8000; // put cflag in upper bit
	}

	// N, V and Z modified
	h8.h8nflag = (res >> 15) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT32 H83002::h8_rotr32(UINT32 src)
{
	UINT32 res;

	// rotate
	res = (src >> 1) & 0x7fffffff;
	h8.h8cflag = src & 1;

	if (h8.h8cflag)
	{
		res |= 0x80000000; // put cflag in upper bit
	}

	// N, V and Z modified
	h8.h8nflag = (res >> 31) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT8 H83002::h8_rotxl8(UINT8 src)
{
	UINT8 res;

	// rotate through carry
	res = (src << 1) & 0xfe;
	res |= (h8.h8cflag & 1);
	h8.h8cflag = (src >> 7) & 1;

	// N, V and Z modified
	h8.h8nflag = (res >> 7) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT16 H83002::h8_rotxl16(UINT16 src)
{
	UINT16 res;

	// rotate through carry
	res = (src << 1) & 0xfffe;
	res |= (h8.h8cflag & 1);
	h8.h8cflag = (src >> 15) & 1;

	// N, V and Z modified
	h8.h8nflag = (res >> 15) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT32 H83002::h8_rotxl32(UINT32 src)
{
	UINT32 res;

	// rotate through carry
	res = (src << 1) & 0xfffffffe;
	res |= (h8.h8cflag & 1);
	h8.h8cflag = (src >> 31) & 1;

	// N, V and Z modified
	h8.h8nflag = (res >> 31) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT8 H83002::h8_rotxr8(UINT8 src)
{
	UINT8 res;

	// rotate through carry right
	res = (src >> 1) & 0x7f;

	if (h8.h8cflag)
	{
		res |= 0x80; // put cflag in upper bit
	}

	h8.h8cflag = src & 1;

	// N, V and Z modified
	h8.h8nflag = (res >> 7) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT16 H83002::h8_rotxr16(UINT16 src)
{
	UINT16 res;

	// rotate through carry right
	res = (src >> 1) & 0x7fff;

	if (h8.h8cflag)
	{
		res |= 0x8000; // put cflag in upper bit
	}

	h8.h8cflag = src & 1;

	// N, V and Z modified
	h8.h8nflag = (res >> 15) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT32 H83002::h8_rotxr32(UINT32 src)
{
	UINT32 res;

	// rotate through carry right
	res = (src >> 1) & 0x7fffffff;

	if (h8.h8cflag)
	{
		res |= 0x80000000; // put cflag in upper bit
	}

	h8.h8cflag = src & 1;

	// N, V and Z modified
	h8.h8nflag = (res >> 31) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT8 H83002::h8_shal8(UINT8 src)
{
	UINT8 res;

	h8.h8cflag = (src >> 7) & 1;
	res = src << 1;

	// N, V and Z modified
	h8.h8nflag = (res >> 7) & 1;
	h8.h8vflag = (src ^ res) >> 7;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT16 H83002::h8_shal16(UINT16 src)
{
	UINT16 res;

	h8.h8cflag = (src >> 15) & 1;
	res = src << 1;

	// N, V and Z modified
	h8.h8nflag = (res >> 15) & 1;
	h8.h8vflag = (src ^ res) >> 15;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT32 H83002::h8_shal32(UINT32 src)
{
	UINT32 res;

	h8.h8cflag = (src >> 31) & 1;
	res = src << 1;

	// N, V and Z modified
	h8.h8nflag = (res >> 31) & 1;
	h8.h8vflag = (src ^ res) >> 31;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT8 H83002::h8_shar8(UINT8 src)
{
	UINT8 res;
	h8.h8cflag = src & 1;
	res = ((src >> 1) & 0x7f) | (src & 0x80);

	// N and Z modified
	h8.h8nflag = (res >> 7) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT16 H83002::h8_shar16(UINT16 src)
{
	UINT16 res;
	h8.h8cflag = src & 1;
	res = ((src >> 1) & 0x7fff) | (src & 0x8000);

	// N, V and Z modified
	h8.h8nflag = (res >> 15) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT32 H83002::h8_shar32(UINT32 src)
{
	UINT32 res;
	h8.h8cflag = src & 1;
	res = ((src >> 1) & 0x7fffffff) | (src & 0x80000000);

	// N, V and Z modified
	h8.h8nflag = (res >> 31) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT8 H83002::h8_shll8(UINT8 src)
{
	UINT8 res;
	h8.h8cflag = (src >> 7) & 1;
	res = (src << 1) & 0xfe;

	// N, V and Z modified
	h8.h8nflag = (res >> 7) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT16 H83002::h8_shll16(UINT16 src)
{
	UINT16 res;
	h8.h8cflag = (src >> 15) & 1;
	res = (src << 1) & 0xfffe;

	// N, V and Z modified
	h8.h8nflag = (res >> 15) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT32 H83002::h8_shll32(UINT32 src)
{
	UINT32 res;
	h8.h8cflag = (src >> 31) & 1;
	res = (src << 1) & 0xfffffffe;

	// N, V and Z modified
	h8.h8nflag = (res >> 31) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT8 H83002::h8_shlr8(UINT8 src)
{
	UINT8 res;
	h8.h8cflag = src & 1;
	res = (src >> 1) & 0x7f;

	// N, V and Z modified
	h8.h8nflag = 0;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT16 H83002::h8_shlr16(UINT16 src)
{
	UINT16 res;
	h8.h8cflag = src & 1;
	res = (src >> 1) & 0x7fff;

	// N, V and Z modified
	h8.h8nflag = 0;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT32 H83002::h8_shlr32(UINT32 src)
{
	UINT32 res;
	h8.h8cflag = src & 1;
	res = (src >> 1) & 0x7fffffff;

	// N and Z modified, V always cleared
	h8.h8nflag = 0;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT8 H83002::h8_sub8(UINT8 src, UINT8 dst)
{
	UINT16 res;

	res = (UINT16)dst - src;

	// H,N,Z,V,C modified
	h8.h8nflag = (res >> 7) & 1;
	h8.h8vflag = (((src ^ dst) & (res ^ dst)) >> 7) & 1;
	h8.h8cflag = (res >> 8) & 1;
	h8.h8zflag = ((res & 0xff) == 0) ? 1 : 0;
	h8.h8hflag = ((src ^ dst ^ res) & 0x10) ? 1 : 0;

	return (UINT8)res;
}

// Verified
inline UINT16 H83002::h8_sub16(UINT16 src, UINT16 dst)
{
	UINT32 res;

	res = (UINT32)dst - src;

	// H,N,Z,V,C modified
	h8.h8nflag = (res >> 15) & 1;
	h8.h8vflag = (((src ^ dst) & (res ^ dst)) >> 15) & 1;
	h8.h8cflag = (res >> 16) & 1;
	h8.h8zflag = ((res & 0xffff) == 0) ? 1 : 0;
	h8.h8hflag = ((src ^ dst ^ res) & 0x1000) ? 1 : 0;

	return (UINT16)res;
}

// Reverified
inline UINT32 H83002::h8_sub32(UINT32 src, UINT32 dst)
{
	UINT64 res;

	res = (UINT64)dst - src;

	// H,N,Z,V,C modified
	h8.h8nflag = (res >> 31) & 1;
	h8.h8vflag = (((src ^ dst) & (res ^ dst)) >> 31) & 1;
	h8.h8cflag = (res >> 32) & 1;
	h8.h8zflag = ((res & 0xffffffff) == 0) ? 1 : 0;
	h8.h8hflag = ((src ^ dst ^ res) & 0x10000000) ? 1 : 0;

	return (UINT32)res;
}

// Reverified
inline UINT8 H83002::h8_subx8(UINT8 src, UINT8 dst)
{
	UINT16 res;
	res = (UINT16)dst - src - ((h8.h8cflag) ? 1 : 0);

	// H,N,Z,V,C modified
	h8.h8nflag = (res >> 7) & 1;
	h8.h8vflag = (((src ^ dst) & (res ^ dst)) >> 7) & 1;
	h8.h8cflag = (res >> 8) & 1;
	//h8.h8zflag = ((res & 0xff) == 0) ? 1 : 0;
	h8.h8hflag = ((src ^ dst ^ res) & 0x10) ? 1 : 0;
	//Nick - From The Docs - though this opcode doesnt appear to be used
	//Z Retains its previous value when the result is zero; otherwise cleared to 0.	
	h8.h8zflag = (res & 0xff) ? 0 : h8.h8zflag;
	return (UINT8)res;
}

// Reverified
inline UINT8 H83002::h8_xor8(UINT8 src, UINT8 dst)
{
	UINT8 res;
	res = src ^ dst;

	// N, V and Z modified
	h8.h8nflag = (res >> 7) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT16 H83002::h8_xor16(UINT16 src, UINT16 dst)
{
	UINT16 res;
	res = src ^ dst;

	// N, V and Z modified
	h8.h8nflag = (res >> 15) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

// Reverified
inline UINT32 H83002::h8_xor32(UINT32 src, UINT32 dst)
{
	UINT32 res;
	res = src ^ dst;

	// N, V and Z modified
	h8.h8nflag = (res >> 31) & 1;
	h8.h8vflag = 0;
	h8.h8zflag = (res == 0) ? 1 : 0;

	return res;
}

