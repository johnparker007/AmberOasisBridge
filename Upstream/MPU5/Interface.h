#pragma once

#include "PA2CoreInterface.h"

#if defined(_WIN32)
#  if defined(MPU5CORE_EXPORTS)
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

// Versioned packet output and pull-based 48 kHz stereo audio.
Interface_API UINT32 GetOutputSnapshotSize(void);
Interface_API UINT32 GetOutputSnapshot(void* OutBuffer, UINT32 OutBufferSize);
Interface_API UINT32 GetAudioFormat(PA2_AudioFormat* Out, UINT32 OutSizeBytes);
Interface_API UINT32 FillAudioFrames(INT16* OutInterleavedStereo, UINT32 FramesRequired);

// Optional MPU5 timing diagnostics.  Older front-ends can ignore these exports.
Interface_API UINT32 GetMPU5TimingStatsSize(void);
Interface_API UINT32 GetMPU5TimingStats(void* OutBuffer, UINT32 OutBufferSize);
Interface_API void ResetMPU5TimingStats(void);
Interface_API void SetMPU5TimingTrace(UINT8 Enable);
Interface_API void SetMPU5DUARTTrace(UINT8 Enable);

// Generic front-end inputs and configuration.
Interface_API void TurnSwitchOn(UINT8 Num);
Interface_API void TurnSwitchOff(UINT8 Num);
Interface_API UINT8 ReadSwitch(UINT8 Num);
Interface_API void SetTestSwitchState(UINT8 State);
Interface_API void SetServiceDoorState(UINT8 State);
Interface_API void SetMainDoorState(UINT8 State);
Interface_API void SetRefillKeyState(UINT8 State);
// Later MPU5/PIC3 cabinets use a second under-shelf Test button. Alien maps
// this to switch 24. The normal Amber Test button automatically performs the
// complete PIC3 entry sequence while the service door is open. These exports
// remain available for diagnostics or a future explicit Test-2 UI control.
Interface_API void SetSecondaryTestSwitchState(UINT8 State);
Interface_API UINT8 RequestTestMode(void);
// Dedicated MPU5 cabinet test button retained as a compatibility alias.
Interface_API void SetTestButton(UINT8 Pressed);
Interface_API void SetDIP(UINT8 Num, UINT8 Value);
Interface_API void SetStake(UINT8 Stake);
Interface_API void SetPrize(UINT8 Prize);
Interface_API void SetPercent(UINT8 Percent);
// Optional MFME-compatible override. SetLegacyPICMode(1) selects the existing
// legacy chip; SetLegacyPICMode(0) selects the programmable PIC. A ROM with an
// explicit programmable-PIC probe takes precedence over a stale legacy value.
Interface_API void SetCharacteriserAddress(UINT32 Address);
Interface_API void SetLegacyPICMode(UINT8 Enable);
// Explicit physical program-card PIC selection: 1=PIC1, 2=PIC2, 3=PIC3.
// Conclusive ROM protocol detection remains authoritative.
Interface_API void SetPICMode(UINT8 Mode);
Interface_API void SetSECFitted(UINT8 Fitted);
// 0 = Compact, 1 = Universal, 2 = Empire Twin, 3 = Serial ccTalk.
Interface_API void SetHopperType(UINT8 Type);
// Generic hopper edit-page ABI used by TechsClass. These names intentionally
// match the existing exports implemented by the other technology DLLs.
Interface_API void SetHopperEnable(UINT8 Num, UINT8 Value);
Interface_API void SetHopperCoinsIn(UINT8 Num, UINT32 Value);
Interface_API void SetHopperCoinsOut(UINT8 Num, UINT32 Value);
Interface_API void SetHopperOptoEnable(UINT8 Num, UINT8 Value);
Interface_API void SetHopperOptoReturn(UINT8 Num, UINT8 Value);
Interface_API void SetHopperMotorEnable(UINT8 Num, UINT8 Value);
Interface_API void SetHopperCoin(UINT8 Num, UINT8 Value);
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
Interface_API void SetHopperLoIndicator(UINT8 Num, UINT8 Value);
Interface_API void SetHopperHiIndicator(UINT8 Num, UINT8 Value);
Interface_API void SetHopperCoinsRefilled(UINT8 Num, UINT32 Value);
Interface_API UINT8 GetHopperEnable(UINT8 Num);
Interface_API UINT32 GetHopperCoinsIn(UINT8 Num);
Interface_API UINT32 GetHopperCoinsOut(UINT8 Num);
Interface_API UINT8 GetHopperOptoEnable(UINT8 Num);
Interface_API UINT8 GetHopperOptoReturn(UINT8 Num);
Interface_API UINT8 GetHopperMotorEnable(UINT8 Num);
Interface_API UINT8 GetHopperCoin(UINT8 Num);
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
Interface_API UINT8 GetHopperLoIndicator(UINT8 Num);
Interface_API UINT8 GetHopperHiIndicator(UINT8 Num);
Interface_API UINT32 GetHopperCoinsRefilled(UINT8 Num);
// Sets the SCH2 EEPROM recovery counters for a legacy 64K RAM image which
// predates persisted hopper state. Paid/unpaid are coin counts from the last
// interrupted payout; passing 0,0 clears the recovery state.
Interface_API void SetSerialHopperRecoveryState(UINT8 Paid, UINT8 Unpaid);
// Marks one logical matrix lamp as failed for lamp-current test feedback only.
Interface_API void SetLampBroken(UINT16 Lamp, UINT8 Broken);
Interface_API void SetOptoInvert(UINT8 ReelNum, UINT8 State);
Interface_API void SetOptoStart(UINT8 ReelNum, UINT8 Start);
Interface_API void SetOptoEnd(UINT8 ReelNum, UINT8 End);
Interface_API void SetSteps(UINT8 ReelNum, UINT8 Steps);
// Profile: 0=Auto, 1=Early/legacy REEL5, 2=Late/PIC3 REEL5.
Interface_API void SetReelJumperProfile(UINT8 Controller, UINT8 Profile);

// MPU5 electronic coin-mech configuration.
Interface_API UINT8 CoinIn(UINT8 MechNum, UINT8 Coin, UINT8 CoinValue);
// Returns 1 only when CoinIn() would be accepted at this instant.
Interface_API UINT8 CanAcceptCoin(UINT8 MechNum, UINT8 Coin, UINT8 CoinValue);
// Bits 0..5 correspond to electronic coin channels 0..5; 1 means locked or
// temporarily unavailable. The same value is exposed in OutputSnapshot.
Interface_API UINT8 GetCoinLockoutState(void);
Interface_API UINT8 MarsCoinIn(UINT8 Coin, UINT8 CoinValue);
Interface_API UINT8 S10CoinIn(UINT8 MechNum, UINT8 CoinValue);
// Electronic coin communication: 0=Parallel, 1=BCD, 2=Serial, 3=ccTalk.
Interface_API void SetCommStyle(UINT8 Style);
Interface_API void SetCommInvert(UINT8 Invert);
Interface_API void SetCycles(UINT32 Cycles);
Interface_API void SetEDCEnable(UINT8 Enable);
Interface_API UINT8* GetEDCString(void);
Interface_API UINT32 PopEDCMessage(char* Output, UINT32 OutputSize);
Interface_API void SetLockoutVal(UINT8 Coin, UINT8 Value);
Interface_API void SetLockoutInvert(UINT8 Coin, UINT8 Invert);
Interface_API void SetCoinValue(UINT8 Coin, UINT8 Value);
Interface_API void SetCoinEnable(UINT8 Coin, UINT8 Enable);
Interface_API void SetMarsCommStyle(UINT8 Style);
Interface_API void SetMarsCommInvert(UINT8 Invert);
Interface_API void SetMarsCycles(UINT32 Cycles);
Interface_API void SetMarsLockoutVal(UINT8 Coin, UINT8 Value);
Interface_API void SetMarsLockoutInvert(UINT8 Coin, UINT8 Invert);
Interface_API void SetMarsCoinEnable(UINT8 Coin, UINT8 Enable);

// Persistence placeholders.
Interface_API void SaveRAM(UINT8* FileString);
Interface_API void LoadRAM(UINT8* FileString);
Interface_API void SetCFolder(UINT8* Folder);
Interface_API void SetCFileName(UINT8* FileName);
Interface_API void SaveState(void);
Interface_API void LoadState(void);

// Legacy-safe output surface retained as a front-end fallback while the packet interface is adopted.
Interface_API void UpdateLamps(void);
Interface_API UINT8 GetLampsOn(UINT16 Num);
Interface_API UINT8 GetLampOn(UINT16 Num);
Interface_API float GetLampBrightness(UINT16 Num);
Interface_API float GetFilamentColourR(UINT16 Num);
Interface_API float GetFilamentColourG(UINT16 Num);
Interface_API float GetFilamentColourB(UINT16 Num);
Interface_API float GetLampFilamentTemperatureK(UINT16 Num);
Interface_API float GetLampFilamentResistanceOhms(UINT16 Num);
Interface_API float GetLampFilamentPowerW(UINT16 Num);
Interface_API float GetLampDuty(UINT16 Num);
Interface_API float GetLampVoltageRMS(UINT16 Num);
Interface_API INT16 GetPosOut(UINT8 Num);
Interface_API UINT8 GetReelLamp(UINT8 Num);
Interface_API UINT16 GetAlphaSegments(UINT8 Num);
Interface_API UINT8 GetAlphaDotComma(UINT8 Num);
Interface_API UINT8 GetAlphaBright(void);
Interface_API UINT8 GetAlphaSegmentCount(void);
Interface_API UINT8 GetAlphaCharacterCount(void);
Interface_API UINT8 GetAlphaChar(UINT8 Num);
Interface_API UINT8 GetSegOn(UINT16 Num);
Interface_API UINT8 GetSegBright(UINT16 Num);
Interface_API void UpdateSegs(void);
Interface_API UINT32 GetMeterCounter(UINT8 Index);
Interface_API UINT8 GetStatusLED(void);

#ifdef __cplusplus
}
#endif
