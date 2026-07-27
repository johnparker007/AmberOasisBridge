#pragma once

#include "PA2CoreInterface.h"
#include <array>

class MC68340Serial
{
public:
    static constexpr UINT16 RegisterSize = 0x22;
    static constexpr UINT32 MaximumMessageLength = 128;

    void Reset();
    bool Tick(UINT32 cycles);

    UINT8 ReadByte(UINT16 offset);
    void WriteByte(UINT16 offset, UINT8 value);

    bool PopTransmittedMessage(UINT8 channel, std::array<UINT8, MaximumMessageLength>& message, UINT32& length);
    void QueueReceive(UINT8 channel, const UINT8* data, UINT32 length);

    void SetInputPort(UINT8 value) { InputPort_ = value; }
    UINT8 GetOutputPort() const { return OutputPort_; }
    bool InterruptPending() const { return (InterruptEnable_ & InterruptStatus_) != 0; }
    UINT8 GetInterruptLevel() const { return static_cast<UINT8>(InterruptLevel_ & 7U); }
    UINT8 GetInterruptVector() const { return InterruptVector_; }

private:
    static constexpr UINT8 RxReady = 0x01;
    static constexpr UINT8 FifoFull = 0x02;
    static constexpr UINT8 TxReady = 0x04;
    static constexpr UINT8 TxEmpty = 0x08;
    static constexpr UINT8 IrqTxA = 0x01;
    static constexpr UINT8 IrqRxA = 0x02;
    static constexpr UINT8 IrqDeltaBreakA = 0x04;
    static constexpr UINT8 IrqTxB = 0x10;
    static constexpr UINT8 IrqRxB = 0x20;
    static constexpr UINT8 IrqDeltaBreakB = 0x40;
    // Top Dollar configures both channel-A clocks as external SCLK/16
    // (CSR = 0xEE).  The MPU5 serial clock is 1.8432 MHz, giving a 115200-baud
    // 8-N-1 stream.  Pace queued slave replies by CPU cycles so delivery is
    // independent of the number and mix of CPU instructions being executed.
    static constexpr UINT32 CPUCyclesPerSecond = 16000000U;
    static constexpr UINT32 ExternalSerialClock = 1843200U;
    static constexpr UINT32 ExternalClockDivider = 16U;
    static constexpr UINT32 AsyncCharacterBits = 10U;
    static constexpr UINT32 ExternalClockBaud = ExternalSerialClock / ExternalClockDivider;
    static constexpr UINT32 ReceiveCharacterCycles =
        (CPUCyclesPerSecond * AsyncCharacterBits + ExternalClockBaud - 1U) / ExternalClockBaud;
    static constexpr UINT8 MessageGapTicks = 100;

    struct Channel
    {
        UINT8 Mode1 = 0;
        UINT8 Mode2 = 0;
        UINT8 ClockSelect = 0;
        UINT8 Command = 0;
        UINT8 Status = 0;
        bool ReceiverEnabled = false;
        bool TransmitterEnabled = false;
        bool RtsAsserted = false;

        bool TxBytePending = false;
        UINT8 TxByte = 0;
        UINT8 MessageGap = 0;
        std::array<UINT8, MaximumMessageLength> TxMessage{};
        UINT32 TxLength = 0;
        std::array<UINT8, MaximumMessageLength> CompletedMessage{};
        UINT32 CompletedLength = 0;
        bool CompletedReady = false;

        std::array<UINT8, 3> RxFifo{};
        UINT8 RxCount = 0;
        UINT8 RxShiftByte = 0;
        bool RxShiftPending = false;
        std::array<UINT8, MaximumMessageLength * 2U> ReceiveQueue{};
        UINT32 ReceiveRead = 0;
        UINT32 ReceiveWrite = 0;
        UINT32 ReceiveGap = 0;
    };

    static UINT8 TxInterruptBit(UINT8 channel) { return channel == 0 ? IrqTxA : IrqTxB; }
    static UINT8 RxInterruptBit(UINT8 channel) { return channel == 0 ? IrqRxA : IrqRxB; }
    static UINT8 DeltaBreakBit(UINT8 channel) { return channel == 0 ? IrqDeltaBreakA : IrqDeltaBreakB; }

    void WriteMode(UINT8 channel, UINT8 value);
    UINT8 ReadMode(UINT8 channel);
    void WriteCommand(UINT8 channel, UINT8 value);
    void WriteTransmit(UINT8 channel, UINT8 value);
    UINT8 ReadReceive(UINT8 channel);
    bool TickChannel(UINT8 channel, UINT32 cycles);
    bool CompleteTransmitMessage(UINT8 channel);
    static bool BarbusFrameComplete(const Channel& channel);
    void PushReceive(UINT8 channel, UINT8 value);
    void UpdateReceiveStatus(UINT8 channel);
    void UpdateOutputHandshake(UINT8 channel);

    std::array<Channel, 2> Channels_{};
    UINT16 ModuleConfiguration_ = 0;
    UINT8 InterruptLevel_ = 0;
    UINT8 InterruptVector_ = 0;
    UINT8 AuxiliaryControl_ = 0;
    UINT8 InputPortChange_ = 0;
    UINT8 InterruptEnable_ = 0;
    UINT8 InterruptStatus_ = 0;
    UINT8 OutputConfiguration_ = 0;
    UINT8 OutputPort_ = 0;
    UINT8 InputPort_ = 0;
};
