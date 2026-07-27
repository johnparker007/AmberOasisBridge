#pragma once

#include "PA2CoreInterface.h"
#include <array>

class MPU5Hoppers
{
public:
    static constexpr UINT32 Count = PA2_NUM_HOPPERS;

    // The SCH2 contains EEPROM-backed payout information which survives a
    // power interruption.  Project Amber stores this beside the MPU5 RAM so
    // an interrupted collect can be reconciled after the next cold boot.
    struct PersistentState
    {
        std::array<UINT32, Count> Level{};
        std::array<UINT32, Count> CoinsIn{};
        std::array<UINT32, Count> CoinsOut{};
        std::array<UINT32, Count> CoinsRefilled{};
        UINT32 SerialDispenseCount = 0U;
        UINT32 SerialLifeDispenseCount = 0U;
        UINT8 SerialLastPayoutPaid = 0U;
        UINT8 SerialLastPayoutUnpaid = 0U;
    };

    enum class Type : UINT8
    {
        Compact = 0,
        Universal = 1,
        EmpireTwin = 2,
        Serial = 3
    };

    void Reset();
    void Tick(UINT32 cycles);
    void SetMotor(UINT8 hopper, UINT8 on);
    void SetEnabled(UINT8 hopper, UINT8 enabled);
    void SetCoinsIn(UINT8 hopper, UINT32 count);
    void SetCoinsOut(UINT8 hopper, UINT32 count);
    void SetOptoEnable(UINT8 hopper, UINT8 port);
    void SetOptoReturn(UINT8 hopper, UINT8 port);
    void SetMotorEnable(UINT8 hopper, UINT8 port);
    void SetCoin(UINT8 hopper, UINT8 coin);
    void SetLevel(UINT8 hopper, UINT32 level);
    void SetFullLevel(UINT8 hopper, UINT32 level);
    void SetLowEnable(UINT8 hopper, UINT8 enabled);
    void SetLowInvert(UINT8 hopper, UINT8 inverted);
    void SetLowSwitch(UINT8 hopper, UINT8 switchNumber);
    void SetLowLevel(UINT8 hopper, UINT32 level);
    void SetHighEnable(UINT8 hopper, UINT8 enabled);
    void SetHighInvert(UINT8 hopper, UINT8 inverted);
    void SetHighSwitch(UINT8 hopper, UINT8 switchNumber);
    void SetHighLevel(UINT8 hopper, UINT32 level);
    void SetLowIndicator(UINT8 hopper, UINT8 lamp);
    void SetHighIndicator(UINT8 hopper, UINT8 lamp);
    void SetCoinsRefilled(UINT8 hopper, UINT32 count);
    void SetType(UINT8 type);
    void SetSerialCoinMechEnabled(UINT8 enabled);

    // Return the MPU5 ASIC hopper input pins using the DUART output-port
    // polarity controls. This follows MFME's Compact/Universal/Empire wiring.
    UINT8 ReadPins(UINT8 duartOutputs) const;
    bool IsSerial() const { return Type_ == Type::Serial; }
    bool IsSerialBusEnabled() const { return IsSerial() || SerialCoinMechEnabled_; }
    void WriteSerialByte(UINT8 value);
    bool SerialReplyReady(UINT8& value) const;
    void ConsumeSerialReplyByte();
    UINT8 ReadOpto(UINT8 hopper) const;
    UINT8 CoinIn(UINT8 coinCode);
    UINT8 SerialCoinIn(UINT8 coin, UINT8 coinValue);
    bool CanAcceptSerialCoin(UINT8 coin, UINT8 coinValue) const;

    UINT8 GetEnabled(UINT8 hopper) const;
    UINT8 GetOptoEnable(UINT8 hopper) const;
    UINT8 GetOptoReturn(UINT8 hopper) const;
    UINT8 GetMotorEnable(UINT8 hopper) const;
    UINT8 GetCoin(UINT8 hopper) const;
    UINT8 GetLowEnable(UINT8 hopper) const;
    UINT8 GetLowInvert(UINT8 hopper) const;
    UINT8 GetLowSwitch(UINT8 hopper) const;
    UINT8 GetHighEnable(UINT8 hopper) const;
    UINT8 GetHighInvert(UINT8 hopper) const;
    UINT8 GetHighSwitch(UINT8 hopper) const;
    UINT8 GetLowIndicator(UINT8 hopper) const;
    UINT8 GetHighIndicator(UINT8 hopper) const;
    UINT32 GetLevel(UINT8 hopper) const;
    UINT32 GetFullLevel(UINT8 hopper) const;
    UINT32 GetLowLevel(UINT8 hopper) const;
    UINT32 GetHighLevel(UINT8 hopper) const;
    UINT32 GetCoinsIn(UINT8 hopper) const;
    UINT32 GetCoinsOut(UINT8 hopper) const;
    UINT32 GetCoinsRefilled(UINT8 hopper) const;
    PersistentState GetPersistentState() const;
    // Version-1 PA2HOP01 state was written by builds which accidentally ran
    // the parallel hopper motor model alongside the serial SCH2.  Pass true
    // when loading that legacy trailer so the phantom level/counter loss can
    // be repaired once the hopper type is known.
    void SetPersistentState(const PersistentState& state, bool legacySerialState = false);
    void SetSerialRecoveryState(UINT8 paid, UINT8 unpaid);

private:
    static constexpr UINT32 UpdatePeriodCycles = 1000U;
    static constexpr UINT32 StartDelayTicks = 400U;
    static constexpr UINT32 PrimaryOptoPeriodTicks = 700U;
    static constexpr UINT32 SecondaryOptoPeriodTicks = 5U;
    static constexpr UINT32 CoinOffPeriodTicks = 15U;
    static constexpr UINT32 SerialMultiCoinPeriodCycles = 1800000U;
    static constexpr UINT32 SerialSingleCoinPeriodCycles = 8000000U;
    static constexpr UINT32 SerialTimeoutUnitCycles = 1600000U;

    struct Hopper
    {
        UINT8 Enabled = 1;
        UINT8 Motor = 0;
        UINT8 Opto = 0;
        UINT8 Opto2 = 0;
        UINT8 State = 0;
        UINT8 Coin = 0;
        // Generic Project Amber edit-page configuration. These are layout
        // properties, not additional physical MPU5 wiring guesses.
        UINT8 OptoEnable = 0;
        UINT8 OptoReturn = 0;
        UINT8 MotorEnable = 0;
        UINT8 LowEnable = 0;
        UINT8 LowInvert = 0;
        UINT8 LowSwitch = 0;
        UINT8 LowIndicator = 0;
        UINT8 HighEnable = 0;
        UINT8 HighInvert = 0;
        UINT8 HighSwitch = 0;
        UINT8 HighIndicator = 0;
        UINT32 Level = 500;
        UINT32 FullLevel = 500;
        UINT32 LowLevel = 10;
        UINT32 HighLevel = 450;
        UINT32 CoinsIn = 0;
        UINT32 CoinsOut = 0;
        UINT32 CoinsRefilled = 0;
        UINT32 TimerTicks = 0;
        UINT32 CycleRemainder = 0;
    };

    static UINT8 ApplyPolarity(UINT8 signal, UINT8 polarity);
    void UpdateHopper(Hopper& hopper);
    void ResetSerialTransport();
    void PowerCycleSerialHopper();
    void SoftwareResetSerialHopper();
    void FinishSerialPayout(bool timedOut);
    void TickSerialPayout(UINT32 cycles);
    void AdvanceSerialEventCounter();
    void ClearParallelDriveState();
    void RepairLegacySerialPersistentState();

    void ProcessSerialMessage();
    void QueueSerialReply(UINT8 destination, UINT8 sourceAddress, UINT8 header, const UINT8* data, UINT8 length);
    static UINT8 CoinValueToCcTalkPosition(UINT8 coin, UINT8 coinValue);

    struct SerialCoinEvent
    {
        UINT8 CreditCode = 0U;
        UINT8 SorterPath = 0U;
    };

    Type Type_ = Type::Compact;
    bool SerialCoinMechEnabled_ = false;
    std::array<Hopper, Count> Hoppers_{};
    std::array<UINT8, 64> SerialCommand_{};
    UINT8 SerialCommandLength_ = 0;
    UINT8 SerialCommandExpected_ = 0;
    std::array<UINT8, 128> SerialReply_{};
    UINT32 SerialReplyRead_ = 0;
    UINT32 SerialReplyWrite_ = 0;
    UINT32 SerialReplyDelayCycles_ = 0;
    std::array<std::array<UINT8, 8>, 4> SerialDataBlocks_{};
    bool SerialPayoutEnabled_ = false;
    bool SerialPowerUpDetected_ = true;
    UINT8 SerialCurrentLimit_ = 0x22;
    UINT8 SerialMotorStopDelay_ = 0x00;
    UINT8 SerialPayoutTimeout_ = 0x1E;
    bool SerialSingleCoinMode_ = false;
    UINT8 SerialEventCounter_ = 0;
    UINT8 SerialCoinsRemaining_ = 0;
    UINT8 SerialCoinsPaid_ = 0;
    UINT8 SerialCoinsUnpaid_ = 0;
    UINT8 SerialHopperAddress_ = 3U;
    bool SerialPayoutActive_ = false;
    bool SerialPayoutTimedOut_ = false;
    bool SerialCipherKeyRequested_ = false;
    UINT32 SerialPayoutCycleRemainder_ = 0U;
    UINT32 SerialEmptyCycleRemainder_ = 0U;
    UINT32 SerialDispenseCount_ = 0U;
    UINT32 SerialLifeDispenseCount_ = 0U;
    UINT8 SerialLastPayoutPaid_ = 0U;
    UINT8 SerialLastPayoutUnpaid_ = 0U;
    bool LegacySerialPersistentStatePending_ = false;

    // ccTalk address 2: Barcrest DMOD serial coin-mech interface.
    bool SerialCoinMasterEnabled_ = false;
    UINT16 SerialCoinInhibitMask_ = 0U;
    UINT8 SerialCoinEventCounter_ = 0U;
    std::array<SerialCoinEvent, 5> SerialCoinEvents_{};
    UINT8 SerialCoinEventCount_ = 0U;
    UINT8 SerialCoinUnreadCount_ = 0U;
    std::array<std::array<UINT8, 4>, 16> SerialCoinSorterPaths_{};
    UINT8 SerialCoinDefaultSorterPath_ = 1U;
    UINT8 SerialCoinSorterOverride_ = 0U;
};
