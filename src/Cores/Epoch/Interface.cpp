#include "stdafx.h"
#include "Interface.h"
#include "Epoch.h"

#include <cstring>
#include <memory>

namespace
{
constexpr float kDLLVersion = 1.3000f;
std::unique_ptr<BoardEpoch> g_Core;
UINT8 g_FlashROMMode = 0U;

BoardEpoch* EnsureCore()
{
    if (!g_Core) { g_Core = std::make_unique<BoardEpoch>(); }
    return g_Core.get();
}

const char* ToChar(UINT8* Value)
{
    return reinterpret_cast<const char*>(Value);
}

char* ToMutableChar(UINT8* Value)
{
    return reinterpret_cast<char*>(Value);
}

void SetEpochSwitchState(UINT8 Switch, UINT8 State, bool ActiveLow)
{
    BoardEpoch* Core = EnsureCore();
    const bool PhysicalOn = ActiveLow ? (State == 0U) : (State != 0U);
    if (PhysicalOn) { Core->TurnSwitchOn(Switch); }
    else { Core->TurnSwitchOff(Switch); }
}
}

Interface_API float GetDLLVersion(void) { return kDLLVersion; }

Interface_API UINT8 Initialise(void)
{
    BoardEpoch* Core = EnsureCore();
    Core->Init();
    return 1U;
}

Interface_API UINT8 Shutdown(void)
{
    if (!g_Core) { return 0U; }
    g_Core.reset();
    return 1U;
}

Interface_API UINT8 Reset(void)
{
    EnsureCore()->PowerOnReset();
    return 1U;
}

Interface_API INT32 Run(UINT32 Cycles)
{
    return g_Core ? static_cast<INT32>(g_Core->Execute(static_cast<int>(Cycles))) : 0;
}

Interface_API void SetFlashROMMode(UINT8 Enable) { g_FlashROMMode = Enable ? 1U : 0U; }

Interface_API UINT32 LoadROM(UINT8* Name1, UINT8* Name2, UINT8* Name3, UINT8* Name4)
{
    return static_cast<UINT32>(EnsureCore()->LoadROM(
        ToMutableChar(Name1), ToMutableChar(Name2), ToMutableChar(Name3), ToMutableChar(Name4),
        static_cast<char>(g_FlashROMMode)));
}

Interface_API UINT32 LoadSoundROM(UINT8* Name1, UINT8* Name2, UINT8* Name3, UINT8* Name4)
{
    return static_cast<UINT32>(EnsureCore()->LoadSoundROM(
        ToMutableChar(Name1), ToMutableChar(Name2), ToMutableChar(Name3), ToMutableChar(Name4)));
}

Interface_API UINT32 GetOutputSnapshotSize(void)
{
    return static_cast<UINT32>(sizeof(PA2_OutputSnapshot));
}

Interface_API UINT32 GetOutputSnapshot(void* OutBuffer, UINT32 OutBufferSize)
{
    if (!g_Core || !OutBuffer || OutBufferSize < sizeof(PA2_OutputSnapshot)) { return 0U; }
    return g_Core->GetOutputSnapshot(*static_cast<PA2_OutputSnapshot*>(OutBuffer));
}

Interface_API UINT32 GetAudioFormat(PA2_AudioFormat* Out, UINT32 OutSizeBytes)
{
    if (!Out || OutSizeBytes < sizeof(PA2_AudioFormat)) { return 0U; }
    std::memset(Out, 0, sizeof(*Out));
    Out->SizeBytes = static_cast<UINT32>(sizeof(*Out));
    Out->Version = PA2_AUDIO_FORMAT_VERSION;
    Out->SampleRate = SampledSound::OutputSampleRate;
    Out->Channels = SampledSound::OutputChannels;
    Out->BitsPerSample = 16U;
    Out->Format = PA2_AUDIO_FORMAT_PCM_S16;
    return static_cast<UINT32>(sizeof(*Out));
}

Interface_API UINT32 FillAudioFrames(INT16* OutInterleavedStereo, UINT32 FramesRequired)
{
    if (!g_Core)
    {
        if (!OutInterleavedStereo || FramesRequired == 0U) { return 0U; }
        std::memset(OutInterleavedStereo, 0,
            static_cast<size_t>(FramesRequired) * SampledSound::OutputChannels * sizeof(INT16));
        return FramesRequired;
    }
    return g_Core->FillAudioFrames(OutInterleavedStereo, FramesRequired);
}

Interface_API void TurnSwitchOn(UINT8 Num) { EnsureCore()->TurnSwitchOn(Num); }
Interface_API void TurnSwitchOff(UINT8 Num) { EnsureCore()->TurnSwitchOff(Num); }
Interface_API UINT8 ReadSwitch(UINT8 Num) { return g_Core ? g_Core->ReadSwitch(Num) : 0U; }
Interface_API void SetTestSwitchState(UINT8 State) { SetEpochSwitchState(255U, State, false); }
Interface_API void SetServiceDoorState(UINT8 State) { SetEpochSwitchState(55U, State, true); }
Interface_API void SetMainDoorState(UINT8 State) { SetEpochSwitchState(56U, State, true); }
Interface_API void SetRefillKeyState(UINT8 State) { SetEpochSwitchState(53U, State, false); }
Interface_API void SetDIP(UINT8 Num, UINT8 Value) { EnsureCore()->SetDIP(Num, Value); }
Interface_API void SetStake(UINT8 Value) { EnsureCore()->SetStake(static_cast<char>(Value)); }
Interface_API void SetPrize(UINT8 Value) { EnsureCore()->SetPrize(static_cast<char>(Value)); }
Interface_API void SetPercent(UINT8 Value) { EnsureCore()->SetPercent(static_cast<char>(Value)); }
Interface_API void SetOptoInvert(UINT8 ReelNum, UINT8 Value) { EnsureCore()->SetOptoInvert(ReelNum, Value); }
Interface_API void SetOptoStart(UINT8 ReelNum, UINT8 Value) { EnsureCore()->SetOptoStart(ReelNum, Value); }
Interface_API void SetOptoEnd(UINT8 ReelNum, UINT8 Value) { EnsureCore()->SetOptoEnd(ReelNum, Value); }
Interface_API void SetSteps(UINT8 ReelNum, UINT8 Value) { EnsureCore()->SetSteps(ReelNum, Value); }
Interface_API void SetReelExt(UINT8 Ext) { EnsureCore()->SetReelExt(Ext); }

Interface_API UINT8 CoinIn(UINT8 MechNum, UINT8 Coin, UINT8 CoinValue)
{
    return EnsureCore()->CoinIn(MechNum, Coin, CoinValue);
}
Interface_API UINT8 MarsCoinIn(UINT8 Coin, UINT8 CoinValue) { return CoinIn(0U, Coin, CoinValue); }
Interface_API void SetCommStyle(UINT8 Style) { EnsureCore()->SetCommStyle(0U, Style); }
Interface_API void SetCommInvert(UINT8 Invert) { EnsureCore()->SetCommInvert(0U, Invert); }
Interface_API void SetCycles(UINT32 Cycles) { EnsureCore()->SetCycles(0U, Cycles); }
Interface_API void SetEDCEnable(UINT8 Enable) { EnsureCore()->SetEDCEnable(0U, Enable); }
Interface_API void SetLockoutVal(UINT8 Coin, UINT8 Value) { EnsureCore()->SetLockoutVal(0U, Coin, Value); }
Interface_API void SetLockoutInvert(UINT8 Coin, UINT8 Invert) { EnsureCore()->SetLockoutInvert(0U, Coin, Invert); }
Interface_API void SetCoinValue(UINT8 Coin, UINT8 Value) { EnsureCore()->SetCoinValue(0U, Coin, Value); }
Interface_API void SetCoinEnable(UINT8 Coin, UINT8 Enable) { EnsureCore()->SetCoinEnable(0U, Coin, Enable); }
Interface_API UINT8 GetCoinLampOnOff(UINT8 LampNum) { return g_Core ? g_Core->GetLampOnOff(0U, LampNum) : 0U; }
Interface_API UINT8* GetEDCString(void)
{
    return g_Core ? reinterpret_cast<UINT8*>(g_Core->getEDCString()) : nullptr;
}

Interface_API void SetMeterEnable(UINT8 Num, UINT8 Value) { EnsureCore()->SetMeterEnable(Num, Value); }
Interface_API void SetMeterCounter(UINT8 Num, UINT32 Value) { EnsureCore()->SetMeterCounter(Num, Value); }
Interface_API UINT32 GetMeterCounter(UINT8 Num) { return g_Core ? g_Core->GetMeterCounter(Num) : 0U; }

Interface_API void SetHopperEnable(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperEnable(Num, Value); }
Interface_API void SetHopperCoin(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperCoin(Num, Value); }
Interface_API void SetHopperCoinsIn(UINT8 Num, UINT32 Value) { EnsureCore()->SetHopperCoinsIn(Num, Value); }
Interface_API void SetHopperCoinsOut(UINT8 Num, UINT32 Value) { EnsureCore()->SetHopperCoinsOut(Num, Value); }
Interface_API void SetHopperLevel(UINT8 Num, UINT32 Value) { EnsureCore()->SetHopperLevel(Num, Value); }
Interface_API void SetHopperFullLevel(UINT8 Num, UINT32 Value) { EnsureCore()->SetHopperFullLevel(Num, Value); }
Interface_API void SetHopperLoEnable(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperLoEnable(Num, Value); }
Interface_API void SetHopperLoInvert(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperLoInvert(Num, Value); }
Interface_API void SetHopperLoSwitch(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperLoSwitch(Num, Value); }
Interface_API void SetHopperLoLevel(UINT8 Num, UINT32 Value) { EnsureCore()->SetHopperLoLevel(Num, Value); }
Interface_API void SetHopperHiEnable(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperHiEnable(Num, Value); }
Interface_API void SetHopperHiInvert(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperHiInvert(Num, Value); }
Interface_API void SetHopperHiSwitch(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperHiSwitch(Num, Value); }
Interface_API void SetHopperHiLevel(UINT8 Num, UINT32 Value) { EnsureCore()->SetHopperHiLevel(Num, Value); }
Interface_API void SetHopperOptoEnable(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperOptoEnable(Num, Value); }
Interface_API void SetHopperOptoReturn(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperOptoReturn(Num, Value); }
Interface_API void SetHopperMotorEnable(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperMotorEnable(Num, Value); }
Interface_API void SetHopperLoIndicator(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperLoIndicator(Num, Value); }
Interface_API void SetHopperHiIndicator(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperHiIndicator(Num, Value); }
Interface_API void SetHopperCoinsRefilled(UINT8 Num, UINT32 Value) { EnsureCore()->SetHopperCoinsRefilled(Num, Value); }
Interface_API UINT8 GetHopperEnable(UINT8 Num) { return g_Core ? g_Core->GetHopperEnable(Num) : 0U; }
Interface_API UINT8 GetHopperCoin(UINT8 Num) { return g_Core ? g_Core->GetHopperCoin(Num) : 0U; }
Interface_API UINT32 GetHopperCoinsIn(UINT8 Num) { return g_Core ? g_Core->GetHopperCoinsIn(Num) : 0U; }
Interface_API UINT32 GetHopperCoinsOut(UINT8 Num) { return g_Core ? g_Core->GetHopperCoinsOut(Num) : 0U; }
Interface_API UINT32 GetHopperLevel(UINT8 Num) { return g_Core ? g_Core->GetHopperLevel(Num) : 0U; }
Interface_API UINT32 GetHopperFullLevel(UINT8 Num) { return g_Core ? g_Core->GetHopperFullLevel(Num) : 0U; }
Interface_API UINT8 GetHopperLoEnable(UINT8 Num) { return g_Core ? g_Core->GetHopperLoEnable(Num) : 0U; }
Interface_API UINT8 GetHopperLoInvert(UINT8 Num) { return g_Core ? g_Core->GetHopperLoInvert(Num) : 0U; }
Interface_API UINT8 GetHopperLoSwitch(UINT8 Num) { return g_Core ? g_Core->GetHopperLoSwitch(Num) : 0U; }
Interface_API UINT32 GetHopperLoLevel(UINT8 Num) { return g_Core ? g_Core->GetHopperLoLevel(Num) : 0U; }
Interface_API UINT8 GetHopperHiEnable(UINT8 Num) { return g_Core ? g_Core->GetHopperHiEnable(Num) : 0U; }
Interface_API UINT8 GetHopperHiInvert(UINT8 Num) { return g_Core ? g_Core->GetHopperHiInvert(Num) : 0U; }
Interface_API UINT8 GetHopperHiSwitch(UINT8 Num) { return g_Core ? g_Core->GetHopperHiSwitch(Num) : 0U; }
Interface_API UINT32 GetHopperHiLevel(UINT8 Num) { return g_Core ? g_Core->GetHopperHiLevel(Num) : 0U; }
Interface_API UINT8 GetHopperOptoEnable(UINT8 Num) { return g_Core ? g_Core->GetHopperOptoEnable(Num) : 0U; }
Interface_API UINT8 GetHopperOptoReturn(UINT8 Num) { return g_Core ? g_Core->GetHopperOptoReturn(Num) : 0U; }
Interface_API UINT8 GetHopperMotorEnable(UINT8 Num) { return g_Core ? g_Core->GetHopperMotorEnable(Num) : 0U; }
Interface_API UINT32 GetHopperCoinsRefilled(UINT8 Num) { return g_Core ? g_Core->GetHopperCoinsRefilled(Num) : 0U; }
Interface_API UINT8 GetHopperHiIndicator(UINT8 Num) { return g_Core ? g_Core->GetHopperHiIndicator(Num) : 0U; }
Interface_API UINT8 GetHopperLoIndicator(UINT8 Num) { return g_Core ? g_Core->GetHopperLoIndicator(Num) : 0U; }

Interface_API void ClearRAM(void) { EnsureCore()->ClearRAM(); }
Interface_API void SaveRAM(UINT8* FileString) { if (FileString) EnsureCore()->SaveRAM(ToChar(FileString)); }
Interface_API void LoadRAM(UINT8* FileString) { if (FileString) EnsureCore()->LoadRAM(ToChar(FileString)); }
Interface_API void SetCFolder(UINT8* Folder) { EnsureCore()->SetCFolder(ToChar(Folder)); }
Interface_API void SetCFileName(UINT8* FileName) { EnsureCore()->SetCFileName(ToChar(FileName)); }
Interface_API void SaveState(void) { if (g_Core) g_Core->SaveState(); }
Interface_API void LoadState(void) { if (g_Core) g_Core->LoadState(); }

Interface_API void UpdateLamps(void) {}
Interface_API UINT8 GetLampsOn(UINT16 Num) { return g_Core && g_Core->GetLampOn(Num) ? 1U : 0U; }
Interface_API UINT8 GetLampOn(UINT16 Num) { return GetLampsOn(Num); }
Interface_API float GetLampBrightness(UINT16 Num) { return g_Core ? g_Core->GetLampBright(Num) : 0.0f; }
Interface_API float GetLampBright(UINT16 Num) { return GetLampBrightness(Num); }
Interface_API float GetFilamentColourR(UINT16 Num) { return g_Core ? g_Core->GetFilamentColorR(Num) : 0.0f; }
Interface_API float GetFilamentColourG(UINT16 Num) { return g_Core ? g_Core->GetFilamentColorG(Num) : 0.0f; }
Interface_API float GetFilamentColourB(UINT16 Num) { return g_Core ? g_Core->GetFilamentColorB(Num) : 0.0f; }
Interface_API INT16 GetPosOut(UINT8 Num) { return g_Core ? g_Core->GetReelPos(Num) : 0; }
Interface_API UINT16 GetAlphaSegments(UINT8 Num) { return g_Core ? static_cast<UINT16>(g_Core->GetAlphaSegs(static_cast<char>(Num))) : 0U; }
Interface_API UINT8 GetAlphaDots(UINT8 CharNum, UINT8 ColumnNum) { return g_Core ? g_Core->GetAlphaDots(static_cast<char>(CharNum), static_cast<char>(ColumnNum)) : 0U; }
Interface_API UINT8 GetAlphaDotComma(UINT8 Num) { return g_Core ? g_Core->GetAlphaDotComma(static_cast<char>(Num)) : 0U; }
Interface_API UINT8 GetAlphaDDotComma(UINT8 Num) { return g_Core ? g_Core->GetAlphaDDotComma(static_cast<char>(Num)) : 0U; }
Interface_API UINT8 GetAlphaBright(void) { return g_Core ? static_cast<UINT8>(g_Core->GetAlphaBright()) : 0U; }
Interface_API UINT8 GetAlphaDBright(void) { return g_Core ? static_cast<UINT8>(g_Core->GetAlphaDBright()) : 0U; }
Interface_API UINT8 GetAlphaSegmentCount(void) { return PA2_ALPHA_SEGMENTS_IMPACT; }
Interface_API UINT8 GetAlphaCharacterCount(void) { return PA2_NUM_ALPHA_CHARS; }
Interface_API UINT8 GetAlphaChar(UINT8 Num) { return g_Core ? g_Core->GetAlphaChar(Num) : 0U; }
Interface_API UINT8 GetSegOn(UINT16 Num) { return g_Core ? g_Core->GetSegOn(Num) : 0U; }
Interface_API UINT8 GetSegBright(UINT16 Num) { return g_Core ? g_Core->GetSegBright(Num) : 0U; }
Interface_API void UpdateSegs(void) {}
Interface_API UINT8 GetStatusLED(void) { return g_Core ? g_Core->GetStatusLED() : PA2_STATUS_LED_OFF; }

Interface_API UINT8 EPOCHInitialise(void) { return Initialise(); }
Interface_API UINT8 EPOCHShutdown(void) { return Shutdown(); }
Interface_API void ResetMachine(void) { Reset(); }
Interface_API UINT8 GetLampOnOff(UINT8 MechNum, UINT8 LampNum)
{
    return g_Core ? g_Core->GetLampOnOff(MechNum, LampNum) : 0U;
}
