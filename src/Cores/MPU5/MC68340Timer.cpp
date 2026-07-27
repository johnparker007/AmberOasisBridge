#include "MC68340Timer.h"

namespace
{
constexpr UINT16 TIMER_MCR = 0x00;
constexpr UINT16 TIMER_IR = 0x04;
constexpr UINT16 TIMER_CR = 0x06;
constexpr UINT16 TIMER_SR = 0x08;
constexpr UINT16 TIMER_CNTR = 0x0A;
constexpr UINT16 TIMER_PREL1 = 0x0C;
constexpr UINT16 TIMER_PREL2 = 0x0E;
constexpr UINT16 TIMER_COM = 0x10;

constexpr UINT16 SWR = 0x8000;
constexpr UINT16 CPE = 0x0200;
constexpr UINT16 CLK = 0x0100;
constexpr UINT16 PCLK = 0x0400;
constexpr UINT16 TGE = 0x0800;
constexpr UINT16 SR_COM = 0x0100;
constexpr UINT16 SR_ON = 0x0400;
constexpr UINT16 SR_TC = 0x1000;
constexpr UINT16 SR_TG = 0x2000;
constexpr UINT16 SR_TO = 0x4000;
constexpr UINT16 SR_IRQ = 0x8000;
constexpr UINT16 InterruptFlags = SR_TO | SR_TG | SR_TC;
constexpr UINT32 PotPrescale[8] = { 256, 2, 4, 8, 16, 32, 64, 128 };
}

bool MC68340Timer::ResolveOffset(UINT16 offset, UINT8& channel, UINT16& localOffset)
{
    if (offset >= RegisterSize) { return false; }
    if (offset >= 0x40)
    {
        channel = 0;
        localOffset = static_cast<UINT16>(offset - 0x40);
    }
    else
    {
        channel = 1;
        localOffset = offset;
    }
    return localOffset < ChannelRegisterSize;
}

UINT16 MC68340Timer::ReadRegisterWord(const Channel& channel, UINT16 offset)
{
    if (offset + 1U >= channel.Registers.size()) { return 0; }
    return static_cast<UINT16>((static_cast<UINT16>(channel.Registers[offset]) << 8) |
        channel.Registers[offset + 1U]);
}

void MC68340Timer::WriteRegisterWord(Channel& channel, UINT16 offset, UINT16 value)
{
    if (offset + 1U >= channel.Registers.size()) { return; }
    channel.Registers[offset] = static_cast<UINT8>(value >> 8);
    channel.Registers[offset + 1U] = static_cast<UINT8>(value);
}

UINT32 MC68340Timer::PrescaleFor(UINT16 control)
{
    if ((control & (CLK | PCLK)) == 0) { return 2; }
    if ((control & CLK) == 0 && (control & PCLK) != 0)
    {
        return 2U * PotPrescale[(control >> 5) & 7U];
    }
    return 0; // External clock modes require a board input and do not advance from the CPU clock.
}

void MC68340Timer::UpdateStatus(Channel& channel)
{
    const UINT16 control = ReadRegisterWord(channel, TIMER_CR);
    UINT16 status = ReadRegisterWord(channel, TIMER_SR);

    if ((control & (SWR | CPE)) != 0) { status |= SR_ON; }
    else { status &= static_cast<UINT16>(~SR_ON); }

    if ((status & control & InterruptFlags) != 0) { status |= SR_IRQ; }
    else { status &= static_cast<UINT16>(~SR_IRQ); }
    WriteRegisterWord(channel, TIMER_SR, status);
}

void MC68340Timer::Reset()
{
    for (Channel& channel : Channels_)
    {
        channel = Channel{};
        WriteRegisterWord(channel, TIMER_MCR, 0);
        WriteRegisterWord(channel, TIMER_IR, 0x000F);
        WriteRegisterWord(channel, TIMER_CR, 0);
        WriteRegisterWord(channel, TIMER_SR, 0x00FF);
        WriteRegisterWord(channel, TIMER_CNTR, 0);
        WriteRegisterWord(channel, TIMER_PREL1, 0xFFFF);
        WriteRegisterWord(channel, TIMER_PREL2, 0xFFFF);
        WriteRegisterWord(channel, TIMER_COM, 0);
        channel.Prescale = 2;
        channel.Ticks = 0;
        channel.ReloadPreload2 = true;
    }
}

UINT8 MC68340Timer::ReadByte(UINT16 offset) const
{
    UINT8 channel = 0;
    UINT16 local = 0;
    return ResolveOffset(offset, channel, local) ? Channels_[channel].Registers[local] : 0;
}

UINT16 MC68340Timer::ReadWord(UINT16 offset) const
{
    UINT8 channel = 0;
    UINT16 local = 0;
    if (!ResolveOffset(offset, channel, local)) { return 0; }
    return ReadRegisterWord(Channels_[channel], static_cast<UINT16>(local & 0xFFFEU));
}

UINT32 MC68340Timer::ReadLong(UINT16 offset) const
{
    return (static_cast<UINT32>(ReadWord(offset)) << 16) | ReadWord(static_cast<UINT16>(offset + 2U));
}

void MC68340Timer::WriteRegister(UINT8 channelIndex, UINT16 offset, UINT16 value, bool byteWrite, UINT8 byteIndex)
{
    if (channelIndex >= Channels_.size()) { return; }
    Channel& channel = Channels_[channelIndex];
    offset = static_cast<UINT16>(offset & 0xFFFEU);
    const UINT16 oldValue = ReadRegisterWord(channel, offset);

    if (byteWrite)
    {
        value = byteIndex == 0
            ? static_cast<UINT16>((static_cast<UINT16>(value & 0xFFU) << 8) | (oldValue & 0x00FFU))
            : static_cast<UINT16>((oldValue & 0xFF00U) | (value & 0x00FFU));
    }

    if (offset == TIMER_SR)
    {
        // TO/TG/TC are write-one-to-clear.  Byte writes must use the byte
        // supplied by the CPU, not the difference between the merged values.
        const UINT16 writeMask = byteWrite
            ? (byteIndex == 0 ? static_cast<UINT16>(value & 0xFF00U) : 0U)
            : value;
        const UINT16 cleared = static_cast<UINT16>(oldValue & ~(writeMask & InterruptFlags));
        WriteRegisterWord(channel, TIMER_SR, cleared);
        UpdateStatus(channel);
        return;
    }

    WriteRegisterWord(channel, offset, value);
    switch (offset)
    {
    case TIMER_CR:
        if ((oldValue & SWR) != 0 && (value & SWR) == 0)
        {
            WriteRegisterWord(channel, TIMER_SR,
                static_cast<UINT16>((ReadRegisterWord(channel, TIMER_SR) | 0x00FFU) & ~SR_COM));
            WriteRegisterWord(channel, TIMER_CNTR, 0);
            channel.ReloadPreload2 = true;
        }
        else if ((oldValue & SWR) == 0 && (value & SWR) != 0)
        {
            WriteRegisterWord(channel, TIMER_CNTR, ReadRegisterWord(channel, TIMER_PREL1));
            channel.ReloadPreload2 = true;
        }
        channel.Prescale = PrescaleFor(value);
        channel.Ticks = 0;
        UpdateStatus(channel);
        break;
    case TIMER_COM:
        WriteRegisterWord(channel, TIMER_SR,
            static_cast<UINT16>(ReadRegisterWord(channel, TIMER_SR) & ~SR_COM));
        UpdateStatus(channel);
        break;
    case TIMER_IR:
    case TIMER_MCR:
    case TIMER_CNTR:
    case TIMER_PREL1:
    case TIMER_PREL2:
        UpdateStatus(channel);
        break;
    default:
        break;
    }
}

void MC68340Timer::WriteByte(UINT16 offset, UINT8 value)
{
    UINT8 channel = 0;
    UINT16 local = 0;
    if (!ResolveOffset(offset, channel, local)) { return; }
    WriteRegister(channel, local, value, true, static_cast<UINT8>(local & 1U));
}

void MC68340Timer::WriteWord(UINT16 offset, UINT16 value)
{
    UINT8 channel = 0;
    UINT16 local = 0;
    if (!ResolveOffset(offset, channel, local)) { return; }
    WriteRegister(channel, local, value, false, 0);
}

void MC68340Timer::WriteLong(UINT16 offset, UINT32 value)
{
    WriteWord(offset, static_cast<UINT16>(value >> 16));
    WriteWord(static_cast<UINT16>(offset + 2U), static_cast<UINT16>(value));
}

void MC68340Timer::Tick(UINT32 cycles)
{
    // Timer channels can run directly from the CPU clock with a divide-by-two
    // prescaler. The earlier implementation updated the counter one decrement
    // at a time, which meant a one-second MPU5 slice could execute millions of
    // C++ loop iterations whenever game code enabled a fast timer. The CPU and
    // ASIC remained alive, but the frontend appeared to freeze with a flashing
    // yellow status LED.
    //
    // Advance between compare/timeout events instead. The resulting counter,
    // alternating PREL1/PREL2 phase and latched SR flags are identical to the
    // original per-decrement model, while the work is bounded per Tick call.
    const auto ticksToTimeout = [](UINT16 counter) -> UINT64
    {
        return counter != 0U ? static_cast<UINT64>(counter) : 65536ULL;
    };
    const auto ticksToCompare = [](UINT16 counter, UINT16 compare) -> UINT64
    {
        const UINT16 difference = static_cast<UINT16>(counter - compare);
        return difference != 0U ? static_cast<UINT64>(difference) : 65536ULL;
    };
    const auto compareOccursInPeriod = [](UINT16 preload, UINT16 compare) -> bool
    {
        return preload == 0U || compare < preload;
    };

    for (Channel& channel : Channels_)
    {
        const UINT16 control = ReadRegisterWord(channel, TIMER_CR);
        if ((control & SWR) == 0 || channel.Prescale == 0) { continue; }

        const UINT16 mode = static_cast<UINT16>(control & 0x001CU);
        const bool fixedDutyMode = mode == 0x0004U;
        const bool variableDutyMode = mode == 0x0008U;
        if ((!fixedDutyMode && !variableDutyMode) ||
            (control & (TGE | CPE | CLK)) != CPE) { continue; }

        UINT16 counter = ReadRegisterWord(channel, TIMER_CNTR);
        if (counter == 0U)
        {
            counter = ReadRegisterWord(channel, TIMER_PREL1);
            channel.ReloadPreload2 = true;
        }

        channel.Ticks += cycles;
        UINT64 decrements = channel.Ticks / channel.Prescale;
        channel.Ticks %= channel.Prescale;
        if (decrements == 0U)
        {
            WriteRegisterWord(channel, TIMER_CNTR, counter);
            continue;
        }

        const UINT16 preload1 = ReadRegisterWord(channel, TIMER_PREL1);
        const UINT16 preload2 = ReadRegisterWord(channel, TIMER_PREL2);
        const UINT16 compare = ReadRegisterWord(channel, TIMER_COM);
        UINT16 status = ReadRegisterWord(channel, TIMER_SR);

        const auto applyPartial = [&](UINT64 count)
        {
            if (count == 0U) return;
            if (ticksToCompare(counter, compare) <= count)
                status = static_cast<UINT16>(status | SR_COM | SR_TC);
            counter = static_cast<UINT16>(counter - static_cast<UINT16>(count));
        };

        const auto applyTimeout = [&](bool variable)
        {
            if (ticksToCompare(counter, compare) <= ticksToTimeout(counter))
                status = static_cast<UINT16>(status | SR_TC);
            status = static_cast<UINT16>((status | SR_TO) & ~SR_COM);
            if (variable)
            {
                counter = channel.ReloadPreload2 ? preload2 : preload1;
                channel.ReloadPreload2 = !channel.ReloadPreload2;
            }
            else
            {
                counter = preload1;
            }
        };

        // Finish the current, potentially partial, countdown period first.
        const UINT64 firstPeriod = ticksToTimeout(counter);
        if (decrements < firstPeriod)
        {
            applyPartial(decrements);
            decrements = 0U;
        }
        else
        {
            decrements -= firstPeriod;
            applyTimeout(variableDutyMode);
        }

        if (decrements != 0U && fixedDutyMode)
        {
            const UINT64 period = ticksToTimeout(preload1);
            const UINT64 completePeriods = decrements / period;
            if (completePeriods != 0U)
            {
                status = static_cast<UINT16>((status | SR_TO) & ~SR_COM);
                if (compareOccursInPeriod(preload1, compare))
                    status = static_cast<UINT16>(status | SR_TC);
                decrements %= period;
                counter = preload1;
            }
            applyPartial(decrements);
            decrements = 0U;
        }
        else if (decrements != 0U && variableDutyMode)
        {
            // At a reload boundary, the current and following periods form a
            // repeating PREL2/PREL1 or PREL1/PREL2 pair. Skip whole pairs in
            // constant time, preserving the next-preload phase exactly.
            const UINT16 nextPreload = channel.ReloadPreload2 ? preload2 : preload1;
            const UINT64 currentPeriod = ticksToTimeout(counter);
            const UINT64 followingPeriod = ticksToTimeout(nextPreload);
            const UINT64 pairPeriod = currentPeriod + followingPeriod;
            const UINT64 completePairs = decrements / pairPeriod;
            if (completePairs != 0U)
            {
                status = static_cast<UINT16>((status | SR_TO) & ~SR_COM);
                if (compareOccursInPeriod(counter, compare) ||
                    compareOccursInPeriod(nextPreload, compare))
                {
                    status = static_cast<UINT16>(status | SR_TC);
                }
                decrements %= pairPeriod;
                // Two timeouts return both the reload value and phase to the
                // same state, so counter/ReloadPreload2 need no adjustment.
            }

            if (decrements >= currentPeriod)
            {
                decrements -= currentPeriod;
                applyTimeout(true);
            }
            applyPartial(decrements);
            decrements = 0U;
        }

        WriteRegisterWord(channel, TIMER_SR, status);
        UpdateStatus(channel);
        WriteRegisterWord(channel, TIMER_CNTR, counter);
    }
}

bool MC68340Timer::InterruptPending(UINT8 channel) const
{
    return channel < Channels_.size() && (ReadRegisterWord(Channels_[channel], TIMER_SR) & SR_IRQ) != 0;
}

UINT8 MC68340Timer::GetInterruptLevel(UINT8 channel) const
{
    return channel < Channels_.size()
        ? static_cast<UINT8>((ReadRegisterWord(Channels_[channel], TIMER_IR) >> 8) & 7U)
        : 0;
}

UINT8 MC68340Timer::GetInterruptVector(UINT8 channel) const
{
    return channel < Channels_.size() ? static_cast<UINT8>(ReadRegisterWord(Channels_[channel], TIMER_IR)) : 0;
}
