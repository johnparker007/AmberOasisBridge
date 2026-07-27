#pragma once

#include "PA2CoreInterface.h"

#if defined(_WIN32)
#  if defined(EPOCHCORE_EXPORTS)
#    define Interface_API __declspec(dllexport)
#  else
#    define Interface_API __declspec(dllimport)
#  endif
#else
#  define Interface_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Project Amber 2 generic core interface.
Interface_API float GetDLLVersion(void);
Interface_API UINT8 Initialise(void);
Interface_API UINT8 Shutdown(void);
Interface_API UINT8 Reset(void);
Interface_API INT32 Run(UINT32 Cycles);
Interface_API UINT32 LoadROM(UINT8* Name1, UINT8* Name2, UINT8* Name3, UINT8* Name4);
Interface_API UINT32 LoadSoundROM(UINT8* Name1, UINT8* Name2, UINT8* Name3, UINT8* Name4);
Interface_API void SetFlashROMMode(UINT8 Enable);

// Versioned bulk component output and front-end pull audio.
Interface_API UINT32 GetOutputSnapshotSize(void);
Interface_API UINT32 GetOutputSnapshot(void* OutBuffer, UINT32 OutBufferSize);
Interface_API UINT32 GetAudioFormat(PA2_AudioFormat* Out, UINT32 OutSizeBytes);
Interface_API UINT32 FillAudioFrames(INT16* OutInterleavedStereo, UINT32 FramesRequired);

// Generic inputs and machine configuration.
Interface_API void TurnSwitchOn(UINT8 Num);
Interface_API void TurnSwitchOff(UINT8 Num);
Interface_API UINT8 ReadSwitch(UINT8 Num);
Interface_API void SetTestSwitchState(UINT8 State);
Interface_API void SetServiceDoorState(UINT8 State);
Interface_API void SetMainDoorState(UINT8 State);
Interface_API void SetRefillKeyState(UINT8 State);
Interface_API void SetDIP(UINT8 Num, UINT8 Value);
Interface_API void SetStake(UINT8 Stake);
Interface_API void SetPrize(UINT8 Prize);
Interface_API void SetPercent(UINT8 Percent);
Interface_API void SetOptoInvert(UINT8 ReelNum, UINT8 State);
Interface_API void SetOptoStart(UINT8 ReelNum, UINT8 Start);
Interface_API void SetOptoEnd(UINT8 ReelNum, UINT8 End);
Interface_API void SetSteps(UINT8 ReelNum, UINT8 Steps);
Interface_API void SetReelExt(UINT8 Ext);

// Electronic coin mech. The current generic ABI exposes mech zero directly.
Interface_API UINT8 CoinIn(UINT8 MechNum, UINT8 Coin, UINT8 CoinValue);
Interface_API UINT8 MarsCoinIn(UINT8 Coin, UINT8 CoinValue);
Interface_API void SetCommStyle(UINT8 Style);
Interface_API void SetCommInvert(UINT8 Invert);
Interface_API void SetCycles(UINT32 Cycles);
Interface_API void SetEDCEnable(UINT8 Enable);
Interface_API void SetLockoutVal(UINT8 Coin, UINT8 Value);
Interface_API void SetLockoutInvert(UINT8 Coin, UINT8 Invert);
Interface_API void SetCoinValue(UINT8 Coin, UINT8 Value);
Interface_API void SetCoinEnable(UINT8 Coin, UINT8 Enable);
Interface_API UINT8 GetCoinLampOnOff(UINT8 LampNum);
Interface_API UINT8* GetEDCString(void);

// Meters.
Interface_API void SetMeterEnable(UINT8 Num, UINT8 Value);
Interface_API void SetMeterCounter(UINT8 Num, UINT32 Value);
Interface_API UINT32 GetMeterCounter(UINT8 Num);

// Hoppers.
Interface_API void SetHopperEnable(UINT8 Num, UINT8 Value);
Interface_API void SetHopperCoin(UINT8 Num, UINT8 Value);
Interface_API void SetHopperCoinsIn(UINT8 Num, UINT32 Value);
Interface_API void SetHopperCoinsOut(UINT8 Num, UINT32 Value);
Interface_API void SetHopperLevel(UINT8 Num, UINT32 Value);
Interface_API void SetHopperFullLevel(UINT8 Num, UINT32 Value);
Interface_API void SetHopperLoEnable(UINT8 Num, UINT8 Value);
Interface_API void SetHopperLoInvert(UINT8 Num, UINT8 Value);
Interface_API void SetHopperLoSwitch(UINT8 Num, UINT8 Value);
Interface_API void SetHopperLoLevel(UINT8 Num, UINT32 Value);
Interface_API void SetHopperHiEnable(UINT8 Num, UINT8 Value);
Interface_API void SetHopperHiInvert(UINT8 Num, UINT8 Value);
Interface_API void SetHopperHiSwitch(UINT8 Num, UINT8 Value);
Interface_API void SetHopperHiLevel(UINT8 Num, UINT32 Value);
Interface_API void SetHopperOptoEnable(UINT8 Num, UINT8 Value);
Interface_API void SetHopperOptoReturn(UINT8 Num, UINT8 Value);
Interface_API void SetHopperMotorEnable(UINT8 Num, UINT8 Value);
Interface_API void SetHopperLoIndicator(UINT8 Num, UINT8 Value);
Interface_API void SetHopperHiIndicator(UINT8 Num, UINT8 Value);
Interface_API void SetHopperCoinsRefilled(UINT8 Num, UINT32 Value);
Interface_API UINT8 GetHopperEnable(UINT8 Num);
Interface_API UINT8 GetHopperCoin(UINT8 Num);
Interface_API UINT32 GetHopperCoinsIn(UINT8 Num);
Interface_API UINT32 GetHopperCoinsOut(UINT8 Num);
Interface_API UINT32 GetHopperLevel(UINT8 Num);
Interface_API UINT32 GetHopperFullLevel(UINT8 Num);
Interface_API UINT8 GetHopperLoEnable(UINT8 Num);
Interface_API UINT8 GetHopperLoInvert(UINT8 Num);
Interface_API UINT8 GetHopperLoSwitch(UINT8 Num);
Interface_API UINT32 GetHopperLoLevel(UINT8 Num);
Interface_API UINT8 GetHopperHiEnable(UINT8 Num);
Interface_API UINT8 GetHopperHiInvert(UINT8 Num);
Interface_API UINT8 GetHopperHiSwitch(UINT8 Num);
Interface_API UINT32 GetHopperHiLevel(UINT8 Num);
Interface_API UINT8 GetHopperOptoEnable(UINT8 Num);
Interface_API UINT8 GetHopperOptoReturn(UINT8 Num);
Interface_API UINT8 GetHopperMotorEnable(UINT8 Num);
Interface_API UINT32 GetHopperCoinsRefilled(UINT8 Num);
Interface_API UINT8 GetHopperHiIndicator(UINT8 Num);
Interface_API UINT8 GetHopperLoIndicator(UINT8 Num);

// Persistence.
Interface_API void ClearRAM(void);
Interface_API void SaveRAM(UINT8* FileString);
Interface_API void LoadRAM(UINT8* FileString);
Interface_API void SetCFolder(UINT8* Folder);
Interface_API void SetCFileName(UINT8* FileName);
Interface_API void SaveState(void);
Interface_API void LoadState(void);

// Transitional component getters retained for older layouts/front-ends.
Interface_API void UpdateLamps(void);
Interface_API UINT8 GetLampsOn(UINT16 Num);
Interface_API UINT8 GetLampOn(UINT16 Num);
Interface_API float GetLampBrightness(UINT16 Num);
Interface_API float GetLampBright(UINT16 Num);
Interface_API float GetFilamentColourR(UINT16 Num);
Interface_API float GetFilamentColourG(UINT16 Num);
Interface_API float GetFilamentColourB(UINT16 Num);
Interface_API INT16 GetPosOut(UINT8 Num);
Interface_API UINT16 GetAlphaSegments(UINT8 Num);
Interface_API UINT8 GetAlphaDots(UINT8 CharNum, UINT8 ColumnNum);
Interface_API UINT8 GetAlphaDotComma(UINT8 Num);
Interface_API UINT8 GetAlphaDDotComma(UINT8 Num);
Interface_API UINT8 GetAlphaBright(void);
Interface_API UINT8 GetAlphaDBright(void);
Interface_API UINT8 GetAlphaSegmentCount(void);
Interface_API UINT8 GetAlphaCharacterCount(void);
Interface_API UINT8 GetAlphaChar(UINT8 Num);
Interface_API UINT8 GetSegOn(UINT16 Num);
Interface_API UINT8 GetSegBright(UINT16 Num);
Interface_API void UpdateSegs(void);
Interface_API UINT8 GetStatusLED(void);

// Legacy Epoch names retained as aliases, excluding the obsolete five-argument
// LoadROM signature which is replaced by SetFlashROMMode plus generic LoadROM.
Interface_API UINT8 EPOCHInitialise(void);
Interface_API UINT8 EPOCHShutdown(void);
Interface_API void ResetMachine(void);
Interface_API UINT8 GetLampOnOff(UINT8 MechNum, UINT8 LampNum);

#ifdef __cplusplus
}
#endif
