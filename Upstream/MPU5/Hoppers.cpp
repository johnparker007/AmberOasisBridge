#include "Hoppers.h"

void MPU5Hoppers::Reset()
{
    ResetSerialTransport();
    PowerCycleSerialHopper();
    SerialCoinMasterEnabled_ = false;
    SerialCoinInhibitMask_ = 0U;
    SerialCoinEventCounter_ = 0U;
    SerialCoinEvents_.fill(SerialCoinEvent{});
    SerialCoinEventCount_ = 0U;
    SerialCoinUnreadCount_ = 0U;
    SerialCoinDefaultSorterPath_ = 1U;
    SerialCoinSorterOverride_ = 0U;
    for (auto& paths : SerialCoinSorterPaths_)
        paths = { 1U, 4U, 4U, 4U };
    for (Hopper& hopper : Hoppers_)
    {
        hopper.Motor = 0;
        hopper.Opto = 0;
        hopper.Opto2 = 0;
        hopper.State = 0;
        hopper.TimerTicks = 0;
        hopper.CycleRemainder = 0;
    }
}

void MPU5Hoppers::ResetSerialTransport()
{
    SerialCommandLength_ = 0U;
    SerialCommandExpected_ = 0U;
    SerialReplyRead_ = 0U;
    SerialReplyWrite_ = 0U;
    SerialReplyDelayCycles_ = 0U;
}

void MPU5Hoppers::ClearParallelDriveState()
{
    for (Hopper& hopper : Hoppers_)
    {
        hopper.Motor = 0U;
        hopper.Opto = 0U;
        hopper.Opto2 = 0U;
        hopper.State = 0U;
        hopper.TimerTicks = 0U;
        hopper.CycleRemainder = 0U;
    }
}

void MPU5Hoppers::RepairLegacySerialPersistentState()
{
    if (!LegacySerialPersistentStatePending_ || Type_ != Type::Serial) return;
    LegacySerialPersistentStatePending_ = false;

    // Builds which wrote PA2HOP01 version 1 still allowed DUART payout outputs
    // to run the old parallel opto state machine while a serial SCH2 was
    // selected.  Every phantom parallel pulse decremented Level and incremented
    // CoinsOut, but did not increment the SCH2 life-dispense counter.  The
    // difference is therefore an exact migration signal for affected saves.
    Hopper& hopper = Hoppers_[0];
    const UINT32 genuineSerialCoins = SerialLifeDispenseCount_;
    if (hopper.CoinsOut <= genuineSerialCoins) return;

    const UINT32 phantomCoins = hopper.CoinsOut - genuineSerialCoins;
    const UINT64 restoredLevel = static_cast<UINT64>(hopper.Level) + phantomCoins;
    hopper.Level = static_cast<UINT32>(restoredLevel > hopper.FullLevel
        ? hopper.FullLevel : restoredLevel);
    hopper.CoinsOut = genuineSerialCoins;
}

void MPU5Hoppers::FinishSerialPayout(bool timedOut)
{
    if (timedOut && SerialCoinsRemaining_ != 0U)
    {
        const UINT16 unpaid = static_cast<UINT16>(SerialCoinsUnpaid_) +
            static_cast<UINT16>(SerialCoinsRemaining_);
        SerialCoinsUnpaid_ = static_cast<UINT8>(unpaid > 255U ? 255U : unpaid);
        SerialCoinsRemaining_ = 0U;
    }

    SerialPayoutActive_ = false;
    SerialPayoutTimedOut_ = timedOut;
    SerialPayoutCycleRemainder_ = 0U;
    SerialEmptyCycleRemainder_ = 0U;
    SerialLastPayoutPaid_ = SerialCoinsPaid_;
    SerialLastPayoutUnpaid_ = SerialCoinsUnpaid_;
}

void MPU5Hoppers::PowerCycleSerialHopper()
{
    // If the host saves or resets while a payout is active, the physical SCH2
    // writes the residual coins as unpaid before the next power-up report.
    if (SerialPayoutActive_ && SerialCoinsRemaining_ != 0U)
        FinishSerialPayout(true);

    SerialPayoutEnabled_ = false;
    SerialPowerUpDetected_ = true;
    SerialCurrentLimit_ = 0x22U;
    SerialMotorStopDelay_ = 0x00U;
    SerialPayoutTimeout_ = 0x1EU;
    SerialSingleCoinMode_ = false;
    SerialEventCounter_ = 0U;
    SerialCoinsRemaining_ = 0U;
    SerialCoinsPaid_ = SerialLastPayoutPaid_;
    SerialCoinsUnpaid_ = SerialLastPayoutUnpaid_;
    SerialHopperAddress_ = 3U;
    SerialPayoutActive_ = false;
    SerialPayoutTimedOut_ = false;
    SerialCipherKeyRequested_ = false;
    SerialPayoutCycleRemainder_ = 0U;
    SerialEmptyCycleRemainder_ = 0U;
}

void MPU5Hoppers::SoftwareResetSerialHopper()
{
    if (SerialPayoutActive_ && SerialCoinsRemaining_ != 0U)
        FinishSerialPayout(true);

    SerialPayoutEnabled_ = false;
    SerialPowerUpDetected_ = false;
    SerialCurrentLimit_ = 0x22U;
    SerialMotorStopDelay_ = 0x00U;
    SerialPayoutTimeout_ = 0x1EU;
    SerialSingleCoinMode_ = false;
    SerialEventCounter_ = 0U;
    SerialCoinsRemaining_ = 0U;
    SerialCoinsPaid_ = SerialLastPayoutPaid_;
    SerialCoinsUnpaid_ = SerialLastPayoutUnpaid_;
    SerialPayoutActive_ = false;
    SerialPayoutTimedOut_ = false;
    SerialCipherKeyRequested_ = false;
    SerialPayoutCycleRemainder_ = 0U;
    SerialEmptyCycleRemainder_ = 0U;
}

void MPU5Hoppers::AdvanceSerialEventCounter()
{
    ++SerialEventCounter_;
    if (SerialEventCounter_ == 0U) SerialEventCounter_ = 1U;
}

void MPU5Hoppers::TickSerialPayout(UINT32 cycles)
{
    if (!SerialPayoutActive_ || SerialCoinsRemaining_ == 0U) return;

    Hopper& hopper = Hoppers_[0];
    if (hopper.Enabled == 0U || hopper.Level == 0U)
    {
        SerialEmptyCycleRemainder_ += cycles;
        const UINT32 timeoutUnits = SerialPayoutTimeout_ == 0U ? 1U : SerialPayoutTimeout_;
        const UINT64 timeoutCycles = static_cast<UINT64>(timeoutUnits) * SerialTimeoutUnitCycles;
        if (static_cast<UINT64>(SerialEmptyCycleRemainder_) >= timeoutCycles)
            FinishSerialPayout(true);
        return;
    }

    SerialEmptyCycleRemainder_ = 0U;
    SerialPayoutCycleRemainder_ += cycles;
    const UINT32 period = SerialSingleCoinMode_ ?
        SerialSingleCoinPeriodCycles : SerialMultiCoinPeriodCycles;

    while (SerialPayoutActive_ && SerialCoinsRemaining_ != 0U &&
        SerialPayoutCycleRemainder_ >= period)
    {
        SerialPayoutCycleRemainder_ -= period;
        if (hopper.Level == 0U) break;

        --hopper.Level;
        ++hopper.CoinsOut;
        ++SerialCoinsPaid_;
        --SerialCoinsRemaining_;
        ++SerialDispenseCount_;
        ++SerialLifeDispenseCount_;

        if (SerialCoinsRemaining_ == 0U)
            FinishSerialPayout(false);
    }
}

void MPU5Hoppers::UpdateHopper(Hopper& hopper)
{
    if (hopper.TimerTicks == 0U) { return; }

    --hopper.TimerTicks;
    if (hopper.TimerTicks != 0U) { return; }

    // This is the four-state opto sequence used by MFME's working MPU5
    // Compact hopper model. A coin sequence is allowed to finish after the
    // motor is removed, just as it would mechanically.
    switch (hopper.State)
    {
    case 0:
        hopper.Opto = 1;
        hopper.TimerTicks = PrimaryOptoPeriodTicks;
        hopper.State = 1;
        if (hopper.Level > 0U) { --hopper.Level; }
        ++hopper.CoinsOut;
        break;

    case 1:
        hopper.Opto2 = 1;
        hopper.TimerTicks = SecondaryOptoPeriodTicks;
        hopper.State = 2;
        break;

    case 2:
        hopper.Opto = 0;
        hopper.TimerTicks = PrimaryOptoPeriodTicks;
        hopper.State = 3;
        break;

    default:
        hopper.Opto2 = 0;
        hopper.State = 0;
        if (hopper.Motor != 0U) { hopper.TimerTicks = CoinOffPeriodTicks; }
        break;
    }
}

void MPU5Hoppers::Tick(UINT32 cycles)
{
    if (SerialReplyDelayCycles_ != 0U)
        SerialReplyDelayCycles_ = cycles >= SerialReplyDelayCycles_ ? 0U : SerialReplyDelayCycles_ - cycles;

    if (Type_ == Type::Serial)
    {
        TickSerialPayout(cycles);
        // A serial SCH2 has no connection to the MPU5 parallel hopper motor
        // outputs.  Running the compact-hopper opto model here used to remove
        // coins continuously whenever unrelated DUART output bits changed.
        return;
    }

    for (Hopper& hopper : Hoppers_)
    {
        if (hopper.Enabled == 0U)
        {
            hopper.Motor = 0;
            hopper.Opto = 0;
            hopper.Opto2 = 0;
            hopper.State = 0;
            hopper.TimerTicks = 0;
            hopper.CycleRemainder = 0;
            continue;
        }

        hopper.CycleRemainder += cycles;
        while (hopper.CycleRemainder >= UpdatePeriodCycles)
        {
            hopper.CycleRemainder -= UpdatePeriodCycles;
            UpdateHopper(hopper);
        }
    }
}

void MPU5Hoppers::SetMotor(UINT8 hopper, UINT8 on)
{
    if (hopper >= Hoppers_.size()) { return; }
    if (Type_ == Type::Serial)
    {
        // The SCH2 motor is controlled only by ccTalk command 167.
        Hoppers_[hopper].Motor = 0U;
        Hoppers_[hopper].TimerTicks = 0U;
        Hoppers_[hopper].CycleRemainder = 0U;
        return;
    }

    Hopper& selected = Hoppers_[hopper];
    const UINT8 requested = on ? 1U : 0U;
    if (requested != 0U && selected.Motor == 0U)
    {
        selected.TimerTicks = StartDelayTicks;
        selected.State = 0;
    }
    selected.Motor = requested;
}

void MPU5Hoppers::SetEnabled(UINT8 hopper, UINT8 enabled)
{
    if (hopper < Hoppers_.size()) { Hoppers_[hopper].Enabled = enabled ? 1U : 0U; }
}

#define HSET8(name, field) \
    void MPU5Hoppers::name(UINT8 hopper, UINT8 value) \
    { \
        if (hopper < Hoppers_.size()) Hoppers_[hopper].field = value; \
    }
#define HSETBOOL(name, field) \
    void MPU5Hoppers::name(UINT8 hopper, UINT8 value) \
    { \
        if (hopper < Hoppers_.size()) Hoppers_[hopper].field = value ? 1U : 0U; \
    }
#define HSET32(name, field) \
    void MPU5Hoppers::name(UINT8 hopper, UINT32 value) \
    { \
        if (hopper < Hoppers_.size()) Hoppers_[hopper].field = value; \
    }
HSET32(SetCoinsIn, CoinsIn)
HSET32(SetCoinsOut, CoinsOut)
HSET8(SetOptoEnable, OptoEnable)
HSET8(SetOptoReturn, OptoReturn)
HSET8(SetMotorEnable, MotorEnable)
HSET8(SetCoin, Coin)
HSET32(SetLevel, Level)
HSET32(SetFullLevel, FullLevel)
HSETBOOL(SetLowEnable, LowEnable)
HSETBOOL(SetLowInvert, LowInvert)
HSET8(SetLowSwitch, LowSwitch)
HSET32(SetLowLevel, LowLevel)
HSETBOOL(SetHighEnable, HighEnable)
HSETBOOL(SetHighInvert, HighInvert)
HSET8(SetHighSwitch, HighSwitch)
HSET32(SetHighLevel, HighLevel)
HSET8(SetLowIndicator, LowIndicator)
HSET8(SetHighIndicator, HighIndicator)
HSET32(SetCoinsRefilled, CoinsRefilled)
#undef HSET8
#undef HSETBOOL
#undef HSET32

void MPU5Hoppers::SetType(UINT8 type)
{
    switch (type)
    {
    case static_cast<UINT8>(Type::Universal):
        Type_ = Type::Universal;
        break;
    case static_cast<UINT8>(Type::EmpireTwin):
        Type_ = Type::EmpireTwin;
        break;
    case static_cast<UINT8>(Type::Serial):
        Type_ = Type::Serial;
        break;
    default:
        Type_ = Type::Compact;
        break;
    }

    if (Type_ == Type::Serial)
    {
        ClearParallelDriveState();
        RepairLegacySerialPersistentState();
    }
}

void MPU5Hoppers::SetSerialCoinMechEnabled(UINT8 enabled)
{
    SerialCoinMechEnabled_ = enabled != 0U;
}

UINT8 MPU5Hoppers::ApplyPolarity(UINT8 signal, UINT8 polarity)
{
    return static_cast<UINT8>((signal ^ (polarity ? 1U : 0U)) & 1U);
}

UINT8 MPU5Hoppers::ReadPins(UINT8 duartOutputs) const
{
    const Hopper& first = Hoppers_[0];

    switch (Type_)
    {
    case Type::Serial:
        // No parallel hopper/opto inputs are connected. The ASIC inputs are
        // pulled high and therefore read as inactive.
        return 0x0FU;

    case Type::Universal:
        // MFME: 1 - optoread2(1,0) + 2 - optoread(2,0)
        return static_cast<UINT8>((first.Opto2 ? 0U : 1U) |
            (first.Opto ? 0U : 2U));

    case Type::EmpireTwin:
        {
            const Hopper& second = Hoppers_[1];
            const UINT8 firstPin = ApplyPolarity(first.Opto,
                static_cast<UINT8>((duartOutputs >> 1U) & 1U));
            const UINT8 secondPin = ApplyPolarity(second.Opto,
                static_cast<UINT8>((duartOutputs >> 3U) & 1U));
            return static_cast<UINT8>(firstPin | (secondPin << 2U));
        }

    case Type::Compact:
    default:
        return ApplyPolarity(first.Opto,
            static_cast<UINT8>((duartOutputs >> 1U) & 1U));
    }
}

UINT8 MPU5Hoppers::ReadOpto(UINT8 hopper) const
{
    return hopper < Hoppers_.size() ? Hoppers_[hopper].Opto : 0U;
}

UINT8 MPU5Hoppers::CoinValueToCcTalkPosition(UINT8 coin, UINT8 coinValue)
{
    // The DMOD profile reports its six UK coin positions in descending value:
    // 1=GBP2, 2=GBP1, 3=50p, 4=20p, 5=10p, 6=5p. Project Amber's
    // configured values use 1..6 for 5p..GBP2 and 7..12 for matching tokens.
    switch (coinValue)
    {
    case 6U: case 12U: return 1U;
    case 5U: case 11U: return 2U;
    case 4U: case 10U: return 3U;
    case 3U: case 9U:  return 4U;
    case 2U: case 8U:  return 5U;
    case 1U: case 7U:  return 6U;
    default:
        // Compatibility for layouts whose six channels are ordered 5p..GBP2
        // but which do not yet have explicit denomination values saved.
        return coin < 6U ? static_cast<UINT8>(6U - coin) : 0U;
    }
}

UINT8 MPU5Hoppers::SerialCoinIn(UINT8 coin, UINT8 coinValue)
{
    if (!CanAcceptSerialCoin(coin, coinValue)) return 0U;

    const UINT8 position = CoinValueToCcTalkPosition(coin, coinValue);

    UINT8 sorterPath = SerialCoinDefaultSorterPath_;
    if (SerialCoinSorterOverride_ != 0U)
        sorterPath = SerialCoinSorterOverride_;
    else if (SerialCoinSorterPaths_[position - 1U][0] != 0U)
        sorterPath = SerialCoinSorterPaths_[position - 1U][0];

    for (std::size_t index = SerialCoinEvents_.size() - 1U; index != 0U; --index)
        SerialCoinEvents_[index] = SerialCoinEvents_[index - 1U];
    SerialCoinEvents_[0] = { position, sorterPath };
    if (SerialCoinEventCount_ < SerialCoinEvents_.size()) ++SerialCoinEventCount_;
    if (SerialCoinUnreadCount_ < SerialCoinEvents_.size()) ++SerialCoinUnreadCount_;
    ++SerialCoinEventCounter_;
    return 1U;
}

bool MPU5Hoppers::CanAcceptSerialCoin(UINT8 coin, UINT8 coinValue) const
{
    if (!SerialCoinMechEnabled_ || !SerialCoinMasterEnabled_ ||
        SerialCoinUnreadCount_ >= SerialCoinEvents_.size())
    {
        return false;
    }

    const UINT8 position = CoinValueToCcTalkPosition(coin, coinValue);
    if (position == 0U || position > SerialCoinSorterPaths_.size()) return false;

    const UINT16 inhibitBit = static_cast<UINT16>(1U << (position - 1U));
    return (SerialCoinInhibitMask_ & inhibitBit) != 0U;
}

UINT8 MPU5Hoppers::CoinIn(UINT8 coinCode)
{
    for (Hopper& hopper : Hoppers_)
    {
        if (hopper.Enabled != 0U && hopper.Coin == coinCode && hopper.Level < hopper.FullLevel)
        {
            ++hopper.Level;
            ++hopper.CoinsIn;
            return 1U;
        }
    }
    return 0U;
}

#define HGET8(name, field) UINT8 MPU5Hoppers::name(UINT8 hopper) const { return hopper < Hoppers_.size() ? Hoppers_[hopper].field : 0U; }
HGET8(GetEnabled, Enabled)
HGET8(GetOptoEnable, OptoEnable)
HGET8(GetOptoReturn, OptoReturn)
HGET8(GetMotorEnable, MotorEnable)
HGET8(GetCoin, Coin)
HGET8(GetLowEnable, LowEnable)
HGET8(GetLowInvert, LowInvert)
HGET8(GetLowSwitch, LowSwitch)
HGET8(GetHighEnable, HighEnable)
HGET8(GetHighInvert, HighInvert)
HGET8(GetHighSwitch, HighSwitch)
HGET8(GetLowIndicator, LowIndicator)
HGET8(GetHighIndicator, HighIndicator)
#undef HGET8

#define HGET(name, field) UINT32 MPU5Hoppers::name(UINT8 hopper) const { return hopper < Hoppers_.size() ? Hoppers_[hopper].field : 0U; }
HGET(GetLevel, Level)
HGET(GetFullLevel, FullLevel)
HGET(GetLowLevel, LowLevel)
HGET(GetHighLevel, HighLevel)
HGET(GetCoinsIn, CoinsIn)
HGET(GetCoinsOut, CoinsOut)
HGET(GetCoinsRefilled, CoinsRefilled)
#undef HGET

MPU5Hoppers::PersistentState MPU5Hoppers::GetPersistentState() const
{
    PersistentState state{};
    for (UINT32 index = 0U; index < Count; ++index)
    {
        state.Level[index] = Hoppers_[index].Level;
        state.CoinsIn[index] = Hoppers_[index].CoinsIn;
        state.CoinsOut[index] = Hoppers_[index].CoinsOut;
        state.CoinsRefilled[index] = Hoppers_[index].CoinsRefilled;
    }

    state.SerialDispenseCount = SerialDispenseCount_;
    state.SerialLifeDispenseCount = SerialLifeDispenseCount_;
    state.SerialLastPayoutPaid = SerialCoinsPaid_;

    const UINT16 interruptedUnpaid = static_cast<UINT16>(SerialCoinsUnpaid_) +
        static_cast<UINT16>(SerialPayoutActive_ ? SerialCoinsRemaining_ : 0U);
    state.SerialLastPayoutUnpaid = static_cast<UINT8>(
        interruptedUnpaid > 255U ? 255U : interruptedUnpaid);
    return state;
}

void MPU5Hoppers::SetPersistentState(const PersistentState& state, bool legacySerialState)
{
    for (UINT32 index = 0U; index < Count; ++index)
    {
        Hoppers_[index].Level = state.Level[index];
        Hoppers_[index].CoinsIn = state.CoinsIn[index];
        Hoppers_[index].CoinsOut = state.CoinsOut[index];
        Hoppers_[index].CoinsRefilled = state.CoinsRefilled[index];
    }

    SerialDispenseCount_ = state.SerialDispenseCount;
    SerialLifeDispenseCount_ = state.SerialLifeDispenseCount;
    SerialLastPayoutPaid_ = state.SerialLastPayoutPaid;
    SerialLastPayoutUnpaid_ = state.SerialLastPayoutUnpaid;
    LegacySerialPersistentStatePending_ = legacySerialState;
    RepairLegacySerialPersistentState();
    PowerCycleSerialHopper();
}

void MPU5Hoppers::SetSerialRecoveryState(UINT8 paid, UINT8 unpaid)
{
    SerialLastPayoutPaid_ = paid;
    SerialLastPayoutUnpaid_ = unpaid;
    SerialCoinsPaid_ = paid;
    SerialCoinsUnpaid_ = unpaid;
    SerialCoinsRemaining_ = 0U;
    SerialPayoutActive_ = false;
    SerialPayoutTimedOut_ = unpaid != 0U;
}


void MPU5Hoppers::QueueSerialReply(UINT8 destination, UINT8 sourceAddress, UINT8 header, const UINT8* data, UINT8 length)
{
    std::array<UINT8, 64> reply{};
    const UINT8 safeLength = length > 59U ? 59U : length;
    reply[0] = destination;
    reply[1] = safeLength;
    reply[2] = sourceAddress;
    reply[3] = header;
    UINT8 checksum = static_cast<UINT8>(reply[0] + reply[1] + reply[2] + reply[3]);
    for (UINT8 index = 0; index < safeLength; ++index)
    {
        reply[4U + index] = data ? data[index] : 0U;
        checksum = static_cast<UINT8>(checksum + reply[4U + index]);
    }
    reply[4U + safeLength] = static_cast<UINT8>(0U - checksum);
    const UINT8 total = static_cast<UINT8>(safeLength + 5U);
    for (UINT8 index = 0; index < total; ++index)
    {
        if (SerialReplyWrite_ - SerialReplyRead_ >= static_cast<UINT32>(SerialReply_.size())) ++SerialReplyRead_;
        SerialReply_[SerialReplyWrite_++ % SerialReply_.size()] = reply[index];
    }
    if (SerialReplyDelayCycles_ == 0U) SerialReplyDelayCycles_ = 1600U;
}

void MPU5Hoppers::ProcessSerialMessage()
{
    if (SerialCommandLength_ < 5U) return;
    UINT8 checksum = 0U;
    for (UINT8 index = 0; index < SerialCommandLength_; ++index)
        checksum = static_cast<UINT8>(checksum + SerialCommand_[index]);
    if (checksum != 0U) return;
    const UINT8 destination = SerialCommand_[0];
    const UINT8 source = SerialCommand_[2];
    const UINT8 header = SerialCommand_[3];

    // Standard ccTalk address 2 is the serial coin acceptor.  Start with the
    // universal discovery response so the game can enumerate the device and
    // reveal the exact identity/command set that it expects.
    if (destination == 2U)
    {
        switch (header)
        {
        case 254U: // Simple poll.
            QueueSerialReply(source, 2U, 0U, nullptr, 0U);
            break;
        case 246U: // Request manufacturer identification.
            {
                static constexpr UINT8 manufacturer[] =
                    { 'M','o','n','e','y',' ','C','o','n','t','r','o','l','s' };
                QueueSerialReply(source, 2U, 0U, manufacturer,
                    static_cast<UINT8>(sizeof(manufacturer)));
            }
            break;
        case 245U: // Request equipment category identification.
            {
                static constexpr UINT8 category[] =
                    { 'C','o','i','n',' ','A','c','c','e','p','t','o','r' };
                QueueSerialReply(source, 2U, 0U, category,
                    static_cast<UINT8>(sizeof(category)));
            }
            break;
        case 244U: // Request product code.
            {
                static constexpr UINT8 product[] = { 'D','M','O','D' };
                QueueSerialReply(source, 2U, 0U, product,
                    static_cast<UINT8>(sizeof(product)));
            }
            break;
        case 241U: // Request software revision.
            {
                static constexpr UINT8 revision[] = { '9','.','9','9' };
                QueueSerialReply(source, 2U, 0U, revision,
                    static_cast<UINT8>(sizeof(revision)));
            }
            break;
        case 192U: // Request build code.
            {
                static constexpr UINT8 build[] = { 'D','M','O','D' };
                QueueSerialReply(source, 2U, 0U, build,
                    static_cast<UINT8>(sizeof(build)));
            }
            break;
        case 4U: // Request communications revision.
            {
                static constexpr UINT8 revision[] = { 1U, 3U, 2U };
                QueueSerialReply(source, 2U, 0U, revision,
                    static_cast<UINT8>(sizeof(revision)));
            }
            break;
        case 242U: // Request serial number.
            {
                static constexpr UINT8 serial[] = { 0U, 0U, 0U };
                QueueSerialReply(source, 2U, 0U, serial,
                    static_cast<UINT8>(sizeof(serial)));
            }
            break;
        case 1U: // Reset device.
            SerialCoinMasterEnabled_ = false;
            SerialCoinInhibitMask_ = 0U;
            SerialCoinEventCounter_ = 0U;
            SerialCoinEvents_.fill(SerialCoinEvent{});
            SerialCoinEventCount_ = 0U;
            SerialCoinUnreadCount_ = 0U;
            SerialCoinDefaultSorterPath_ = 1U;
            SerialCoinSorterOverride_ = 0U;
            for (auto& paths : SerialCoinSorterPaths_)
                paths = { 1U, 4U, 4U, 4U };
            QueueSerialReply(source, 2U, 0U, nullptr, 0U);
            break;
        case 232U: // Perform self-check.
            {
                const UINT8 faultCode = 0U; // No fault.
                QueueSerialReply(source, 2U, 0U, &faultCode, 1U);
            }
            break;
        case 249U: // Request polling priority.
            {
                const UINT8 priority[2] = { 2U, 20U }; // 20 x 10 ms = 200 ms.
                QueueSerialReply(source, 2U, 0U, priority, 2U);
            }
            break;
        case 184U: // Request coin id.
            {
                static constexpr UINT8 coinIds[16][6] =
                {
                    { 'G','B','2','0','0','A' },
                    { 'G','B','1','0','0','A' },
                    { 'G','B','0','5','0','B' },
                    { 'G','B','0','2','0','A' },
                    { 'G','B','0','1','0','B' },
                    { 'G','B','0','0','5','B' },
                    { '.','.','.','.','.','.' },
                    { '.','.','.','.','.','.' },
                    { '.','.','.','.','.','.' },
                    { '.','.','.','.','.','.' },
                    { '.','.','.','.','.','.' },
                    { '.','.','.','.','.','.' },
                    { '.','.','.','.','.','.' },
                    { '.','.','.','.','.','.' },
                    { '.','.','.','.','.','.' },
                    { '.','.','.','.','.','.' }
                };
                const UINT8 position = SerialCommand_[1] >= 1U ? SerialCommand_[4] : 0U;
                if (position >= 1U && position <= 16U)
                    QueueSerialReply(source, 2U, 0U, coinIds[position - 1U], 6U);
                else
                    QueueSerialReply(source, 2U, 5U, nullptr, 0U);
            }
            break;
        case 227U: // Request master inhibit status.
            {
                const UINT8 enabled = SerialCoinMasterEnabled_ ? 1U : 0U;
                QueueSerialReply(source, 2U, 0U, &enabled, 1U);
            }
            break;
        case 228U: // Modify master inhibit status.
            if (SerialCommand_[1] == 1U)
            {
                SerialCoinMasterEnabled_ = SerialCommand_[4] != 0U;
                QueueSerialReply(source, 2U, 0U, nullptr, 0U);
            }
            else
            {
                QueueSerialReply(source, 2U, 5U, nullptr, 0U);
            }
            break;
        case 231U: // Modify inhibit status.
            if (SerialCommand_[1] >= 2U)
            {
                SerialCoinInhibitMask_ = static_cast<UINT16>(SerialCommand_[4]) |
                    (static_cast<UINT16>(SerialCommand_[5]) << 8U);
                QueueSerialReply(source, 2U, 0U, nullptr, 0U);
            }
            else
            {
                QueueSerialReply(source, 2U, 5U, nullptr, 0U);
            }
            break;
        case 230U: // Request inhibit status.
            {
                const UINT8 mask[2] =
                {
                    static_cast<UINT8>(SerialCoinInhibitMask_ & 0xFFU),
                    static_cast<UINT8>(SerialCoinInhibitMask_ >> 8U)
                };
                QueueSerialReply(source, 2U, 0U, mask, 2U);
            }
            break;
        case 229U: // Read buffered credit or error codes.
            {
                // The event counter advances whenever the front end inserts a
                // coin. The five newest credit/sorter pairs remain buffered,
                // allowing firmware polling to be slower than coin insertion.
                UINT8 events[11] = { SerialCoinEventCounter_ };
                for (UINT8 index = 0U; index < SerialCoinEvents_.size(); ++index)
                {
                    if (index >= SerialCoinEventCount_) break;
                    events[1U + (index * 2U)] = SerialCoinEvents_[index].CreditCode;
                    events[2U + (index * 2U)] = SerialCoinEvents_[index].SorterPath;
                }
                QueueSerialReply(source, 2U, 0U, events,
                    static_cast<UINT8>(sizeof(events)));
                // A buffered-credit poll acknowledges every event currently
                // waiting in the device. Retain the five-event history and
                // monotonic counter, but free the unread capacity so the
                // frontend can safely accept further coins without silently
                // overwriting an event that the game has not yet observed.
                SerialCoinUnreadCount_ = 0U;
            }
            break;
        case 209U: // Request sorter paths.
            if (SerialCommand_[1] == 1U && SerialCommand_[4] >= 1U &&
                SerialCommand_[4] <= SerialCoinSorterPaths_.size())
            {
                const auto& paths = SerialCoinSorterPaths_[SerialCommand_[4] - 1U];
                QueueSerialReply(source, 2U, 0U, paths.data(),
                    static_cast<UINT8>(paths.size()));
            }
            else
            {
                QueueSerialReply(source, 2U, 5U, nullptr, 0U);
            }
            break;
        case 210U: // Modify sorter paths.
            if ((SerialCommand_[1] == 2U || SerialCommand_[1] == 5U) &&
                SerialCommand_[4] >= 1U && SerialCommand_[4] <= SerialCoinSorterPaths_.size())
            {
                auto& paths = SerialCoinSorterPaths_[SerialCommand_[4] - 1U];
                if (SerialCommand_[1] == 2U)
                {
                    paths[0] = SerialCommand_[5];
                }
                else
                {
                    for (UINT8 index = 0; index < paths.size(); ++index)
                        paths[index] = SerialCommand_[5U + index];
                }
                QueueSerialReply(source, 2U, 0U, nullptr, 0U);
            }
            else
            {
                QueueSerialReply(source, 2U, 5U, nullptr, 0U);
            }
            break;
        case 188U: // Request default sorter path.
            QueueSerialReply(source, 2U, 0U, &SerialCoinDefaultSorterPath_, 1U);
            break;
        case 189U: // Modify default sorter path.
            if (SerialCommand_[1] == 1U)
            {
                SerialCoinDefaultSorterPath_ = SerialCommand_[4];
                QueueSerialReply(source, 2U, 0U, nullptr, 0U);
            }
            else
            {
                QueueSerialReply(source, 2U, 5U, nullptr, 0U);
            }
            break;
        case 221U: // Request sorter override status.
            QueueSerialReply(source, 2U, 0U, &SerialCoinSorterOverride_, 1U);
            break;
        case 222U: // Modify sorter override status.
            if (SerialCommand_[1] == 1U)
            {
                SerialCoinSorterOverride_ = SerialCommand_[4];
                QueueSerialReply(source, 2U, 0U, nullptr, 0U);
            }
            else
            {
                QueueSerialReply(source, 2U, 5U, nullptr, 0U);
            }
            break;
        default:
            QueueSerialReply(source, 2U, 5U, nullptr, 0U);
            break;
        }
        return;
    }

    if (Type_ != Type::Serial) return;
    if (destination != SerialHopperAddress_ && destination != 0U) return;
    switch (header)
    {
    case 251U: // Address change.
        if (SerialCommand_[1] == 1U && SerialCommand_[4] >= 3U)
        {
            // The acknowledgement is sent from the address at which the
            // command was received.  The volatile address takes effect only
            // after that reply has been queued.
            const UINT8 oldAddress = SerialHopperAddress_;
            QueueSerialReply(source, oldAddress, 0U, nullptr, 0U);
            SerialHopperAddress_ = SerialCommand_[4];
        }
        else
        {
            QueueSerialReply(source, SerialHopperAddress_, 5U, nullptr, 0U);
        }
        break;
    case 254U: // Simple poll.
        QueueSerialReply(source, SerialHopperAddress_, 0U, nullptr, 0U);
        break;
    case 246U: // Request manufacturer identification.
        {
            static constexpr UINT8 manufacturer[] = { 'M','o','n','e','y',' ','C','o','n','t','r','o','l','s' };
            QueueSerialReply(source, SerialHopperAddress_, 0U, manufacturer, static_cast<UINT8>(sizeof(manufacturer)));
        }
        break;
    case 245U: // Request equipment category identification.
        {
            static constexpr UINT8 category[] = { 'P','a','y','o','u','t' };
            QueueSerialReply(source, SerialHopperAddress_, 0U, category, static_cast<UINT8>(sizeof(category)));
        }
        break;
    case 244U: // Request product code.
        {
            static constexpr UINT8 product[] = { 'S','C','H','2' };
            QueueSerialReply(source, SerialHopperAddress_, 0U, product, static_cast<UINT8>(sizeof(product)));
        }
        break;
    case 242U: // Request serial number.
        {
            // SCH2 serial numbers are 24-bit values, least-significant byte first.
            static constexpr UINT8 serial[] = { 0x00U, 0x00U, 0x00U };
            QueueSerialReply(source, SerialHopperAddress_, 0U, serial, static_cast<UINT8>(sizeof(serial)));
        }
        break;
    case 241U: // Request software revision.
        {
            static constexpr UINT8 revision[] = { 'S','C','H','2','-','V','2','.','4' };
            QueueSerialReply(source, SerialHopperAddress_, 0U, revision, static_cast<UINT8>(sizeof(revision)));
        }
        break;
    case 217U: // Request payout high / low status.
        {
            // The configured SCH2 reports the standard build with no external
            // high/low level plates fitted. Bits 0 and 1 are therefore clear.
            const UINT8 levelStatus = 0U;
            QueueSerialReply(source, SerialHopperAddress_, 0U, &levelStatus, 1U);
        }
        break;
    case 215U: // Read data block.
        if (SerialCommand_[1] == 1U && SerialCommand_[4] < SerialDataBlocks_.size())
        {
            const auto& block = SerialDataBlocks_[SerialCommand_[4]];
            QueueSerialReply(source, SerialHopperAddress_, 0U, block.data(), static_cast<UINT8>(block.size()));
        }
        else
        {
            QueueSerialReply(source, SerialHopperAddress_, 5U, nullptr, 0U);
        }
        break;
    case 214U: // Write data block.
        // SCH2 blocks contain eight bytes. The first command byte selects the
        // block and the ACK is returned only after the write has completed.
        if (SerialCommand_[1] == 9U && SerialCommand_[4] < SerialDataBlocks_.size())
        {
            auto& block = SerialDataBlocks_[SerialCommand_[4]];
            for (UINT8 index = 0; index < block.size(); ++index)
                block[index] = SerialCommand_[5U + index];
            QueueSerialReply(source, SerialHopperAddress_, 0U, nullptr, 0U);
        }
        else
        {
            QueueSerialReply(source, SerialHopperAddress_, 5U, nullptr, 0U);
        }
        break;
    case 192U: // Request build code.
        {
            static constexpr UINT8 build[] = { 'S','t','a','n','d','a','r','d' };
            QueueSerialReply(source, SerialHopperAddress_, 0U, build, static_cast<UINT8>(sizeof(build)));
        }
        break;
    case 4U: // Request communications revision.
        {
            static constexpr UINT8 revision[] = { 1U, 3U, 2U };
            QueueSerialReply(source, SerialHopperAddress_, 0U, revision, static_cast<UINT8>(sizeof(revision)));
        }
        break;
    case 165U: // Modify variable set.
        if (SerialCommand_[1] >= 4U)
        {
            SerialCurrentLimit_ = SerialCommand_[4];
            SerialMotorStopDelay_ = SerialCommand_[5];
            SerialPayoutTimeout_ = SerialCommand_[6];
            // Once selected, SCH2 single-coin mode cannot be cleared until reset.
            if (SerialCommand_[7] == 1U) SerialSingleCoinMode_ = true;
            QueueSerialReply(source, SerialHopperAddress_, 0U, nullptr, 0U);
        }
        else
        {
            QueueSerialReply(source, SerialHopperAddress_, 5U, nullptr, 0U);
        }
        break;
    case 160U: // Request cipher key.
        {
            static constexpr UINT8 noEncryptionKey[8] =
                { 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U };
            SerialCipherKeyRequested_ = true;
            QueueSerialReply(source, SerialHopperAddress_, 0U,
                noEncryptionKey, static_cast<UINT8>(sizeof(noEncryptionKey)));
        }
        break;
    case 161U: // Pump RNG / re-key.
        SerialCipherKeyRequested_ = true;
        QueueSerialReply(source, SerialHopperAddress_, 0U, nullptr, 0U);
        break;
    case 167U: // Dispense hopper coins.
        {
            bool valid = SerialCommand_[1] == 9U && SerialCipherKeyRequested_ &&
                SerialPayoutEnabled_ && !SerialPayoutActive_;

            // Alien uses the encrypted SCH2 identity.  The emulated unit
            // validates the required key-request handshake but accepts the
            // resulting eight security bytes without attempting to recreate
            // the proprietary hardware cipher.

            const UINT8 requestedCoins = SerialCommand_[1] == 9U ? SerialCommand_[12U] : 0U;
            if (requestedCoins == 0U || (SerialSingleCoinMode_ && requestedCoins != 1U))
                valid = false;

            SerialCipherKeyRequested_ = false;
            if (!valid)
            {
                QueueSerialReply(source, SerialHopperAddress_, 5U, nullptr, 0U);
                break;
            }

            AdvanceSerialEventCounter();
            SerialCoinsRemaining_ = requestedCoins;
            SerialCoinsPaid_ = 0U;
            SerialCoinsUnpaid_ = 0U;
            SerialPayoutActive_ = true;
            SerialPayoutTimedOut_ = false;
            SerialPayoutCycleRemainder_ = 0U;
            SerialEmptyCycleRemainder_ = 0U;
            // Standard SCH2 returns the event counter for header 167.  The
            // host then polls header 166 for remaining, paid and unpaid counts.
            QueueSerialReply(source, SerialHopperAddress_, 0U,
                &SerialEventCounter_, 1U);
        }
        break;
    case 166U: // Request hopper status.
        {
            const UINT8 status[] =
            {
                SerialEventCounter_,
                SerialCoinsRemaining_,
                SerialCoinsPaid_,
                SerialCoinsUnpaid_
            };
            QueueSerialReply(source, SerialHopperAddress_, 0U, status, static_cast<UINT8>(sizeof(status)));
        }
        break;
    case 168U: // Request hopper dispense count.
        {
            const UINT8 count[3] =
            {
                static_cast<UINT8>(SerialDispenseCount_ & 0xFFU),
                static_cast<UINT8>((SerialDispenseCount_ >> 8U) & 0xFFU),
                static_cast<UINT8>((SerialDispenseCount_ >> 16U) & 0xFFU)
            };
            QueueSerialReply(source, SerialHopperAddress_, 0U, count, 3U);
        }
        break;
    case 172U: // Emergency stop.
        {
            const UINT8 residual = SerialCoinsRemaining_;
            if (SerialPayoutActive_)
                FinishSerialPayout(true);
            QueueSerialReply(source, SerialHopperAddress_, 0U, &residual, 1U);
        }
        break;
    case 171U: // Request hopper coin identification.
        {
            // Match the denomination selected on Amber's hopper edit page.
            // Token entries 7..12 use the corresponding cash denomination.
            static constexpr UINT8 coinIds[7][6] =
            {
                { 'G','B','0','0','2','B' }, // 2p
                { 'G','B','0','0','5','B' }, // 5p
                { 'G','B','0','1','0','B' }, // 10p
                { 'G','B','0','2','0','A' }, // 20p
                { 'G','B','0','5','0','B' }, // 50p
                { 'G','B','1','0','0','A' }, // GBP1
                { 'G','B','2','0','0','A' }  // GBP2
            };
            UINT8 coin = Hoppers_[0].Coin;
            if (coin >= 7U && coin <= 12U) coin = static_cast<UINT8>(coin - 6U);
            if (coin > 6U) coin = 5U;
            QueueSerialReply(source, SerialHopperAddress_, 0U, coinIds[coin], 6U);
        }
        break;
    case 169U: // Request address mode.
        {
            const UINT8 addressMode = 1U; // connector/volatile addressing supported
            QueueSerialReply(source, SerialHopperAddress_, 0U, &addressMode, 1U);
        }
        break;
    case 247U: // Request variable set.
        {
            const UINT8 variables[4] =
            {
                SerialCurrentLimit_, SerialMotorStopDelay_,
                SerialPayoutTimeout_, static_cast<UINT8>(SerialSingleCoinMode_ ? 1U : 0U)
            };
            QueueSerialReply(source, SerialHopperAddress_, 0U, variables, 4U);
        }
        break;
    case 236U: // Read opto states.
        {
            const UINT8 optos = 0U;
            QueueSerialReply(source, SerialHopperAddress_, 0U, &optos, 1U);
        }
        break;
    case 218U: // Enter PIN number.
    case 219U: // Enter new PIN number.
    case 3U:   // Clear communications status variables.
        QueueSerialReply(source, SerialHopperAddress_, 0U, nullptr, 0U);
        break;
    case 2U: // Request communications status variables.
        {
            const UINT8 status[2] = { 0U, 0U };
            QueueSerialReply(source, SerialHopperAddress_, 0U, status, 2U);
        }
        break;
    case 164U: // Enable hopper.
        if (SerialCommand_[1] == 1U)
        {
            SerialPayoutEnabled_ = SerialCommand_[4] == 0xA5U;
            QueueSerialReply(source, SerialHopperAddress_, 0U, nullptr, 0U);
        }
        else
        {
            QueueSerialReply(source, SerialHopperAddress_, 5U, nullptr, 0U);
        }
        break;
    case 163U: // Test hopper.
        {
            UINT8 status[2] =
            {
                static_cast<UINT8>((SerialPayoutEnabled_ ? 0x00U : 0x80U) |
                    (SerialPowerUpDetected_ ? 0x40U : 0x00U) |
                    (SerialPayoutTimedOut_ ? 0x02U : 0x00U)),
                static_cast<UINT8>(SerialSingleCoinMode_ ? 0x02U : 0x00U)
            };
            QueueSerialReply(source, SerialHopperAddress_, 0U, status, static_cast<UINT8>(sizeof(status)));
        }
        break;
    case 1U: // Reset device.
        // Preserve the final transmitted-byte echo and the pending reply queue.
        // Resetting the host-side transport here would drop the checksum echo.
        SoftwareResetSerialHopper();
        {
            const UINT8 oldAddress = SerialHopperAddress_;
            QueueSerialReply(source, oldAddress, 0U, nullptr, 0U);
            SerialHopperAddress_ = 3U;
        }
        break;
    default:
        // Unknown commands are rejected with a ccTalk NAK.
        QueueSerialReply(source, SerialHopperAddress_, 5U, nullptr, 0U);
        break;
    }
}

void MPU5Hoppers::WriteSerialByte(UINT8 value)
{
    if (!IsSerialBusEnabled()) return;

    // ccTalk is a single-wire bus. The controller receives an echo of every
    // transmitted byte before the peripheral response.
    if (SerialReplyWrite_ - SerialReplyRead_ >= static_cast<UINT32>(SerialReply_.size())) ++SerialReplyRead_;
    SerialReply_[SerialReplyWrite_++ % SerialReply_.size()] = value;
    if (SerialReplyDelayCycles_ == 0U) SerialReplyDelayCycles_ = 200U;
    if (SerialCommandLength_ >= SerialCommand_.size())
    {
        SerialCommandLength_ = 0U;
        SerialCommandExpected_ = 0U;
    }
    SerialCommand_[SerialCommandLength_++] = value;
    if (SerialCommandLength_ == 2U)
    {
        const UINT16 expected = static_cast<UINT16>(SerialCommand_[1]) + 5U;
        if (expected > SerialCommand_.size())
        {
            SerialCommandLength_ = 0U;
            SerialCommandExpected_ = 0U;
            return;
        }
        SerialCommandExpected_ = static_cast<UINT8>(expected);
    }
    if (SerialCommandExpected_ != 0U && SerialCommandLength_ == SerialCommandExpected_)
    {
        ProcessSerialMessage();
        SerialCommandLength_ = 0U;
        SerialCommandExpected_ = 0U;
    }
}

bool MPU5Hoppers::SerialReplyReady(UINT8& value) const
{
    if (!IsSerialBusEnabled() || SerialReplyRead_ == SerialReplyWrite_ || SerialReplyDelayCycles_ != 0U)
        return false;
    value = SerialReply_[SerialReplyRead_ % SerialReply_.size()];
    return true;
}

void MPU5Hoppers::ConsumeSerialReplyByte()
{
    if (SerialReplyRead_ == SerialReplyWrite_) return;
    ++SerialReplyRead_;
    SerialReplyDelayCycles_ = 1600U;
}
