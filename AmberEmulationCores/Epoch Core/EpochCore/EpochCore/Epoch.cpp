// ###########################################################################
// #
// # TechEpoch - Definition of main Epoch Emulation Classes
// # Copyright (C) 2002-2012 Tony Friery [DialTone]
// #
// # ALL RIGHTS RESERVED
// #
// # Packed Into DLL form April 2016 with kind permission from Tony Friery
// ###########################################################################

#include "stdafx.h"
#include "Epoch.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>
using namespace std;

EPOCHCPU::EPOCHCPU()
{	
	ZeroMemory(IOMAP_LAMPS, 512);
	ZeroMemory(IOMAP_LEDS, 512);
	ZeroMemory(IOMAP_INPUTS, 512);
	ZeroMemory(IOMAP_INPUT_IRQEN, 512);	
	
	ZeroMemory(IOMAP_LAMP_TIMER, 12);	
	ZeroMemory(lamp_flash, 12);
	ZeroMemory(lamp_flash_counts, 12);

	ZeroMemory(IOMAP_LED_TIMER, 12);
	ZeroMemory(led_flash, 12);
	ZeroMemory(led_flash_counts, 12);
		
	ZeroMemory(RAM, RAMSIZE);
	ZeroMemory(ROM, ROMSIZE);

	ZeroMemory(fReelPatterns, NUMREELS);
	

}

EPOCHCPU::~EPOCHCPU()
{	
}

UINT8 __fastcall BoardEpoch::GetAlphaDots(char CharNum, char ColumnNum){
	return fAlphaDotMatrix ? fAlphaDotMatrix->GetAlphaDots(CharNum, ColumnNum) : 0;
}
BoardEpoch::BoardEpoch()
{
	fRTCDevice = new DeviceEpochRTC(FPGAVERSION);
	fMainCPU = new EPOCHCPU();	
	fMainCPU->fOwner = this;
	fAlphaStarburst = new AlphaStarburst();
	fAlphaDotMatrix = new AlphaDotMatrix();
	
	ZeroMemory(fCoinMech, sizeof(fCoinMech));

	ReelExt = 0;
	PrevValue = 0;	
	fProgramLoaded = 0;
	fFrameCyclesElapsed = 0;
	memoryOffset = 0;
	lastMemoryDebug = 0;
	lastMemoryScroll = 0;
	Stake = 0;
	Prize = 0;
	Percent = 0;
	ReelExt = 0;
	PrevValue = 0;
	CFolder.clear();
	CFileName.clear();
}

BoardEpoch::~BoardEpoch()
{	
	if (fMainCPU)
	 {
		delete fMainCPU;
		fMainCPU = NULL;
	 }

	if (fRTCDevice)
	{
		delete fRTCDevice;
		fRTCDevice = NULL;
	}

	if (fAlphaStarburst)
	{
		delete fAlphaStarburst;
		fAlphaStarburst = NULL;
	}

	if (fAlphaDotMatrix)
	{
		delete fAlphaDotMatrix;
		fAlphaDotMatrix = NULL;
	}

	
}
void __fastcall	BoardEpoch::ClearRAM(void){
	
	fMainCPU->ClearRAM();
}

void __fastcall BoardEpoch::SaveRAM(const char* FileString){

	fMainCPU->SaveRAM(FileString);	
}

void __fastcall BoardEpoch::LoadRAM(const char* FileString){

	fMainCPU->LoadRAM(FileString);
}

void __fastcall	EPOCHCPU::ClearRAM(void){
	
	UINT32 Cnt;
	
	for (Cnt = 0; Cnt < RAMSIZE; Cnt++){
		RAM[Cnt] = 0;
	}

}

void __fastcall EPOCHCPU::SaveRAM(const char* FileString){

	unsigned long Cnt;
	streampos size;
	char * memblock;

	ofstream file (FileString, ios::out|ios::binary|ios::trunc);
	if (file.is_open())	{
		size = RAMSIZE;
		memblock = new char [RAMSIZE];
		for (Cnt = 0; Cnt < size; Cnt++){
			memblock[Cnt] = (RAM[Cnt] & 0xff);
		}		
		file.write (memblock, size);
		file.close();		
		
		delete[] memblock;
	}	
}

void EPOCHCPU::SetAudioIRQ(void) {
	h8.per_regs[EPINTSTT] |= INTAUDIO;
}

void __fastcall EPOCHCPU::LoadRAM(const char* FileString){

	unsigned long Cnt;
	streampos size;
	char * memblock;

	ifstream file (FileString, ios::in|ios::binary|ios::ate);
	if (file.is_open())	{
		size = file.tellg();
		if (size < 0) {
			file.close();
			return;
		}

		streamsize readSize = (size > (streampos)RAMSIZE) ? (streamsize)RAMSIZE : (streamsize)size;
		memblock = new char [RAMSIZE];
		ZeroMemory(memblock, RAMSIZE);
		file.seekg (0, ios::beg);
		file.read (memblock, readSize);
		file.close();		
		for (Cnt = 0; Cnt < RAMSIZE; Cnt++){
			RAM[Cnt] = (memblock[Cnt] & 0xff);
		}
		delete[] memblock;
	}
}

signed long BoardEpoch::LoadROM(char* name1, char* name2, char* name3, char* name4, char FlashSw){
	const signed long ret = fMainCPU->LoadROM(name1, name2, name3, name4, FlashSw);
	if (ret > 0 && FlashSw != 0 && ret > 0x80000) {
		const UINT32 audioBytes = std::min<UINT32>(static_cast<UINT32>(ret - 0x80000), SampledSound::SoundMemorySize);
		fSound.LoadSoundData(fMainCPU->ROM + 0x80000, audioBytes);
	}
	return ret;
}
void BoardEpoch::Init(){	
	
}

///////////////////////////////////////////////////////////////////////
//
//		EPOCH CPU Class Implementation
//
///////////////////////////////////////////////////////////////////////

UINT8 ascokitab[64] = {
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,		// 00 - 07
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,		// 08 - 1f
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,		// 10 - 17
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,		// 18 - 1f
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,		// 20 - 27
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,		// 28 - 2f
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,		// 30 - 37
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f		// 38 - 3f
};

signed long EPOCHCPU::LoadROM(char* name1, char* name2, char* name3, char* name4, char FlashSw){
		
	FILE *file1,*file2,*file3,*file4;
	//FILE *DebugFile;
	char *buffer1, *buffer2, *buffer3, *buffer4;	
	
	char Enable1, Enable2, Enable3, Enable4;
	UINT32 TotalSize, fileLen1, fileLen2, fileLen3, fileLen4, cnt;	

	if (name1 == NULL){
		Enable1 = 0;
	} else {
		Enable1 = 1;
	}
	if (name2 == NULL){
		Enable2 = 0;
	} else {
		Enable2 = 1;
	}
	if (name3 == NULL){
		Enable3 = 0;
	} else {
		Enable3 = 1;
	}
	if (name4 == NULL){
		Enable4 = 0;
	} else {
		Enable4 = 1;
	}

	buffer1 = 0;
	buffer2 = 0;
	buffer3 = 0;
	buffer4 = 0;
	
	//fopen_s(&DebugFile, "Debug.txt","a");   	   
   	   
	
	//ROM File 1
	if (Enable1){
		//Open file
		//fprintf(DebugFile, "ROM1: %s \n", name1);
		fopen_s(&file1, name1, "rb");
		if (!file1){
			return 0;
		}
	
		//Get file length
		fseek(file1, 0L, SEEK_END);
		fileLen1 = ftell(file1);
		fseek(file1, 0L, SEEK_SET);	

		//Check for strlen error
		if (fileLen1 < 0) return 0;

		//Allocate memory
		buffer1 = (char *)malloc(fileLen1);
		if (!buffer1)
		{		
			fclose(file1);
			return 0;
		}

		//Read file contents into buffer
		fread(buffer1, fileLen1, 1, file1);
		fclose(file1);
	} else {
		fileLen1 = 0;
	}
	//ROM File 2
	if (Enable2){
		//Open file
		//fprintf(DebugFile, "ROM2: %s \n", name2);
		fopen_s(&file2, name2, "rb");
		if (!file2){
			return 0;
		}
	
		//Get file length
		fseek(file2, 0L, SEEK_END);
		fileLen2 = ftell(file2);
		fseek(file2, 0L, SEEK_SET);	

		//Check for strlen error
		if (fileLen2 < 0) return 0;

		//Allocate memory
		buffer2 = (char *)malloc(fileLen2);
		if (!buffer2)
		{		
			fclose(file2);
			return 0;
		}

		//Read file contents into buffer
		fread(buffer2, fileLen2, 1, file2);
		fclose(file2);
	} else {
		fileLen2 = 0;
	}
	//ROM File 3
	if (Enable3){
		//Open file
		//fprintf(DebugFile, "ROM3: %s \n", name3);
		fopen_s(&file3, name3, "rb");
		if (!file3){
			return 0;
		}
	
		//Get file length
		fseek(file3, 0L, SEEK_END);
		fileLen3 = ftell(file3);
		fseek(file3, 0L, SEEK_SET);	

		//Check for strlen error
		if (fileLen3 < 0) return 0;

		//Allocate memory
		buffer3 = (char *)malloc(fileLen3);
		if (!buffer3)
		{		
			fclose(file3);
			return 0;
		}

		//Read file contents into buffer
		fread(buffer3, fileLen3, 1, file3);
		fclose(file3);
	} else {
		fileLen3 = 0;
	}
	//ROM File 4
	if (Enable4){
		//Open file
		//fprintf(DebugFile, "ROM4: %s \n", name4);
		fopen_s(&file4, name4, "rb");
		if (!file4){
			return 0;
		}
	
		//Get file length
		fseek(file4, 0L, SEEK_END);
		fileLen4 = ftell(file4);
		fseek(file4, 0L, SEEK_SET);	

		//Check for strlen error
		if (fileLen4 < 0) return 0;

		//Allocate memory
		buffer4 = (char *)malloc(fileLen4);
		if (!buffer4)
		{		
			fclose(file4);
			return 0;
		}

		//Read file contents into buffer
		fread(buffer4, fileLen4, 1, file4);
		fclose(file4);
	} else {
		fileLen4 = 0;
	}
	
	

	//Clear ROM Space
	for (cnt = 0; cnt < ROMSIZE; cnt++) {
		ROM[cnt] = 0;
	}

	
	//fprintf(DebugFile, "ROM1 Size: %X \n", fileLen1);
	//fprintf(DebugFile, "ROM2 Size: %X \n", fileLen2);
	//fprintf(DebugFile, "ROM3 Size: %X \n", fileLen3);
	//fprintf(DebugFile, "ROM4 Size: %X \n", fileLen4);
	TotalSize = (fileLen1 + fileLen2 + fileLen3 + fileLen4);	
	//fprintf(DebugFile, "Total ROM Size: %X \n", TotalSize);
	
	if (FlashSw == 0){

		if (fileLen1 > 0x100000) return 0;
		if (fileLen2 > 0x100000) return 0;
		if (fileLen3 > 0x100000) return 0;
		if (fileLen4 > 0x100000) return 0;

		//ROM1	LO
		if (Enable1){		
			for (cnt = 0; (cnt < (fileLen1 * 2)); cnt += 2) {
				ROM[cnt] = (buffer1[cnt >> 1] & 255);
			}
			free(buffer1);
		}
		//ROM2 HI
		if (Enable2){		
			for (cnt = 1; (cnt < (fileLen2 * 2)); cnt += 2) {
				ROM[cnt] = (buffer2[(cnt - 1) >> 1] & 255);
			}
			free(buffer2);	
		}
		unsigned int IntermediateSize = (fileLen1 + fileLen2);
			   
		//ROM3	LO
		if (Enable3){		
			for (cnt = 0; (cnt < (fileLen3 * 2)); cnt += 2) {
				if ((IntermediateSize + cnt) < ROMSIZE) {
					ROM[IntermediateSize + cnt] = (buffer3[cnt >> 1] & 255);
				}
			}
			free(buffer3);
		}
		//ROM4 HI
		if (Enable4){		
			for (cnt = 1; (cnt < (fileLen4 * 2)); cnt += 2) {
				if ((IntermediateSize + cnt) < ROMSIZE) {
					ROM[IntermediateSize + cnt] = (buffer4[(cnt - 1) >> 1] & 255);
				}
			}
			free(buffer4);	
		}

	} else {
		//FLASH ROM
		if (Enable1){	

			if (fileLen1 > 0x400000) return 0;			

			for (cnt = 0; (cnt < (fileLen1)); cnt++) {
				ROM[cnt] = (buffer1[cnt] & 255);
			}
			free(buffer1);
			
			// Flash images contain the YMZ280B data after the first 512 KiB.
			// BoardEpoch transfers it directly to the pull-audio device.

		}
	}
	

	

	//fclose (DebugFile);

	return TotalSize;
}

UINT8 BoardEpoch::GetStatusLED(void){
	return fMainCPU->fStatusLED;
}

void __fastcall BoardEpoch::PowerOnReset(void)
{

	fRTCDevice->Reset();

	fMainCPU->last_int_sts_reg = 0x00;
	fMainCPU->rfsh_int_count = 0;
	fMainCPU->sync_int_count = 0;
	fMainCPU->lamp_timer_count = 0;
	fMainCPU->func_sw = false;
	fMainCPU->fReelToggle = false;
	fMainCPU->IOMAP_LAMP_DIM = 0x00;
	fMainCPU->IOMAP_LED_DIM = 0x00;
	fMainCPU->IOMAP_MECHDRIVE = 0x00;
	fMainCPU->IOMAP_DIVERTS = 0x00;
	fMainCPU->IOMAP_HOPDRIVE = 0x00;
	fMainCPU->IOMAP_DIPS1 = 0x00; //0xff; // 0x00
	fMainCPU->IOMAP_DIPS2 = 0x00; //0x66; // 0x66
	
	for (int i = 0; i < 6; i++)
	{
		fMainCPU->fReelPatterns[i] = 0x00;
	}

	for (int i = 0; i < 512; i++)
	{
		fMainCPU->IOMAP_INPUTS[i] = 0x00;
		fMainCPU->IOMAP_LAMPS[i] = 0x00;
		fMainCPU->IOMAP_LEDS[i] = 0x00;
	}

	for (int i = 0; i < 8; i++)
	{
		fMainCPU->IOMAP_DIPS1 |= 0; //(dipSwitches[i]->Checked ? (0x80 >> (i & 0x07)) : 0);
		fMainCPU->IOMAP_DIPS2 |= 0;// (dipSwitches[i + 8]->Checked ? (0x80 >> (i & 0x07)) : 0);
	}

	for (int i = 0; i < 12; i++)
	{
		fMainCPU->IOMAP_LAMP_TIMER[i] = 0x00;
		fMainCPU->lamp_flash_counts[i] = 0;
		fMainCPU->lamp_flash[i] = false;
		fMainCPU->IOMAP_LED_TIMER[i] = 0x00;
		fMainCPU->led_flash_counts[i] = 0;
		fMainCPU->led_flash[i] = false;
	}

	fHoppers.Reset(0);
	fHoppers.Reset(1);
	fMainCPU->reset();
	fReels.reset();
	fSound.YMZReset();
	if (fAlphaStarburst) fAlphaStarburst->Reset();
	if (fAlphaDotMatrix) fAlphaDotMatrix->Reset();

	ZeroMemory(fMainCPU->RAM, RAMSIZE * sizeof(UINT8));

}
///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	CPUEpoch::io_read_byte_8
//
//  PURPOSE:	Read handler for the H8 I/O Ports
//
//  INPUTS:		UINT32					Address of Read
//
//  OUTPUT:		UINT8						Value read from device
//
///////////////////////////////////////////////////////////////////////
UINT8 __fastcall EPOCHCPU::io_read_byte_8(UINT32 address)
{
	UINT8 rv = 0x00;

	switch (address & 0xff)
	{
		case H8_PORT8:
			// H8 Port 8 contains part of the RTC Device
			rv = fOwner->fRTCDevice->GetPort8();
			break;
		case H8_PORTA:
			// H8 Port A contains part of the RTC Device
			rv = fOwner->fRTCDevice->GetPortA();
			break;
		case H8_SERIAL_A:
			break;
		case H8_SERIAL_B:
			break;
		default:
			//Unknown
			break;
	}

	return rv;
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	CPUEpoch::io_write_byte_8
//
//  PURPOSE:	Write handler for the H8 I/O Ports
//
//  INPUTS:		UINT32					Address of Read
//					UINT8						Value written to device
//
//  OUTPUT:		none
//
///////////////////////////////////////////////////////////////////////
void __fastcall EPOCHCPU::io_write_byte_8(UINT32 address, UINT8 value) {
	switch (address & 0xff)
	{
		case H8_PORT8:
			break;
		case H8_PORTA:
			{
				
			bool cpuClock = (value & 0x02) ? true : false;
			bool cpuData = (value & 0x04) ? true : false;
			fOwner->fRTCDevice->Write(cpuClock, cpuData);
			}
			break;
		case H8_SERIAL_A:
			break;
		case H8_SERIAL_B:
			break;
		default:
			break;
	}
}
///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	CPUEpoch::cpu_readop16
//
//  PURPOSE:	Memory Read handler used directly by the CPU core
//
//  INPUTS:		UINT32					Address of Read
//
//  OUTPUT:		UINT16					Value read from memory
//
///////////////////////////////////////////////////////////////////////
UINT16 __fastcall EPOCHCPU::cpu_readop16(UINT32 address)
{
	AccessType = MEMORY_AREA;

	if (address > 0xffffff){
		h8.err = 84;
	}
	if (address < 0x7ffff)
	{
		return ROM[address + 1] + (256 * ROM[address]);
	}
	else
	{
		return program_read_word(address);
	}
}

UINT32 EPOCHCPU::h8_mem_read32(UINT32 address)
{
	AccessType = MEMORY_AREA;

	if (address > 0xffffff){
		h8.err = 85;
	}
	if (address < 0x7fffd)
	{
		return (UINT32)ROM[address + 3] | ((UINT32)ROM[address + 2] << 8) |	((UINT32)ROM[address + 1] << 16) | ((UINT32)ROM[address] << 24);
	}
	else
	{
		return ((UINT32)program_read_word(address) << 16) |
			(UINT32)program_read_word(address + 2);

	}
}

void EPOCHCPU::h8_mem_write32(UINT32 address, UINT32 data)
{
	AccessType = MEMORY_AREA;

	if (address > 0xffffff){
		h8.err = 86;
	}
	program_write_word(address, (data >> 16));
	program_write_word(address + 2, (data & 0xffff));
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	CPUEpoch::program_read_byte
//
//  PURPOSE:	Memory Read handler for 8-bit memory reads. This
//					will include 16-bit reads that are delegated into a pair
//					of 8-bit reads in order to keep MMIO decoding in a
//					single location within the source code.
//
//  INPUTS:		UINT32					Address of Read
//
//  OUTPUT:		UINT8						Value read from memory
//
///////////////////////////////////////////////////////////////////////
UINT8 __fastcall EPOCHCPU::program_read_byte(UINT32 address)
{
	AccessType = MEMORY_AREA;

	UINT8 value = 0xff;

	if (address < 0x80000)
	{
		value = ROM[address];

		return value;
	}
	else if ((address >= 0xffff10) && (address <= 0xffffff))
	{
		AccessType = ON_CHIP_MODULE;
		switch (address & 0xff)
		{
			//Epoch System Registers
			case EPSYSSTT: //Epoch System Status
				value = h8.per_regs[EPSYSSTT];
				break;
			case EPIOCNTL://Epoch IO Control & status
				value = (fStatusLED << 1);
				break;
			case EPIOMODE://Epoch IO Mode
				value = h8.per_regs[EPIOMODE];
				break;
			case EPSYSCTL://Epoch System Control
				value = h8.per_regs[EPSYSCTL];
				break;
			case EPINTENB://Epoch Interrupt Enable
				value = h8.per_regs[EPINTENB];
				break;
			case EPINTSTT://Epoch Interrupt Status
				//Status is cleared by writing to it, reading from it simply returns the reg.
				value = h8.per_regs[EPINTSTT];
				value &= ~INTFUNCSW;
				value |= (func_sw ? INTFUNCSW : 0x00);
				last_int_sts_reg = value;
				break;
			case EPI2CCTL://Epoch I2C Control
				value = h8.per_regs[EPI2CCTL];
				break;
			case EPI2CDAT://Epoch I2C Data
				value = h8.per_regs[EPI2CDAT];
				break;
			case EPFPGAVER:	
				// FPGA version register
				value = 0x02;	// Version 2 FPGA for now ;)
				break;
			case 0x16:
			case 0x1a:
			case 0x1c:
			case 0x1d:
			case 0x1e:
			case 0x1f:
				h8.err = 89; 
				break;
			default:
				value = (UINT32)h8_register_read8(address);
				break;
		}

		return value;
	}
	else if ((address >= 0xfe0000) && (address < 0xff0000))
	{
		if ((address >= 0xfe0800) && (address < 0xfe121c))
		{
			switch (address & 0xff00)
			{				
				case 0x0800:  // Lamps
				case 0x0900:  // Lamps
					value = IOMAP_LAMPS[(address & 0x1ff)];
					break;
				case 0x0A00:  // LEDs
				case 0x0B00:  // LEDs
					value = IOMAP_LEDS[(address & 0x1ff)];
					break;
				case 0x0C00:  // Inputs
				case 0x0D00:  // Inputs
					switch (address)
					{
					//case 0xfe0c00:	// 
					{
						//Bit 0x01 is Switch 0 
						//Bit 0x02 is Switch 1
						//Bit 0x04 is Switch 2
						//Bit 0x08 is Switch 3
						//Bit 0x10 is Switch 4
						//Bit 0x20 Is Switch 5
						//Bit 0x40 Is Switch 6
						//Bit 0x80 is Switch 7

					}
					break;
					//case 0xfe0c01:	// 
					{
						//Bit 0x01 is Switch 8
						//Bit 0x02 is Switch 9
						//Bit 0x04 is Switch 10
						//Bit 0x08 is Switch 11
						//Bit 0x10 is Switch 12
						//Bit 0x20 Is Switch 13
						//Bit 0x40 Is Switch 14
						//Bit 0x80 is Switch 15
						
					}
					break;
					case 0xfe0c02:	// Coin Mech + Stake Key
					{
						//Bit 0x01 is Coin Mech 1
						//Bit 0x02 is Coin Mech 6
						//Bit 0x04 is Coin Mech 2
						//Bit 0x08 is Coin Mech 3
						//Bit 0x10 is Coin Mech 4
						//Bit 0x20 Is Coin Mech 5
						//Bit 0x40 Is Stake 1
						//Bit 0x80 is Stake 2

						//Coin Mech
						value = IOMAP_INPUTS[(address & 0x1ff)] & 0x3f;
						// S/P bits 1 and 2
						UINT8 stakePrize = ((fOwner->Stake << 4) | fOwner->Prize);
						value |= ((stakePrize & 0x03) << 6);
					}
					break;
					case 0xfe0c03:	// Stake + Percent Keys
					{
						//Bit 0x01 is Stake 3
						//Bit 0x02 is Stake 4
						//Bit 0x04 is Stake 6
						//Bit 0x08 is Stake 5
						//Bit 0x10 is Percent 1
						//Bit 0x20 Is Percent 2
						//Bit 0x40 Is Percent 3
						//Bit 0x80 is Percent 4

						value = 0;								
						//Top 4 Bits Percent Key
						value |= ((fOwner->Percent << 7) & 0x80);
						value |= ((fOwner->Percent << 5) & 0x40);
						value |= ((fOwner->Percent << 3) & 0x20);
						value |= ((fOwner->Percent << 1) & 0x10);
																	
						// S/P Bits 3 and 4
						UINT8 stakePrize = ((fOwner->Stake << 4) | fOwner->Prize);

						value |= ((stakePrize & 0x0c) >> 2);
						value |= ((stakePrize & 0x10) >> 1);
						if (IOMAP_MECHDRIVE & 0x01)
						{
							// Extra S/P bits requested
							value |= ((stakePrize & 0x40) >> 4);									
						}
						else
						{
							value |= ((stakePrize & 0x20) >> 3);
						}
					}
					break;
					case 0xfe0c04:
						//Bit 0x01 is 
						//Bit 0x02 is
						//Bit 0x04 is Reel Opto 5 (Reel Extender)
						//Bit 0x08 is Reel Opto 6 (Reel Extender)
						//Bit 0x10 is Reel Opto 1
						//Bit 0x20 Is Reel Opto 2
						//Bit 0x40 Is Reel Opto 3
						//Bit 0x80 is Reel Opto 4
						if (fOwner->ReelExt){
							value = 0x01 | ((fOwner->fReels.optos << 2) & 0xfc);
						} else {
							value = 0x01 | ((fOwner->fReels.optos << 4) & 0xf0);
						}
						break;
					case 0xfe0c05:	// Hopper Returns + meter Sw + Note Alm
						//Bit 0x01 is Hopper 3
						//Bit 0x02 is Hopper 6
						//Bit 0x04 is Hopper 4
						//Bit 0x08 is Hopper 5
						//Bit 0x10 is Hopper 7
						//Bit 0x20 Is Hopper 8
						//Bit 0x40 Is Meter Return
						//Bit 0x80 is Note Alarm

						value = 0x40;	//Meter Return
						value |= (IOMAP_INPUTS[(address & 0x1ff)] & 0x80); //Switches							
						value |= fOwner->fHoppers.ReadOpto(0);
						break;
					//case 0xfe0c06:	//
						//Bit 0x01 is Note Acceptor 1
						//Bit 0x02 is Note Acceptor 2
						//Bit 0x04 is Note Acceptor 3
						//Bit 0x08 is Note Acceptor 4
						//Bit 0x10 is Cab Switches 1
						//Bit 0x20 Is Cab Switches 2
						//Bit 0x40 Is Cab Switches 3
						//Bit 0x80 is Cab Switches 4
						
						break;
					//case 0xfe0c07:	//
						//Bit 0x01 is Cab Switches 5
						//Bit 0x02 is Cab Switches 6
						//Bit 0x04 is Cab Switches 7
						//Bit 0x08 is Cab Switches 8
						//Bit 0x10 is 
						//Bit 0x20 Is 
						//Bit 0x40 Is Hopper 1
						//Bit 0x80 is Hopper 2						
						break;
					default:
						value = IOMAP_INPUTS[(address & 0x1ff)];
						break;
					}
					break;
				case 0x0E00:  // Input Interrupt Enable
				case 0x0F00:  // Input Interrupt Enable
					value = IOMAP_INPUT_IRQEN[(address & 0x1ff)] + (256 * IOMAP_INPUT_IRQEN[((address + 1) & 0x1ff)]);
					break;
				case 0x1000:  // Outputs
					switch (address)
					{
						case 0xfe1001:	// Alpha Control
							break;
						case 0xfe1002:	// Mech
							value = IOMAP_MECHDRIVE;
							break;
						case 0xfe1003:	// Diverts
							value = IOMAP_DIVERTS;
							break;
						case 0xfe1004:	// Reel 1/2 Outputs
							if (fOwner->ReelExt)
							{
								// Handle 8 reels
								value = fReelToggle ? ((fReelPatterns[4] << 4) | fReelPatterns[3]) : ((fReelPatterns[1] << 4) | fReelPatterns[0]);
							} else {				
								// Handle 4 reels
								value = ((fReelPatterns[1] << 4) | fReelPatterns[0]);
							}
							break;
						case 0xfe1005:	// Reel 3/4 Outputs
							if (fOwner->ReelExt)
							{
								// Handle 6 reels
								value = fReelToggle ? 0x10 : 0x00;
								value |= (fReelToggle ? fReelPatterns[5]: fReelPatterns[2]);
							}
							else
							{						
								// Handle 6 reels
								value = ((fReelPatterns[3] << 4) | fReelPatterns[2]);
							}
							break;
						case 0xfe1007:	// Hoppers
							value = IOMAP_HOPDRIVE;
							break;
						default:
							break;
					}
					break;
				case 0x1200:  // Misc
					if ((address >= 0xfe1200) && (address <= 0xfe120b))
					{
						value = lamp_flash_counts[(address - 0xfe1200)];
						break;
					}
					else if ((address >= 0xfe120c) && (address <= 0xfe1217))
					{
						value = led_flash_counts[(address - 0xfe120c)];
						break;
					}
					else if (address == 0xfe1218)
					{
						value = IOMAP_LAMP_DIM;
						break;
					}
					else if (address == 0xfe1219)
					{
						value = IOMAP_LED_DIM;
						break;
					}
					else if ((address >= 0xfe121a) && (address <= 0xfe121b))
					{
						switch (address)
						{
							case 0xfe121a:
								value = IOMAP_DIPS1;
								break;
							case 0xfe121b:
								value = IOMAP_DIPS2;
								break;
						}
						break;
					}
					else
					{
						h8.err = 118;
					}
					break;
				default:
					//h8.err = 119;
					break;
			}

			return value;
		}

		address -= 0xFE0000;
		if ((address >= 0) && (address < RAMSIZE)) {
			value = RAM[address];
		}
		else {
			h8.err = 109;
		}

		return value;
	}
	else
	{
		if ((address >= 0xFFFD10) && (address <= 0xFFFF0F) && (h8.per_regs[SYSCR] & RAME)) {
			AccessType = ON_CHIP_MEMORY;
			return h8.onchip_ram[address - 0xFFFD10];
		}

		switch (address)
		{
			case 0xfffc00:
				value = fOwner->fSound.YMZReadReg();
				break;
			case 0xfffc02:
				value = fOwner->fSound.YMZReadStatus();
				break;
			default:
				h8.err = 117;
				break;
		}

		// Absolute fall-through
		return value;
	}
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	CPUEpoch::program_read_word
//
//  PURPOSE:	Memory Read handler for 16-bit memory reads. Some of
//					these reads are delegated into a pair of 8-bit reads
//					in order to keep MMIO decoding in a single location
//					within the source code.
//
//  INPUTS:		UINT32					Address of Read
//
//  OUTPUT:		UINT16					Value read from memory
//
///////////////////////////////////////////////////////////////////////
UINT16 __fastcall EPOCHCPU::program_read_word(UINT32 address)
{
	AccessType = MEMORY_AREA;
	
	UINT16 value = 0xffff;

	if (address & 1)
	{
		// Data alignment mismatch
		h8.err = 90;
	}

	if (address < 0x7ffff)
	{
		value = (UINT16)ROM[address + 1];
		value |= ((UINT16)ROM[address] << 8);

		return value;
	}
	else if ((address >= 0xffff10) && (address <= 0xffffff))
	{
		AccessType = ON_CHIP_MODULE;
		// Delegate this through as a pair of 8-bit reads because
		// the 8-bit handler contains the I/O Register Decoding
		value = (UINT16)program_read_byte(address + 1);
		value |= ((UINT16)program_read_byte(address) << 8);

		return value;
	}
	else if ((address >= 0xfe0000) && (address < 0xfe121c))
	{
		value = (UINT16)program_read_byte(address + 1);
		value |= ((UINT16)program_read_byte(address) << 8);

		return value;
	}
	else if ((address >= 0xfe121c) && (address < 0xff0000))
	{
		address -= 0xFE0000;
		if ((address >= 0) && (address < RAMSIZE - 1)) {
			value = (UINT16)RAM[address + 1];
			value |= ((UINT16)RAM[address] << 8);
		}
		else {
			h8.err = 120;
		}
		return value;
	}
	else if ((address >= 0xfffd10) && (address <= 0xffff0e))
	{
		// On-chip RAM is 512 bytes at FFFD10-FFFF0F when RAME is set.
		if (h8.per_regs[SYSCR] & RAME) {
			AccessType = ON_CHIP_MEMORY;
			UINT32 offset = address - 0xfffd10;
			value = ((UINT16)h8.onchip_ram[offset] << 8) | h8.onchip_ram[offset + 1];
			return value;
		}

	}
	else
	{
		h8.err = 121;
	}
	// Catch-all return value
	return value;
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	CPUEpoch::program_write_byte
//
//  PURPOSE:	Memory Read handler for 8-bit memory writes. This
//					will include 16-bit writes that are delegated into a pair
//					of 8-bit writes in order to keep MMIO decoding in a
//					single location within the source code.
//
//  INPUTS:		UINT32					Address of Read
//					UINT8						Value to be written
//
//  OUTPUT:		none
//
///////////////////////////////////////////////////////////////////////
void __fastcall EPOCHCPU::program_write_byte(UINT32 address, UINT8 value)
{

	AccessType = MEMORY_AREA;

	if ((address >= 0xffff10) && (address <= 0xffffff))
	{
		AccessType = ON_CHIP_MODULE;

		switch (address & 0xff)
		{
			//Epoch System Registers
			case EPSYSSTT://Epoch System Status
				h8.per_regs[EPSYSSTT] = value;
				break;
			case EPIOCNTL://Epoch IO Control / Status Reg
				
				//0x40	// force primary and boot reset
				//0x80	// force primary, boot and security reset

				if (value & 0xc0)
				{
					h8.err = 91;
				}

				value = (value & SSLED) >> 1;

				if (value != fStatusLED)
				{
					fStatusLED = value;
				}
				break;
			case EPIOMODE://Epoch IO Mode
				h8.per_regs[EPIOMODE] = value;
				break;
			case EPSYSCTL://Epoch System Control
				h8.per_regs[EPSYSCTL] = value;
				//0x01	// Watchdog bit
				//0x02	// NV Ram enable bit
				//0x04	// I/O link enable bit
				//0x08  ??
				//0x10  ??
				//0x20	// multi-plane matrix enable
				//0x40	// 2nd matrix plane enable
				//0x80	// force matrix display sync
				if (value & 0x18) {
					h8.err = 92;
				}
				break;
			case EPINTENB://Epoch Interrupt Enable
				h8.per_regs[EPINTENB] = value;
				break;
			case EPINTSTT://Epoch Interrupt Status
				// A write of "1" in a bit clears the status
				// provided that the bit was 1 when last read
				if (value & INTINPUT	& last_int_sts_reg) h8.per_regs[EPINTSTT] &= ~(INTINPUT);
				if (value & INTAUDIO	& last_int_sts_reg) h8.per_regs[EPINTSTT] &= ~(INTAUDIO);
				if (value & INTI2C		& last_int_sts_reg) h8.per_regs[EPINTSTT] &= ~(INTI2C);
				if (value & INTSYNC		& last_int_sts_reg) h8.per_regs[EPINTSTT] &= ~(INTSYNC);
				if (value & INTFUNCSW	& last_int_sts_reg) h8.per_regs[EPINTSTT] &= ~(INTFUNCSW);
				if (value & INTRFRSH	& last_int_sts_reg) h8.per_regs[EPINTSTT] &= ~(INTRFRSH);
				if (value & INTFRAME	& last_int_sts_reg) h8.per_regs[EPINTSTT] &= ~(INTFRAME);
				if (value & INTMATRIX	& last_int_sts_reg) h8.per_regs[EPINTSTT] &= ~(INTMATRIX);
				break;
			case EPI2CCTL://Epoch I2C Control
				h8.per_regs[EPI2CCTL] = value;
				break;
			case EPI2CDAT://Epoch I2C Data
				h8.per_regs[EPI2CDAT] = value;
				break;
			case 0x16:
			case 0x1a:
			case 0x1c:
			case 0x1d:
			case 0x1e:
			case 0x1f:
				h8.err = 93;
			default:
				h8_register_write8(address, value);
				break;
		}
	}
	else if ((address >= 0xfe0000) && (address < 0xff0000))
	{
		if ((address >= 0xfe0800) && (address < 0xfe121c))
		{
			switch (address & 0xff00)
			{
				case 0x0000: // Dot Matrix
				case 0x0100: // Dot Matrix
				case 0x0200: // Dot Matrix
				case 0x0300: // Dot Matrix
				case 0x0400: // Dot Matrix
				case 0x0500: // Dot Matrix
				case 0x0600: // Dot Matrix
				case 0x0700: // Dot Matrix
					break;
				case 0x0800:  // Lamps
				case 0x0900:  // Lamps
					IOMAP_LAMPS[(address & 0x1ff)] = value;
					break;
				case 0x0A00:  // LEDs
				case 0x0B00:  // LEDs
					IOMAP_LEDS[(address & 0x1ff)] = value;
					break;
				case 0x0C00:  // Inputs
				case 0x0D00:  // Inputs
					break;
				case 0x0E00:  // Input Interrupt Enable
				case 0x0F00:  // Input Interrupt Enable
					IOMAP_INPUT_IRQEN[(address & 0x1ff)] = value;
					break;
				case 0x1000:  // Outputs
					switch (address)
					{
						case 0xfe1000:	
							// SDK: FE1000 bit 0 = alpha serial data, bit 1 = alpha data send/clock.
							// Keep the old starburst byte path for starburst layouts, but do not feed
							// the complete output byte directly to the dot-matrix Samsung command decoder.
							if (fOwner)
							{
								if (fOwner->fAlphaStarburst)
								{
									fOwner->fAlphaStarburst->Enable(1);
									for (int i = 0; i < 16; i++)
									{
										fOwner->fAlphaStarburst->OutChars[i] = ((char)(ascokitab[fOwner->fAlphaStarburst->Chars[i] & 0x3f]));
									}

									if (value != 0xb0)
										fOwner->fAlphaStarburst->DoChar(value);
								}

								if (fOwner->fAlphaDotMatrix)
								{									
									fOwner->fAlphaDotMatrix->WriteOutput0(value);
								}
							}							
							break;
					case 0xfe1001:	
							//Bit 0x01 is Alpha Inter Char Strobe
							//Bit 0x02 is Alpha Reset
							//Bit 0x04 is Refill Meter
							//Bit 0x08 is Coin Mech Lamp Left
							//Bit 0x10 is Coin Mech Lamp Middle
							//Bit 0x20 Is Coin Mech Lamp Right
							//Bit 0x40 Is 
							//Bit 0x80 is 
							if (fOwner->fAlphaDotMatrix) fOwner->fAlphaDotMatrix->WriteOutput1(value);
							break;
					case 0xfe1002:	
							//Bit 0x01 is Extra Stake Key Pin Select
							//Bit 0x02 is Coin Mech 4
							//Bit 0x04 is Coin Mech 1
							//Bit 0x08 is Coin Mech 2
							//Bit 0x10 is Coin Mech 3
							//Bit 0x20 Is Coin Mech 7
							//Bit 0x40 Is Coin Mech 5
							//Bit 0x80 is Coin Mech 6
							IOMAP_MECHDRIVE = value;
							fOwner->fCoinMech[0].SetLockoutPort(value);
							break;
						case 0xfe1003:	
							//Bit 0x01 is Divert 1
							//Bit 0x02 is Divert 2
							//Bit 0x04 is Divert 3
							//Bit 0x08 is Divert 4
							//Bit 0x10 is Divert 5
							//Bit 0x20 Is Divert 6
							//Bit 0x40 Is Divert 7
							//Bit 0x80 is Divert 8
							IOMAP_DIVERTS = value;
							break;
						case 0xfe1004:	
							//Bit 0x01 is Reel Motor 1 / 5
							//Bit 0x02 is Reel Motor 1 / 5
							//Bit 0x04 is Reel Motor 1 / 5
							//Bit 0x08 is Reel Motor 1 / 5
							//Bit 0x10 is Reel Motor 2 / 6
							//Bit 0x20 Is Reel Motor 2 / 6
							//Bit 0x40 Is Reel Motor 2 / 6
							//Bit 0x80 is Reel Motor 2 / 6
							if (fOwner)
							{								
								if (fOwner->ReelExt)
								{
									// Handle 8 reels
									if (fReelToggle)
									{
										fReelPatterns[4] = value & 0x0f;
										fReelPatterns[5] = value >> 4;
										fOwner->fReels.write(fReelPatterns[4], 4);
										fOwner->fReels.write(fReelPatterns[5], 5);
									}
									else
									{
										fReelPatterns[0] = value & 0x0f;
										fReelPatterns[1] = value >> 4;
										fOwner->fReels.write(fReelPatterns[0], 0);
										fOwner->fReels.write(fReelPatterns[1], 1);
									}
								}
								else
								{
									// Handle 4 reels
									fReelPatterns[0] = value & 0x0f;
									fReelPatterns[1] = value >> 4;
									fOwner->fReels.write(fReelPatterns[0], 0);
									fOwner->fReels.write(fReelPatterns[1], 1);
								}								
							}
							break;
						case 0xfe1005:	
							//Bit 0x01 is Reel Motor 3 / 7
							//Bit 0x02 is Reel Motor 3 / 7
							//Bit 0x04 is Reel Motor 3 / 7
							//Bit 0x08 is Reel Motor 3 / 7
							//Bit 0x10 is Reel Motor 4 / 8
							//Bit 0x20 Is Reel Motor 4 / 8
							//Bit 0x40 Is Reel Motor 4 / 8
							//Bit 0x80 is Reel Motor 4 / 8
							if (fOwner)
							{								
								if (fOwner->ReelExt)
								{
									fReelToggle = ((value & 0x10) == 0x10);

									// Handle 8 reels
									if (fReelToggle)
									{
										fReelPatterns[6] = value & 0x0f;
										fReelPatterns[7] = value >> 4;
										fOwner->fReels.write(fReelPatterns[6], 6);
										fOwner->fReels.write(fReelPatterns[7], 7);
									}
									else
									{
										fReelPatterns[2] = value & 0x0f;
										fReelPatterns[3] = value >> 4;
										fOwner->fReels.write(fReelPatterns[2], 2);
										fOwner->fReels.write(fReelPatterns[3], 3);
									}
								}
								else
								{
									// Handle 4 reels
									fReelPatterns[2] = value & 0x0f;
									fReelPatterns[3] = value >> 4;
									fOwner->fReels.write(fReelPatterns[2], 2);
									fOwner->fReels.write(fReelPatterns[3], 3);
								}								
							}
							break;
						case 0xfe1006:	
							//Bit 0x01 is Meter 1
							//Bit 0x02 is Meter 2
							//Bit 0x04 is Meter 3
							//Bit 0x08 is Meter 4
							//Bit 0x10 is Notes 1
							//Bit 0x20 Is Notes 2
							//Bit 0x40 Is Notes 3
							//Bit 0x80 is Notes 4
							int loop;	
							if (fOwner->fMeters.SECSwitch){
								//SEC Meters - TBC
								fOwner->fMeters.SetEnable(value & 4);								
								fOwner->fMeters.SetData(value & 2);
								fOwner->fMeters.SetClock(value & 1);
							} else {
								//Mechanical Meters
								if ((value & 0xf) == 0xf){
									for (loop = 0; loop < 4; loop++){
										fOwner->fMeters.Write(loop, 1);									
									}
								} else if (fOwner->PrevValue == 0xf) {
									for (loop = 0; loop < 4; loop++){								
										if (((value >> loop) & 1) == 0) {
											fOwner->fMeters.Write(loop, 0);
										}																
									}
								}
								fOwner->PrevValue = (value & 0xf);
							}
							break;
						case 0xfe1007:	
							IOMAP_HOPDRIVE = value;
							//Bit 0x01 is Notes Escrow
							//Bit 0x02 is Hopper
							//Bit 0x04 is Hopper
							//Bit 0x08 is Hopper
							//Bit 0x10 is Hopper
							//Bit 0x20 Is Hopper Opto Test Enable (strim / blocked chute detection)
							//Bit 0x40 Is Hopper Motor
							//Bit 0x80 is Hopper

							//The motor circuit also activates the opto enable						
							fOwner->fHoppers.WriteOptoEnable(0, (IOMAP_HOPDRIVE & 0x60));
							fOwner->fHoppers.WriteMotor(0, (IOMAP_HOPDRIVE & 0x40));							
							break;
						default:
							break;
					}
					break;
				case 0x1200:  // Misc
					if ((address >= 0xfe1200) && (address <= 0xfe120b))
					{
						IOMAP_LAMP_TIMER[(address - 0xfe1200)] = value;
						lamp_flash_counts[(address - 0xfe1200)] = value;
					}
					else if ((address >= 0xfe120c) && (address <= 0xfe1217))
					{
						IOMAP_LED_TIMER[(address - 0xfe120c)] = value;
						led_flash_counts[(address - 0xfe120c)] = value;
					}
					else if (address == 0xfe1218)
					{
						IOMAP_LAMP_DIM = value;
					}
					else if (address == 0xfe1219)
					{
						IOMAP_LED_DIM = value;
					}
					else if ((address >= 0xfe121a) && (address <= 0xfe121b))
					{
						//DIL SWITCHES
					}
					break;
				default:
					break;
			}

			return;
		}

		address -= 0xFE0000;
		if ((address >= 0) && (address < RAMSIZE)) {
			RAM[address] = value;
		}
		else {
			h8.err = 110;
		}

		return;
	}
	else
	{
		if ((address >= 0xFFFD10) && (address <= 0xFFFF0F) && (h8.per_regs[SYSCR] & RAME)) {
			AccessType = ON_CHIP_MEMORY;
			h8.onchip_ram[address - 0xFFFD10] = value;
			return;
		}

		switch (address)
		{
			case 0xfffc00:
				fOwner->fSound.YMZWriteRegSelect(value);
				break;
			case 0xfffc02:
				fOwner->fSound.YMZWriteReg(value);
				break;
			default:
				h8.err = 114;
				break;
		}
	}
}

//---------------------------------------------------------------------------
void __fastcall EPOCHCPU::program_write_word(UINT32 address, UINT16 value)
{ 
	AccessType = MEMORY_AREA;
	if (address & 1)
	{
		// Data alignment mismatch
		h8.err = 94;
	}

	if ((address >= 0xffff10) && (address <= 0xffffff))
	{
		AccessType = ON_CHIP_MODULE;
		// Delegate this through as a pair of 8-bit writes because
		// the 8-bit handler contains the I/O Register Decoding
		switch(address & 0xff)
		{
			case 0xa8:	// TCSR or TCNT
				if ((value & 0xff00) == 0x5a00)
				{
					// TCNT
					h8_register_write8(address + 1, (value & 0xff));
				}
				else if ((value & 0xff00) == 0xa500)
				{
					// TCSR
					h8_register_write8(address, (value & 0xff));
				}
				break;
			case 0xaa:	// RSTCSR
				if ((value & 0xff00) == 0x5a00)
				{
					// Write bit to TCOE
					h8_register_write8(address, (value & 0x40));
				}
				else if (value == 0xa500)
				{
					// Write 0 in WRST
					h8_register_write8(address + 1, 0x00);
				}
				break;
			default:
				program_write_byte(address, (value & 0xff00) >> 8);
				program_write_byte(address + 1, (value & 0xff));
				break;
		}
	}
	else if ((address >= 0xfe0000) && (address < 0xfe121c))
	{
		program_write_byte(address, (value & 0xff00) >> 8);
		program_write_byte(address + 1, (value & 0xff));
	}
	else if ((address >= 0xfe121c) && (address < 0xff0000))
	{
		address -= 0xFE0000;
		if ((address >= 0) && (address < RAMSIZE - 1)) {
			RAM[address] = (value & 0xff00) >> 8;
			RAM[(address + 1)] = (value & 0xff);
		}
		else {
			h8.err = 115;
		}
	}
	else if ((address >= 0xfffd10) && (address <= 0xffff0e) && (h8.per_regs[SYSCR] & RAME)) {
		// Internal on-chip RAM
		AccessType = ON_CHIP_MEMORY;
		UINT32 offset = address - 0xfffd10;
		h8.onchip_ram[offset] = (value & 0xff00) >> 8;
		h8.onchip_ram[offset + 1] = (value & 0xff);
	}
	else
	{
		h8.err = 111;
	}
}
//---------------------------------------------------------------------------
int BoardEpoch::Execute(int Cycles){
	
	//Function: Takes in the minimum number of CPU cycles to execute, executes them, and returns the total number actually executed
	
	int TotalCycles = 0;
	int InstructionCycles;
	int CyclesToExecute = Cycles;

	while (CyclesToExecute > 0){
		
		//Execute Instruction
		if (TotalCycles == 6524) {
			int val = 0;
		}

		InstructionCycles = fMainCPU->executeNew(TotalCycles);	

		if (InstructionCycles <= 0) {
			fMainCPU->h8.err = 95;
		}

		TotalCycles += InstructionCycles;		
		
		//On board device tick
		fMainCPU->h8_tick(InstructionCycles);				
		
		//Dot Matrix Alphanumeric Display
		fAlphaDotMatrix->RunDotAlpha();

		//Reels
		fReels.run(InstructionCycles);
		
		//Meters
		fMeters.Run(InstructionCycles);
		
		//Hoppers
		fHoppers.Update(0, InstructionCycles);
		fHoppers.Update(1, InstructionCycles);
		
		//Coin Mech		
		if (fCoinMech[0].Run(InstructionCycles)){
			TurnSwitchOn(16 + fCoinMech[0].GetSelectedCoin());
		} else {
			TurnSwitchOff(16 + fCoinMech[0].GetSelectedCoin());
		}
		
		//Error check
		if (fMainCPU->h8.err){
			fMainCPU->h8.ppc;
			fMainCPU->h8.pc;
			fMainCPU->h8.regs[7];
            fMainCPU->h8.err = 0;
		}	
		
		// Epoch Refresh interrupt (6.4KHz) 16,000,000 / 6400
		fMainCPU->rfsh_int_count += InstructionCycles;
		if (fMainCPU->rfsh_int_count >= 2500)
		{
			//Remove cycles
			fMainCPU->rfsh_int_count -= 2500;
			
			//Set Status Register Bit for Epoch
			fMainCPU->h8.per_regs[EPINTSTT] |= INTRFRSH;
			
			//Audio Update
			fSound.YMZUpdate();
		}

		//Check if Audio IRQ happened()
		if (fSound.YMZGetIRQ()) {
			// Mirror the live YMZ IRQ line into the Epoch interrupt status.
			fMainCPU->h8.per_regs[EPINTSTT] |= INTAUDIO;
		}
		else {
			fMainCPU->h8.per_regs[EPINTSTT] &= ~(INTAUDIO);
		}

		// Epoch Voltage SYNC interrupt (100 Hz) 16,000,000 / 100
		fMainCPU->sync_int_count += InstructionCycles;
		if (fMainCPU->sync_int_count >= 160000)
		{
			//Remove Cycles
			fMainCPU->sync_int_count -= 160000;
					
			//Set Status Register Bit for Epoch
			fMainCPU->h8.per_regs[EPINTSTT] |= INTSYNC;


			// Update flash counts
			for (int i = 0; i < 12; i++)
			{
				//Lamps
				if (fMainCPU->lamp_flash_counts[i] == 0)				{
					fMainCPU->lamp_flash_counts[i] = fMainCPU->IOMAP_LAMP_TIMER[i];
					fMainCPU->lamp_flash[i] = !fMainCPU->lamp_flash[i];
				}
				else
				{
					fMainCPU->lamp_flash_counts[i]--;
				}

				//LEDs
				if (fMainCPU->led_flash_counts[i] == 0)
				{
					fMainCPU->led_flash_counts[i] = fMainCPU->IOMAP_LED_TIMER[i];
					fMainCPU->led_flash[i] = !fMainCPU->led_flash[i];
				}
				else
				{
					fMainCPU->led_flash_counts[i]--;
				}
			}
		}
		
		CyclesToExecute -= InstructionCycles;
	}

	return TotalCycles;
}

void BoardEpoch::SetCFolder(const char* Folder){
	CFolder = Folder ? Folder : "";
}

void BoardEpoch::SetCFileName(const char* FileName) {
	CFileName = FileName ? FileName : "";
}

void BoardEpoch::SaveState(void) {
	if (CFileName.empty()) { return; }
	const char Last = CFolder.empty() ? '\0' : CFolder.back();
	const char* Separator = (CFolder.empty() || Last == '/' || Last == '\\') ? "" : "\\";
	const std::string Path = CFolder + Separator + CFileName + ".ram";
	SaveRAM(Path.c_str());
}

void BoardEpoch::LoadState(void) {
	if (CFileName.empty()) { return; }
	const char Last = CFolder.empty() ? '\0' : CFolder.back();
	const char* Separator = (CFolder.empty() || Last == '/' || Last == '\\') ? "" : "\\";
	const std::string Path = CFolder + Separator + CFileName + ".ram";
	LoadRAM(Path.c_str());
}

char* BoardEpoch::getEDCString(){
	return fMainCPU->fEDC.getEDCString();
}

int BoardEpoch::GetAlphaSegs(char SegNum){
	int ret;
	ret = fAlphaStarburst ? fAlphaStarburst->GetAlphaSegments(SegNum) : 0;
	return ret;
}

UINT8 BoardEpoch::GetAlphaDotComma(char SegNum){
	UINT8 ret;
	ret = fAlphaStarburst ? fAlphaStarburst->GetAlphaDotComma(SegNum) : 0;
	return ret;
}
UINT8 BoardEpoch::GetAlphaDDotComma(char SegNum){
	UINT8 ret;
	ret = fAlphaDotMatrix ? fAlphaDotMatrix->GetAlphaDotComma(SegNum) : 0;
	return ret;
}
void BoardEpoch::SetOptoInvert(UINT8 ReelNum, UINT8 State){
	fReels.reels[ReelNum].inverted = State;
}
void BoardEpoch::SetOptoStart(UINT8 ReelNum, UINT8 Start){
	fReels.reels[ReelNum].startopto = Start;
}
void BoardEpoch::SetOptoEnd(UINT8 ReelNum, UINT8 End){
	fReels.reels[ReelNum].endopto = End;
}
void BoardEpoch::SetSteps(UINT8 ReelNum, UINT8 Steps){
	fReels.reels[ReelNum].steps = Steps;
}
char BoardEpoch::GetAlphaBright(){
	return fAlphaStarburst ? fAlphaStarburst->Intensity : 0;
}
UINT8 BoardEpoch::GetAlphaChar(UINT8 num){
	return fAlphaStarburst ? fAlphaStarburst->GetAlphaChar(num) : 0;
}
char BoardEpoch::GetAlphaDBright(){
	return fAlphaDotMatrix ? fAlphaDotMatrix->GetIntensity() : 0;
}
void BoardEpoch::SetStake(char StakeIn){
	if (StakeIn == 0)
	{
		Stake = 0;
	}
	else
	{
		Stake = (StakeIn - 1);
	}	
}

void BoardEpoch::SetPrize(char PrizeIn){
	Prize = PrizeIn;
}

void BoardEpoch::SetPercent(char PercentIn){
	Percent = PercentIn;
}

void BoardEpoch::TurnSwitchOn(int Num){
	if (Num == 255){
		//Test Switch
		fMainCPU->func_sw = true;
	} else {
		//IO Map
		fMainCPU->IOMAP_INPUTS[(Num / 8)] |=  (1 << (Num & 0x07));
	}
}

void BoardEpoch::TurnSwitchOff(int Num){

	if (Num == 255){
		//Test Switch
		fMainCPU->func_sw = false;
	} else {
		//IO Map
		fMainCPU->IOMAP_INPUTS[(Num / 8)] &= (255 ^ (1 << (Num & 0x07)));
	}
}

UINT8 BoardEpoch::ReadSwitch(UINT8 Num) const {
	if (Num == 255U) { return fMainCPU->func_sw ? 1U : 0U; }
	return static_cast<UINT8>((fMainCPU->IOMAP_INPUTS[Num / 8U] >> (Num & 0x07U)) & 1U);
}

UINT8 BoardEpoch::GetSegOn(unsigned short num){
	
	UINT8 ret;
	//Lamp internal on/off
	int SNum = ((num & 0xf) << 4) | ((num & 0xf0) >> 4) | (num & 0x100); 

	ret = (fMainCPU->IOMAP_LEDS[SNum] & 1);
	
	if (ret){
		//If lamp is 'on' it may be in a flash group so check if it's flashing and off.
		int lampVal = fMainCPU->IOMAP_LEDS[SNum];
		//Check for flash
		if (lampVal & 0x0e)
		{
			// Lamp is flashing
			bool flashState;
			if (lampVal & 0x80) {
				//Inverse Flash
				flashState = !fMainCPU->led_flash[(lampVal & 0x0e) >> 1];
			} else {		
				//Flash
				flashState = fMainCPU->led_flash[(lampVal & 0x0e) >> 1];
			}
			if (flashState)
			{
				//Lamp is OFF
				return 0;
			}
		}	
	}
	return ret;
}

short BoardEpoch::GetReelPos(UINT8 num){
	return fReels.reels[num].reelpos;
}

bool BoardEpoch::GetLampOn(unsigned short num) {

	bool ret;

	//Lamp internal on/off
	ret = (bool)(fMainCPU->IOMAP_LAMPS[num] & 1);

	if (ret) {
		//If lamp is 'on' it may be in a flash group so check if it's flashing and off.
		int lampVal = fMainCPU->IOMAP_LAMPS[num];
		//Check for flash
		if (lampVal & 0x0e)
		{
			// Lamp is flashing
			bool flashState;
			if (lampVal & 0x80) {
				//Inverse Flash
				flashState = !fMainCPU->lamp_flash[(lampVal & 0x0e) >> 1];
			}
			else {
				//Flash
				flashState = fMainCPU->lamp_flash[(lampVal & 0x0e) >> 1];
			}
			if (flashState)
			{
				//Lamp is OFF
				return false;
			}
		}
	}
	return ret;
}

float BoardEpoch::GetLampBright(unsigned short num){

	float ret = 0.f;		
	float lampBright = 0.f;
	int lampVal = fMainCPU->IOMAP_LAMPS[num];
	
	if (lampVal & 0x01)
	{
		// Lamp is on		
		if (lampVal & 0x40) {
			//Lamp is Dimmed
			lampBright = (float)(((fMainCPU->IOMAP_LAMP_DIM & 7) << 4) + 128);
		} else {
			//Lamp is Full Brightness
			lampBright = 255.f;
		}

		if (lampVal & 0x0e)
		{
			// Lamp is flashing
			bool flashState;

			if (lampVal & 0x80) {
				flashState = !fMainCPU->lamp_flash[(lampVal & 0x0e) >> 1];
			} else {
				flashState = fMainCPU->lamp_flash[(lampVal & 0x0e) >> 1];
			}

			if (flashState)
			{
				lampBright = 0.f;
			}
		}
	}

	return ((float)lampBright / 255.f);
}


//TODO: get the right colours here
float BoardEpoch::GetFilamentColorR(unsigned short) {	
	float ret = 1.f;
	return ret;
}

float BoardEpoch::GetFilamentColorG(unsigned short) {
	float ret = 0.7f;
	return ret;
}

float BoardEpoch::GetFilamentColorB(unsigned short) {
	float ret = 0.05f;
	return ret;
}

UINT8 BoardEpoch::GetSegBright(unsigned short num){
	UINT8 ret = 0;
	
	int SNum = ((num & 0xf) << 4) | ((num & 0xf0) >> 4) | (num & 0x100); 
	int lampVal = fMainCPU->IOMAP_LEDS[SNum];
	int lampCol = 0;
	
	if (lampVal & 0x01)
	{
		// Lamp is on		
		if (lampVal & 0x40) {
			//Lamp is Dimmed
			lampCol = (((fMainCPU->IOMAP_LED_DIM & 7) << 4) + 128);
		} else {
			//Lamp is Full Brightness
			lampCol = 255;
		}

		if (lampVal & 0x0e)
		{
			// Lamp is flashing
			bool flashState;

			if (lampVal & 0x80) {
				flashState = !fMainCPU->led_flash[(lampVal & 0x0e) >> 1];
			} else {
				flashState = fMainCPU->led_flash[(lampVal & 0x0e) >> 1];
			}

			if (flashState)
			{
				lampCol = 0;
			}
		}
	}

	return lampCol;
}

signed long  BoardEpoch::LoadSoundROM(char *name1, char *name2, char *name3, char *name4){
	unsigned long ret;
	ret = fSound.LoadSoundROM(name1, name2, name3, name4);
	return ret;
}

void BoardEpoch::SetReelExt(UINT8 Ext){
	ReelExt = Ext;
}

UINT8 BoardEpoch::CoinIn(UINT8 Num, UINT8 Coin, UINT8 CoinValue){	
	
	fCoinMech[Num].SetSelectedCoin(Coin);	
	if (fCoinMech[Num].CoinIn(Coin)){
		if (fHoppers.CoinIn(CoinValue) == 0xff){
			//if not accepted by a hopper, dump to cashbox.
			fCashBox.CoinIn(CoinValue);
		}
		return 1;//Coin Accepted
	}
	return 0;//Coin Rejected
}
void BoardEpoch::SetCommStyle(UINT8 Num, UINT8 Style){
	fCoinMech[Num].SetCommStyle(Style);
}
void BoardEpoch::SetCommInvert(UINT8 Num, UINT8 Invert){
	fCoinMech[Num].SetCommInvert(Invert);
}
void BoardEpoch::SetCycles(UINT8 Num, unsigned int Cycles){
	fCoinMech[Num].SetCycles(Cycles);
}
void BoardEpoch::SetEDCEnable(UINT8 Num, UINT8 Enable){
	fCoinMech[Num].SetEDCEnable(Enable);
}
void BoardEpoch::SetLockoutVal(UINT8 Num, UINT8 Coin, UINT8 Value){
	fCoinMech[Num].SetLockoutVal(Coin, Value);
}
void BoardEpoch::SetLockoutInvert(UINT8 Num, UINT8 Coin, UINT8 Invert){
	fCoinMech[Num].SetLockoutInvert(Coin, Invert);
}
void BoardEpoch::SetCoinValue(UINT8 Num, UINT8 CoinNum, UINT8 Value)
{
	fCoinMech[Num].SetCoinValue(CoinNum, Value);
}
void BoardEpoch::SetCoinEnable(UINT8 Num, UINT8 CoinNum, UINT8 Value)
	{
	fCoinMech[Num].SetCoinEnable(CoinNum, Value);
}
UINT8 BoardEpoch::GetLampOnOff(UINT8 Num, UINT8 LampNum)
{
	UINT8 ret;
	ret = fCoinMech[Num].GetLampOnOff(LampNum);
	return ret;
}	
void BoardEpoch::SetMeterEnable(UINT8 Num, UINT8 Value){
	fMeters.SetMeterEnable(Num, Value);
}
void BoardEpoch::SetMeterCounter(UINT8 num, unsigned int Value){
	fMeters.SetCounter(num, Value);
}
unsigned int BoardEpoch::GetMeterCounter(UINT8 num){
	return fMeters.GetCounter(num);
}
void BoardEpoch::SetDIP(UINT8 Num, UINT8 Value){
	fMainCPU->SetDIP(Num, Value);
}

UINT8 BoardEpoch::GetDIP(UINT8 Num) const {
	if (Num >= PA2_NUM_DIPS) { return 0U; }
	if (Num > 7U) { return static_cast<UINT8>((fMainCPU->IOMAP_DIPS1 >> (Num - 8U)) & 1U); }
	return static_cast<UINT8>((fMainCPU->IOMAP_DIPS2 >> Num) & 1U);
}

UINT32 BoardEpoch::FillAudioFrames(INT16* OutInterleavedStereo, UINT32 FramesRequired) {
	return fSound.FillAudioFrames(OutInterleavedStereo, FramesRequired);
}

UINT32 BoardEpoch::GetOutputSnapshot(PA2_OutputSnapshot& Out) {
	std::memset(&Out, 0, sizeof(Out));
	Out.SizeBytes = sizeof(Out);
	Out.Version = PA2_OUTPUT_SNAPSHOT_VERSION;

	Out.MatrixLampCount = PA2_MAX_MATRIX_LAMPS;
	for (UINT32 Lamp = 0; Lamp < Out.MatrixLampCount; ++Lamp) {
		PA2_LampState& State = Out.MatrixLamps[Lamp];
		State.OnOff = GetLampOn(static_cast<UINT16>(Lamp)) ? 1U : 0U;
		State.Brightness = GetLampBright(static_cast<UINT16>(Lamp));
		State.FilamentR = GetFilamentColorR(static_cast<UINT16>(Lamp));
		State.FilamentG = GetFilamentColorG(static_cast<UINT16>(Lamp));
		State.FilamentB = GetFilamentColorB(static_cast<UINT16>(Lamp));
	}

	Out.LedCount = PA2_MAX_LEDS;
	for (UINT32 Led = 0; Led < Out.LedCount; ++Led) {
		PA2_LampState& State = Out.Leds[Led];
		State.OnOff = GetSegOn(static_cast<UINT16>(Led));
		State.Brightness = static_cast<float>(GetSegBright(static_cast<UINT16>(Led))) / 255.0f;
		State.FilamentR = 1.0f;
		State.FilamentG = 0.0f;
		State.FilamentB = 0.0f;
	}

	Out.ReelCount = PA2_NUM_REELS;
	for (UINT32 Reel = 0; Reel < Out.ReelCount; ++Reel) {
		Out.Reels[Reel].Position = GetReelPos(static_cast<UINT8>(Reel));
	}

	Out.AlphaSegmentedDisplayCount = 1U;
	PA2_AlphaSegmentedState& Segmented = Out.AlphaSegmented[0];
	Segmented.SegmentCount = PA2_ALPHA_SEGMENTS_IMPACT;
	for (UINT32 Character = 0; Character < PA2_NUM_ALPHA_CHARS; ++Character) {
		Segmented.Segments[Character] = static_cast<UINT16>(GetAlphaSegs(static_cast<char>(Character)) & 0xffff);
		Segmented.DotComma[Character] = GetAlphaDotComma(static_cast<char>(Character));
	}
	Segmented.Brightness = std::clamp(static_cast<float>(static_cast<unsigned char>(GetAlphaBright())) / 31.0f, 0.0f, 1.0f);

	Out.AlphaDotDisplayCount = 1U;
	PA2_AlphaDotState& Dot = Out.AlphaDot[0];
	for (UINT32 Character = 0; Character < PA2_NUM_ALPHA_CHARS; ++Character) {
		for (UINT32 Column = 0; Column < 5U; ++Column) {
			Dot.Columns[Character][Column] = GetAlphaDots(static_cast<char>(Character), static_cast<char>(Column));
		}
		Dot.DotComma[Character] = GetAlphaDDotComma(static_cast<char>(Character));
	}
	Dot.Brightness = std::clamp((static_cast<float>(static_cast<unsigned char>(GetAlphaDBright())) + 8.0f) / 16.0f, 0.0f, 1.0f);

	Out.LedDisplayCount = PA2_NUM_LED_DISPLAYS;
	for (UINT32 Display = 0; Display < Out.LedDisplayCount; ++Display) {
		UINT32 Mask = 0U;
		UINT8 Brightness = 0U;
		for (UINT32 Segment = 0; Segment < PA2_NUM_LED_SEGMENTS; ++Segment) {
			const UINT16 Index = static_cast<UINT16>((Display * 16U) + Segment);
			Mask = (Mask << 1U) | (GetSegOn(Index) ? 1U : 0U);
			Brightness = (((Brightness) > (GetSegBright(Index))) ? (Brightness) : (GetSegBright(Index)));
		}
		Out.LedDisplays[Display].OnOff = Mask;
		Out.LedDisplays[Display].Brightness = static_cast<float>(Brightness) / 255.0f;
	}

	Out.ElectronicMechCount = 1U;
	Out.ElectronicMechs[0].CoinLamp[0] = GetLampOnOff(0U, 0U);
	Out.ElectronicMechs[0].CoinLamp[1] = GetLampOnOff(0U, 1U);

	Out.MeterCount = PA2_NUM_METERS;
	for (UINT32 Meter = 0; Meter < Out.MeterCount; ++Meter) {
		Out.Meters[Meter] = GetMeterCounter(static_cast<UINT8>(Meter));
	}

	Out.DipCount = PA2_NUM_DIPS;
	for (UINT32 Dip = 0; Dip < Out.DipCount; ++Dip) {
		Out.Dips[Dip] = GetDIP(static_cast<UINT8>(Dip));
	}

	Out.HopperCount = PA2_NUM_HOPPERS;
	for (UINT32 Hopper = 0; Hopper < Out.HopperCount; ++Hopper) {
		const UINT8 Index = static_cast<UINT8>(Hopper);
		Out.HopperLevel[Hopper] = GetHopperLevel(Index);
		Out.HopperFullLevel[Hopper] = GetHopperFullLevel(Index);
		Out.HopperLoLevel[Hopper] = GetHopperLoLevel(Index);
		Out.HopperHiLevel[Hopper] = GetHopperHiLevel(Index);
		Out.HopperCoinsIn[Hopper] = GetHopperCoinsIn(Index);
		Out.HopperCoinsOut[Hopper] = GetHopperCoinsOut(Index);
		Out.HopperCoinsRefilled[Hopper] = GetHopperCoinsRefilled(Index);
	}

	Out.StatusLED = GetStatusLED();
	return sizeof(Out);
}

void EPOCHCPU::SetDIP(UINT8 Num, UINT8 Value){
	
	if (Num > 7){
		//Clear it
		IOMAP_DIPS1 &= ((1 << (Num - 8)) ^ 0xff);
		if (Value) {
			//Set it
			IOMAP_DIPS1 |= (1 << (Num - 8));
		}
	} else {
		//Clear it
		IOMAP_DIPS2 &= ((1 << Num) ^ 0xff);
		if (Value) {
			//Set it
			IOMAP_DIPS2 |= (1 << Num);
		}
	}

}

 void BoardEpoch::SetHopperEnable(UINT8 Num, UINT8 Value){
	 fHoppers.SetHopperEnable(Num, Value);
}
 void BoardEpoch::SetHopperCoin(UINT8 Num, UINT8 Value){
	fHoppers.SetHopperCoin(Num, Value);
}
 void BoardEpoch::SetHopperCoinsIn(UINT8 Num, UINT32 Value){
	fHoppers.SetHopperCoinsIn(Num, Value);
}
 void BoardEpoch::SetHopperCoinsOut(UINT8 Num, UINT32 Value){
	fHoppers.SetHopperCoinsOut(Num, Value);
}
 void BoardEpoch::SetHopperLevel(UINT8 Num, UINT32 Value){
	fHoppers.SetHopperLevel(Num, Value);
}
 void BoardEpoch::SetHopperFullLevel(UINT8 Num, UINT32 Value){
	fHoppers.SetHopperFullLevel(Num, Value);
}
 void BoardEpoch::SetHopperLoEnable(UINT8 Num, UINT8 Value){
	fHoppers.SetHopperLoEnable(Num, Value);
}
 void BoardEpoch::SetHopperLoInvert(UINT8 Num, UINT8 Value){
	fHoppers.SetHopperLoInvert(Num, Value);
}
 void BoardEpoch::SetHopperLoSwitch(UINT8 Num, UINT8 Value){
	fHoppers.SetHopperLoSwitch(Num, Value);
}
 void BoardEpoch::SetHopperLoLevel(UINT8 Num, UINT32 Value){
	fHoppers.SetHopperLoLevel(Num, Value);
}
 void BoardEpoch::SetHopperHiEnable(UINT8 Num, UINT8 Value){
	fHoppers.SetHopperHiEnable(Num, Value);
}
 void BoardEpoch::SetHopperHiInvert(UINT8 Num, UINT8 Value){
	fHoppers.SetHopperHiInvert(Num, Value);
}
 void BoardEpoch::SetHopperHiSwitch(UINT8 Num, UINT8 Value){
	fHoppers.SetHopperHiSwitch(Num, Value);
}
 void BoardEpoch::SetHopperHiLevel(UINT8 Num, UINT32 Value){
	fHoppers.SetHopperHiLevel(Num, Value);
}
 void BoardEpoch::SetHopperOptoEnable(UINT8 Num, UINT8 Value){
	fHoppers.SetHopperOptoEnable(Num, Value);
}
 void BoardEpoch::SetHopperOptoReturn(UINT8 Num, UINT8 Value){
	fHoppers.SetHopperOptoReturn(Num, Value);
}
 void BoardEpoch::SetHopperMotorEnable(UINT8 Num, UINT8 Value){
	fHoppers.SetHopperMotorEnable(Num, Value);
}
void BoardEpoch::SetHopperLoIndicator(UINT8 Num, UINT8 Value){
	fHoppers.SetHopperLoIndicator(Num, Value);
}
void BoardEpoch::SetHopperHiIndicator(UINT8 Num, UINT8 Value){
	fHoppers.SetHopperHiIndicator(Num, Value);
}
void BoardEpoch::SetHopperCoinsRefilled(UINT8 Num, UINT32 Value){
	fHoppers.SetHopperCoinsRefilled(Num, Value);
}

 UINT8 BoardEpoch::GetHopperEnable(UINT8 Num){
	UINT8 ret = 0;
	ret = fHoppers.GetHopperEnable(Num);
	return ret;
}
 UINT8 BoardEpoch::GetHopperCoin(UINT8 Num){
	UINT8 ret = 0;
	ret = fHoppers.GetHopperCoin(Num);
	return ret;
}
 UINT32 BoardEpoch::GetHopperCoinsIn(UINT8 Num){
	UINT32 ret = 0;
	ret = fHoppers.GetHopperCoinsIn(Num);
	return ret;
}
 UINT32 BoardEpoch::GetHopperCoinsOut(UINT8 Num){
	UINT32 ret = 0;
	ret = fHoppers.GetHopperCoinsOut(Num);
	return ret;
}
 UINT32 BoardEpoch::GetHopperLevel(UINT8 Num){
	UINT32 ret = 0;
	ret = fHoppers.GetHopperLevel(Num);
	return ret;
}
 UINT32 BoardEpoch::GetHopperFullLevel(UINT8 Num){
	UINT32 ret = 0;
	ret = fHoppers.GetHopperFullLevel(Num);
	return ret;
}
 UINT8 BoardEpoch::GetHopperLoEnable(UINT8 Num){
	UINT8 ret = 0;
	ret = fHoppers.GetHopperLoEnable(Num);
	return ret;
}
 UINT8 BoardEpoch::GetHopperLoInvert(UINT8 Num){
	UINT8 ret = 0;
	ret = fHoppers.GetHopperLoInvert(Num);
	return ret;
}
 UINT8 BoardEpoch::GetHopperLoSwitch(UINT8 Num){
	UINT8 ret = 0;
	ret = fHoppers.GetHopperLoSwitch(Num);
	return ret;
}
 UINT32 BoardEpoch::GetHopperLoLevel(UINT8 Num){
	UINT32 ret = 0;
	ret = fHoppers.GetHopperLoLevel(Num);
	return ret;
}
 UINT8 BoardEpoch::GetHopperHiEnable(UINT8 Num){
	UINT8 ret = 0;
	ret = fHoppers.GetHopperHiEnable(Num);
	return ret;
}
 UINT8 BoardEpoch::GetHopperHiInvert(UINT8 Num){
	UINT8 ret = 0;
	ret = fHoppers.GetHopperHiInvert(Num);
	return ret;
}
 UINT8 BoardEpoch::GetHopperHiSwitch(UINT8 Num){
	UINT8 ret = 0;
	ret = fHoppers.GetHopperHiSwitch(Num);
	return ret;
}
 UINT32 BoardEpoch::GetHopperHiLevel(UINT8 Num){
	UINT32 ret = 0;
	ret = fHoppers.GetHopperHiLevel(Num);
	return ret;
}
 UINT8 BoardEpoch::GetHopperOptoEnable(UINT8 Num){
	UINT8 ret = 0;
	ret = fHoppers.GetHopperOptoEnable(Num);
	return ret;
}
 UINT8 BoardEpoch::GetHopperOptoReturn(UINT8 Num){
	UINT8 ret = 0;
	ret = fHoppers.GetHopperOptoReturn(Num);
	return ret;
}
 UINT8 BoardEpoch::GetHopperMotorEnable(UINT8 Num){
	UINT8 ret = 0;
	ret = fHoppers.GetHopperMotorEnable(Num);
	return ret;
}

UINT32 BoardEpoch::GetHopperCoinsRefilled(UINT8 Num){
	UINT32 ret = 0;
	ret = fHoppers.GetHopperCoinsRefilled(Num);
	return ret;
}
UINT8 BoardEpoch::GetHopperHiIndicator(UINT8 Num){
	UINT8 ret = 0;
	ret = fHoppers.GetHopperHiIndicator(Num);
	return ret;
}
UINT8 BoardEpoch::GetHopperLoIndicator(UINT8 Num){
	UINT8 ret = 0;
	ret = fHoppers.GetHopperLoIndicator(Num);
	return ret;
}