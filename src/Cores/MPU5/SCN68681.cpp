#include "SCN68681.h"

namespace
{
constexpr UINT8 RxReady = 0x01;
constexpr UINT8 RxFull = 0x02;
constexpr UINT8 TxReady = 0x04;
constexpr UINT8 TxEmpty = 0x08;
constexpr UINT8 IrqTxA = 0x01;
constexpr UINT8 IrqRxA = 0x02;
constexpr UINT8 IrqCounter = 0x08;
constexpr UINT8 IrqTxB = 0x10;
constexpr UINT8 IrqRxB = 0x20;
}

void MPU5DUART::Reset()
{
    A_ = Channel{};
    B_ = Channel{};
    AuxControl_ = 0;
    InputPort_ = 0xFF;
    InputPortChange_ = 0;
    InterruptStatus_ = 0;
    InterruptMask_ = 0;
    InterruptVector_ = 0x0F;
    OutputConfig_ = 0;
    OutputRegister_ = 0;
    OutputPort_ = 0xFF;
    OutputChanges_ = 0;
    CounterReload_ = 0;
    Counter_ = 0;
    CounterRunning_ = false;
    TxEvents_.fill(TxEvent{});
    TxEventRead_ = 0;
    TxEventWrite_ = 0;
}

void MPU5DUART::PushTransmittedByte(UINT8 channelIndex, const Channel& channel)
{
    if (TxEventWrite_ - TxEventRead_ >= TxEventQueueSize)
    {
        ++TxEventRead_;
    }

    TxEvent& event = TxEvents_[TxEventWrite_ % TxEventQueueSize];
    event.Channel = channelIndex;
    event.Value = channel.Tx;
    event.ClockSelect = channel.ClockSelect;
    event.Mode1 = channel.Mode1;
    event.Mode2 = channel.Mode2;
    event.Command = channel.Command;
    ++TxEventWrite_;
}

bool MPU5DUART::Tick(UINT32 cycles)
{
    bool transmitted = false;
    auto tickChannel = [this, cycles, &transmitted](Channel& channel, UINT8 channelIndex, UINT8 irqBit)
    {
        if (!channel.TxDelay) { return; }
        if (cycles >= channel.TxDelay)
        {
            channel.TxDelay = 0;
            channel.Status |= static_cast<UINT8>(TxReady | TxEmpty);
            InterruptStatus_ |= irqBit;
            PushTransmittedByte(channelIndex, channel);
            transmitted = true;
        }
        else { channel.TxDelay -= cycles; }
    };
    tickChannel(A_, 0, IrqTxA);
    tickChannel(B_, 1, IrqTxB);

    if (CounterRunning_ && CounterReload_)
    {
        if (cycles >= Counter_)
        {
            Counter_ = CounterReload_;
            InterruptStatus_ |= IrqCounter;
        }
        else { Counter_ -= cycles; }
    }
    return transmitted;
}

void MPU5DUART::WriteMode(Channel& channel, UINT8 value)
{
    if (!channel.ModePointer) { channel.Mode1 = value; channel.ModePointer = 1; }
    else { channel.Mode2 = value; }
}

UINT8 MPU5DUART::ReadMode(Channel& channel)
{
    const UINT8 value = channel.ModePointer ? channel.Mode2 : channel.Mode1;
    channel.ModePointer = 1;
    return value;
}

void MPU5DUART::WriteCommand(Channel& channel, UINT8 value, UINT8 rxInterruptBit, UINT8 txInterruptBit)
{
    channel.Command = value;
    if (value & 0x01) { channel.RxEnabled = 1; }
    if (value & 0x02) { channel.RxEnabled = 0; }
    if (value & 0x04) { channel.TxEnabled = 1; channel.Status |= static_cast<UINT8>(TxReady | TxEmpty); InterruptStatus_ |= txInterruptBit; }
    if (value & 0x08) { channel.TxEnabled = 0; channel.Status &= static_cast<UINT8>(~(TxReady | TxEmpty)); InterruptStatus_ &= static_cast<UINT8>(~txInterruptBit); }
    switch ((value >> 4) & 7U)
    {
    case 1: channel.ModePointer = 0; break;
    case 2: channel.Status &= static_cast<UINT8>(~(RxReady | RxFull)); InterruptStatus_ &= static_cast<UINT8>(~rxInterruptBit); break;
    case 3: channel.Status &= static_cast<UINT8>(~(TxReady | TxEmpty)); InterruptStatus_ &= static_cast<UINT8>(~txInterruptBit); break;
    default: break;
    }
}

void MPU5DUART::Write(UINT8 offset, UINT8 value)
{
    switch (offset & 0x0F)
    {
    case 0x0: WriteMode(A_, value); break;
    case 0x1: A_.ClockSelect = value; break;
    case 0x2: WriteCommand(A_, value, IrqRxA, IrqTxA); break;
    case 0x3:
        A_.Tx = value;
        A_.Status &= static_cast<UINT8>(~(TxReady | TxEmpty));
        InterruptStatus_ &= static_cast<UINT8>(~IrqTxA);
        A_.TxDelay = 100;
        if ((A_.Mode2 & 0x80) && A_.RxEnabled) { ReceiveA(value); }
        break;
    case 0x4: AuxControl_ = value; break;
    case 0x5: InterruptMask_ = value; break;
    case 0x6: CounterReload_ = static_cast<UINT16>((CounterReload_ & 0x00FFU) | (static_cast<UINT16>(value) << 8)); break;
    case 0x7: CounterReload_ = static_cast<UINT16>((CounterReload_ & 0xFF00U) | value); break;
    case 0x8: WriteMode(B_, value); break;
    case 0x9: B_.ClockSelect = value; break;
    case 0xA: WriteCommand(B_, value, IrqRxB, IrqTxB); break;
    case 0xB:
        B_.Tx = value;
        B_.Status &= static_cast<UINT8>(~(TxReady | TxEmpty));
        InterruptStatus_ &= static_cast<UINT8>(~IrqTxB);
        B_.TxDelay = 100;
        if ((B_.Mode2 & 0x80) && B_.RxEnabled) { ReceiveB(value); }
        break;
    case 0xC: InterruptVector_ = value; break;
    case 0xD: OutputConfig_ = value; break;
    case 0xE:
        {
            const UINT8 old = OutputPort_;
            OutputRegister_ |= value;
            OutputPort_ = static_cast<UINT8>(~OutputRegister_);
            OutputChanges_ |= static_cast<UINT8>(old ^ OutputPort_);
        }
        break;
    case 0xF:
        {
            const UINT8 old = OutputPort_;
            OutputRegister_ &= static_cast<UINT8>(~value);
            OutputPort_ = static_cast<UINT8>(~OutputRegister_);
            OutputChanges_ |= static_cast<UINT8>(old ^ OutputPort_);
        }
        break;
    default: break;
    }
}

UINT8 MPU5DUART::Read(UINT8 offset)
{
    switch (offset & 0x0F)
    {
    case 0x0: return ReadMode(A_);
    case 0x1: return A_.Status;
    case 0x2: return 0;
    case 0x3: A_.Status &= static_cast<UINT8>(~(RxReady | RxFull)); InterruptStatus_ &= static_cast<UINT8>(~IrqRxA); return A_.Rx;
    case 0x4: InputPortChange_ = 0; return InputPort_;
    case 0x5: return InterruptStatus_;
    case 0x6: return static_cast<UINT8>(Counter_ >> 8);
    case 0x7: return static_cast<UINT8>(Counter_);
    case 0x8: return ReadMode(B_);
    case 0x9: return B_.Status;
    case 0xA: return 0;
    case 0xB: { const UINT8 v = B_.Rx; B_.Status &= static_cast<UINT8>(~(RxReady | RxFull)); InterruptStatus_ &= static_cast<UINT8>(~IrqRxB); return v; }
    case 0xC: return InterruptVector_;
    case 0xD: return InputPort_;
    case 0xE: CounterRunning_ = true; Counter_ = CounterReload_; InterruptStatus_ &= static_cast<UINT8>(~IrqCounter); return static_cast<UINT8>(Counter_ >> 8);
    case 0xF: CounterRunning_ = false; InterruptStatus_ &= static_cast<UINT8>(~IrqCounter); return static_cast<UINT8>(Counter_);
    default: return 0;
    }
}

void MPU5DUART::ReceiveA(UINT8 value)
{
    if (!A_.RxEnabled) { return; }
    A_.Rx = value; A_.Status |= static_cast<UINT8>(RxReady | RxFull); InterruptStatus_ |= IrqRxA;
}

void MPU5DUART::ReceiveB(UINT8 value)
{
    (void)TryReceiveB(value);
}

bool MPU5DUART::TryReceiveB(UINT8 value)
{
    if (!B_.RxEnabled || (B_.Status & RxReady) != 0U) { return false; }
    B_.Rx = value; B_.Status |= static_cast<UINT8>(RxReady | RxFull); InterruptStatus_ |= IrqRxB;
    return true;
}

bool MPU5DUART::PopTransmittedByte(TxEvent& event)
{
    if (TxEventRead_ == TxEventWrite_) { return false; }
    event = TxEvents_[TxEventRead_ % TxEventQueueSize];
    ++TxEventRead_;
    return true;
}

UINT8 MPU5DUART::ConsumeOutputChanges()
{
    const UINT8 changes = OutputChanges_;
    OutputChanges_ = 0;
    return changes;
}
