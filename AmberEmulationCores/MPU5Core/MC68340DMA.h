#pragma once

#include "PA2CoreInterface.h"
#include <array>

class MC68340DMABus
{
public:
    virtual ~MC68340DMABus() = default;
    virtual UINT8 DMARead8(UINT32 address) = 0;
    virtual UINT16 DMARead16(UINT32 address) = 0;
    virtual UINT32 DMARead32(UINT32 address) = 0;
    virtual void DMAWrite8(UINT32 address, UINT8 value) = 0;
    virtual void DMAWrite16(UINT32 address, UINT16 value) = 0;
    virtual void DMAWrite32(UINT32 address, UINT32 value) = 0;
};

class MC68340DMA
{
public:
    static constexpr UINT16 RegisterSize = 0x40;

    explicit MC68340DMA(MC68340DMABus& bus) : Bus_(bus) {}

    void Reset();
    void Tick(UINT32 cycles, UINT8 serialOutputPort);

    UINT8 ReadByte(UINT16 offset) const;
    UINT16 ReadWord(UINT16 offset) const;
    UINT32 ReadLong(UINT16 offset) const;
    void WriteByte(UINT16 offset, UINT8 value);
    void WriteWord(UINT16 offset, UINT16 value);
    void WriteLong(UINT16 offset, UINT32 value);

    bool InterruptPending(UINT8 channel) const;
    UINT8 GetInterruptLevel(UINT8 channel) const;
    UINT8 GetInterruptVector(UINT8 channel) const;

private:
    static constexpr UINT16 ChannelRegisterSize = 0x20;

    struct Channel
    {
        std::array<UINT8, ChannelRegisterSize> Registers{};
    };

    static bool ResolveOffset(UINT16 offset, UINT8& channel, UINT16& localOffset);
    static UINT16 ReadRegisterWord(const Channel& channel, UINT16 offset);
    static UINT32 ReadRegisterLong(const Channel& channel, UINT16 offset);
    static void WriteRegisterWord(Channel& channel, UINT16 offset, UINT16 value);
    static void WriteRegisterLong(Channel& channel, UINT16 offset, UINT32 value);
    static UINT8 TransferWidth(UINT16 control, bool source);
    static void UpdateSummary(Channel& channel);
    void Transfer(UINT8 channel, bool freeRun);

    MC68340DMABus& Bus_;
    std::array<Channel, 2> Channels_{};
};
