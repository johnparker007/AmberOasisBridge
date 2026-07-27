#include <ctime>
#include "MPU5PIC.h"

namespace
{
// Preserve MFME's table exactly. Entry 7 intentionally returns 0x0F rather
// than the mathematically expected 0x0E; existing MPU5 key data relies on it.
constexpr std::array<UINT8, 16> kReversed{{
    0x00U, 0x08U, 0x04U, 0x0CU,
    0x02U, 0x0AU, 0x06U, 0x0FU,
    0x01U, 0x09U, 0x05U, 0x0DU,
    0x03U, 0x0BU, 0x07U, 0x0FU
}};
}

UINT8 MPU5PIC1::ReverseNibble(UINT8 value)
{
    return kReversed[value & 0x0FU];
}

void MPU5PIC1::SetCharacteriserID(const std::array<UINT8, 4>& id, bool present)
{
    CharacteriserID_ = id;
    CharacteriserPresent_ = present;
}

void MPU5PIC1::RebuildKeyMap()
{
    KeyMap_[0] = static_cast<UINT8>((KeyMap_[0] & 0xF8U) | (Stake_ & 0x07U));
    KeyMap_[1] = ReverseNibble(static_cast<UINT8>(Dip2_ >> 4U));
    KeyMap_[2] = ReverseNibble(Dip2_);
    KeyMap_[3] = ReverseNibble(Percent_);
    KeyMap_[4] = Prize_;
}

void MPU5PIC1::SetKeys(UINT8 stake, UINT8, UINT8 dip2, UINT8 percent, UINT8 prize)
{
    Stake_ = stake;
    Dip2_ = dip2;
    Percent_ = percent;
    Prize_ = prize;
    RebuildKeyMap();
}

void MPU5PIC1::SetTestSwitch(UINT8 pressed)
{
    if (pressed != 0U)
        KeyMap_[0] = static_cast<UINT8>(KeyMap_[0] | 0x08U);
    else
        KeyMap_[0] = static_cast<UINT8>(KeyMap_[0] & static_cast<UINT8>(~0x08U));
}

void MPU5PIC1::SelectBootstrap()
{
    ReadingKeyMap_ = false;
    Position_ = 0;
    Input_ = Bootstrap_[0];
}

void MPU5PIC1::SelectKeyMap()
{
    RebuildKeyMap();
    ReadingKeyMap_ = true;
    Position_ = 0;
    Input_ = KeyMap_[0];
}

void MPU5PIC1::AdvanceInput()
{
    if (ReadingKeyMap_)
    {
        if (Position_ + 1U < KeyMap_.size())
            ++Position_;
        Input_ = KeyMap_[Position_];
    }
    else
    {
        if (Position_ + 1U < Bootstrap_.size())
            ++Position_;
        Input_ = Bootstrap_[Position_];
    }
}

void MPU5PIC1::Reset(UINT8 stake, UINT8 dip1, UINT8 dip2, UINT8 percent, UINT8 prize)
{
    Clock_ = 0;
    Bit_ = 0;
    Data_ = 0;
    InputBit_ = 0;
    Clocks_ = 0;
    Start_ = false;
    Position_ = 0;
    SetKeys(stake, dip1, dip2, percent, prize);
    SelectBootstrap();
}

void MPU5PIC1::Write(UINT8 offset, UINT8 value)
{
    switch (offset)
    {
    case 0xD5:
        SelectKeyMap();
        [[fallthrough]];

    case 0xD4:
        if (Clock_ != 0U)
            Start_ = true;
        Bit_ = static_cast<UINT8>(value & 1U);
        break;

    case 0xD6:
    case 0xD7:
        if (value == 0U && Clock_ != 0U && !Start_)
        {
            Data_ = static_cast<UINT8>((Data_ << 1U) | Bit_);
            Input_ = static_cast<UINT8>(Input_ << 1U);
            if (++Clocks_ == 8U)
            {
                AdvanceInput();
                Data_ = 0;
                Clocks_ = 0;
            }
        }
        else if (value != 0U && Clock_ == 0U)
        {
            InputBit_ = static_cast<UINT8>((Input_ >> 7U) & 1U);
        }
        Start_ = false;
        Clock_ = value != 0U ? 1U : 0U;
        break;

    default:
        break;
    }
}

UINT8 MPU5PIC2::ReverseNibble(UINT8 value)
{
    return kReversed[value & 0x0FU];
}

void MPU5PIC2::SetCharacteriserID(const std::array<UINT8, 4>& id, bool present)
{
    CharacteriserID_ = id;
    CharacteriserPresent_ = present;
}

void MPU5PIC2::RebuildConfigurationMap()
{
    ConfigurationMap_[0] = static_cast<UINT8>(
        (ConfigurationMap_[0] & 0x08U) | (Stake_ & 0x07U));
    ConfigurationMap_[1] = ReverseNibble(static_cast<UINT8>(Dip2_ >> 4U));
    ConfigurationMap_[2] = ReverseNibble(Dip2_);
    ConfigurationMap_[3] = ReverseNibble(Percent_);
    ConfigurationMap_[4] = Prize_;
}

void MPU5PIC2::SetKeys(UINT8 stake, UINT8, UINT8 dip2, UINT8 percent, UINT8 prize)
{
    Stake_ = stake;
    Dip2_ = dip2;
    Percent_ = percent;
    Prize_ = prize;
    RebuildConfigurationMap();
}

void MPU5PIC2::SetTestSwitch(UINT8 pressed)
{
    TestSwitch_ = pressed != 0U ? 1U : 0U;
    if (TestSwitch_ != 0U)
        ConfigurationMap_[0] = static_cast<UINT8>(ConfigurationMap_[0] | 0x08U);
    else
        ConfigurationMap_[0] = static_cast<UINT8>(
            ConfigurationMap_[0] & static_cast<UINT8>(~0x08U));
}

std::array<UINT8, 3> MPU5PIC2::ConfigurationPacket() const
{
    // Type-2 transfers are simultaneous: the first response byte is shifted
    // out while the class byte is shifted in, followed by bytes two and three.
    return {{
        static_cast<UINT8>(((ConfigurationMap_[0] & 0x0FU) << 4U) |
            (ConfigurationMap_[4] & 0x0FU)),
        static_cast<UINT8>(ConfigurationMap_[3] & 0x0FU),
        static_cast<UINT8>(((ConfigurationMap_[1] & 0x0FU) << 4U) |
            (ConfigurationMap_[2] & 0x0FU))
    }};
}

void MPU5PIC2::BeginTransaction()
{
    RebuildConfigurationMap();
    Data_ = 0;
    Clocks_ = 0;
    TransactionClass_ = 0;
    TransactionCommand_ = 0;
    TransactionPosition_ = 0;

    // Class zero starts returning configuration during the class byte itself.
    Input_ = ConfigurationPacket()[0];
}

UINT8 MPU5PIC2::ResponseForCommand(UINT8 transactionClass, UINT8 command) const
{
    if (!CharacteriserPresent_)
        return 0;

    if (transactionClass == 2U && command == 0x16U)
        return 0xA5U;

    if (transactionClass == 1U && command < CharacteriserID_.size())
        return CharacteriserID_[command];

    // Class four is the independent security read of the program-module test
    // input.  The game compares it with the same switch sampled through the
    // sound/control processor; both paths must report the physical level.
    if (transactionClass == 4U && command == 0U)
        return TestSwitch_;

    if (transactionClass == 2U)
    {
        switch (command)
        {
        case 0x13U: return ConfigurationMap_[4];
        case 0x14U: return ConfigurationMap_[3];
        case 0x15U: return ConfigurationMap_[0];
        case 0x1CU: return ConfigurationMap_[1];
        case 0x1DU: return ConfigurationMap_[2];
        case 0x1EU: return ConfigurationMap_[3];
        case 0x1FU: return ConfigurationMap_[4];
        default: break;
        }
    }

    return 0;
}

void MPU5PIC2::CompleteByte(UINT8 value)
{
    const std::array<UINT8, 3> packet = ConfigurationPacket();

    switch (TransactionPosition_)
    {
    case 0:
        TransactionClass_ = value;
        TransactionPosition_ = 1;
        Input_ = TransactionClass_ == 0U ? packet[1] : 0U;
        break;

    case 1:
        TransactionCommand_ = value;
        TransactionPosition_ = 2;
        Input_ = TransactionClass_ == 0U
            ? packet[2]
            : ResponseForCommand(TransactionClass_, TransactionCommand_);
        break;

    default:
        TransactionPosition_ = 3;
        Input_ = 0;
        break;
    }
}

void MPU5PIC2::Reset(UINT8 stake, UINT8 dip1, UINT8 dip2, UINT8 percent, UINT8 prize)
{
    Clock_ = 0;
    Bit_ = 0;
    Data_ = 0;
    Input_ = 0;
    InputBit_ = 0;
    Clocks_ = 0;
    Start_ = false;
    TransactionClass_ = 0;
    TransactionCommand_ = 0;
    TransactionPosition_ = 0;
    SetKeys(stake, dip1, dip2, percent, prize);
}

void MPU5PIC2::Write(UINT8 offset, UINT8 value)
{
    switch (offset)
    {
    case 0xD5:
        [[fallthrough]];

    case 0xD4:
        if (Clock_ != 0U)
            Start_ = true;
        Bit_ = static_cast<UINT8>(value & 1U);
        break;

    case 0xD6:
    case 0xD7:
        if (value == 0U && Clock_ != 0U)
        {
            if (Start_)
            {
                BeginTransaction();
            }
            else
            {
                Data_ = static_cast<UINT8>((Data_ << 1U) | Bit_);
                Input_ = static_cast<UINT8>(Input_ << 1U);
                if (++Clocks_ == 8U)
                {
                    const UINT8 received = Data_;
                    Data_ = 0;
                    Clocks_ = 0;
                    CompleteByte(received);
                }
            }
        }
        else if (value != 0U && Clock_ == 0U)
        {
            InputBit_ = static_cast<UINT8>((Input_ >> 7U) & 1U);
        }
        Start_ = false;
        Clock_ = value != 0U ? 1U : 0U;
        break;

    default:
        break;
    }
}

UINT8 MPU5PIC3::ReverseNibble(UINT8 value)
{
    return kReversed[value & 0x0FU];
}

void MPU5PIC3::SetCharacteriserID(const std::array<UINT8, 4>& id, bool present)
{
    CharacteriserID_ = id;
    CharacteriserPresent_ = present;
}

void MPU5PIC3::RebuildConfigurationMap()
{
    ConfigurationMap_[0] = static_cast<UINT8>(
        (ConfigurationMap_[0] & 0x08U) | (Stake_ & 0x07U));
    ConfigurationMap_[1] = ReverseNibble(static_cast<UINT8>(Dip2_ >> 4U));
    ConfigurationMap_[2] = ReverseNibble(Dip2_);
    ConfigurationMap_[3] = ReverseNibble(Percent_);
    ConfigurationMap_[4] = Prize_;
}

void MPU5PIC3::SetKeys(UINT8 stake, UINT8, UINT8 dip2, UINT8 percent, UINT8 prize)
{
    Stake_ = stake;
    Dip2_ = dip2;
    Percent_ = percent;
    Prize_ = prize;
    RebuildConfigurationMap();
}

void MPU5PIC3::SetTestSwitch(UINT8 pressed)
{
    const UINT8 state = pressed != 0U ? 1U : 0U;
    if (state != 0U && TestSwitch_ == 0U && SecurityPulseQueue_ < 8U)
        ++SecurityPulseQueue_;
    TestSwitch_ = state;
    if (TestSwitch_ != 0U)
        ConfigurationMap_[0] = static_cast<UINT8>(ConfigurationMap_[0] | 0x08U);
    else
        ConfigurationMap_[0] = static_cast<UINT8>(
            ConfigurationMap_[0] & static_cast<UINT8>(~0x08U));
}

UINT8 MPU5PIC3::PairSecuritySample(UINT8 source)
{
    // PIC3 exposes the physical Test line through two independent security
    // paths: the class-four PIC transaction and DSP engine 7. The firmware
    // compares those replies, so both halves of one comparison must observe
    // the same electrical sample even when the frontend changes the button
    // between reads.
    if (SecurityPairSource_ != 0U)
    {
        if (SecurityPairSource_ != source)
        {
            const UINT8 sample = SecurityPairSample_;
            SecurityPairSource_ = 0U;
            return sample;
        }

        // A repeated read from the same path remains part of the outstanding
        // comparison. Do not replace a latched press with the released level
        // before the opposite security path has consumed it.
        return SecurityPairSample_;
    }

    // PIC3 security polling is much slower than a desktop mouse click. Latch
    // each physical rising edge until one complete PIC/DSP comparison has seen
    // it, then force one complete released comparison to re-arm the firmware's
    // edge detector. Multiple quick clicks are retained in order.
    if (SecurityReleasePending_)
    {
        SecurityPairSample_ = 0U;
        SecurityReleasePending_ = false;
    }
    else if (SecurityPulseQueue_ != 0U)
    {
        --SecurityPulseQueue_;
        SecurityPairSample_ = 1U;
        SecurityReleasePending_ = true;
    }
    else
    {
        SecurityPairSample_ = TestSwitch_;
    }

    SecurityPairSource_ = source;
    return SecurityPairSample_;
}

UINT8 MPU5PIC3::ReadSecurityTestForDSP()
{
    return PairSecuritySample(2U);
}

std::array<UINT8, 3> MPU5PIC3::ConfigurationPacket() const
{
    return {{
        static_cast<UINT8>(((ConfigurationMap_[0] & 0x0FU) << 4U) |
            (ConfigurationMap_[4] & 0x0FU)),
        static_cast<UINT8>(ConfigurationMap_[3] & 0x0FU),
        static_cast<UINT8>(((ConfigurationMap_[1] & 0x0FU) << 4U) |
            (ConfigurationMap_[2] & 0x0FU))
    }};
}

void MPU5PIC3::BeginTransaction()
{
    RebuildConfigurationMap();
    Data_ = 0;
    Clocks_ = 0;
    TransactionClass_ = 0;
    TransactionCommand_ = 0;
    TransactionPosition_ = 0;

    // Class zero returns its first configuration byte while the class byte is
    // being shifted in, as on PIC2.
    Input_ = ConfigurationPacket()[0];
}

UINT8 MPU5PIC3::ResponseForCommand(UINT8 transactionClass, UINT8 command) const
{
    if (!CharacteriserPresent_)
        return 0;

    if (transactionClass == 2U && command == 0x16U)
    {
        if (!PrimaryProbeRejected_) { PrimaryProbeRejected_ = true; return 0U; }
        return 0xA5U;
    }

    if (transactionClass == 1U && command < CharacteriserID_.size())
        return CharacteriserID_[command];

    if (transactionClass == 2U)
    {
        switch (command)
        {
        case 0x13U: return ConfigurationMap_[4];
        case 0x14U: return ConfigurationMap_[3];
        case 0x15U: return ConfigurationMap_[0];
        case 0x1CU: return static_cast<UINT8>(RealTimeCounter_ >> 24U);
        case 0x1DU: return static_cast<UINT8>(RealTimeCounter_ >> 16U);
        case 0x1EU: return static_cast<UINT8>(RealTimeCounter_ >> 8U);
        case 0x1FU: return static_cast<UINT8>(RealTimeCounter_);
        case 0x30U:
        case 0x31U:
        case 0x32U:
        case 0x33U: return SecurityRAM_[command];
        default: break;
        }
    }

    return 0;
}

void MPU5PIC3::CompleteByte(UINT8 value)
{
    const std::array<UINT8, 3> packet = ConfigurationPacket();

    switch (TransactionPosition_)
    {
    case 0:
        TransactionClass_ = value;
        TransactionPosition_ = 1;

        if (TransactionClass_ == 0U)
        {
            Input_ = packet[1];
        }
        else if (TransactionClass_ == 4U)
        {
            // PIC3's class-four transaction is only two bytes long. The
            // physical program-module test input is returned concurrently
            // with the second byte, so it must be armed as soon as the class
            // byte has completed rather than after a command byte.
            Input_ = PairSecuritySample(1U);
        }
        else
        {
            Input_ = 0U;
        }
        break;

    case 1:
        TransactionCommand_ = value;
        TransactionPosition_ = 2;
        Input_ = TransactionClass_ == 0U
            ? packet[2]
            : ResponseForCommand(TransactionClass_, TransactionCommand_);
        break;

    default:
        if (TransactionClass_ == 3U)
        {
            SecurityRAM_[TransactionCommand_] = value;
            if (TransactionCommand_ >= 0x1CU && TransactionCommand_ <= 0x1FU)
            {
                const UINT32 shift = static_cast<UINT32>(0x1FU - TransactionCommand_) * 8U;
                RealTimeCounter_ = (RealTimeCounter_ & ~(0xFFU << shift)) |
                    (static_cast<UINT32>(value) << shift);
            }
        }
        TransactionPosition_ = 3;
        Input_ = 0U;
        break;
    }
}

void MPU5PIC3::Reset(UINT8 stake, UINT8 dip1, UINT8 dip2, UINT8 percent, UINT8 prize)
{
    Clock_ = 0;
    Bit_ = 0;
    Data_ = 0;
    Input_ = 0;
    InputBit_ = 0;
    Clocks_ = 0;
    Start_ = false;
    TransactionClass_ = 0;
    TransactionCommand_ = 0;
    TransactionPosition_ = 0;
    SecurityRAM_.fill(0U);
    PrimaryProbeRejected_ = false;
    SecurityPairSample_ = TestSwitch_;
    SecurityPairSource_ = 0U;
    SecurityPulseQueue_ = 0U;
    SecurityReleasePending_ = false;
    RTCCycleAccumulator_ = 0U;
    if (!RTCInitialised_)
    {
        const std::time_t now = std::time(nullptr);
        if (now > 0)
            RealTimeCounter_ = static_cast<UINT32>(static_cast<UINT64>(now) + 2208988800ULL);
        RTCInitialised_ = true;
    }
    SetKeys(stake, dip1, dip2, percent, prize);
    if (TestSwitch_ != 0U)
        ConfigurationMap_[0] = static_cast<UINT8>(ConfigurationMap_[0] | 0x08U);
    else
        ConfigurationMap_[0] = static_cast<UINT8>(
            ConfigurationMap_[0] & static_cast<UINT8>(~0x08U));
}

void MPU5PIC3::Tick(UINT32 cycles)
{
    static constexpr UINT32 kCyclesPerSecond = 16000000U;
    RTCCycleAccumulator_ += cycles;
    while (RTCCycleAccumulator_ >= kCyclesPerSecond)
    {
        RTCCycleAccumulator_ -= kCyclesPerSecond;
        ++RealTimeCounter_;
    }
}

void MPU5PIC3::Write(UINT8 offset, UINT8 value)
{
    switch (offset)
    {
    case 0xD5:
        [[fallthrough]];

    case 0xD4:
        if (Clock_ != 0U)
            Start_ = true;
        Bit_ = static_cast<UINT8>(value & 1U);
        break;

    case 0xD6:
    case 0xD7:
        if (value == 0U && Clock_ != 0U)
        {
            if (Start_)
            {
                BeginTransaction();
            }
            else
            {
                Data_ = static_cast<UINT8>((Data_ << 1U) | Bit_);
                Input_ = static_cast<UINT8>(Input_ << 1U);
                if (++Clocks_ == 8U)
                {
                    const UINT8 received = Data_;
                    Data_ = 0;
                    Clocks_ = 0;
                    CompleteByte(received);
                }
            }
        }
        else if (value != 0U && Clock_ == 0U)
        {
            InputBit_ = static_cast<UINT8>((Input_ >> 7U) & 1U);
        }
        Start_ = false;
        Clock_ = value != 0U ? 1U : 0U;
        break;

    default:
        break;
    }
}

MPU5PICDevice& MPU5PIC::Active()
{
    if (Mode_ == Mode::PIC2) return PIC2_;
    if (Mode_ == Mode::PIC3) return PIC3_;
    return PIC1_;
}

const MPU5PICDevice& MPU5PIC::Active() const
{
    if (Mode_ == Mode::PIC2) return PIC2_;
    if (Mode_ == Mode::PIC3) return PIC3_;
    return PIC1_;
}

void MPU5PIC::Reset(UINT8 stake, UINT8 dip1, UINT8 dip2, UINT8 percent, UINT8 prize)
{
    Active().Reset(stake, dip1, dip2, percent, prize);
}

void MPU5PIC::SetKeys(UINT8 stake, UINT8 dip1, UINT8 dip2, UINT8 percent, UINT8 prize)
{
    PIC1_.SetKeys(stake, dip1, dip2, percent, prize);
    PIC2_.SetKeys(stake, dip1, dip2, percent, prize);
    PIC3_.SetKeys(stake, dip1, dip2, percent, prize);
}

void MPU5PIC::SetTestSwitch(UINT8 pressed)
{
    PIC1_.SetTestSwitch(pressed);
    PIC2_.SetTestSwitch(pressed);
    PIC3_.SetTestSwitch(pressed);
}

void MPU5PIC::SetCharacteriserID(const std::array<UINT8, 4>& id, bool present)
{
    PIC1_.SetCharacteriserID(id, present);
    PIC2_.SetCharacteriserID(id, present);
    PIC3_.SetCharacteriserID(id, present);
}

void MPU5PIC::Write(UINT8 offset, UINT8 value)
{
    Active().Write(offset, value);
}

UINT8 MPU5PIC::Read() const
{
    return Active().Read();
}


void MPU5PIC::Tick(UINT32 cycles)
{
    Active().Tick(cycles);
}

UINT8 MPU5PIC::ReadSecurityTestForDSP(UINT8 fallback)
{
    return Mode_ == Mode::PIC3 ? PIC3_.ReadSecurityTestForDSP() : fallback;
}

bool MPU5PIC::HasCharacteriser() const
{
    return Active().HasCharacteriser();
}

const std::array<UINT8, 4>& MPU5PIC::GetCharacteriserID() const
{
    return Active().GetCharacteriserID();
}
