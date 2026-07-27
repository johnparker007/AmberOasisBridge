#pragma once

#include "PA2CoreInterface.h"
#include <array>

class MC68340Timer
{
public:
    static constexpr UINT16 RegisterSize = 0x80;

    void Reset();
    void Tick(UINT32 cycles);

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
        UINT64 Ticks = 0;
        UINT32 Prescale = 2;
        bool ReloadPreload2 = true;
    };

    static bool ResolveOffset(UINT16 offset, UINT8& channel, UINT16& localOffset);
    static UINT16 ReadRegisterWord(const Channel& channel, UINT16 offset);
    static void WriteRegisterWord(Channel& channel, UINT16 offset, UINT16 value);
    static UINT32 PrescaleFor(UINT16 control);
    static void UpdateStatus(Channel& channel);
    void WriteRegister(UINT8 channel, UINT16 offset, UINT16 value, bool byteWrite, UINT8 byteIndex);

    std::array<Channel, 2> Channels_{};
};
