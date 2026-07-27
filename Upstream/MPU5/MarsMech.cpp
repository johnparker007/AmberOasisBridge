#include "MarsMech.h"

#include <algorithm>

namespace
{
// C435A five-line binary-mech patterns as presented at the MPU5 ASIC input.
// Bit 2 is the separate binary-mech select line and is applied later by
// PresentedCoinByte().  Project Amber coin values are:
//   0=2p, 1=5p, 2=10p, 3=20p, 4=50p, 5=£1, 6=£2,
//   7..12=the matching token denominations.
//
// Top Dollar's own decoder table expects the resulting ASIC values:
//   5p=EB, 10p=C3, 20p=2B, 50p=4B, £1=A3, £2=03.
// The ASIC returns the active-low complement, so the physical input patterns
// below become 14,3C,D4,B4,5C,FC after the select bit is added.
constexpr std::array<UINT8, 13> kBinaryCoinCodesByValue{{
    0x00U, // 2p is not present in the standard six-coin C435A profile
    0x10U, // 5p
    0x38U, // 10p
    0xD0U, // 20p
    0xB0U, // 50p
    0x58U, // £1
    0xF8U, // £2
    0x10U, // 5p token
    0x38U, // 10p token
    0xD0U, // 20p token
    0xB0U, // 50p token
    0x58U, // £1 token
    0xF8U  // £2 token
}};

// Compatibility fallback for old layouts which never supplied CoinValue and
// relied on the electronic-mech channel number itself.
constexpr std::array<UINT8, MPU5MarsMech::CoinCount> kLegacyBinaryCodes{{
    0x08U, 0x0CU, 0x0BU, 0x0EU, 0x0DU, 0x1AU
}};
}

MPU5MarsMech::MPU5MarsMech(UINT32 cpuCyclesPerSecond) : CpuCyclesPerSecond_(cpuCyclesPerSecond)
{
    CoinEnable_.fill(1U);
    for (UINT8 i = 0; i < CoinCount; ++i) { LockoutDrive_[i] = i; }
}

void MPU5MarsMech::Reset()
{
    InputCounter_ = 0;
    LockCounter_ = 0;
    CoinByte_ = 0;
    Lamps_.fill(0);
    CoinOutputs_ = 0xFFU;
}

void MPU5MarsMech::Tick(UINT32 cycles)
{
    // Coin-mech outputs are physical pulses. Advance them from emulated CPU
    // time so monitor refresh and ASIC polling frequency cannot shorten them.
    if (InputCounter_ > 0)
    {
        InputCounter_ -= static_cast<INT64>(cycles);
        if (InputCounter_ <= 0)
        {
            InputCounter_ = 0;
            CoinByte_ = 0;
            LockCounter_ = static_cast<INT64>(CpuCyclesPerSecond_ / 2U);
        }
    }

    if (LockCounter_ > 0)
    {
        LockCounter_ -= static_cast<INT64>(cycles);
        if (LockCounter_ < 0) { LockCounter_ = 0; }
    }
}

UINT8 MPU5MarsMech::ParallelCode(UINT8 inputLine)
{
    return inputLine < 8U ? static_cast<UINT8>(1U << inputLine) : 0U;
}

UINT8 MPU5MarsMech::BinaryCode(UINT8 coinValue, UINT8 fallbackChannel)
{
    if (coinValue < kBinaryCoinCodesByValue.size())
    {
        const UINT8 code = kBinaryCoinCodesByValue[coinValue];
        if (code != 0U) return code;
    }
    return fallbackChannel < kLegacyBinaryCodes.size()
        ? kLegacyBinaryCodes[fallbackChannel] : 0U;
}

UINT8 MPU5MarsMech::BCDCode(UINT8 coinValue)
{
    // Project Amber stores the selected denomination as the standard coin
    // value index used by the other electronic-mech implementations.
    static constexpr std::array<UINT8, 13> codes{{
        0x02U, 0x05U, 0x0AU, 0x14U, 0x32U, 0x3AU, 0xC8U,
        0x05U, 0x0AU, 0x14U, 0x32U, 0x64U, 0xC8U
    }};
    return coinValue < codes.size() ? codes[coinValue] : coinValue;
}

bool MPU5MarsMech::IsCoinUnlocked(UINT8 coin) const
{
    if (coin >= CoinCount) { return false; }
    const UINT8 drive = static_cast<UINT8>(LockoutDrive_[coin] & 7U);

    // The ASIC register drives open-drain inhibit hardware. A set software
    // bit releases the corresponding inhibit output and permits that coin
    // line; a cleared bit actively inhibits it.  Big Brother writes 0xF6,
    // enabling the same input lines that its coin scanner monitors.
    const bool outputPermitsCoin = (CoinOutputs_ & (1U << drive)) != 0U;
    return LockoutInvert_[coin] != 0U ? !outputPermitsCoin : outputPermitsCoin;
}

UINT8 MPU5MarsMech::CoinIn(UINT8 coin, UINT8 coinValue)
{
    if (!CanAcceptCoin(coin, coinValue)) { return 0U; }

    const UINT8 configuredValue = coinValue != 0U ? coinValue : CoinValue_[coin];
    switch (CommStyle_)
    {
    case 0: // MPU5 parallel mech: one dedicated physical input per coin.
        CoinByte_ = ParallelCode(static_cast<UINT8>(LockoutDrive_[coin] & 7U));
        InputCounter_ = static_cast<INT64>(EffectivePulseCycles());
        break;

    case 1: // C435A binary mechanism: encode the configured denomination.
        CoinByte_ = BinaryCode(configuredValue, coin);
        InputCounter_ = static_cast<INT64>(EffectivePulseCycles());
        break;

    case 2: // BCD electronic mech
        CoinByte_ = BCDCode(configuredValue);
        InputCounter_ = static_cast<INT64>(EffectivePulseCycles());
        break;

    default: // Retain the generic channel-number form for older layouts.
        CoinByte_ = static_cast<UINT8>(coin + 1U);
        InputCounter_ = static_cast<INT64>(EffectivePulseCycles());
        break;
    }

    return CoinByte_ != 0U ? 1U : 0U;
}

bool MPU5MarsMech::CanAcceptCoin(UINT8 coin, UINT8 coinValue) const
{
    if (coin >= CoinCount || CoinEnable_[coin] == 0U ||
        CoinByte_ != 0U || InputCounter_ != 0 || LockCounter_ != 0 ||
        !IsCoinUnlocked(coin))
    {
        return false;
    }

    const UINT8 configuredValue = coinValue != 0U ? coinValue : CoinValue_[coin];
    switch (CommStyle_)
    {
    case 0U:
        return ParallelCode(static_cast<UINT8>(LockoutDrive_[coin] & 7U)) != 0U;
    case 1U:
        return BinaryCode(configuredValue, coin) != 0U;
    case 2U:
        return BCDCode(configuredValue) != 0U;
    default:
        return static_cast<UINT8>(coin + 1U) != 0U;
    }
}

UINT8 MPU5MarsMech::PresentedCoinByte() const
{
    UINT8 value = CoinByte_;
    if (CommStyle_ == 1U) { value = static_cast<UINT8>(value | 0x04U); }
    return CommInvert_ != 0U ? static_cast<UINT8>(~value) : value;
}

UINT8 MPU5MarsMech::ReadCoinByte()
{
    return PresentedCoinByte();
}

UINT32 MPU5MarsMech::EffectivePulseCycles() const
{
    // The MPU5 service manual specifies an approximately 80 ms input pulse.
    // Use a conservative 100 ms minimum so the firmware always sees several
    // samples, while still honouring any longer layout-configured pulse.
    const UINT32 minimum = std::max<UINT32>(1U, CpuCyclesPerSecond_ / 10U);
    return PulseCycles_ < minimum ? minimum : PulseCycles_;
}

UINT8 MPU5MarsMech::GetCoinByte() const
{
    return PresentedCoinByte();
}

void MPU5MarsMech::SetLockoutValue(UINT8 coin, UINT8 value)
{
    // Amber stores the physical coin/inhibit line as a bit number (x1..x80).
    // MPU5 parallel inputs and their matching inhibit outputs use that line.
    if (coin < CoinCount) { LockoutDrive_[coin] = static_cast<UINT8>(value & 7U); }
}

void MPU5MarsMech::SetCoinOutputs(UINT8 outputs)
{
    CoinOutputs_ = outputs;
}

void MPU5MarsMech::SetLockoutInvert(UINT8 coin, UINT8 value)
{
    if (coin < CoinCount) { LockoutInvert_[coin] = value ? 1U : 0U; }
}

void MPU5MarsMech::SetCoinValue(UINT8 coin, UINT8 value)
{
    if (coin < CoinCount) { CoinValue_[coin] = value; }
}

void MPU5MarsMech::SetCoinEnable(UINT8 coin, UINT8 value)
{
    if (coin < CoinCount) { CoinEnable_[coin] = value ? 1U : 0U; }
}

UINT8 MPU5MarsMech::GetLockoutState() const
{
    UINT8 result = 0;
    const bool temporarilyBusy = CoinByte_ != 0U || InputCounter_ != 0 || LockCounter_ != 0;
    for (UINT8 i = 0; i < CoinCount; ++i)
    {
        if (temporarilyBusy || CoinEnable_[i] == 0U || !IsCoinUnlocked(i))
            result |= static_cast<UINT8>(1U << i);
    }
    return result;
}
