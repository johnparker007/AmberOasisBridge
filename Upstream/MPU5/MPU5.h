#pragma once

#include "PA2CoreInterface.h"
#include "m68kcpu.h"
#include "Alpha.h"
#include "Lamps.h"
#include "MarsMech.h"
#include "Meters.h"
#include "Reels.h"
#include "Segments.h"
#include "Hoppers.h"
#include "SCN68681.h"
#include "MPU5ASIC.h"
#include "MPU5PIC.h"
#include "MPU5SEC.h"
#include "MPU5Sound.h"
#include "MC68340SIM.h"
#include "MC68340Timer.h"
#include "MC68340Serial.h"
#include "MC68340DMA.h"
#include "EDC.h"

#include <array>
#include <chrono>
#include <string>
#include <vector>

class MPU5 final : private mc68000, private MC68340DMABus
{
public:
    static constexpr UINT32 CPUCyclesPerSecond = 16000000U;
    static constexpr UINT32 MaximumProgramROM = 0x800000U;

    MPU5();

    bool Initialise();
    void Shutdown();
    bool Reset();
    INT32 Run(UINT32 cycles);

    UINT32 LoadProgramROM(const char* name1, const char* name2, const char* name3, const char* name4);
    UINT32 LoadSoundROM(const char* name1, const char* name2, const char* name3, const char* name4);

    UINT32 GetOutputSnapshot(PA2_OutputSnapshot& out) const;
    UINT32 FillAudioFrames(INT16* outInterleavedStereo, UINT32 framesRequired);
    UINT32 GetTimingStats(PA2_MPU5TimingStats& out) const;
    void ResetTimingStats();
    void SetTimingTrace(UINT8 enable);
    void SetDUARTTrace(UINT8 enable);

    void SetSwitch(UINT8 num, UINT8 value);
    void SetServiceDoorState(UINT8 value);
    void SetMainDoorState(UINT8 value);
    void SetRefillKeyState(UINT8 value);
    void SetSecondaryTestSwitchState(UINT8 value);
    UINT8 RequestTestMode();
    UINT8 GetSwitch(UINT8 num) const;
    void SetDIP(UINT8 num, UINT8 value);
    void SetStake(UINT8 value);
    void SetPrize(UINT8 value);
    void SetPercent(UINT8 value);
    void SetCharacteriserAddress(UINT32 address);
    void SetLegacyPICMode(UINT8 enable);
    void SetPICMode(UINT8 mode);
    void SetSECFitted(UINT8 fitted);
    void SetHopperType(UINT8 type);
    void SetHopperEnable(UINT8 hopper, UINT8 enabled);
    void SetHopperCoinsIn(UINT8 hopper, UINT32 count);
    void SetHopperCoinsOut(UINT8 hopper, UINT32 count);
    void SetHopperOptoEnable(UINT8 hopper, UINT8 port);
    void SetHopperOptoReturn(UINT8 hopper, UINT8 port);
    void SetHopperMotorEnable(UINT8 hopper, UINT8 port);
    void SetHopperCoin(UINT8 hopper, UINT8 coin);
    void SetHopperLevel(UINT8 hopper, UINT32 level);
    void SetHopperFullLevel(UINT8 hopper, UINT32 level);
    void SetHopperLoEnable(UINT8 hopper, UINT8 enabled);
    void SetHopperLoInvert(UINT8 hopper, UINT8 inverted);
    void SetHopperLoSwitch(UINT8 hopper, UINT8 switchNumber);
    void SetHopperLoLevel(UINT8 hopper, UINT32 level);
    void SetHopperHiEnable(UINT8 hopper, UINT8 enabled);
    void SetHopperHiInvert(UINT8 hopper, UINT8 inverted);
    void SetHopperHiSwitch(UINT8 hopper, UINT8 switchNumber);
    void SetHopperHiLevel(UINT8 hopper, UINT32 level);
    void SetHopperLoIndicator(UINT8 hopper, UINT8 lamp);
    void SetHopperHiIndicator(UINT8 hopper, UINT8 lamp);
    void SetHopperCoinsRefilled(UINT8 hopper, UINT32 count);
    UINT8 GetHopperEnable(UINT8 hopper) const;
    UINT32 GetHopperCoinsIn(UINT8 hopper) const;
    UINT32 GetHopperCoinsOut(UINT8 hopper) const;
    UINT8 GetHopperOptoEnable(UINT8 hopper) const;
    UINT8 GetHopperOptoReturn(UINT8 hopper) const;
    UINT8 GetHopperMotorEnable(UINT8 hopper) const;
    UINT8 GetHopperCoin(UINT8 hopper) const;
    UINT32 GetHopperLevel(UINT8 hopper) const;
    UINT32 GetHopperFullLevel(UINT8 hopper) const;
    UINT8 GetHopperLoEnable(UINT8 hopper) const;
    UINT8 GetHopperLoInvert(UINT8 hopper) const;
    UINT8 GetHopperLoSwitch(UINT8 hopper) const;
    UINT32 GetHopperLoLevel(UINT8 hopper) const;
    UINT8 GetHopperHiEnable(UINT8 hopper) const;
    UINT8 GetHopperHiInvert(UINT8 hopper) const;
    UINT8 GetHopperHiSwitch(UINT8 hopper) const;
    UINT32 GetHopperHiLevel(UINT8 hopper) const;
    UINT8 GetHopperLoIndicator(UINT8 hopper) const;
    UINT8 GetHopperHiIndicator(UINT8 hopper) const;
    UINT32 GetHopperCoinsRefilled(UINT8 hopper) const;
    void SetSerialHopperRecoveryState(UINT8 paid, UINT8 unpaid);
    void SetLampBroken(UINT16 lamp, UINT8 broken);
    void SetOptoInvert(UINT8 reelNum, UINT8 value);
    void SetOptoStart(UINT8 reelNum, UINT8 value);
    void SetOptoEnd(UINT8 reelNum, UINT8 value);
    void SetSteps(UINT8 reelNum, UINT8 value);
    // profile: 0=Auto, 1=Early/legacy REEL5, 2=Late/PIC3 REEL5.
    void SetReelJumperProfile(UINT8 controller, UINT8 profile);

    UINT8 CoinIn(UINT8 mechNum, UINT8 coin, UINT8 coinValue);
    UINT8 CanAcceptCoin(UINT8 mechNum, UINT8 coin, UINT8 coinValue) const;
    UINT8 GetCoinLockoutState() const;
    void SetCommStyle(UINT8 value);
    void SetCommInvert(UINT8 value);
    void SetCoinCycles(UINT32 value);
    void SetEDCEnable(UINT8 value);
    UINT8* GetEDCString();
    UINT32 PopEDCMessage(char* output, UINT32 outputSize);
    void SetLockoutVal(UINT8 coin, UINT8 value);
    void SetLockoutInvert(UINT8 coin, UINT8 value);
    void SetCoinValue(UINT8 coin, UINT8 value);
    void SetCoinEnable(UINT8 coin, UINT8 value);

    UINT8 GetLampOn(UINT16 lamp) const;
    float GetLampBrightness(UINT16 lamp) const;
    float3 GetLampColour(UINT16 lamp) const;
    float GetLampTemperatureK(UINT16 lamp) const;
    float GetLampResistanceOhms(UINT16 lamp) const;
    float GetLampElectricalPowerW(UINT16 lamp) const;
    float GetLampDuty(UINT16 lamp) const;
    float GetLampVoltageRMS(UINT16 lamp) const;
    UINT8 GetStatusLED() const;
    INT16 GetReelPosition(UINT8 reel) const;
    UINT8 GetReelLamp(UINT8 reel) const;
    UINT16 GetAlphaSegments(UINT8 character) const;
    UINT8 GetAlphaDotComma(UINT8 character) const;
    UINT8 GetAlphaBrightness() const;
    UINT8 GetAlphaCharacter(UINT8 character) const;
    UINT8 GetSegmentOn(UINT16 display) const;
    UINT8 GetSegmentBrightness(UINT16 display) const;
    UINT32 GetMeterCounter(UINT8 meter) const;

    void SetCFolder(const char* folder);
    void SetCFileName(const char* fileName);
    bool SaveRAM(const char* fileName) const;
    bool LoadRAM(const char* fileName);
    void SaveState() const;
    void LoadState();

private:
    static bool ReadFile(const char* name, std::vector<UINT8>& data);
    static bool LoadConcatenatedFiles(std::vector<UINT8>& destination,
        const char* name1, const char* name2, const char* name3, const char* name4,
        UINT32& totalBytes);
    std::string StateFilePath() const;
    UINT8 DipBank1Byte() const;
    UINT8 DipBank2Byte() const;
    UINT8 StakeKeyCode() const;
    UINT8 PrizeKeyCode() const;
    UINT8 PercentKeyCode() const;
    void UpdatePICKeys();
    void ApplyRequestedHardwareConfiguration();
    void ApplyEffectivePICMode();
    void ApplyCabinetInputMappings();
    void ApplyReelJumperProfiles();
    void ScheduleConfigurationReset();
    void StartNextTestPulse();
    void TickTestInput(UINT32 cycles);
    void FinishAutomaticTestModeSequence();
    UINT8 BeginAutomaticTestModeSequence();
    UINT8 ReadSwitchNibble() const;
    UINT8 ReadIO(UINT32 address);
    void WriteIO(UINT32 address, UINT8 value);
    UINT8 ReadMemoryByte(UINT32 address);
    UINT16 ReadMemoryWord(UINT32 address);
    UINT32 ReadMemoryLong(UINT32 address);
    void WriteMemoryByte(UINT32 address, UINT8 value);
    void WriteMemoryWord(UINT32 address, UINT16 value);
    void WriteMemoryLong(UINT32 address, UINT32 value);
    UINT8 ReadMemoryByte(UINT32 address, UINT8 functionCode);
    UINT16 ReadMemoryWord(UINT32 address, UINT8 functionCode);
    UINT32 ReadMemoryLong(UINT32 address, UINT8 functionCode);
    UINT16 ReadProgramWordDirect(UINT32 address) const;
    UINT32 ReadProgramLongDirect(UINT32 address) const;
    UINT16 ReadRAMWordDirect(UINT32 address) const;
    UINT32 ReadRAMLongDirect(UINT32 address) const;
    void WriteRAMWordDirect(UINT32 address, UINT16 value);
    void WriteRAMLongDirect(UINT32 address, UINT32 value);
    void WriteMemoryByte(UINT32 address, UINT8 value, UINT8 functionCode);
    void WriteMemoryWord(UINT32 address, UINT16 value, UINT8 functionCode);
    void WriteMemoryLong(UINT32 address, UINT32 value, UINT8 functionCode);
    UINT8 ReadInternalByte(UINT16 offset);
    UINT16 ReadInternalWord(UINT16 offset);
    UINT32 ReadInternalLong(UINT16 offset);
    void WriteInternalByte(UINT16 offset, UINT8 value);
    void WriteInternalWord(UINT16 offset, UINT16 value);
    void WriteInternalLong(UINT16 offset, UINT32 value);
    void ApplyCPUAccessTiming(UINT32 address, UINT8 accessBytes);
    void TickDevices(UINT32 cycles);
    void ApplyDUARTOutputs();
    void ProcessDSPCommand();
    void RefreshSoundSamples();
    void ProcessInternalSerialMessages();
    void ProcessDUARTTransmit();
    void ProcessEDCByte(const MPU5DUART::TxEvent& event);
    void TraceDUARTByte(const MPU5DUART::TxEvent& event);
    void MaybeWriteTimingTrace();
    void InitialiseTimingTraceFile();
    void InitialiseDUARTTraceFile();
    void ProcessBarbusMessage(std::array<UINT8, MC68340Serial::MaximumMessageLength>& message, UINT32 length);
    void QueueBarbusReply(const UINT8* message, UINT32 length);
    void UpdateInterruptLine();
    UINT8 HighestInterruptLevel() const;
    INT32 InterruptVectorForLevel(UINT8 level);
    static UINT32 DeescapeBarbusMessage(std::array<UINT8, MC68340Serial::MaximumMessageLength>& message, UINT32 length);

    UINT8 DMARead8(UINT32 address) override;
    UINT16 DMARead16(UINT32 address) override;
    UINT32 DMARead32(UINT32 address) override;
    void DMAWrite8(UINT32 address, UINT8 value) override;
    void DMAWrite16(UINT32 address, UINT16 value) override;
    void DMAWrite32(UINT32 address, UINT32 value) override;

    int __fastcall cpu_irq_ack(int level) override;
    void __fastcall cpu_set_fc(int functionCode) override;
    void __fastcall cpu_inst_hook(int cycles) override;
    void __fastcall cpu_pulse_reset() override;
    UINT8 __fastcall cpu_read_byte(int address) override;
    UINT16 __fastcall cpu_read_word(int address) override;
    UINT32 __fastcall cpu_read_long(int address) override;
    void __fastcall cpu_write_byte(int address, UINT8 value) override;
    void __fastcall cpu_write_word(int address, UINT16 value) override;
    void __fastcall cpu_write_long(int address, UINT32 value) override;

    bool Initialised_ = false;
    bool ProgramROMLoaded_ = false;
    UINT8 CurrentFunctionCode_ = 0;
    UINT64 TotalCycles_ = 0;
    INT64 RunCycleCarry_ = 0;
    UINT8 AssertedIRQLevel_ = 0;
    UINT32 CharacteriserHookAddress_ = 0;
    bool CharacteriserAddressOverridden_ = false;
    bool CharacteriserHookPending_ = false;
    MPU5PIC::Mode PICMode_ = MPU5PIC::Mode::LegacyMFME;
    MPU5PIC::Mode RequestedPICMode_ = MPU5PIC::Mode::LegacyMFME;
    // 0=Auto, 1=Early/legacy, 2=Late/PIC3. Auto preserves existing
    // machines while selecting the later REEL5 profile for PIC3 software.
    std::array<UINT8, 2> RequestedReelJumperProfile_{{ 0U, 0U }};
    bool PICModeExplicit_ = false;
    bool ROMUsesPIC3Protocol_ = false;
    bool ROMUsesPIC2Protocol_ = false;
    bool ROMUsesLegacyPICProtocol_ = false;
    bool DSPStatusResponsePending_ = false;
    bool ConfigurationResetPending_ = false;
    UINT8 RequestedCommStyle_ = 0;
    bool RequestedEDCEnabled_ = false;
    bool SECFitted_ = true;
    bool EDCEnabled_ = false;

    // MUX5 lamp-test/current-sense transaction state. The default model treats
    // every fitted bulb as serviceable and returns the expected short pulse.
    std::array<bool, 4> LampCurrentTestActive_{};
    std::array<bool, 4> LampCurrentTestSawFullDrive_{};
    std::array<bool, 4> LampCurrentSensePulseIssued_{};
    std::array<UINT8, 4> LampCurrentSensePulseReplies_{};

    PA2_MPU5TimingStats TimingStats_{};
    bool TimingTraceEnabled_ = true;
    bool TimingTraceFileInitialised_ = false;
    bool DUARTTraceEnabled_ = false;
    bool DUARTTraceFileInitialised_ = false;
    std::chrono::steady_clock::time_point TimingTraceStart_{};
    std::chrono::steady_clock::time_point TimingTraceLastWrite_{};
    UINT64 TimingTracePreviousRequestedCycles_ = 0;
    UINT64 TimingTracePreviousExecutedCycles_ = 0;
    UINT64 TimingTracePreviousBusPenaltyCycles_ = 0;

    std::array<UINT8, 0x10000> RAM_{};
    std::array<UINT8, 0x1000> InternalRegisters_{};
    std::vector<UINT8> ProgramROM_;
    std::vector<UINT8> SoundROM_;
    std::array<UINT8, 256> Switches_{};
    static constexpr UINT32 TestPulseHighCycles = CPUCyclesPerSecond * 160U / 1000U;
    static constexpr UINT32 TestPulseGapCycles = CPUCyclesPerSecond * 100U / 1000U;
    UINT32 TestPulseCyclesRemaining_ = 0;
    UINT8 TestPulseQueue_ = 0;
    UINT8 TestSwitchRequested_ = 0;
    bool TestPulseHigh_ = false;
    bool TestPulseGap_ = false;
    UINT8 ServiceDoorState_ = 0U;
    UINT8 MainDoorState_ = 0U;
    UINT8 RefillKeyState_ = 0U;
    UINT8 SecondaryTestSwitchState_ = 0U;
    bool AutomaticTestModeSequence_ = false;
    std::array<UINT8, PA2_NUM_DIPS> Dips_{};

    UINT8 Stake_ = 0;
    UINT8 Prize_ = 0;
    UINT8 Percent_ = 0;

    MC68340SIM SIM_;
    MC68340Timer Timer_;
    MC68340Serial Serial_;
    MC68340DMA DMA_;
    MPU5ASIC ASIC_;
    MPU5PIC PIC_;
    MPU5SEC SEC_;
    MPU5Sound Sound_;
    MPU5DUART DUART_;
    EDCUNIT EDC_;
    std::array<MPU5Alpha, 3> Alpha_{};
    MPU5Lamps Lamps_;
    MPU5MarsMech Mars_;
    MPU5Meters Meters_;
    MPU5Reels Reels_;
    MPU5Segments Segments_;
    MPU5Hoppers Hoppers_;

    std::string CFolder_;
    std::string CFileName_;
};
