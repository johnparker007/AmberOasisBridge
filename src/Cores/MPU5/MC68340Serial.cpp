#include "MC68340Serial.h"

#include <algorithm>

void MC68340Serial::Reset()
{
    Channels_ = {};
    ModuleConfiguration_ = 0;
    InterruptLevel_ = 0;
    InterruptVector_ = 0;
    AuxiliaryControl_ = 0;
    InputPortChange_ = 0;
    InterruptEnable_ = 0;
    InterruptStatus_ = 0;
    OutputConfiguration_ = 0;
    OutputPort_ = 0;
    InputPort_ = 0;
}

void MC68340Serial::WriteMode(UINT8 channelIndex, UINT8 value)
{
    if (channelIndex >= Channels_.size()) { return; }
    Channels_[channelIndex].Mode1 = value;
}

UINT8 MC68340Serial::ReadMode(UINT8 channelIndex)
{
    if (channelIndex >= Channels_.size()) { return 0; }
    return Channels_[channelIndex].Mode1;
}

void MC68340Serial::UpdateOutputHandshake(UINT8 channelIndex)
{
    if (channelIndex >= Channels_.size()) { return; }
    const Channel& channel = Channels_[channelIndex];
    const UINT8 txMask = channelIndex == 0 ? 0x40 : 0x80;
    const UINT8 rxMask = channelIndex == 0 ? 0x10 : 0x20;

    if ((OutputConfiguration_ & txMask) != 0)
    {
        // In local loopback the transmitter-ready output doubles as the DMA
        // request handshake.  Once a byte has entered the receive FIFO the
        // request must be deasserted until software drains that byte; otherwise
        // cycle-steal DMA can overrun the FIFO during the MC68340 power-on test.
        if ((channel.Mode2 & 0x80U) != 0 && channel.RxCount != 0)
        {
            OutputPort_ |= txMask;
        }
        else if ((channel.Status & TxReady) != 0)
        {
            OutputPort_ &= static_cast<UINT8>(~txMask);
        }
        else
        {
            OutputPort_ |= txMask;
        }
    }
    if ((OutputConfiguration_ & rxMask) != 0)
    {
        if ((channel.Status & RxReady) != 0) { OutputPort_ &= static_cast<UINT8>(~rxMask); }
        else { OutputPort_ |= rxMask; }
    }
}

void MC68340Serial::WriteCommand(UINT8 channelIndex, UINT8 value)
{
    if (channelIndex >= Channels_.size()) { return; }
    Channel& channel = Channels_[channelIndex];
    const UINT8 txInterrupt = TxInterruptBit(channelIndex);
    const UINT8 rxInterrupt = RxInterruptBit(channelIndex);
    channel.Command = value;

    if ((value & 0x01U) != 0) { channel.ReceiverEnabled = true; }
    if ((value & 0x02U) != 0) { channel.ReceiverEnabled = false; }
    if ((value & 0x04U) != 0)
    {
        channel.TransmitterEnabled = true;
        channel.Status |= static_cast<UINT8>(TxReady | TxEmpty);
        InterruptStatus_ |= txInterrupt;
    }
    if ((value & 0x08U) != 0)
    {
        channel.TransmitterEnabled = false;
        channel.Status &= static_cast<UINT8>(~(TxReady | TxEmpty));
        InterruptStatus_ &= static_cast<UINT8>(~txInterrupt);
    }

    switch (value >> 4)
    {
    case 1:
        // MR1 and MR2 have separate addresses on the MC68340 serial block.
        break;
    case 2:
        channel.ReceiverEnabled = false;
        channel.RxCount = 0;
        channel.RxShiftPending = false;
        channel.Status &= static_cast<UINT8>(~(RxReady | FifoFull));
        InterruptStatus_ &= static_cast<UINT8>(~rxInterrupt);
        break;
    case 3:
        channel.TransmitterEnabled = false;
        channel.TxBytePending = false;
        channel.Status &= static_cast<UINT8>(~(TxReady | TxEmpty));
        InterruptStatus_ &= static_cast<UINT8>(~txInterrupt);
        break;
    case 4:
        channel.Status &= 0x0F;
        break;
    case 5:
        InterruptStatus_ &= static_cast<UINT8>(~(IrqDeltaBreakA | IrqDeltaBreakB));
        break;
    case 8:
        channel.RtsAsserted = true;
        break;
    case 9:
        channel.RtsAsserted = false;
        break;
    default:
        break;
    }
    UpdateOutputHandshake(channelIndex);
}

void MC68340Serial::UpdateReceiveStatus(UINT8 channelIndex)
{
    if (channelIndex >= Channels_.size()) { return; }
    Channel& channel = Channels_[channelIndex];
    const bool ready = channel.RxCount != 0;
    const bool full = channel.RxCount >= channel.RxFifo.size();

    if (ready) { channel.Status |= RxReady; }
    else { channel.Status &= static_cast<UINT8>(~RxReady); }

    if (full) { channel.Status |= FifoFull; }
    else { channel.Status &= static_cast<UINT8>(~FifoFull); }

    // MR1 bit 6 selects whether the receive interrupt reflects RxRDY or FFULL.
    const bool receiveInterrupt = (channel.Mode1 & 0x40U) != 0 ? full : ready;
    if (receiveInterrupt) { InterruptStatus_ |= RxInterruptBit(channelIndex); }
    else { InterruptStatus_ &= static_cast<UINT8>(~RxInterruptBit(channelIndex)); }
}

void MC68340Serial::PushReceive(UINT8 channelIndex, UINT8 value)
{
    if (channelIndex >= Channels_.size()) { return; }
    Channel& channel = Channels_[channelIndex];
    if (!channel.ReceiverEnabled) { return; }

    // The MC68340 has three FIFO holding registers plus the receiver shift
    // register.  If a fourth character completes while the FIFO is full it
    // remains in the shift register and moves into the FIFO as soon as the
    // CPU reads one position.  Only a subsequent character is an overrun.
    if (channel.RxCount < channel.RxFifo.size())
    {
        channel.RxFifo[channel.RxCount++] = value;
    }
    else if (!channel.RxShiftPending)
    {
        channel.RxShiftByte = value;
        channel.RxShiftPending = true;
    }
    else
    {
        channel.Status |= 0x10U; // OE - receiver overrun error.
    }

    UpdateReceiveStatus(channelIndex);
    UpdateOutputHandshake(channelIndex);
}

void MC68340Serial::WriteTransmit(UINT8 channelIndex, UINT8 value)
{
    if (channelIndex >= Channels_.size()) { return; }
    Channel& channel = Channels_[channelIndex];

    if ((channel.Mode2 & 0x80U) != 0)
    {
        PushReceive(channelIndex, value);
        return;
    }
    if (!channel.TransmitterEnabled) { return; }

    channel.TxByte = value;
    channel.TxBytePending = true;
    channel.MessageGap = 0;
    channel.Status &= static_cast<UINT8>(~(TxReady | TxEmpty));
    InterruptStatus_ &= static_cast<UINT8>(~TxInterruptBit(channelIndex));
    UpdateOutputHandshake(channelIndex);
}

UINT8 MC68340Serial::ReadReceive(UINT8 channelIndex)
{
    if (channelIndex >= Channels_.size()) { return 0; }
    Channel& channel = Channels_[channelIndex];
    const UINT8 result = channel.RxCount ? channel.RxFifo[0] : 0;
    if (channel.RxCount)
    {
        for (UINT8 index = 1; index < channel.RxCount; ++index)
        {
            channel.RxFifo[index - 1U] = channel.RxFifo[index];
        }
        --channel.RxCount;

        if (channel.RxShiftPending)
        {
            channel.RxFifo[channel.RxCount++] = channel.RxShiftByte;
            channel.RxShiftPending = false;
        }
    }

    UpdateReceiveStatus(channelIndex);
    UpdateOutputHandshake(channelIndex);
    return result;
}

void MC68340Serial::WriteByte(UINT16 offset, UINT8 value)
{
    switch (offset)
    {
    case 0x00: ModuleConfiguration_ = static_cast<UINT16>((static_cast<UINT16>(value) << 8) | (ModuleConfiguration_ & 0x00FFU)); break;
    case 0x01: ModuleConfiguration_ = static_cast<UINT16>((ModuleConfiguration_ & 0xFF00U) | value); break;
    case 0x04: InterruptLevel_ = value; break;
    case 0x05: InterruptVector_ = value; break;
    case 0x10: WriteMode(0, value); break;
    case 0x11: Channels_[0].ClockSelect = value; break;
    case 0x12: WriteCommand(0, value); break;
    case 0x13: WriteTransmit(0, value); break;
    case 0x14: AuxiliaryControl_ = value; break;
    case 0x15: InterruptEnable_ = value; break;
    case 0x18: WriteMode(1, value); break;
    case 0x19: Channels_[1].ClockSelect = value; break;
    case 0x1A: WriteCommand(1, value); break;
    case 0x1B: WriteTransmit(1, value); break;
    case 0x1D:
        OutputConfiguration_ = value;
        UpdateOutputHandshake(0);
        UpdateOutputHandshake(1);
        break;
    case 0x1E: OutputPort_ |= value; break;
    case 0x1F: OutputPort_ &= static_cast<UINT8>(~value); break;
    case 0x20: Channels_[0].Mode2 = value; break;
    case 0x21: Channels_[1].Mode2 = value; break;
    default: break;
    }
}

UINT8 MC68340Serial::ReadByte(UINT16 offset)
{
    switch (offset)
    {
    case 0x00: return static_cast<UINT8>(ModuleConfiguration_ >> 8);
    case 0x01: return static_cast<UINT8>(ModuleConfiguration_);
    case 0x04: return InterruptLevel_;
    case 0x05: return InterruptVector_;
    case 0x10: return ReadMode(0);
    case 0x11: return Channels_[0].Status;
    case 0x13: return ReadReceive(0);
    case 0x14: return InputPortChange_;
    case 0x15: return InterruptStatus_;
    case 0x18: return ReadMode(1);
    case 0x19: return Channels_[1].Status;
    case 0x1B: return ReadReceive(1);
    case 0x1D: return InputPort_;
    case 0x20: return Channels_[0].Mode2;
    case 0x21: return Channels_[1].Mode2;
    default: return 0;
    }
}


bool MC68340Serial::BarbusFrameComplete(const Channel& channel)
{
    // Channel A is the MPU5 Barbus. Its frame carries an explicit payload
    // length, so an inter-byte software delay must never be mistaken for the
    // end of a packet. Bytes equal to 0x7F are doubled on the wire after the
    // initial synchronisation byte.
    if (channel.TxLength < 3U || channel.TxMessage[0] != 0x7FU) { return false; }

    std::array<UINT8, 4> header{};
    UINT32 logicalLength = 0;
    for (UINT32 read = 0; read < channel.TxLength; ++read)
    {
        const UINT8 value = channel.TxMessage[read];
        if (logicalLength < header.size()) { header[logicalLength] = value; }
        ++logicalLength;

        if (read != 0U && value == 0x7FU)
        {
            // A trailing 0x7F is only the first half of an escaped data byte.
            if (read + 1U >= channel.TxLength) { return false; }
            if (channel.TxMessage[read + 1U] == 0x7FU) { ++read; }
        }
    }

    if (logicalLength < 3U) { return false; }
    const bool extended = (header[2] & 0x0FU) == 0x0FU;
    if (extended && logicalLength < 4U) { return false; }

    const UINT32 payloadLength = extended ? header[3] : (header[2] & 0x0FU);
    const UINT32 expectedLength = (extended ? 4U : 3U) + payloadLength + 2U;
    return logicalLength >= expectedLength;
}

bool MC68340Serial::CompleteTransmitMessage(UINT8 channelIndex)
{
    if (channelIndex >= Channels_.size()) { return false; }
    Channel& channel = Channels_[channelIndex];
    if (channel.TxLength == 0U || channel.CompletedReady) { return false; }

    channel.CompletedLength = channel.TxLength;
    std::copy_n(channel.TxMessage.begin(), channel.TxLength, channel.CompletedMessage.begin());
    channel.CompletedReady = true;
    channel.TxLength = 0;
    channel.MessageGap = 0;

    if ((channel.Mode2 & 0x20U) != 0U)
    {
        // Auto-RTS is released at the actual end of the framed message, not
        // after an arbitrary number of emulator service calls.
        channel.TransmitterEnabled = false;
        channel.RtsAsserted = false;
        channel.Status &= static_cast<UINT8>(~TxReady);
        InterruptStatus_ &= static_cast<UINT8>(~TxInterruptBit(channelIndex));
        UpdateOutputHandshake(channelIndex);
    }
    return true;
}

bool MC68340Serial::TickChannel(UINT8 channelIndex, UINT32 cycles)
{
    bool messageCompleted = false;
    Channel& channel = Channels_[channelIndex];
    if (channel.TxBytePending)
    {
        if (channel.TxLength < channel.TxMessage.size())
        {
            channel.TxMessage[channel.TxLength++] = channel.TxByte;
        }
        channel.TxBytePending = false;
        channel.MessageGap = MessageGapTicks;
        channel.Status |= static_cast<UINT8>(TxReady | TxEmpty);
        InterruptStatus_ |= TxInterruptBit(channelIndex);
        UpdateOutputHandshake(channelIndex);

        // Barbus frames are self-delimiting. Completing at their declared
        // length prevents interrupt latency between two bytes from splitting
        // one REEL5/MUX5 command into several invalid messages.
        if (channelIndex == 0U && BarbusFrameComplete(channel))
        {
            messageCompleted = CompleteTransmitMessage(channelIndex);
        }
    }
    else if (channel.MessageGap != 0U)
    {
        // Retain the compatibility gap for unframed channel-B traffic and for
        // channel-A diagnostics which are not Barbus packets. A recognised
        // 0x7F Barbus frame waits for its declared payload and checksum.
        const bool incompleteBarbus = channelIndex == 0U && channel.TxLength != 0U &&
            channel.TxMessage[0] == 0x7FU;
        if (!incompleteBarbus && --channel.MessageGap == 0U)
        {
            messageCompleted = CompleteTransmitMessage(channelIndex);
        }
    }

    if (channel.ReceiveRead != channel.ReceiveWrite && !channel.RtsAsserted)
    {
        if (cycles >= channel.ReceiveGap)
        {
            const UINT32 index = channel.ReceiveRead % channel.ReceiveQueue.size();
            PushReceive(channelIndex, channel.ReceiveQueue[index]);
            ++channel.ReceiveRead;
            channel.ReceiveGap = ReceiveCharacterCycles;
        }
        else
        {
            channel.ReceiveGap -= cycles;
        }
    }
    return messageCompleted || (channel.CompletedReady && !channel.RtsAsserted);
}

bool MC68340Serial::Tick(UINT32 cycles)
{
    const bool channelA = TickChannel(0, cycles);
    const bool channelB = TickChannel(1, cycles);
    return channelA || channelB;
}

bool MC68340Serial::PopTransmittedMessage(UINT8 channelIndex,
    std::array<UINT8, MaximumMessageLength>& message, UINT32& length)
{
    length = 0;
    if (channelIndex >= Channels_.size()) { return false; }
    Channel& channel = Channels_[channelIndex];
    if (!channel.CompletedReady || channel.RtsAsserted) { return false; }

    length = channel.CompletedLength;
    std::copy_n(channel.CompletedMessage.begin(), length, message.begin());
    channel.CompletedLength = 0;
    channel.CompletedReady = false;
    return true;
}

void MC68340Serial::QueueReceive(UINT8 channelIndex, const UINT8* data, UINT32 length)
{
    if (channelIndex >= Channels_.size() || !data || length == 0) { return; }
    Channel& channel = Channels_[channelIndex];
    const UINT32 capacity = static_cast<UINT32>(channel.ReceiveQueue.size());
    for (UINT32 index = 0; index < length; ++index)
    {
        if (channel.ReceiveWrite - channel.ReceiveRead >= capacity) { break; }
        channel.ReceiveQueue[channel.ReceiveWrite % capacity] = data[index];
        ++channel.ReceiveWrite;
    }
    if (channel.ReceiveGap == 0) { channel.ReceiveGap = ReceiveCharacterCycles; }
}
