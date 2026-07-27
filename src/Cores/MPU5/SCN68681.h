#pragma once

#include "PA2CoreInterface.h"
#include <array>

class MPU5DUART
{
public:
    struct TxEvent
    {
        UINT8 Channel = 0;
        UINT8 Value = 0;
        UINT8 ClockSelect = 0;
        UINT8 Mode1 = 0;
        UINT8 Mode2 = 0;
        UINT8 Command = 0;
    };

    void Reset();
    bool Tick(UINT32 cycles);
    void Write(UINT8 offset, UINT8 value);
    UINT8 Read(UINT8 offset);
    void ReceiveA(UINT8 value);
    void ReceiveB(UINT8 value);
    bool TryReceiveB(UINT8 value);

    void SetInputPort(UINT8 value) { InputPort_ = value; }
    UINT8 GetOutputPort() const { return OutputPort_; }
    UINT8 ConsumeOutputChanges();
    bool PopTransmittedByte(TxEvent& event);
    bool InterruptPending() const { return (InterruptStatus_ & InterruptMask_) != 0; }
    UINT8 GetInterruptVector() const { return InterruptVector_; }

private:
    struct Channel
    {
        UINT8 Mode1 = 0;
        UINT8 Mode2 = 0;
        UINT8 ModePointer = 0;
        UINT8 ClockSelect = 0;
        UINT8 Command = 0;
        UINT8 Status = 0;
        UINT8 Rx = 0;
        UINT8 Tx = 0;
        UINT8 RxEnabled = 0;
        UINT8 TxEnabled = 0;
        UINT32 TxDelay = 0;
    };

    void WriteCommand(Channel& channel, UINT8 value, UINT8 rxInterruptBit, UINT8 txInterruptBit);
    void PushTransmittedByte(UINT8 channelIndex, const Channel& channel);
    UINT8 ReadMode(Channel& channel);
    void WriteMode(Channel& channel, UINT8 value);

    Channel A_{};
    Channel B_{};
    UINT8 AuxControl_ = 0;
    UINT8 InputPort_ = 0xFF;
    UINT8 InputPortChange_ = 0;
    UINT8 InterruptStatus_ = 0;
    UINT8 InterruptMask_ = 0;
    UINT8 InterruptVector_ = 0x0F;
    UINT8 OutputConfig_ = 0;
    UINT8 OutputRegister_ = 0;
    UINT8 OutputPort_ = 0xFF;
    UINT8 OutputChanges_ = 0;
    UINT16 CounterReload_ = 0;
    UINT32 Counter_ = 0;
    bool CounterRunning_ = false;
    static constexpr UINT32 TxEventQueueSize = 64;
    std::array<TxEvent, TxEventQueueSize> TxEvents_{};
    UINT32 TxEventRead_ = 0;
    UINT32 TxEventWrite_ = 0;
};
