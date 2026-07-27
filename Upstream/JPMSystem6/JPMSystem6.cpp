#include "stdafx.h"
#include "JPMSystem6.h"
#include <iostream>
#include <fstream>
#include <stdio.h>

using namespace std;
FILE* IOFile;

JPMSystem6::JPMSystem6()
{
	ZeroMemory(ROM, 0x100000 * sizeof(UINT8));
	ZeroMemory(RAM, 0x4000 * sizeof(UINT8));

	LSC = new LoadSaveClass();
}

JPMSystem6::~JPMSystem6()
{
	delete LSC;
}

void JPMSystem6::SetCFolder(UINT8* Folder) {

	CFolder = Folder;

}

void JPMSystem6::SetCFileName(UINT8* FileName) {

	CFileName = FileName;

}


void JPMSystem6::LoadState(void) {
	
	UINT8* OutStr = nullptr;
	int OutLen;
	
	//Load State
	OutLen = CombineStrings(OutStr, CFolder, (UINT8*)"STATE");
	bool loadOK = LSC->LoadInit(OutStr);
	if (OutLen > 0) {
		delete[] OutStr;
	}

	if (!loadOK) {
		return;
	}

	//Retrieve from buffer.  Reject missing/old/truncated files before touching
	//machine state; otherwise a bad STATE file can zero RAM/CPU registers.
	if (!LSC->LoadVersionFromBuffer()) {
		LSC->LoadEnd();
		return;
	}

	Lamps.LoadState();	
	Mars->LoadState();	
	Alpha1.LoadState();
	PPIO.LoadState();
	LoadCPUState();
	Meters.LoadState();
	Reels.LoadState();
	Hoppers.LoadState();
	DUART.LoadState();
	DUART2.LoadState();
	Seg7.LoadState();
	Tubes.LoadState();
	Sound.LoadState();
	CashBox.LoadState();

	// JPM System 6 state
	LSC->LoadFromBuffer(StatusLED);
	LSC->LoadFromBuffer(RAMEnable);
	LSC->LoadFromBuffer(IMPACT3);
	LSC->LoadFromBuffer(SndCnt);
	LSC->LoadFromBuffer(DivCycles);

	bool stateLoadOK = LSC->IsLoadValid();

	//Memory Cleanup
	LSC->LoadEnd();

	// If a state file had the right version but was truncated/corrupt, the safe
	// option is to return to a clean reset rather than continue with partly-zeroed
	// CPU/device state.
	if (!stateLoadOK) {
		Reset();
	}

}

UINT32 JPMSystem6::CombineStrings(UINT8*& OutStr, UINT8* In1, UINT8* In2) {

	UINT32 len1, len2, outlen, loop;

	OutStr = NULL;
	if (!In1 || !In2) return 0;

	//Get input string lengths
	size_t len1Size = strlen((char*)In1);
	size_t len2Size = strlen((char*)In2);

	if (len1Size == 0 || len2Size == 0) return 0;
	if (len1Size > 0xffffffffUL || len2Size > 0xffffffffUL) return 0;
	if (len1Size > 0xffffffffUL - len2Size) return 0;

	len1 = static_cast<UINT32>(len1Size);
	len2 = static_cast<UINT32>(len2Size);

	//Set output length
	outlen = len1 + len2;
	//allocate memory
	OutStr = new UINT8[outlen + 1];

	if (OutStr) {
		//Clear Memory
		ZeroMemory(OutStr, outlen + 1);
		//Combine Strings
		for (loop = 0; loop < outlen; loop++) {

			if (loop < len1)
				OutStr[loop] = In1[loop];
			else
				OutStr[loop] = In2[loop - len1];
		}
	}

	//Return Length
	return outlen;

}

void JPMSystem6::SaveState(void) {

	UINT8* OutStr = nullptr;
	UINT32 OutLen;

	//Initialize
	LSC->SaveInit(0x100000);
	LSC->SaveVersionToBuffer();

	//Dump to buffer
	Lamps.SaveState();	
	Mars->SaveState();	
	Alpha1.SaveState();
	PPIO.SaveState();
	SaveCPUState();
	Meters.SaveState();
	Reels.SaveState();
	Hoppers.SaveState();
	DUART.SaveState();
	DUART2.SaveState();
	Seg7.SaveState();
	Tubes.SaveState();
	Sound.SaveState();
	CashBox.SaveState();

	// JPM System 6 state
	LSC->SaveToBuffer(StatusLED);
	LSC->SaveToBuffer(RAMEnable);
	LSC->SaveToBuffer(IMPACT3);
	LSC->SaveToBuffer(SndCnt);
	LSC->SaveToBuffer(DivCycles);


	//Save Layout Info
	OutLen = CombineStrings(OutStr, CFolder, (UINT8 *)"STATE");
	LSC->SaveToFile((UINT8*)OutStr);
	if (OutLen > 0) {
		delete[] OutStr;
	}	
}

void JPMSystem6::SaveCPUState(void) {

	UINT32 loop;

	//Do Switches Here
	for (loop = 0; loop < 256; loop++) {
		LSC->SaveToBuffer(Switches.ReadSwitch(loop));
	}
	//RAM
	for (loop = 0; loop < 0x4000; loop++) {
		LSC->SaveToBuffer(RAM[loop]);
	}
	//CPU
	for (loop = 0; loop < 16; loop++) {
		LSC->SaveToBuffer(m68ki_cpu.dar[loop]);
	}
	for (loop = 0; loop < 6; loop++) {
		LSC->SaveToBuffer(m68ki_cpu.sp[loop]);
	}
	LSC->SaveToBuffer(m68ki_cpu.cpu_type);
	LSC->SaveToBuffer(m68ki_cpu.ppc);
	LSC->SaveToBuffer(m68ki_cpu.pc);
	LSC->SaveToBuffer(m68ki_cpu.vbr);
	LSC->SaveToBuffer(m68ki_cpu.sfc);
	LSC->SaveToBuffer(m68ki_cpu.dfc);
	LSC->SaveToBuffer(m68ki_cpu.cacr);
	LSC->SaveToBuffer(m68ki_cpu.caar);
	LSC->SaveToBuffer(m68ki_cpu.ir);
	LSC->SaveToBuffer(m68ki_cpu.t1_flag);
	LSC->SaveToBuffer(m68ki_cpu.t0_flag);
	LSC->SaveToBuffer(m68ki_cpu.s_flag);
	LSC->SaveToBuffer(m68ki_cpu.m_flag);
	LSC->SaveToBuffer(m68ki_cpu.x_flag);
	LSC->SaveToBuffer(m68ki_cpu.n_flag);
	LSC->SaveToBuffer(m68ki_cpu.not_z_flag);
	LSC->SaveToBuffer(m68ki_cpu.v_flag);
	LSC->SaveToBuffer(m68ki_cpu.c_flag);
	LSC->SaveToBuffer(m68ki_cpu.int_mask);
	LSC->SaveToBuffer(m68ki_cpu.int_level);
	LSC->SaveToBuffer(m68ki_cpu.int_cycles);
	LSC->SaveToBuffer(m68ki_cpu.stopped);
	LSC->SaveToBuffer(m68ki_cpu.pref_addr);
	LSC->SaveToBuffer(m68ki_cpu.pref_data);
	LSC->SaveToBuffer(m68ki_cpu.address_mask);
	LSC->SaveToBuffer(m68ki_cpu.sr_mask);
	LSC->SaveToBuffer(m68ki_cpu.cyc_bcc_notake_b);
	LSC->SaveToBuffer(m68ki_cpu.cyc_bcc_notake_w);
	LSC->SaveToBuffer(m68ki_cpu.cyc_dbcc_f_noexp);
	LSC->SaveToBuffer(m68ki_cpu.cyc_dbcc_f_exp);
	LSC->SaveToBuffer(m68ki_cpu.cyc_scc_r_false);
	LSC->SaveToBuffer(m68ki_cpu.cyc_movem_w);
	LSC->SaveToBuffer(m68ki_cpu.cyc_movem_l);
	LSC->SaveToBuffer(m68ki_cpu.cyc_shift);
	LSC->SaveToBuffer(m68ki_cpu.cyc_reset);

}
void JPMSystem6::LoadCPUState(void) {

	int loop;
	UINT8 temp;

	//Do Switches Here
	for (loop = 0; loop < 256; loop++) {
		LSC->LoadFromBuffer(temp);
		if (temp) {
			TurnSwitchOn(loop);
		}
		else {
			TurnSwitchOff(loop);
		}
	}
	//RAM
	for (loop = 0; loop < 0x4000; loop++) {
		LSC->LoadFromBuffer(RAM[loop]);
	}
	//CPU
	for (loop = 0; loop < 16; loop++) {
		LSC->LoadFromBuffer(m68ki_cpu.dar[loop]);
	}
	for (loop = 0; loop < 6; loop++) {
		LSC->LoadFromBuffer(m68ki_cpu.sp[loop]);
	}
	LSC->LoadFromBuffer(m68ki_cpu.cpu_type);
	LSC->LoadFromBuffer(m68ki_cpu.ppc);
	LSC->LoadFromBuffer(m68ki_cpu.pc);
	LSC->LoadFromBuffer(m68ki_cpu.vbr);
	LSC->LoadFromBuffer(m68ki_cpu.sfc);
	LSC->LoadFromBuffer(m68ki_cpu.dfc);
	LSC->LoadFromBuffer(m68ki_cpu.cacr);
	LSC->LoadFromBuffer(m68ki_cpu.caar);
	LSC->LoadFromBuffer(m68ki_cpu.ir);
	LSC->LoadFromBuffer(m68ki_cpu.t1_flag);
	LSC->LoadFromBuffer(m68ki_cpu.t0_flag);
	LSC->LoadFromBuffer(m68ki_cpu.s_flag);
	LSC->LoadFromBuffer(m68ki_cpu.m_flag);
	LSC->LoadFromBuffer(m68ki_cpu.x_flag);
	LSC->LoadFromBuffer(m68ki_cpu.n_flag);
	LSC->LoadFromBuffer(m68ki_cpu.not_z_flag);
	LSC->LoadFromBuffer(m68ki_cpu.v_flag);
	LSC->LoadFromBuffer(m68ki_cpu.c_flag);
	LSC->LoadFromBuffer(m68ki_cpu.int_mask);
	LSC->LoadFromBuffer(m68ki_cpu.int_level);
	LSC->LoadFromBuffer(m68ki_cpu.int_cycles);
	LSC->LoadFromBuffer(m68ki_cpu.stopped);
	LSC->LoadFromBuffer(m68ki_cpu.pref_addr);
	LSC->LoadFromBuffer(m68ki_cpu.pref_data);
	LSC->LoadFromBuffer(m68ki_cpu.address_mask);
	LSC->LoadFromBuffer(m68ki_cpu.sr_mask);
	LSC->LoadFromBuffer(m68ki_cpu.cyc_bcc_notake_b);
	LSC->LoadFromBuffer(m68ki_cpu.cyc_bcc_notake_w);
	LSC->LoadFromBuffer(m68ki_cpu.cyc_dbcc_f_noexp);
	LSC->LoadFromBuffer(m68ki_cpu.cyc_dbcc_f_exp);
	LSC->LoadFromBuffer(m68ki_cpu.cyc_scc_r_false);
	LSC->LoadFromBuffer(m68ki_cpu.cyc_movem_w);
	LSC->LoadFromBuffer(m68ki_cpu.cyc_movem_l);
	LSC->LoadFromBuffer(m68ki_cpu.cyc_shift);
	LSC->LoadFromBuffer(m68ki_cpu.cyc_reset);

}

UINT8 JPMSystem6::GetStatusLED(void) {
	return StatusLED;
}

void JPMSystem6::Reset()
{
	// Full System 6 board reset.  This replaces the old Init()+CPU-only Reset()
	// sequence so both Initialise() and Reset() enter through one path.
	TotalCycles = 0;
	StatusLED = 0;
	RAMEnable = 0;
	IMPACT3 = 0;
	SndCnt = 0;
	DivCycles = 0;

	Mars->Init(LSC);
	Alpha1.Initialise(LSC);
	Reels.Initialise(LSC);
	Meters.Init(LSC);
	Hoppers.Init(LSC);
	DUART.reset(LSC);
	DUART2.reset(LSC);
	PPIO.Reset(LSC);
	Lamps.Reset(LSC);
	Seg7.Reset(LSC);
	Tubes.Init(LSC);
	Sound.NECInit(LSC);
	CashBox.Init(LSC);

	this->m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	this->m68k_pulse_reset();
}

int JPMSystem6::Run(int Cycles) {
	int ret;

	ret = m68k_execute(Cycles);

	return ret;
}
void JPMSystem6::SaveRAM(UINT8* FileString) {

	unsigned long Cnt;
	streampos size;
	UINT8* memblock;

	ofstream file((char*)FileString, ios::out | ios::binary | ios::trunc);
	if (file.is_open()) {
		size = 0x4000;
		memblock = new UINT8[0x4000];
		for (Cnt = 0; Cnt < size; Cnt++) {
			memblock[Cnt] = (RAM[Cnt] & 0xff);
		}
		file.write((char*)memblock, size);
		file.close();

		delete[] memblock;
	}

}

void JPMSystem6::LoadRAM(UINT8* FileString) {

	unsigned long Cnt;
	std::streamsize readSize;
	UINT8* memblock;

	ifstream file((char*)FileString, ios::in | ios::binary | ios::ate);
	if (file.is_open()) {
		std::streampos size = file.tellg();
		memblock = new UINT8[0x4000];
		ZeroMemory(memblock, 0x4000 * sizeof(UINT8));

		readSize = 0;
		if (size > 0) {
			readSize = static_cast<std::streamsize>(size);
			if (readSize > 0x4000) {
				readSize = 0x4000;
			}
		}

		file.seekg(0, ios::beg);
		if (readSize > 0) {
			file.read((char*)memblock, readSize);
		}
		file.close();

		for (Cnt = 0; Cnt < 0x4000; Cnt++) {
			RAM[Cnt] = (memblock[Cnt] & 0xff);
		}
		delete[] memblock;
	}

}
UINT32 JPMSystem6::FillOutputSnapshot(PA2_OutputSnapshot* Out) {
	if (!Out) {
		return 0;
	}

	Out->SizeBytes = sizeof(PA2_OutputSnapshot);
	Out->Version = PA2_OUTPUT_SNAPSHOT_VERSION;

	Out->MatrixLampCount = PA2_MAX_MATRIX_LAMPS;
	Out->DirectLampCount = 0;
	Out->FloLampCount = 0;
	Out->PrismLampCount = 0;
	Out->LedCount = PA2_MAX_LEDS;
	Out->TriacLampCount = 0;
	Out->FluorescentLampCount = 0;
	Out->DiscoLampCount = 0;
	Out->ReelCount = PA2_NUM_REELS;
	Out->AlphaSegmentedDisplayCount = PA2_NUM_ALPHA_DISPLAYS;
	Out->AlphaDotDisplayCount = 0;
	Out->LedDisplayCount = PA2_NUM_LED_DISPLAYS;
	Out->ElectronicMechCount = PA2_MAX_ELECTRONIC_MECHS;
	Out->MechanicalMechCount = 0;
	Out->CoinEntryLampCount = 0;
	Out->MeterCount = PA2_NUM_METERS;
	Out->TubeCount = PA2_NUM_TUBES;
	Out->DipCount = PA2_NUM_DIPS;
	Out->HopperCount = PA2_NUM_HOPPERS;

	UpdateLamps();
	UpdateSegs();

	for (UINT32 loop = 0; loop < PA2_MAX_MATRIX_LAMPS; loop++) {
		const UINT16 lampNum = static_cast<UINT16>(loop);
		float3 colour = GetFilamentColour(lampNum);
		Out->MatrixLamps[loop].OnOff = GetLampsOn(lampNum) ? 1 : 0;
		Out->MatrixLamps[loop].Brightness = GetLampBrightness(lampNum);
		Out->MatrixLamps[loop].FilamentR = colour.x;
		Out->MatrixLamps[loop].FilamentG = colour.y;
		Out->MatrixLamps[loop].FilamentB = colour.z;
	}

	for (UINT32 loop = 0; loop < PA2_MAX_LEDS; loop++) {
		const UINT16 ledNum = static_cast<UINT16>(loop);
		Out->Leds[loop].OnOff = GetSegOn(ledNum) ? 1 : 0;
		Out->Leds[loop].Brightness = (1.0f / 255.0f) * GetSegBright(ledNum);
		Out->Leds[loop].FilamentR = 1.0f;
		Out->Leds[loop].FilamentG = 1.0f;
		Out->Leds[loop].FilamentB = 1.0f;
	}

	for (UINT32 loop = 0; loop < PA2_NUM_REELS; loop++) {
		Out->Reels[loop].Position = GetPosOut(static_cast<UINT8>(loop));
	}

	for (UINT32 display = 0; display < PA2_NUM_ALPHA_DISPLAYS; display++) {
		Out->AlphaSegmented[display].SegmentCount = PA2_ALPHA_SEGMENTS_IMPACT;
		for (UINT32 ch = 0; ch < PA2_NUM_ALPHA_CHARS; ch++) {
			Out->AlphaSegmented[display].Segments[ch] = static_cast<UINT16>(GetAlphaSegs(static_cast<UINT8>(ch)) & 0xffff);
			Out->AlphaSegmented[display].DotComma[ch] = GetAlphaDotComma(static_cast<UINT8>(ch));
		}
		Out->AlphaSegmented[display].Brightness = (1.0f / 31.0f) * GetAlphaBright();
	}

	for (UINT32 display = 0; display < PA2_NUM_LED_DISPLAYS; display++) {
		UINT32 onOff = GetSegOn(static_cast<UINT16>(display * 16)) ? 1 : 0;
		for (UINT32 segment = 1; segment < PA2_NUM_LED_SEGMENTS; segment++) {
			onOff = (onOff << 1);
			onOff += GetSegOn(static_cast<UINT16>((display * 16) + segment)) ? 1 : 0;
		}
		Out->LedDisplays[display].OnOff = onOff;
		Out->LedDisplays[display].Brightness = (1.0f / 255.0f) * GetSegBright(static_cast<UINT16>(display & 0xff));
	}

	Out->ElectronicMechs[0].CoinLamp[0] = GetCoinLampOnOff(0) ? 1 : 0;
	Out->ElectronicMechs[0].CoinLamp[1] = GetCoinLampOnOff(1) ? 1 : 0;
	Out->ElectronicMechs[0].LockoutState = 0;

	for (UINT32 loop = 0; loop < PA2_NUM_METERS; loop++) {
		Out->Meters[loop] = GetMeterCounter(static_cast<UINT8>(loop));
	}

	for (UINT32 loop = 0; loop < PA2_NUM_TUBES; loop++) {
		const UINT8 num = static_cast<UINT8>(loop);
		Out->TubeLevel[loop] = GetLevel(num);
		Out->TubeFullLevel[loop] = GetFullLevel(num);
		Out->TubeLoLevel[loop] = GetLoLevel(num);
		Out->TubeHiLevel[loop] = GetHiLevel(num);
	}

	for (UINT32 loop = 0; loop < PA2_NUM_DIPS; loop++) {
		Out->Dips[loop] = ReadSwitch(static_cast<UINT8>(loop)) ? 1 : 0;
	}

	for (UINT32 loop = 0; loop < PA2_NUM_HOPPERS; loop++) {
		const UINT8 num = static_cast<UINT8>(loop);
		Out->HopperLevel[loop] = GetHopperLevel(num);
		Out->HopperFullLevel[loop] = GetHopperFullLevel(num);
		Out->HopperLoLevel[loop] = GetHopperLoLevel(num);
		Out->HopperHiLevel[loop] = GetHopperHiLevel(num);
		Out->HopperCoinsIn[loop] = GetHopperCoinsIn(num);
		Out->HopperCoinsOut[loop] = GetHopperCoinsOut(num);
		Out->HopperCoinsRefilled[loop] = GetHopperCoinsRefilled(num);
	}

	Out->StatusLED = GetStatusLED();

	return sizeof(PA2_OutputSnapshot);
}

UINT8 JPMSystem6::GetAlphaChar(UINT8 num) {
	return 0;
}
UINT32 JPMSystem6::GetAlphaSegs(UINT8 CharIn) {	
	UINT32 ret = Alpha1.GetAlphaSegments(CharIn);
	return ret;
}
UINT8 JPMSystem6::GetAlphaDotComma(UINT8 SegIn) {
	char ret;
	ret = Alpha1.GetAlphaDotComma(SegIn);
	return ret;
}
UINT8 JPMSystem6::GetAlphaBright() {
	char ret;
	ret = Alpha1.GetAlphaBright();
	return ret;
}
INT16 JPMSystem6::GetPosOut(UINT8 num) {
	INT16 ret = Reels.GetPosOut(num);
	return ret;
}

void JPMSystem6::UpdateLamps(void) {

	Lamps.Update();

}

float JPMSystem6::GetLampBrightness(UINT16 num) {
	return Lamps.GetLampBrightness(num);
}
bool JPMSystem6::GetLampsOn(UINT16 num) {
	return Lamps.GetLampsOn(num);
}
float3 JPMSystem6::GetFilamentColour(UINT16 num) {
	return Lamps.GetFilamentColour(num);
}
void JPMSystem6::UpdateSegs(void) {

	Seg7.Update();

}
UINT8 JPMSystem6::GetSegOn(unsigned short num) {	
	if (num > 255) return 0;
	UINT8 ret = Seg7.GetOn(num & 0xff);
	return ret;
}
UINT8 JPMSystem6::GetSegBright(unsigned short num) {	
	if (num > 255) return 0;
	UINT8 ret = Seg7.GetBrightness(num & 0xff);
	return ret;
}
unsigned int JPMSystem6::GetMeterCounter(UINT8 num) {
	unsigned int ret;
	ret = Meters.GetCounter(num);
	return ret;
}
void JPMSystem6::TurnSwitchOn(UINT8 num) {
	Switches.TurnSwitchOn(num);
}

void JPMSystem6::TurnSwitchOff(UINT8 num) {
	Switches.TurnSwitchOff(num);
}

UINT8 JPMSystem6::ReadSwitch(UINT8 num) {
	UINT8 ret;
	ret = Switches.ReadSwitch(num);
	return ret;
}
UINT8 JPMSystem6::MarsCoinIn(UINT8 Coin, UINT8 CoinValue) {	
	if (Mars->CoinIn(Coin, CoinValue)) {
		if (Tubes.CoinIn(CoinValue) == 0xff) {
			if (Hoppers.CoinIn(CoinValue) == 0xff) {
				CashBox.CoinIn(CoinValue);
			}
		}
		return 1;//Coin Accepted
	}
	return 0;//Coin Rejected
}
void JPMSystem6::SetCommStyle(UINT8 Style) {
	Mars->SetCommStyle(Style);
}
void JPMSystem6::SetCommInvert(UINT8 Invert) {
	Mars->SetCommInvert(Invert);
}
void JPMSystem6::SetCycles(UINT32 Cycles) {
	Mars->SetCycles(Cycles);
}
void JPMSystem6::SetEDCEnable(UINT8 Enable) {
	Mars->SetEDCEnable(Enable);
}
void JPMSystem6::SetLockoutVal(UINT8 Coin, UINT8 Value) {
	Mars->SetLockoutVal(Coin, Value);
}
void JPMSystem6::SetLockoutInvert(UINT8 Coin, UINT8 Invert) {
	Mars->SetLockoutInvert(Coin, Invert);
}
void JPMSystem6::SetCoinValue(UINT8 CoinNum, UINT8 Value)
{
	Mars->SetCoinValue(CoinNum, Value);
}
void JPMSystem6::SetCoinEnable(UINT8 CoinNum, UINT8 Value)
{
	Mars->SetCoinEnable(CoinNum, Value);
}
UINT8 JPMSystem6::GetCoinLampOnOff(UINT8 LampNum)
{
	UINT8 ret = Mars->GetLampOnOff(LampNum);
	return ret;
}

int __fastcall  JPMSystem6::cpu_irq_ack(int level)
{
	int res = M68K_INT_ACK_AUTOVECTOR; // Default is Auto-Vector

	switch (level)
	{
	case 5:
		m68k_set_irq(M68K_IRQ_NONE);
		// UART supplies Vector
		res = DUART.ivr;
		break;
	default:
		m68k_set_irq(M68K_IRQ_NONE);
		break;
	}

	return res;
}

void __fastcall	JPMSystem6::cpu_set_fc(int discard)
{

}

void __fastcall  JPMSystem6::cpu_inst_hook(int cycles)
{

	int TickCycles = 0;
	int loop;

	//Update Total Cycles
	TotalCycles += cycles;
	//ImpactTraceInstruction(m68ki_cpu.ppc, ROM);

	//Divide Cycles by 4, account for remainders.
	DivCycles += cycles;
	while (DivCycles >= 4) {
		DivCycles -= 4;
		TickCycles += 1;
	}

	//Tick Duarts
	DUART.tick(TickCycles);
	DUART2.tick(TickCycles);

	//Interrupts from DUARTs
	if (DUART.isr & DUART.imr)
	{
		m68k_set_irq(M68K_IRQ_5);
	}
	/*if (DUART2.isr & DUART2.imr)
	 {
	  m68k_set_irq(M68K_IRQ_5);
	 }*/


	 //Run Components
	Lamps.Run(cycles);
	Seg7.RunJPMSegs(cycles, TotalCycles);
	Meters.Run(cycles);
	Mars->Run(cycles);


	//Coin Mech
	UINT8 marsByte = Mars->GetCoinByte();
	//MarsByte = 1 based;
	UINT8 index = (marsByte - 1);	
	UINT8 switchBase = 72;		

	if (marsByte)
	{			
		TurnSwitchOn(switchBase + index);
	}
	else
	{
		for (UINT8 i = 0; i < 6; i++)
		{
			TurnSwitchOff(switchBase + i);
		}		
	}
	

	//Hoppers
	Hoppers.Update(0, cycles);
	Hoppers.Update(1, cycles);
	for (loop = 0; loop < 2; loop++) {
		if (Hoppers.GetHopperHiEnable(loop)) {
			if (Hoppers.GetHopperHiIndicator(loop)) {
				if (Hoppers.GetHopperHiInvert(loop)) {
					TurnSwitchOff(Hoppers.GetHopperHiSwitch(loop));
				}
				else {
					TurnSwitchOn(Hoppers.GetHopperHiSwitch(loop));
				}
			}
			else {
				if (Hoppers.GetHopperHiInvert(loop)) {
					TurnSwitchOn(Hoppers.GetHopperHiSwitch(loop));
				}
				else {
					TurnSwitchOff(Hoppers.GetHopperHiSwitch(loop));
				}
			}
		}

		if (Hoppers.GetHopperLoEnable(loop)) {
			if (Hoppers.GetHopperLoIndicator(loop)) {
				if (Hoppers.GetHopperLoInvert(loop)) {
					TurnSwitchOff(Hoppers.GetHopperLoSwitch(loop));
				}
				else {
					TurnSwitchOn(Hoppers.GetHopperLoSwitch(loop));
				}
			}
			else {
				if (Hoppers.GetHopperLoInvert(loop)) {
					TurnSwitchOn(Hoppers.GetHopperLoSwitch(loop));
				}
				else {
					TurnSwitchOff(Hoppers.GetHopperLoSwitch(loop));
				}
			}
		}
	}
	//Sound
	Sound.NECRun(cycles);
	SndCnt += cycles;
	bool SoundTicked = false;
	while (SndCnt >= 2000) {
		SndCnt -= 2000;// 1/4000 seconds
		// Audio sample generation is now demand-driven by the front-end
		// audio thread through FillAudioFrames().  Keep this 4 kHz
		// tick for the existing non-audio housekeeping that was tied to
		// the old sound poll cadence.
		SoundTicked = true;
	}

	if (SoundTicked) {
		//Coin Tubes
		Tubes.Update();
		for (loop = 0; loop < 4; loop++) {
			if (Tubes.GetHiEnable(loop)) {
				if (Tubes.GetHiState(loop)) {
					if (Tubes.GetHiInvert(loop)) {
						TurnSwitchOff(Tubes.GetHiSwitch(loop));
					}
					else {
						TurnSwitchOn(Tubes.GetHiSwitch(loop));
					}
				}
				else {
					if (Tubes.GetHiInvert(loop)) {
						TurnSwitchOn(Tubes.GetHiSwitch(loop));
					}
					else {
						TurnSwitchOff(Tubes.GetHiSwitch(loop));
					}
				}
			}
			else {
				TurnSwitchOff(Tubes.GetHiSwitch(loop));
			}

			if (Tubes.GetLoEnable(loop)) {
				if (Tubes.GetLoState(loop)) {
					if (Tubes.GetLoInvert(loop)) {
						TurnSwitchOff(Tubes.GetLoSwitch(loop));
					}
					else {
						TurnSwitchOn(Tubes.GetLoSwitch(loop));
					}
				}
				else {
					if (Tubes.GetLoInvert(loop)) {
						TurnSwitchOn(Tubes.GetLoSwitch(loop));
					}
					else {
						TurnSwitchOff(Tubes.GetLoSwitch(loop));
					}
				}
			}
			else {
				TurnSwitchOff(Tubes.GetLoSwitch(loop));
			}
		}
	}

}

void __fastcall		JPMSystem6::cpu_pulse_reset(void)
{

}

/*
0x400000	RAM
0x480000	DUART 1

0x480021	Switch Start
0x480035	Valid Switches
0x480041	Reel Optos
0x480060	PIA
0x480061	PIA Port A - Hopper Outputs
0x480063	PIA Port B - Hopper Inputs, SEC Data
0x480065	PIA Port C - Hopper Inputs, Alpha Display Outputs
0x480067	PIA Control
0x480081	Sample Num
0x480083	Sample Control (Sample Stop 0x1, Page 0x2 - 0x8, Volume Step 0x10, Volume Dir 0x20, Vol Control 0x40, Volume Disable 0x40 should be 0x80?)
0x480085	Sample Status (Busy 0x1, Feedback 0x10 - 0x80)

0x4800a0	0x1 Watchdog / 0x200 LED / 0x100 RAM
0x4800a2	Reels A - D
0x4800a4	Reels E - F
0x4800a6	Meters (bits 9 - 15)
0x4800a8	Lamp Data
0x4800ab	LED Data
0x4800ad	Lamp Intesnity
0x4800af	Sinks + Ctrl (Strobe Enable 0x10, Intensity Enable 0x20)
0x4800e0	SEC Address (SEC Meters or Security?)
0x4801e0	DUART 2
*/

UINT8 __fastcall 	JPMSystem6::cpu_read_byte(int address)
{
	UINT8 value = 0;
	int Val;
	if (address < 0x100000)//ROM
	{
		value = ROM[address];
	}
	else if ((address >= 0x400000) && (address < 0x404000))//RAM
	{
		if (RAMEnable) {
			Val = 1;
		}
		value = RAM[address - 0x400000];
	}
	else if ((address >= 0x480060) && (address < 0x480068))//PIA
	{
		//Port B	- Hoppers
		//0x1		- Opto Input (Both Hoppers)
		//0x2		- Low Switch Hopper 0 (Switches 64+72 Hopper 1 Low)
		//0x4		- SEC Input (SEC Meters)
		//0x8		- 
		//0x10		- 
		//0x20		- Hopper Detect
		//0x40		- Hopper Detect
		//0x80		- 
		PPIO.PortBIn = 0xff;
		if (Hoppers.Enable[0]) {
			PPIO.PortBIn &= ~0x21;
		}
		if (Hoppers.Enable[1]) {
			PPIO.PortBIn &= ~0x41;
		}
		UINT8 Opto1, Opto2;
		static UINT8 LastOpto = 0;
		Opto1 = Hoppers.ReadOpto(0);
		Opto2 = Hoppers.ReadOpto(1);
		if ((Opto1 | (Opto2 << 1)) != LastOpto) {
			UINT8 Stop = 0;
		}

		PPIO.PortBIn |= (Opto1);
		LastOpto = (Opto1 | (Opto2 << 1));
		//Port C	- Upper 4 Bits		(Lower 4 Bits are output and drive Alpha).
		//0x80		- Vogue Cab Games wont pay 20p if this is high. (Hopper 1)
		//0x40		- Vogue Cab Games wont pay �1 if this is high (Hopper 0)
		//0x20		- Unknown
		//0x10		- Unknown
		PPIO.PortCIn = 0xf0;//0x30


		value = PPIO.Read((address - 0x480060) >> 1);
	}
	else if (address >= 0x480000 && address < 0x480020)//DUART 1
	{

		//Input Port 0x10 - Meters Return
		if (Meters.Check()) {
			DUART.ip &= ~0x10;
		}
		else {
			DUART.ip |= 0x10;
		}
		//Input Port 0x20 - Test Button		
		if (Switches.ReadSwitch(255)) {
			DUART.ip &= ~0x20;
		}
		else {
			DUART.ip |= 0x20;
		}
		//DUART.ip |= 0xff;
		value = DUART.read((address - 0x480000) >> 1);
	}
	else if (address >= 0x480020 && address < 0x480034)//Switch Matrix
	{
		UINT8 index = ((address - 0x480020) >> 1);

		value = ~(Switches.ReadMatrix(index));
	}
	else if (address == 0x480041)//Reel Optos
	{
		value = Reels.GetOptos();
	}
	else if (address == 0x480085)//Sample Status
	{
		value = Sound.GetBusy();
	}
	else if (address == 0x480035)//Valid Coin Switches
	{
		value = 0xff;
	}
	else if (address >= 0x4801e0 && address < 0x480200)//DUART 2
	{
		value = DUART2.read((address - 0x4801e0) >> 1);
	}
	else if (address == 0x4801DD) //Not Sure but game locks up without it (Lamp MUX Ready or OK?)
	{
		//Bit 0 is checked specifically
		value = 1;
	}

	else
	{
		//Unknown
	}

	return value;
}

UINT16 __fastcall 	JPMSystem6::cpu_read_word(int address)
{
	UINT16 value = 0;
	if ((address >= 0) && (address <= 0x0ffffe))//ROM
	{
		value = ROM[address];
		value = ROM[address + 1] + (value << 8);
	}
	else if ((address >= 0x400000) && (address <= 0x403ffe))//RAM
	{
		address -= 0x400000;
		value = RAM[address];
		value = RAM[address + 1] + (value << 8);
	}
	else
	{
		//unknown
	}

	return value;
}

UINT32 __fastcall 	JPMSystem6::cpu_read_long(int address)
{
	UINT32 value = 0;
	if ((address >= 0) && (address <= 0x0ffffc))//ROM
	{
		value = ROM[address];
		value = ROM[address + 1] + (value << 8);
		value = ROM[address + 2] + (value << 8);
		value = ROM[address + 3] + (value << 8);
	}
	else if ((address >= 0x400000) && (address <= 0x403ffc))
	{
		address -= 0x400000;//RAM
		value = RAM[address];
		value = RAM[address + 1] + (value << 8);
		value = RAM[address + 2] + (value << 8);
		value = RAM[address + 3] + (value << 8);
	}
	else
	{
		//unknown
	}

	return value;
}

void __fastcall 	JPMSystem6::cpu_write_byte(int address, UINT8 value)
{

	int Sec;

	if ((address >= 0x400000) && (address < 0x404000))//RAM
	{
		RAM[address - 0x400000] = value;
	}
	else if ((address >= 0x480060) && (address < 0x480068))//PIA
	{
		PPIO.Write(((address - 0x480060) >> 1), value);

		if (PPIO.PortCChanged)//Alpha Displays
		{
			Alpha1.WriteAlphaBits((PPIO.PortC & 4) >> 2, (PPIO.PortC & 1), (PPIO.PortC & 2) >> 1);
		}
		if (PPIO.PortAChanged)//Hoppers
		{
			//0x80	- 
			//0x40	- 
			//0x20	- Opto Enable?			
			//0x10	- 50Hz Motor Supply Enable
			//0x2	- 
			//0x2	- 
			//0x2	- Opto Enable?
			//0x1	- Motor Select
			Hoppers.WriteOptoEnable(0, PPIO.PortA & 0x2); //�1
			Hoppers.WriteOptoEnable(1, PPIO.PortA & 0x2); //10p

			switch (PPIO.PortA & 0x33) {
			case 0x2:
			case 0x22:
				//Hoppers Off
				Hoppers.WriteMotor(0, 0);
				Hoppers.WriteMotor(1, 0);
				break;
			case 0x12:
			case 0x32:
				//Paying 20p
				Hoppers.WriteMotor(1, 1);
				//Hopper 0 Off
				Hoppers.WriteMotor(0, 0);
				break;
			case 0x13:
			case 0x33:
				//Paying �1
				Hoppers.WriteMotor(0, 1);
				//Hopper 1 Off
				Hoppers.WriteMotor(1, 0);
				break;
			case 0x20:
			case 0x0:
				//Hoppers Off
				Hoppers.WriteMotor(0, 0);
				Hoppers.WriteMotor(1, 0);
				break;
			default:
				UINT8 Stop = 1;
				break;
			}
		}
	}
	else if ((address >= 0x480000) && (address < 0x480020))//DUART
	{
		DUART.write(((address - 0x480000) >> 1), value);
		if (DUART.op_changed) {
			Mars->SetLockoutVal(SYS6_COINPORT, DUART.GetOutputPort());
		}
	}
	else if (address == 0x480081)//Sound Sample Value + Play
	{
		Sound.NECSetTune(value);
		if (Sound.GetBusy()) {//Busy is inverted
			Sound.NECPlay();
		}
	}
	else if (address == 0x480083)//Sample Control (Sample Stop 0x1, Page 0x2 - 0x8, Volume Step 0x10, Volume Dir 0x20, Vol Control 0x40, Volume Disable 0x40 should be 0x80?)
	{
		Sound.NECSetBank((value >> 1) & 7);
		Sound.NECSetVolumeControl((value >> 4) & 0xf);
		if (value & 1)
		{
			// Reset
			Sound.NECReset();
		}

	}
	else if (address == 0x4800ab)//LED Data
	{
		Seg7.WriteJPMSegs(value);
	}
	else if (address == 0x4800af)//Sinks + Ctrl (Strobe Enable 0x10, Intensity Enable 0x20)
	{

		if (value & 0x10)//Strobe Enable
		{
			Lamps.WriteStrobe((value + 1) & 0xf);

			//7 Segs
			Seg7.SetLastMuxValue(Lamps.GetStrobeVal());
			Seg7.SetMuxValue((value + 1) & 0xf);
		}
		if (value & 0x20)//Intensity Enable
		{
			//If any value written at all then this is IMPACT3?
			IMPACT3 = 1;
			Lamps.SetIntensityEnable(value & 0xf);
		}
	}
	else if (address == 0x4800ad)//Lamp Intensity
	{
		if (IMPACT3) {
			if (Lamps.GetIntensityEnable()) {
				Lamps.SetIntensity(value);
				Seg7.SetIntensity(value);
			}
		}
		else {
			Lamps.SetIntensity(value);
			Seg7.SetIntensity(value);
		}
	}
	else if (address == 0x4800e0)//Sec
	{
		Sec = value;
	}
	else if ((address >= 0x4801e0) && (address < 0x480200))//DUART 2
	{
		DUART2.write(((address - 0x4801e0) >> 1), value);
		if (DUART2.op_changed) {

		}
	}
	else
	{
		//unknown
	}
}

void __fastcall 	JPMSystem6::cpu_write_word(int address, UINT16 value)
{
	if ((address >= 0x400000) && (address <= 0x403ffe))
	{
		address -= 0x400000;
		RAM[address] = value >> 8;
		RAM[address + 1] = value & 0xff;
	}
	else if (address == 0x4800a0)//Watchdog
	{
		//0x100 = RAM, 0x01 = Watchdog, 0x200 = Status LED		
		StatusLED = (value & 0x200) >> 9;
		RAMEnable = (value & 0x100) >> 8;
	}
	else if (address == 0x4800a2)//Reels A - D
	{
		Reels.WriteJPMReel(value & 0xf, 0);
		Reels.WriteJPMReel((value & 0xf0) >> 4, 1);
		Reels.WriteJPMReel((value & 0xf00) >> 8, 2);
		Reels.WriteJPMReel((value & 0xf000) >> 12, 3);
	}
	else if (address == 0x4800a4)//Reels D - E
	{
		Reels.WriteJPMReel(value & 0xf, 4);
		Reels.WriteJPMReel((value & 0xf0) >> 4, 5);
	}
	else if (address == 0x4800a6)//Meters + Payouts	
	{
		//Meter Writes
		Meters.Write(0, ((value >> 10) & 1));
		Meters.Write(1, ((value >> 11) & 1));
		Meters.Write(2, ((value >> 12) & 1));
		Meters.Write(3, ((value >> 13) & 1));
		Meters.Write(4, ((value >> 14) & 1));
		Meters.Write(5, ((value >> 15) & 1));

		//Triac Writes
		if (value & 0x10) {//50v AC Circuit Enable
			
			Lamps.VoltageDrop(Tubes.Write(value & 0x0f));
			
		}

	}
	else if (address == 0x4800a8)//Lamp Data
	{
		Lamps.WriteData(value);
	}
	else
	{
		//unknown
	}
}

void __fastcall 	JPMSystem6::cpu_write_long(int address, UINT32 value)
{
	if ((address >= 0x400000) && (address <= 0x403ffc)) //RAM
	{
		address -= 0x400000;
		RAM[address] = value >> 24;
		RAM[address + 1] = (value >> 16) & 0xff;
		RAM[address + 2] = (value >> 8) & 0xff;
		RAM[address + 3] = value & 0xff;
	}
	else
	{
		//unknown
	}
}

void JPMSystem6::SetEnable(UINT8 Num, UINT8 Enabl) {
	Tubes.SetEnable(Num, Enabl);
}
void JPMSystem6::SetCounterIn(UINT8 Num, UINT32 Count) {
	Tubes.SetCounterIn(Num, Count);
}
void JPMSystem6::SetCounterOut(UINT8 Num, UINT32 Count) {
	Tubes.SetCounterOut(Num, Count);
}
void JPMSystem6::SetPortIndex(UINT8 Num, UINT8 Index) {
	Tubes.SetPortIndex(Num, Index);
}
void JPMSystem6::SetCoin(UINT8 Num, UINT8 CoinIn) {
	Tubes.SetCoin(Num, CoinIn);
}
void JPMSystem6::SetLevel(UINT8 Num, UINT8 LevelIn) {
	Tubes.SetLevel(Num, LevelIn);
}
void JPMSystem6::SetFullLevel(UINT8 Num, UINT8 LevelIn) {
	Tubes.SetFullLevel(Num, LevelIn);
}
void JPMSystem6::SetLoEnable(UINT8 Num, UINT8 Enabl) {
	Tubes.SetLoEnable(Num, Enabl);
}
void JPMSystem6::SetLoInvert(UINT8 Num, UINT8 Invert) {
	Tubes.SetLoInvert(Num, Invert);
}
void JPMSystem6::SetLoSwitch(UINT8 Num, UINT8 Switch) {
	Tubes.SetLoSwitch(Num, Switch);
}
void JPMSystem6::SetLoLevel(UINT8 Num, UINT32 LevelIn) {
	Tubes.SetLoLevel(Num, LevelIn);
}
void JPMSystem6::SetHiEnable(UINT8 Num, UINT8 Enabl) {
	Tubes.SetHiEnable(Num, Enabl);
}
void JPMSystem6::SetHiInvert(UINT8 Num, UINT8 Invert) {
	Tubes.SetHiInvert(Num, Invert);
}
void JPMSystem6::SetHiSwitch(UINT8 Num, UINT8 Switch) {
	Tubes.SetHiSwitch(Num, Switch);
}
void JPMSystem6::SetHiLevel(UINT8 Num, UINT32 LevelIn) {
	Tubes.SetHiLevel(Num, LevelIn);
}

UINT8 JPMSystem6::GetEnable(UINT8 Num) {
	UINT8 ret = Tubes.GetEnable(Num);
	return ret;
}
UINT32 JPMSystem6::GetCounterIn(UINT8 Num) {	
	UINT32 ret = Tubes.GetCounterIn(Num);
	return ret;
}
UINT32 JPMSystem6::GetCounterOut(UINT8 Num) {
	UINT32 ret = Tubes.GetCounterOut(Num);
	return ret;
}
UINT8 JPMSystem6::GetPortIndex(UINT8 Num) {
	UINT8 ret = Tubes.GetPortIndex(Num);
	return ret;
}
UINT8 JPMSystem6::GetCoin(UINT8 Num) {
	UINT8 ret = Tubes.GetCoin(Num);
	return ret;
}
UINT32 JPMSystem6::GetLevel(UINT8 Num) {
	UINT32 ret = Tubes.GetLevel(Num);
	return ret;
}
UINT32 JPMSystem6::GetFullLevel(UINT8 Num) {	
	UINT32 ret = Tubes.GetFullLevel(Num);
	return ret;
}
UINT8 JPMSystem6::GetLoEnable(UINT8 Num) {
	UINT8 ret = Tubes.GetLoEnable(Num);
	return ret;
}
UINT8 JPMSystem6::GetLoInvert(UINT8 Num) {
	UINT8 ret = Tubes.GetLoInvert(Num);
	return ret;
}
UINT8 JPMSystem6::GetLoSwitch(UINT8 Num) {
	UINT8 ret = Tubes.GetLoSwitch(Num);
	return ret;
}
UINT32 JPMSystem6::GetLoLevel(UINT8 Num) {
	UINT32 ret = Tubes.GetLoLevel(Num);
	return ret;
}
UINT8 JPMSystem6::GetHiEnable(UINT8 Num) {
	UINT8 ret = Tubes.GetHiEnable(Num);
	return ret;
}
UINT8 JPMSystem6::GetHiInvert(UINT8 Num) {
	UINT8 ret = Tubes.GetHiInvert(Num);
	return ret;
}
UINT8 JPMSystem6::GetHiSwitch(UINT8 Num) {
	UINT8 ret = Tubes.GetHiSwitch(Num);
	return ret;
}
UINT32 JPMSystem6::GetHiLevel(UINT8 Num) {
	UINT32 ret = Tubes.GetHiLevel(Num);
	return ret;
}
void JPMSystem6::SetOptoInvert(UINT8 ReelNum, UINT8 State) {
	Reels.SetOptoInvert(ReelNum, State);
}
void JPMSystem6::SetOptoStart(UINT8 ReelNum, UINT8 Start) {
	Reels.SetOptoStart(ReelNum, Start);
}
void JPMSystem6::SetOptoEnd(UINT8 ReelNum, UINT8 End) {
	Reels.SetOptoEnd(ReelNum, End);
}
void JPMSystem6::SetSteps(UINT8 ReelNum, UINT8 Steps) {
	Reels.SetSteps(ReelNum, Steps);
}


void JPMSystem6::SetDIP(UINT8 Num, UINT8 Value) {

	if (Value) {
		Switches.TurnSwitchOn(Num);
	}
	else {
		Switches.TurnSwitchOff(Num);
	}
}

void JPMSystem6::SetHopperEnable(UINT8 Num, UINT8 Value) {
	Hoppers.SetHopperEnable(Num, Value);
}
void JPMSystem6::SetHopperCoin(UINT8 Num, UINT8 Value) {
	Hoppers.SetHopperCoin(Num, Value);
}
void JPMSystem6::SetHopperCoinsIn(UINT8 Num, UINT32 Value) {
	Hoppers.SetHopperCoinsIn(Num, Value);
}
void JPMSystem6::SetHopperCoinsOut(UINT8 Num, UINT32 Value) {
	Hoppers.SetHopperCoinsOut(Num, Value);
}
void JPMSystem6::SetHopperLevel(UINT8 Num, UINT32 Value) {
	Hoppers.SetHopperLevel(Num, Value);
}
void JPMSystem6::SetHopperFullLevel(UINT8 Num, UINT32 Value) {
	Hoppers.SetHopperFullLevel(Num, Value);
}
void JPMSystem6::SetHopperLoEnable(UINT8 Num, UINT8 Value) {
	Hoppers.SetHopperLoEnable(Num, Value);
}
void JPMSystem6::SetHopperLoInvert(UINT8 Num, UINT8 Value) {
	Hoppers.SetHopperLoInvert(Num, Value);
}
void JPMSystem6::SetHopperLoSwitch(UINT8 Num, UINT8 Value) {
	Hoppers.SetHopperLoSwitch(Num, Value);
}
void JPMSystem6::SetHopperLoLevel(UINT8 Num, UINT32 Value) {
	Hoppers.SetHopperLoLevel(Num, Value);
}
void JPMSystem6::SetHopperHiEnable(UINT8 Num, UINT8 Value) {
	Hoppers.SetHopperHiEnable(Num, Value);
}
void JPMSystem6::SetHopperHiInvert(UINT8 Num, UINT8 Value) {
	Hoppers.SetHopperHiInvert(Num, Value);
}
void JPMSystem6::SetHopperHiSwitch(UINT8 Num, UINT8 Value) {
	Hoppers.SetHopperHiSwitch(Num, Value);
}
void JPMSystem6::SetHopperHiLevel(UINT8 Num, UINT32 Value) {
	Hoppers.SetHopperHiLevel(Num, Value);
}
void JPMSystem6::SetHopperOptoEnable(UINT8 Num, UINT8 Value) {
	Hoppers.SetHopperOptoEnable(Num, Value);
}
void JPMSystem6::SetHopperOptoReturn(UINT8 Num, UINT8 Value) {
	Hoppers.SetHopperOptoReturn(Num, Value);
}
void JPMSystem6::SetHopperMotorEnable(UINT8 Num, UINT8 Value) {
	Hoppers.SetHopperMotorEnable(Num, Value);
}
void JPMSystem6::SetHopperLoIndicator(UINT8 Num, UINT8 Value) {
	Hoppers.SetHopperLoIndicator(Num, Value);
}
void JPMSystem6::SetHopperHiIndicator(UINT8 Num, UINT8 Value) {
	Hoppers.SetHopperHiIndicator(Num, Value);
}
void JPMSystem6::SetHopperCoinsRefilled(UINT8 Num, UINT32 Value) {
	Hoppers.SetHopperCoinsRefilled(Num, Value);
}

UINT8 JPMSystem6::GetHopperEnable(UINT8 Num) {
	UINT8 ret = 0;
	ret = Hoppers.GetHopperEnable(Num);
	return ret;
}
UINT8 JPMSystem6::GetHopperCoin(UINT8 Num) {
	UINT8 ret = 0;
	ret = Hoppers.GetHopperCoin(Num);
	return ret;
}
UINT32 JPMSystem6::GetHopperCoinsIn(UINT8 Num) {
	UINT32 ret = 0;
	ret = Hoppers.GetHopperCoinsIn(Num);
	return ret;
}
UINT32 JPMSystem6::GetHopperCoinsOut(UINT8 Num) {
	UINT32 ret = 0;
	ret = Hoppers.GetHopperCoinsOut(Num);
	return ret;
}
UINT32 JPMSystem6::GetHopperLevel(UINT8 Num) {
	UINT32 ret = 0;
	ret = Hoppers.GetHopperLevel(Num);
	return ret;
}
UINT32 JPMSystem6::GetHopperFullLevel(UINT8 Num) {
	UINT32 ret = 0;
	ret = Hoppers.GetHopperFullLevel(Num);
	return ret;
}
UINT8 JPMSystem6::GetHopperLoEnable(UINT8 Num) {
	UINT8 ret = 0;
	ret = Hoppers.GetHopperLoEnable(Num);
	return ret;
}
UINT8 JPMSystem6::GetHopperLoInvert(UINT8 Num) {
	UINT8 ret = 0;
	ret = Hoppers.GetHopperLoInvert(Num);
	return ret;
}
UINT8 JPMSystem6::GetHopperLoSwitch(UINT8 Num) {
	UINT8 ret = 0;
	ret = Hoppers.GetHopperLoSwitch(Num);
	return ret;
}
UINT32 JPMSystem6::GetHopperLoLevel(UINT8 Num) {
	UINT32 ret = 0;
	ret = Hoppers.GetHopperLoLevel(Num);
	return ret;
}
UINT8 JPMSystem6::GetHopperHiEnable(UINT8 Num) {
	UINT8 ret = 0;
	ret = Hoppers.GetHopperHiEnable(Num);
	return ret;
}
UINT8 JPMSystem6::GetHopperHiInvert(UINT8 Num) {
	UINT8 ret = 0;
	ret = Hoppers.GetHopperHiInvert(Num);
	return ret;
}
UINT8 JPMSystem6::GetHopperHiSwitch(UINT8 Num) {
	UINT8 ret = 0;
	ret = Hoppers.GetHopperHiSwitch(Num);
	return ret;
}
UINT32 JPMSystem6::GetHopperHiLevel(UINT8 Num) {
	UINT32 ret = 0;
	ret = Hoppers.GetHopperHiLevel(Num);
	return ret;
}
UINT8 JPMSystem6::GetHopperOptoEnable(UINT8 Num) {
	UINT8 ret = 0;
	ret = Hoppers.GetHopperOptoEnable(Num);
	return ret;
}
UINT8 JPMSystem6::GetHopperOptoReturn(UINT8 Num) {
	UINT8 ret = 0;
	ret = Hoppers.GetHopperOptoReturn(Num);
	return ret;
}
UINT8 JPMSystem6::GetHopperMotorEnable(UINT8 Num) {
	UINT8 ret = 0;
	ret = Hoppers.GetHopperMotorEnable(Num);
	return ret;
}

UINT32 JPMSystem6::GetHopperCoinsRefilled(UINT8 Num) {
	UINT32 ret = 0;
	ret = Hoppers.GetHopperCoinsRefilled(Num);
	return ret;
}
UINT8 JPMSystem6::GetHopperHiIndicator(UINT8 Num) {
	UINT8 ret = 0;
	ret = Hoppers.GetHopperHiIndicator(Num);
	return ret;
}
UINT8 JPMSystem6::GetHopperLoIndicator(UINT8 Num) {
	UINT8 ret = 0;
	ret = Hoppers.GetHopperLoIndicator(Num);
	return ret;
}

void JPMSystem6::SetStake(UINT8 StakeIn) {

	UINT8 loop;

	for (loop = 0; loop < 4; loop++) {
		if (StakeIn & (1 << loop)) {
			TurnSwitchOn(20 + loop);
		}
		else {
			TurnSwitchOff(20 + loop);
		}
	}
}

void JPMSystem6::SetPrize(UINT8 PrizeIn) {

	UINT8 loop;

	for (loop = 0; loop < 4; loop++) {
		if (PrizeIn & (1 << loop)) {
			TurnSwitchOn(16 + loop);
		}
		else {
			TurnSwitchOff(16 + loop);
		}
	}
}

void JPMSystem6::SetPercent(UINT8 PercentIn) {

	UINT8 loop;

	for (loop = 0; loop < 4; loop++) {
		if (PercentIn & (1 << loop)) {
			TurnSwitchOn(8 + loop);
		}
		else {
			TurnSwitchOff(8 + loop);
		}
	}
}

UINT8* JPMSystem6::GetEDCString(void) {

	return DUART.GetEDCString();

}
