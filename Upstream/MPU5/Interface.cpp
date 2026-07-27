#include "Interface.h"
#include "MPU5.h"

#include <cstring>
#include <memory>

namespace
{
constexpr float kDLLVersion = 1.2978f;
constexpr UINT32 kAudioSampleRate = 48000;
constexpr UINT32 kAudioChannels = 2;
std::unique_ptr<MPU5> g_Core;

MPU5* EnsureCore()
{
    if (!g_Core) { g_Core = std::make_unique<MPU5>(); }
    return g_Core.get();
}

const char* ToChar(UINT8* Value)
{
    return reinterpret_cast<const char*>(Value);
}
}

Interface_API float GetDLLVersion(void) { return kDLLVersion; }

Interface_API UINT8 Initialise(void)
{
    return EnsureCore()->Initialise() ? 1 : 0;
}

Interface_API UINT8 Shutdown(void)
{
    if (!g_Core) { return 0; }
    g_Core->Shutdown();
    g_Core.reset();
    return 1;
}

Interface_API UINT8 Reset(void)
{
    return EnsureCore()->Reset() ? 1 : 0;
}

Interface_API INT32 Run(UINT32 Cycles)
{
    return g_Core ? g_Core->Run(Cycles) : 0;
}

Interface_API UINT32 LoadROM(UINT8* Name1, UINT8* Name2, UINT8* Name3, UINT8* Name4)
{
    return EnsureCore()->LoadProgramROM(ToChar(Name1), ToChar(Name2), ToChar(Name3), ToChar(Name4));
}

Interface_API UINT32 LoadSoundROM(UINT8* Name1, UINT8* Name2, UINT8* Name3, UINT8* Name4)
{
    return EnsureCore()->LoadSoundROM(ToChar(Name1), ToChar(Name2), ToChar(Name3), ToChar(Name4));
}

Interface_API UINT32 GetOutputSnapshotSize(void)
{
    return static_cast<UINT32>(sizeof(PA2_OutputSnapshot));
}

Interface_API UINT32 GetOutputSnapshot(void* OutBuffer, UINT32 OutBufferSize)
{
    if (!g_Core || !OutBuffer || OutBufferSize < sizeof(PA2_OutputSnapshot)) { return 0; }
    return g_Core->GetOutputSnapshot(*static_cast<PA2_OutputSnapshot*>(OutBuffer));
}

Interface_API UINT32 GetAudioFormat(PA2_AudioFormat* Out, UINT32 OutSizeBytes)
{
    if (!Out || OutSizeBytes < sizeof(PA2_AudioFormat)) { return 0; }
    std::memset(Out, 0, sizeof(*Out));
    Out->SizeBytes = static_cast<UINT32>(sizeof(*Out));
    Out->Version = PA2_AUDIO_FORMAT_VERSION;
    Out->SampleRate = kAudioSampleRate;
    Out->Channels = kAudioChannels;
    Out->BitsPerSample = 16;
    Out->Format = PA2_AUDIO_FORMAT_PCM_S16;
    return static_cast<UINT32>(sizeof(*Out));
}

Interface_API UINT32 FillAudioFrames(INT16* OutInterleavedStereo, UINT32 FramesRequired)
{
    if (!g_Core)
    {
        if (!OutInterleavedStereo || FramesRequired == 0) { return 0; }
        std::memset(OutInterleavedStereo, 0,
            static_cast<size_t>(FramesRequired) * kAudioChannels * sizeof(INT16));
        return FramesRequired;
    }
    return g_Core->FillAudioFrames(OutInterleavedStereo, FramesRequired);
}

Interface_API UINT32 GetMPU5TimingStatsSize(void)
{
    return static_cast<UINT32>(sizeof(PA2_MPU5TimingStats));
}

Interface_API UINT32 GetMPU5TimingStats(void* OutBuffer, UINT32 OutBufferSize)
{
    if (!g_Core || !OutBuffer || OutBufferSize < sizeof(PA2_MPU5TimingStats)) { return 0; }
    return g_Core->GetTimingStats(*static_cast<PA2_MPU5TimingStats*>(OutBuffer));
}

Interface_API void ResetMPU5TimingStats(void)
{
    if (g_Core) { g_Core->ResetTimingStats(); }
}

Interface_API void SetMPU5TimingTrace(UINT8 Enable)
{
    EnsureCore()->SetTimingTrace(Enable);
}

Interface_API void SetMPU5DUARTTrace(UINT8 Enable)
{
    EnsureCore()->SetDUARTTrace(Enable);
}

Interface_API void TurnSwitchOn(UINT8 Num) { EnsureCore()->SetSwitch(Num, 1); }
Interface_API void TurnSwitchOff(UINT8 Num) { EnsureCore()->SetSwitch(Num, 0); }
Interface_API UINT8 ReadSwitch(UINT8 Num) { return g_Core ? g_Core->GetSwitch(Num) : 0; }

Interface_API void SetTestSwitchState(UINT8 State) { EnsureCore()->SetSwitch(255U, State); }

// MPU5 cabinet inputs from the MFME configuration page.
Interface_API void SetServiceDoorState(UINT8 State) { EnsureCore()->SetServiceDoorState(State); }
Interface_API void SetMainDoorState(UINT8 State) { EnsureCore()->SetMainDoorState(State); }
Interface_API void SetRefillKeyState(UINT8 State) { EnsureCore()->SetRefillKeyState(State); }
Interface_API void SetSecondaryTestSwitchState(UINT8 State) { EnsureCore()->SetSecondaryTestSwitchState(State); }
Interface_API UINT8 RequestTestMode(void) { return EnsureCore()->RequestTestMode(); }

Interface_API void SetTestButton(UINT8 Pressed) { SetTestSwitchState(Pressed); }
Interface_API void SetDIP(UINT8 Num, UINT8 Value) { EnsureCore()->SetDIP(Num, Value); }
Interface_API void SetStake(UINT8 Value) { EnsureCore()->SetStake(Value); }
Interface_API void SetPrize(UINT8 Value) { EnsureCore()->SetPrize(Value); }
Interface_API void SetPercent(UINT8 Value) { EnsureCore()->SetPercent(Value); }
Interface_API void SetCharacteriserAddress(UINT32 Address) { EnsureCore()->SetCharacteriserAddress(Address); }
Interface_API void SetLegacyPICMode(UINT8 Enable) { EnsureCore()->SetLegacyPICMode(Enable); }
Interface_API void SetPICMode(UINT8 Mode) { EnsureCore()->SetPICMode(Mode); }
Interface_API void SetSECFitted(UINT8 Fitted) { EnsureCore()->SetSECFitted(Fitted); }
Interface_API void SetHopperType(UINT8 Type) { EnsureCore()->SetHopperType(Type); }
Interface_API void SetHopperEnable(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperEnable(Num, Value); }
Interface_API void SetHopperCoinsIn(UINT8 Num, UINT32 Value) { EnsureCore()->SetHopperCoinsIn(Num, Value); }
Interface_API void SetHopperCoinsOut(UINT8 Num, UINT32 Value) { EnsureCore()->SetHopperCoinsOut(Num, Value); }
Interface_API void SetHopperOptoEnable(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperOptoEnable(Num, Value); }
Interface_API void SetHopperOptoReturn(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperOptoReturn(Num, Value); }
Interface_API void SetHopperMotorEnable(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperMotorEnable(Num, Value); }
Interface_API void SetHopperCoin(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperCoin(Num, Value); }
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
Interface_API void SetHopperLoIndicator(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperLoIndicator(Num, Value); }
Interface_API void SetHopperHiIndicator(UINT8 Num, UINT8 Value) { EnsureCore()->SetHopperHiIndicator(Num, Value); }
Interface_API void SetHopperCoinsRefilled(UINT8 Num, UINT32 Value) { EnsureCore()->SetHopperCoinsRefilled(Num, Value); }
Interface_API UINT8 GetHopperEnable(UINT8 Num) { return g_Core ? g_Core->GetHopperEnable(Num) : 0U; }
Interface_API UINT32 GetHopperCoinsIn(UINT8 Num) { return g_Core ? g_Core->GetHopperCoinsIn(Num) : 0U; }
Interface_API UINT32 GetHopperCoinsOut(UINT8 Num) { return g_Core ? g_Core->GetHopperCoinsOut(Num) : 0U; }
Interface_API UINT8 GetHopperOptoEnable(UINT8 Num) { return g_Core ? g_Core->GetHopperOptoEnable(Num) : 0U; }
Interface_API UINT8 GetHopperOptoReturn(UINT8 Num) { return g_Core ? g_Core->GetHopperOptoReturn(Num) : 0U; }
Interface_API UINT8 GetHopperMotorEnable(UINT8 Num) { return g_Core ? g_Core->GetHopperMotorEnable(Num) : 0U; }
Interface_API UINT8 GetHopperCoin(UINT8 Num) { return g_Core ? g_Core->GetHopperCoin(Num) : 0U; }
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
Interface_API UINT8 GetHopperLoIndicator(UINT8 Num) { return g_Core ? g_Core->GetHopperLoIndicator(Num) : 0U; }
Interface_API UINT8 GetHopperHiIndicator(UINT8 Num) { return g_Core ? g_Core->GetHopperHiIndicator(Num) : 0U; }
Interface_API UINT32 GetHopperCoinsRefilled(UINT8 Num) { return g_Core ? g_Core->GetHopperCoinsRefilled(Num) : 0U; }
Interface_API void SetSerialHopperRecoveryState(UINT8 Paid, UINT8 Unpaid)
{
    EnsureCore()->SetSerialHopperRecoveryState(Paid, Unpaid);
}
Interface_API void SetLampBroken(UINT16 Lamp, UINT8 Broken) { EnsureCore()->SetLampBroken(Lamp, Broken); }
Interface_API void SetOptoInvert(UINT8 ReelNum, UINT8 Value) { EnsureCore()->SetOptoInvert(ReelNum, Value); }
Interface_API void SetOptoStart(UINT8 ReelNum, UINT8 Value) { EnsureCore()->SetOptoStart(ReelNum, Value); }
Interface_API void SetOptoEnd(UINT8 ReelNum, UINT8 Value) { EnsureCore()->SetOptoEnd(ReelNum, Value); }
Interface_API void SetSteps(UINT8 ReelNum, UINT8 Value) { EnsureCore()->SetSteps(ReelNum, Value); }
Interface_API void SetReelJumperProfile(UINT8 Controller, UINT8 Profile)
{
    EnsureCore()->SetReelJumperProfile(Controller, Profile);
}

Interface_API UINT8 CoinIn(UINT8 MechNum, UINT8 Coin, UINT8 CoinValue)
{
    return EnsureCore()->CoinIn(MechNum, Coin, CoinValue);
}
Interface_API UINT8 CanAcceptCoin(UINT8 MechNum, UINT8 Coin, UINT8 CoinValue)
{
    return g_Core ? g_Core->CanAcceptCoin(MechNum, Coin, CoinValue) : 0U;
}
Interface_API UINT8 GetCoinLockoutState(void)
{
    return g_Core ? g_Core->GetCoinLockoutState() : 0x3FU;
}
Interface_API UINT8 MarsCoinIn(UINT8 Coin, UINT8 CoinValue) { return CoinIn(0, Coin, CoinValue); }
Interface_API UINT8 S10CoinIn(UINT8 MechNum, UINT8 CoinValue) { return CoinIn(MechNum, 0, CoinValue); }
Interface_API void SetCommStyle(UINT8 Style) { EnsureCore()->SetCommStyle(Style); }
Interface_API void SetCommInvert(UINT8 Invert) { EnsureCore()->SetCommInvert(Invert); }
Interface_API void SetCycles(UINT32 Cycles) { EnsureCore()->SetCoinCycles(Cycles); }
Interface_API void SetEDCEnable(UINT8 Enable) { EnsureCore()->SetEDCEnable(Enable); }
Interface_API UINT8* GetEDCString(void) { return g_Core ? g_Core->GetEDCString() : nullptr; }
Interface_API UINT32 PopEDCMessage(char* Output, UINT32 OutputSize)
{
    return g_Core ? g_Core->PopEDCMessage(Output, OutputSize) : 0U;
}
Interface_API void SetLockoutVal(UINT8 Coin, UINT8 Value) { EnsureCore()->SetLockoutVal(Coin, Value); }
Interface_API void SetLockoutInvert(UINT8 Coin, UINT8 Invert) { EnsureCore()->SetLockoutInvert(Coin, Invert); }
Interface_API void SetCoinValue(UINT8 Coin, UINT8 Value) { EnsureCore()->SetCoinValue(Coin, Value); }
Interface_API void SetCoinEnable(UINT8 Coin, UINT8 Enable) { EnsureCore()->SetCoinEnable(Coin, Enable); }
Interface_API void SetMarsCommStyle(UINT8 Style) { SetCommStyle(Style); }
Interface_API void SetMarsCommInvert(UINT8 Invert) { SetCommInvert(Invert); }
Interface_API void SetMarsCycles(UINT32 Cycles) { SetCycles(Cycles); }
Interface_API void SetMarsLockoutVal(UINT8 Coin, UINT8 Value) { SetLockoutVal(Coin, Value); }
Interface_API void SetMarsLockoutInvert(UINT8 Coin, UINT8 Invert) { SetLockoutInvert(Coin, Invert); }
Interface_API void SetMarsCoinEnable(UINT8 Coin, UINT8 Enable) { SetCoinEnable(Coin, Enable); }

Interface_API void SaveRAM(UINT8* FileString) { if (FileString) EnsureCore()->SaveRAM(ToChar(FileString)); }
Interface_API void LoadRAM(UINT8* FileString) { if (FileString) EnsureCore()->LoadRAM(ToChar(FileString)); }
Interface_API void SetCFolder(UINT8* Folder) { EnsureCore()->SetCFolder(ToChar(Folder)); }
Interface_API void SetCFileName(UINT8* FileName) { EnsureCore()->SetCFileName(ToChar(FileName)); }
Interface_API void SaveState(void) { if (g_Core) g_Core->SaveState(); }
Interface_API void LoadState(void) { if (g_Core) g_Core->LoadState(); }

Interface_API void UpdateLamps(void) {}
Interface_API UINT8 GetLampsOn(UINT16 Num) { return g_Core ? g_Core->GetLampOn(Num) : 0; }
Interface_API UINT8 GetLampOn(UINT16 Num) { return GetLampsOn(Num); }
Interface_API float GetLampBrightness(UINT16 Num) { return g_Core ? g_Core->GetLampBrightness(Num) : 0.0f; }
Interface_API float GetFilamentColourR(UINT16 Num) { return g_Core ? g_Core->GetLampColour(Num).x : 0.0f; }
Interface_API float GetFilamentColourG(UINT16 Num) { return g_Core ? g_Core->GetLampColour(Num).y : 0.0f; }
Interface_API float GetFilamentColourB(UINT16 Num) { return g_Core ? g_Core->GetLampColour(Num).z : 0.0f; }
Interface_API float GetLampFilamentTemperatureK(UINT16 Num) { return g_Core ? g_Core->GetLampTemperatureK(Num) : 0.0f; }
Interface_API float GetLampFilamentResistanceOhms(UINT16 Num) { return g_Core ? g_Core->GetLampResistanceOhms(Num) : 0.0f; }
Interface_API float GetLampFilamentPowerW(UINT16 Num) { return g_Core ? g_Core->GetLampElectricalPowerW(Num) : 0.0f; }
Interface_API float GetLampDuty(UINT16 Num) { return g_Core ? g_Core->GetLampDuty(Num) : 0.0f; }
Interface_API float GetLampVoltageRMS(UINT16 Num) { return g_Core ? g_Core->GetLampVoltageRMS(Num) : 0.0f; }
Interface_API INT16 GetPosOut(UINT8 Num) { return g_Core ? g_Core->GetReelPosition(Num) : 0; }
Interface_API UINT8 GetReelLamp(UINT8 Num) { return g_Core ? g_Core->GetReelLamp(Num) : 0; }
Interface_API UINT16 GetAlphaSegments(UINT8 Num) { return g_Core ? g_Core->GetAlphaSegments(Num) : 0; }
Interface_API UINT8 GetAlphaDotComma(UINT8 Num) { return g_Core ? g_Core->GetAlphaDotComma(Num) : 0; }
Interface_API UINT8 GetAlphaBright(void) { return g_Core ? g_Core->GetAlphaBrightness() : 0; }
Interface_API UINT8 GetAlphaSegmentCount(void) { return PA2_ALPHA_SEGMENTS_IMPACT; }
Interface_API UINT8 GetAlphaCharacterCount(void) { return PA2_NUM_ALPHA_CHARS; }
Interface_API UINT8 GetAlphaChar(UINT8 Num) { return g_Core ? g_Core->GetAlphaCharacter(Num) : 32; }
Interface_API UINT8 GetSegOn(UINT16 Num) { return g_Core ? g_Core->GetSegmentOn(Num) : 0; }
Interface_API UINT8 GetSegBright(UINT16 Num) { return g_Core ? g_Core->GetSegmentBrightness(Num) : 0; }
Interface_API void UpdateSegs(void) {}
Interface_API UINT32 GetMeterCounter(UINT8 Index) { return g_Core ? g_Core->GetMeterCounter(Index) : 0; }
Interface_API UINT8 GetStatusLED(void) { return g_Core ? g_Core->GetStatusLED() : PA2_STATUS_LED_OFF; }
