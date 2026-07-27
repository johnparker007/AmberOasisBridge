// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "Interface.h"
#include "JPMSystem6.h"
#include <new>

using namespace std;

namespace
{
	static void ReleaseBuffer(UINT8*& buffer)
	{
		delete[] buffer;
		buffer = NULL;
	}

	static bool GetFileLength(FILE* file, UINT32& fileLen)
	{
		fileLen = 0;
		if (!file) return false;
		if (fseek(file, 0L, SEEK_END) != 0) return false;
		long pos = ftell(file);
		if (pos < 0) return false;
		if (fseek(file, 0L, SEEK_SET) != 0) return false;
		fileLen = static_cast<UINT32>(pos);
		return true;
	}

	static bool LoadFileToBuffer(UINT8* name, UINT8*& buffer, UINT32& fileLen)
	{
		buffer = NULL;
		fileLen = 0;
		if (!name) return false;

		FILE* file = NULL;
		fopen_s(&file, (char*)name, "rb");
		if (!file) return false;

		if (!GetFileLength(file, fileLen)) {
			fclose(file);
			return false;
		}

		buffer = new (std::nothrow) UINT8[fileLen ? fileLen : 1];
		if (!buffer) {
			fclose(file);
			return false;
		}

		if (fileLen > 0 && fread(buffer, 1, fileLen, file) != fileLen) {
			fclose(file);
			ReleaseBuffer(buffer);
			fileLen = 0;
			return false;
		}

		fclose(file);
		return true;
	}
}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
// Single board instance owned by this DLL.
static JPMSystem6* sys6_board = nullptr;
static constexpr float DLL_VERSION = 1.0000f;

Interface_API float GetDLLVersion(void)
{
	return DLL_VERSION;
}
Interface_API void LoadState(void){
	sys6_board->LoadState();
}
Interface_API void SaveState(void){
	sys6_board->SaveState();
}
Interface_API void SetCFolder(UINT8 * Folder){
	sys6_board->SetCFolder(Folder);
}
Interface_API void SetCFileName(UINT8 * FileName){
	sys6_board->SetCFileName(FileName);
}

Interface_API UINT8 Shutdown(void)
{
	UINT8 ret = 0;

	if (sys6_board)
	{
		delete sys6_board;
		sys6_board = NULL;
		ret = 1;
	}
	
	return ret;
}

// Controls
 Interface_API UINT8 Initialise(void)
{
	UINT8 ret = 0;

	if (!sys6_board)
	{		
		sys6_board = new (std::nothrow) JPMSystem6();
		
	}
	
	if (sys6_board){
		sys6_board->Reset();
		ret = 1;
	}

	return ret;
}
 Interface_API UINT32 LoadROM(UINT8 *name1, UINT8*name2, UINT8*name3, UINT8*name4){
	
	UINT8 *buffer1 = NULL;
	UINT8 *buffer2 = NULL;
	UINT32 fileLen1 = 0;
	UINT32 fileLen2 = 0;
	UINT32 cnt = 0;

	if (!sys6_board) return 0;
	if (!name1 || !name2) return 0;

	if (!LoadFileToBuffer(name1, buffer1, fileLen1)){
		return 0;
	}

	if (!LoadFileToBuffer(name2, buffer2, fileLen2)){
		ReleaseBuffer(buffer1);
		return 0;
	}

	if (fileLen1 > 0x80000 || fileLen2 > 0x80000){
		ReleaseBuffer(buffer1);
		ReleaseBuffer(buffer2);
		return 0;
	}

	UINT32 TotalSize = fileLen1 + fileLen2;
	if (TotalSize < fileLen1 || TotalSize > 0x100000){
		ReleaseBuffer(buffer1);
		ReleaseBuffer(buffer2);
		return 0;
	}

	//Clear ROM Space
	ZeroMemory(sys6_board->ROM, 0x100000);

	//ROM1	
	for (cnt = 0; cnt < fileLen1; cnt++) {
		sys6_board->ROM[cnt * 2] = (buffer1[cnt] & 255);
	}
	//ROM2
	for (cnt = 0; cnt < fileLen2; cnt++) {
		sys6_board->ROM[cnt * 2 + 1] = (buffer2[cnt] & 255);
	}

	ReleaseBuffer(buffer1);
	ReleaseBuffer(buffer2);

	return TotalSize;
}

 Interface_API void Reset(void)
{
	if (sys6_board){
		sys6_board->Reset();
	}
}

 Interface_API INT32 Run(UINT32 Cycles)
 {
	INT32 ret = sys6_board->Run(Cycles);
	return ret;
 } 

Interface_API UINT32 GetOutputSnapshotSize(void)
{
	return sizeof(PA2_OutputSnapshot);
}

Interface_API UINT32 GetOutputSnapshot(void* OutBuffer, UINT32 OutBufferSize)
{
	if (!sys6_board || !OutBuffer)
	{
		return 0;
	}

	if (OutBufferSize < sizeof(PA2_OutputSnapshot))
	{
		return 0;
	}

	PA2_OutputSnapshot* Out = reinterpret_cast<PA2_OutputSnapshot*>(OutBuffer);
	ZeroMemory(Out, sizeof(PA2_OutputSnapshot));
	Out->SizeBytes = sizeof(PA2_OutputSnapshot);
	Out->Version = PA2_OUTPUT_SNAPSHOT_VERSION;

	sys6_board->FillOutputSnapshot(Out);
	return sizeof(PA2_OutputSnapshot);
}

Interface_API UINT8 GetAlphaSegmentCount(void)
{
	return PA2_ALPHA_SEGMENTS_IMPACT;
}

Interface_API void SetOptoInvert(UINT8 ReelNum, UINT8 State){
	sys6_board->SetOptoInvert(ReelNum, State);
}
Interface_API void SetOptoStart(UINT8 ReelNum, UINT8 Start){
	sys6_board->SetOptoStart(ReelNum, Start);
}
Interface_API void SetOptoEnd(UINT8 ReelNum, UINT8 End){
	sys6_board->SetOptoEnd(ReelNum, End);
}
Interface_API void SetSteps(UINT8 ReelNum, UINT8 Steps){
	sys6_board->SetSteps(ReelNum, Steps);
}
Interface_API void TurnSwitchOn(UINT8 num){
	sys6_board->TurnSwitchOn(num);
}
Interface_API void TurnSwitchOff(UINT8 num){
	sys6_board->TurnSwitchOff(num);
}
Interface_API UINT8 ReadSwitch(UINT8 num){
	UINT8 ret = sys6_board->ReadSwitch(num);
	return ret;
}

static void SetImpactSwitchState(UINT8 Switch, UINT8 State)
{
	if (!sys6_board) { return; }
	if (State) { sys6_board->TurnSwitchOn(Switch); }
	else { sys6_board->TurnSwitchOff(Switch); }
}

Interface_API void SetTestSwitchState(UINT8 State) { SetImpactSwitchState(255, State); }
Interface_API void SetServiceDoorState(UINT8 State) { SetImpactSwitchState(32, State); }
Interface_API void SetMainDoorState(UINT8 State) { SetImpactSwitchState(61, State); }
Interface_API void SetRefillKeyState(UINT8 State) { SetImpactSwitchState(59, State); }
Interface_API UINT8 CoinIn(UINT8 Coin, UINT8 CoinValue){
	UINT8 ret = sys6_board->MarsCoinIn(Coin, CoinValue);
	return ret;
}
Interface_API void SetCommStyle(UINT8 Style){
	sys6_board->SetCommStyle(Style);
}
Interface_API void SetCommInvert(UINT8 Invert){
	sys6_board->SetCommInvert(Invert);
}
Interface_API void SetCycles(UINT32 Cycles){
	sys6_board->SetCycles(Cycles);
}
Interface_API void SetEDCEnable(UINT8 Enable){
	sys6_board->SetEDCEnable(Enable);
}
Interface_API void SetLockoutVal(UINT8 Coin, UINT8 Value){
	sys6_board->SetLockoutVal(Coin, Value);
}
Interface_API void SetLockoutInvert(UINT8 Coin, UINT8 Invert){
	sys6_board->SetLockoutInvert(Coin, Invert);
}
Interface_API UINT8 MarsCoinIn(UINT8 Coin, UINT8 CoinValue)
{
	return sys6_board->MarsCoinIn(Coin, CoinValue);
}
Interface_API void SetCoinValue(UINT8 CoinNum, UINT8 Value)
{
	sys6_board->SetCoinValue(CoinNum, Value);
}
Interface_API void SetCoinEnable(UINT8 CoinNum, UINT8 Value)
{
	sys6_board->SetCoinEnable(CoinNum, Value);
}
Interface_API UINT32 LoadSoundROM(UINT8*name1, UINT8*name2, UINT8*name3, UINT8*name4){

	UINT8 *buffer1 = NULL;
	UINT8 *buffer2 = NULL;
	UINT8 *buffer3 = NULL;
	UINT8 *buffer4 = NULL;

	UINT32 fileLen1 = 0;
	UINT32 fileLen2 = 0;
	UINT32 fileLen3 = 0;
	UINT32 fileLen4 = 0;
	UINT32 cnt = 0;
	UINT32 TotalSize = 0;
	UINT32 nextPos = 0;

	if (!sys6_board) return 0;

	if (name1 && !LoadFileToBuffer(name1, buffer1, fileLen1)) goto fail;
	if (name2 && !LoadFileToBuffer(name2, buffer2, fileLen2)) goto fail;
	if (name3 && !LoadFileToBuffer(name3, buffer3, fileLen3)) goto fail;
	if (name4 && !LoadFileToBuffer(name4, buffer4, fileLen4)) goto fail;

	if (fileLen1 > 0xffffffffUL - fileLen2) goto fail;
	TotalSize = fileLen1 + fileLen2;
	if (TotalSize > 0xffffffffUL - fileLen3) goto fail;
	TotalSize += fileLen3;
	if (TotalSize > 0xffffffffUL - fileLen4) goto fail;
	TotalSize += fileLen4;

	if (TotalSize == 0 || TotalSize > SOUNDMEMORYSIZE) goto fail;

	//Clear ROM Space
	sys6_board->Sound.ClearMemory();

	// Preserve the existing load order: ROM1 occupies the highest block, then
	// ROM2 below it, then ROM3, then ROM4 lowest.
	nextPos = TotalSize;

	if (buffer1) {
		nextPos -= fileLen1;
		sys6_board->Sound.CopyROM(nextPos, buffer1, fileLen1);
	}
	if (buffer2) {
		nextPos -= fileLen2;
		sys6_board->Sound.CopyROM(nextPos, buffer2, fileLen2);
	}
	if (buffer3) {
		nextPos -= fileLen3;
		sys6_board->Sound.CopyROM(nextPos, buffer3, fileLen3);
	}
	if (buffer4) {
		nextPos -= fileLen4;
		sys6_board->Sound.CopyROM(nextPos, buffer4, fileLen4);
	}

	ReleaseBuffer(buffer1);
	ReleaseBuffer(buffer2);
	ReleaseBuffer(buffer3);
	ReleaseBuffer(buffer4);

	sys6_board->Sound.SetROMSize(TotalSize);
	sys6_board->Sound.ExtractROM();

	return TotalSize;

fail:
	ReleaseBuffer(buffer1);
	ReleaseBuffer(buffer2);
	ReleaseBuffer(buffer3);
	ReleaseBuffer(buffer4);
	return 0;
}


Interface_API UINT32 GetAudioFormat(PA2_AudioFormat* Out, UINT32 OutSizeBytes)
{
	if (!Out || OutSizeBytes < sizeof(PA2_AudioFormat)) {
		return 0;
	}

	ZeroMemory(Out, sizeof(PA2_AudioFormat));
	Out->SizeBytes = sizeof(PA2_AudioFormat);
	Out->Version = PA2_AUDIO_FORMAT_VERSION;
	Out->SampleRate = PA2_AUDIO_OUTPUT_SAMPLE_RATE;
	Out->Channels = PA2_AUDIO_OUTPUT_CHANNELS;
	Out->BitsPerSample = 16;
	Out->Format = PA2_AUDIO_FORMAT_PCM_S16;
	return sizeof(PA2_AudioFormat);
}

Interface_API UINT32 FillAudioFrames(INT16* OutInterleavedStereo, UINT32 FramesRequired)
{
	if (!sys6_board || !OutInterleavedStereo || FramesRequired == 0) {
		return 0;
	}

	return sys6_board->Sound.FillAudioFrames(OutInterleavedStereo, FramesRequired);
}

Interface_API void SetEnable(UINT8 Num, UINT8 Enabl){
	sys6_board->SetEnable(Num, Enabl);
}
Interface_API void SetCounterIn(UINT8 Num, UINT32 Count){
	sys6_board->SetCounterIn(Num, Count);
}
Interface_API void SetCounterOut(UINT8 Num, UINT32 Count){
	sys6_board->SetCounterOut(Num, Count);
}
Interface_API void SetPortIndex(UINT8 Num, UINT8 Index){
	sys6_board->SetPortIndex(Num, Index);
}
Interface_API void SetCoin(UINT8 Num, UINT8 CoinIn){
	sys6_board->SetCoin(Num, CoinIn);
}
Interface_API void SetLevel(UINT8 Num, UINT8 LevelIn){
	sys6_board->SetLevel(Num, LevelIn);
}
Interface_API void SetFullLevel(UINT8 Num, UINT8 LevelIn){
	sys6_board->SetFullLevel(Num, LevelIn);
}
Interface_API void SetLoEnable(UINT8 Num, UINT8 Enabl){
	sys6_board->SetLoEnable(Num, Enabl);
}
Interface_API void SetLoInvert(UINT8 Num, UINT8 Invert){
	sys6_board->SetLoInvert(Num, Invert);
}
Interface_API void SetLoSwitch(UINT8 Num, UINT8 Switch){
	sys6_board->SetLoSwitch(Num, Switch);
}
Interface_API void SetLoLevel(UINT8 Num, UINT32 LevelIn){
	sys6_board->SetLoLevel(Num, LevelIn);
}
Interface_API void SetHiEnable(UINT8 Num, UINT8 Enabl){
	sys6_board->SetHiEnable(Num, Enabl);
}
Interface_API void SetHiInvert(UINT8 Num, UINT8 Invert){
	sys6_board->SetHiInvert(Num, Invert);
}
Interface_API void SetHiSwitch(UINT8 Num, UINT8 Switch){
	sys6_board->SetHiSwitch(Num, Switch);
}
Interface_API void SetHiLevel(UINT8 Num, UINT32 LevelIn){
	sys6_board->SetHiLevel(Num, LevelIn);
}

Interface_API UINT8 GetEnable(UINT8 Num){
	UINT8 ret = sys6_board->GetEnable(Num);
	return ret;
}
Interface_API UINT32 GetCounterIn(UINT8 Num){
	UINT32 ret = sys6_board->GetCounterIn(Num);
	return ret;
}
Interface_API UINT32 GetCounterOut(UINT8 Num){
	UINT32 ret = sys6_board->GetCounterOut(Num);
	return ret;
}
Interface_API UINT8 GetPortIndex(UINT8 Num){
	UINT8 ret = sys6_board->GetPortIndex(Num);
	return ret;
}
Interface_API UINT8 GetCoin(UINT8 Num){
	UINT8 ret = sys6_board->GetCoin(Num);
	return ret;
}
Interface_API UINT32 GetLevel(UINT8 Num){	
	UINT32 ret = sys6_board->GetLevel(Num);
	return ret;
}
Interface_API UINT32 GetFullLevel(UINT8 Num){
	UINT32 ret = sys6_board->GetFullLevel(Num);
	return ret;
}
Interface_API UINT8 GetLoEnable(UINT8 Num){
	UINT8 ret = sys6_board->GetLoEnable(Num);
	return ret;
}
Interface_API UINT8 GetLoInvert(UINT8 Num){
	UINT8 ret = sys6_board->GetLoInvert(Num);
	return ret;
}
Interface_API UINT8 GetLoSwitch(UINT8 Num){
	UINT8 ret = sys6_board->GetLoSwitch(Num);
	return ret;
}
Interface_API UINT32 GetLoLevel(UINT8 Num){	
	UINT32 ret = sys6_board->GetLoLevel(Num);
	return ret;
}
Interface_API UINT8 GetHiEnable(UINT8 Num){
	UINT8 ret = sys6_board->GetHiEnable(Num);
	return ret;
}
Interface_API UINT8 GetHiInvert(UINT8 Num){
	UINT8 ret = sys6_board->GetHiInvert(Num);
	return ret;
}
Interface_API UINT8 GetHiSwitch(UINT8 Num){
	UINT8 ret = sys6_board->GetHiSwitch(Num);
	return ret;
}
Interface_API UINT32 GetHiLevel(UINT8 Num){	
	UINT32 ret = sys6_board->GetHiLevel(Num);
	return ret;
}
Interface_API void SaveRAM(UINT8 * FileString){
	sys6_board->SaveRAM(FileString);
}
Interface_API void LoadRAM(UINT8 * FileString){
	sys6_board->LoadRAM(FileString);
}
Interface_API void SetDIP(UINT8 Num, UINT8 Value){
	sys6_board->SetDIP(Num, Value);
}
Interface_API void SetHopperEnable(UINT8 Num, UINT8 Value){
	sys6_board->SetHopperEnable(Num, Value);
}
Interface_API void SetHopperCoin(UINT8 Num, UINT8 Value){
	sys6_board->SetHopperCoin(Num, Value);
}
Interface_API void SetHopperCoinsIn(UINT8 Num, UINT32 Value){
	sys6_board->SetHopperCoinsIn(Num, Value);
}
Interface_API void SetHopperCoinsOut(UINT8 Num, UINT32 Value){
	sys6_board->SetHopperCoinsOut(Num, Value);
}
Interface_API void SetHopperLevel(UINT8 Num, UINT32 Value){
	sys6_board->SetHopperLevel(Num, Value);
}
Interface_API void SetHopperFullLevel(UINT8 Num, UINT32 Value){
	sys6_board->SetHopperFullLevel(Num, Value);
}
Interface_API void SetHopperLoEnable(UINT8 Num, UINT8 Value){
	sys6_board->SetHopperLoEnable(Num, Value);
}
Interface_API void SetHopperLoInvert(UINT8 Num, UINT8 Value){
	sys6_board->SetHopperLoInvert(Num, Value);
}
Interface_API void SetHopperLoSwitch(UINT8 Num, UINT8 Value){
	sys6_board->SetHopperLoSwitch(Num, Value);
}
Interface_API void SetHopperLoLevel(UINT8 Num, UINT32 Value){
	sys6_board->SetHopperLoLevel(Num, Value);
}
Interface_API void SetHopperHiEnable(UINT8 Num, UINT8 Value){
	sys6_board->SetHopperHiEnable(Num, Value);
}
Interface_API void SetHopperHiInvert(UINT8 Num, UINT8 Value){
	sys6_board->SetHopperHiInvert(Num, Value);
}
Interface_API void SetHopperHiSwitch(UINT8 Num, UINT8 Value){
	sys6_board->SetHopperHiSwitch(Num, Value);
}
Interface_API void SetHopperHiLevel(UINT8 Num, UINT32 Value){
	sys6_board->SetHopperHiLevel(Num, Value);
}
Interface_API void SetHopperOptoEnable(UINT8 Num, UINT8 Value){
	sys6_board->SetHopperOptoEnable(Num, Value);
}
Interface_API void SetHopperOptoReturn(UINT8 Num, UINT8 Value){
	sys6_board->SetHopperOptoReturn(Num, Value);
}
Interface_API void SetHopperMotorEnable(UINT8 Num, UINT8 Value){
	sys6_board->SetHopperMotorEnable(Num, Value);
}
Interface_API void SetHopperLoIndicator(UINT8 Num, UINT8 Value){
	sys6_board->SetHopperLoIndicator(Num, Value);
}
Interface_API void SetHopperHiIndicator(UINT8 Num, UINT8 Value){
	sys6_board->SetHopperHiIndicator(Num, Value);
}
Interface_API void SetHopperCoinsRefilled(UINT8 Num, UINT32 Value){
	sys6_board->SetHopperCoinsRefilled(Num, Value);
}

Interface_API UINT8 GetHopperEnable(UINT8 Num){
	UINT8 ret = sys6_board->GetHopperEnable(Num);
	return ret;
}
Interface_API UINT8 GetHopperCoin(UINT8 Num){
	UINT8 ret = sys6_board->GetHopperCoin(Num);
	return ret;
}
Interface_API UINT32 GetHopperCoinsIn(UINT8 Num){
	UINT32 ret = sys6_board->GetHopperCoinsIn(Num);
	return ret;
}
Interface_API UINT32 GetHopperCoinsOut(UINT8 Num){
	UINT32 ret = sys6_board->GetHopperCoinsOut(Num);
	return ret;
}
Interface_API UINT32 GetHopperLevel(UINT8 Num){
	UINT32 ret = sys6_board->GetHopperLevel(Num);
	return ret;
}
Interface_API UINT32 GetHopperFullLevel(UINT8 Num){
	UINT32 ret = sys6_board->GetHopperFullLevel(Num);
	return ret;
}
Interface_API UINT8 GetHopperLoEnable(UINT8 Num){
	UINT8 ret = sys6_board->GetHopperLoEnable(Num);
	return ret;
}
Interface_API UINT8 GetHopperLoInvert(UINT8 Num){
	UINT8 ret = sys6_board->GetHopperLoInvert(Num);
	return ret;
}
Interface_API UINT8 GetHopperLoSwitch(UINT8 Num){
	UINT8 ret = sys6_board->GetHopperLoSwitch(Num);
	return ret;
}
Interface_API UINT32 GetHopperLoLevel(UINT8 Num){
	UINT32 ret = sys6_board->GetHopperLoLevel(Num);
	return ret;
}
Interface_API UINT8 GetHopperHiEnable(UINT8 Num){
	UINT8 ret = sys6_board->GetHopperHiEnable(Num);
	return ret;
}
Interface_API UINT8 GetHopperHiInvert(UINT8 Num){
	UINT8 ret = sys6_board->GetHopperHiInvert(Num);
	return ret;
}
Interface_API UINT8 GetHopperHiSwitch(UINT8 Num){
	UINT8 ret = sys6_board->GetHopperHiSwitch(Num);
	return ret;
}
Interface_API UINT32 GetHopperHiLevel(UINT8 Num){
	UINT32 ret = sys6_board->GetHopperHiLevel(Num);
	return ret;
}
Interface_API UINT8 GetHopperOptoEnable(UINT8 Num){
	UINT8 ret = sys6_board->GetHopperOptoEnable(Num);
	return ret;
}
Interface_API UINT8 GetHopperOptoReturn(UINT8 Num){
	UINT8 ret = sys6_board->GetHopperOptoReturn(Num);
	return ret;
}
Interface_API UINT8 GetHopperMotorEnable(UINT8 Num){
	UINT8 ret = sys6_board->GetHopperMotorEnable(Num);
	return ret;
}
Interface_API UINT32 GetHopperCoinsRefilled(UINT8 Num){
	UINT32 ret = sys6_board->GetHopperCoinsRefilled(Num);
	return ret;
}
Interface_API UINT8 GetHopperHiIndicator(UINT8 Num){
	UINT8 ret = sys6_board->GetHopperHiIndicator(Num);
	return ret;
}
Interface_API UINT8 GetHopperLoIndicator(UINT8 Num){
	UINT8 ret = sys6_board->GetHopperLoIndicator(Num);
	return ret;
}

Interface_API void SetStake(UINT8 Stake){
	sys6_board->SetStake(Stake);
}
Interface_API void SetPrize(UINT8 Prize){
	sys6_board->SetPrize(Prize);
}
Interface_API void SetPercent(UINT8 Percent){
	sys6_board->SetPercent(Percent);
}
Interface_API UINT8* GetEDCString(){
	return sys6_board->GetEDCString();
}
