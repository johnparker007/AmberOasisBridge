#include "MC68340DMA.h"

#include <algorithm>

namespace
{
constexpr UINT16 DMA_MCR = 0x00;
constexpr UINT16 DMA_IR = 0x04;
constexpr UINT16 DMA_CCR = 0x08;
constexpr UINT16 DMA_CSR = 0x0A;
constexpr UINT16 DMA_FCR = 0x0B;
constexpr UINT16 DMA_SAR = 0x0C;
constexpr UINT16 DMA_DAR = 0x10;
constexpr UINT16 DMA_BTC = 0x14;

constexpr UINT16 CCR_START = 0x0001;
constexpr UINT16 CCR_DEST_INCREMENT = 0x0400;
constexpr UINT16 CCR_SOURCE_INCREMENT = 0x0800;
constexpr UINT16 CCR_INTERRUPT_ENABLE = 0x4000;
constexpr UINT8 CSR_DONE = 0x40;
constexpr UINT8 CSR_SUMMARY = 0x80;
constexpr UINT8 CSR_STATUS_MASK = 0x7C;
}

bool MC68340DMA::ResolveOffset(UINT16 offset, UINT8& channel, UINT16& localOffset)
{
    if (offset >= RegisterSize) { return false; }
    if (offset >= 0x20)
    {
        channel = 0;
        localOffset = static_cast<UINT16>(offset - 0x20);
    }
    else
    {
        channel = 1;
        localOffset = offset;
    }
    return localOffset < ChannelRegisterSize;
}

UINT16 MC68340DMA::ReadRegisterWord(const Channel& channel, UINT16 offset)
{
    if (offset + 1U >= channel.Registers.size()) { return 0; }
    return static_cast<UINT16>((static_cast<UINT16>(channel.Registers[offset]) << 8) |
        channel.Registers[offset + 1U]);
}

UINT32 MC68340DMA::ReadRegisterLong(const Channel& channel, UINT16 offset)
{
    if (offset + 3U >= channel.Registers.size()) { return 0; }
    return (static_cast<UINT32>(channel.Registers[offset]) << 24) |
        (static_cast<UINT32>(channel.Registers[offset + 1U]) << 16) |
        (static_cast<UINT32>(channel.Registers[offset + 2U]) << 8) |
        channel.Registers[offset + 3U];
}

void MC68340DMA::WriteRegisterWord(Channel& channel, UINT16 offset, UINT16 value)
{
    if (offset + 1U >= channel.Registers.size()) { return; }
    channel.Registers[offset] = static_cast<UINT8>(value >> 8);
    channel.Registers[offset + 1U] = static_cast<UINT8>(value);
}

void MC68340DMA::WriteRegisterLong(Channel& channel, UINT16 offset, UINT32 value)
{
    if (offset + 3U >= channel.Registers.size()) { return; }
    channel.Registers[offset] = static_cast<UINT8>(value >> 24);
    channel.Registers[offset + 1U] = static_cast<UINT8>(value >> 16);
    channel.Registers[offset + 2U] = static_cast<UINT8>(value >> 8);
    channel.Registers[offset + 3U] = static_cast<UINT8>(value);
}

void MC68340DMA::UpdateSummary(Channel& channel)
{
    UINT8& status = channel.Registers[DMA_CSR];
    if ((status & CSR_STATUS_MASK) != 0) { status |= CSR_SUMMARY; }
    else { status &= static_cast<UINT8>(~CSR_SUMMARY); }
}

void MC68340DMA::Reset()
{
    for (Channel& channel : Channels_)
    {
        channel = Channel{};
        WriteRegisterWord(channel, DMA_MCR, 0x0080);
        WriteRegisterWord(channel, DMA_IR, 0x000F);
        WriteRegisterWord(channel, DMA_CCR, 0);
        channel.Registers[DMA_CSR] = 0;
        channel.Registers[DMA_FCR] = 0;
        WriteRegisterLong(channel, DMA_SAR, 0);
        WriteRegisterLong(channel, DMA_DAR, 0);
        WriteRegisterLong(channel, DMA_BTC, 0);
    }
}

UINT8 MC68340DMA::ReadByte(UINT16 offset) const
{
    UINT8 channel = 0;
    UINT16 local = 0;
    return ResolveOffset(offset, channel, local) ? Channels_[channel].Registers[local] : 0;
}

UINT16 MC68340DMA::ReadWord(UINT16 offset) const
{
    UINT8 channel = 0;
    UINT16 local = 0;
    if (!ResolveOffset(offset, channel, local)) { return 0; }
    return ReadRegisterWord(Channels_[channel], static_cast<UINT16>(local & 0xFFFEU));
}

UINT32 MC68340DMA::ReadLong(UINT16 offset) const
{
    UINT8 channel = 0;
    UINT16 local = 0;
    if (!ResolveOffset(offset, channel, local)) { return 0; }
    return ReadRegisterLong(Channels_[channel], static_cast<UINT16>(local & 0xFFFCU));
}

void MC68340DMA::WriteByte(UINT16 offset, UINT8 value)
{
    UINT8 channelIndex = 0;
    UINT16 local = 0;
    if (!ResolveOffset(offset, channelIndex, local)) { return; }
    Channel& channel = Channels_[channelIndex];
    if (local == DMA_CSR)
    {
        channel.Registers[DMA_CSR] &= static_cast<UINT8>(~(value & CSR_STATUS_MASK));
        UpdateSummary(channel);
        return;
    }
    channel.Registers[local] = value;
}

void MC68340DMA::WriteWord(UINT16 offset, UINT16 value)
{
    UINT8 channelIndex = 0;
    UINT16 local = 0;
    if (!ResolveOffset(offset, channelIndex, local)) { return; }
    Channel& channel = Channels_[channelIndex];
    local = static_cast<UINT16>(local & 0xFFFEU);
    if (local == DMA_CSR)
    {
        WriteByte(offset, static_cast<UINT8>(value >> 8));
        WriteByte(static_cast<UINT16>(offset + 1U), static_cast<UINT8>(value));
        return;
    }
    WriteRegisterWord(channel, local, value);
}

void MC68340DMA::WriteLong(UINT16 offset, UINT32 value)
{
    UINT8 channelIndex = 0;
    UINT16 local = 0;
    if (!ResolveOffset(offset, channelIndex, local)) { return; }
    WriteRegisterLong(Channels_[channelIndex], static_cast<UINT16>(local & 0xFFFCU), value);
}

UINT8 MC68340DMA::TransferWidth(UINT16 control, bool source)
{
    static constexpr UINT8 widths[4] = { 4, 1, 2, 0 };
    const UINT8 index = source
        ? static_cast<UINT8>((control >> 8) & 3U)
        : static_cast<UINT8>((control >> 6) & 3U);
    return widths[index];
}

void MC68340DMA::Transfer(UINT8 channelIndex, bool freeRun)
{
    if (channelIndex >= Channels_.size()) { return; }
    Channel& channel = Channels_[channelIndex];
    UINT16 control = ReadRegisterWord(channel, DMA_CCR);
    UINT32 count = ReadRegisterLong(channel, DMA_BTC);
    const UINT8 sourceWidth = TransferWidth(control, true);
    const UINT8 destinationWidth = TransferWidth(control, false);
    if ((control & CCR_START) == 0 || count == 0 || sourceWidth == 0 || destinationWidth == 0) { return; }

    UINT32 source = ReadRegisterLong(channel, DMA_SAR);
    UINT32 destination = ReadRegisterLong(channel, DMA_DAR);
    UINT32 transferGuard = 0;

    do
    {
        UINT32 value = 0;
        switch (sourceWidth)
        {
        case 1: value = Bus_.DMARead8(source); break;
        case 2: value = Bus_.DMARead16(source); break;
        case 4: value = Bus_.DMARead32(source); break;
        default: break;
        }
        switch (destinationWidth)
        {
        case 1: Bus_.DMAWrite8(destination, static_cast<UINT8>(value)); break;
        case 2: Bus_.DMAWrite16(destination, static_cast<UINT16>(value)); break;
        case 4: Bus_.DMAWrite32(destination, value); break;
        default: break;
        }

        if ((control & CCR_SOURCE_INCREMENT) != 0) { source += sourceWidth; }
        if ((control & CCR_DEST_INCREMENT) != 0) { destination += destinationWidth; }
        count = count > sourceWidth ? count - sourceWidth : 0;
        ++transferGuard;
    } while (freeRun && count != 0 && transferGuard < 0x100000U);

    WriteRegisterLong(channel, DMA_SAR, source);
    WriteRegisterLong(channel, DMA_DAR, destination);
    WriteRegisterLong(channel, DMA_BTC, count);

    if (count == 0)
    {
        channel.Registers[DMA_CSR] |= CSR_DONE;
        control &= static_cast<UINT16>(~CCR_START);
        WriteRegisterWord(channel, DMA_CCR, control);
    }
    UpdateSummary(channel);
}

void MC68340DMA::Tick(UINT32 cycles, UINT8 serialOutputPort)
{
    (void)cycles;
    for (UINT8 channel = 0; channel < Channels_.size(); ++channel)
    {
        const UINT16 control = ReadRegisterWord(Channels_[channel], DMA_CCR);
        if ((control & CCR_START) == 0) { continue; }

        const UINT16 requestMode = static_cast<UINT16>(control & 0x0030U);
        if (requestMode == 0)
        {
            Transfer(channel, true);
        }
        else if (requestMode == 0x0030U && (serialOutputPort & 0x40U) == 0)
        {
            Transfer(channel, false);
        }
    }
}

bool MC68340DMA::InterruptPending(UINT8 channel) const
{
    if (channel >= Channels_.size()) { return false; }
    return (ReadRegisterWord(Channels_[channel], DMA_CCR) & CCR_INTERRUPT_ENABLE) != 0 &&
        (Channels_[channel].Registers[DMA_CSR] & CSR_DONE) != 0;
}

UINT8 MC68340DMA::GetInterruptLevel(UINT8 channel) const
{
    return channel < Channels_.size()
        ? static_cast<UINT8>((ReadRegisterWord(Channels_[channel], DMA_IR) >> 8) & 7U)
        : 0;
}

UINT8 MC68340DMA::GetInterruptVector(UINT8 channel) const
{
    return channel < Channels_.size() ? static_cast<UINT8>(ReadRegisterWord(Channels_[channel], DMA_IR)) : 0;
}
