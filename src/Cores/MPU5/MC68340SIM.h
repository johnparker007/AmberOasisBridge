#pragma once

#include "PA2CoreInterface.h"
#include <array>

class MC68340SIM
{
public:
    static constexpr UINT16 RegisterSize = 0x60;

    struct ExternalAccessTiming
    {
        UINT8 ChipSelect = 0;       // 1..4, zero when no external device selected
        UINT8 PortWidthBytes = 0;   // 1 or 2
        UINT8 WaitStates = 0;       // programmed DD1/DD0 value
        UINT8 FastTermination = 0;  // FTE gives a two-clock cycle
        UINT32 BusCycles = 0;       // physical external bus cycles required
        UINT32 AdditionalCycles = 0;// clocks beyond CPU32 two-clock timing tables
    };

    void Reset();
    void Tick(UINT32 cycles);

    UINT8 ChipSelect(UINT32 address, UINT8 functionCode) const;
    ExternalAccessTiming GetExternalAccessTiming(UINT32 address, UINT8 functionCode, UINT8 accessBytes) const;
    UINT8 ReadByte(UINT16 offset) const;
    void WriteByte(UINT16 offset, UINT8 value);

    UINT32 GetChipSelectBase(UINT8 index) const;
    UINT32 GetModuleBase() const { return ModuleBaseRegister_ & 0xFFFFF000U; }
    bool ModulesEnabled() const { return (ModuleBaseRegister_ & 1U) != 0; }
    bool IsModuleAddress(UINT32 address, UINT8 functionCode) const;
    UINT16 ModuleOffset(UINT32 address) const;

    UINT8 ReadModuleBaseByte(UINT32 address) const;
    void WriteModuleBaseByte(UINT32 address, UINT8 value);

    bool InterruptPending() const { return PeriodicInterruptPending_; }
    UINT8 GetInterruptLevel() const;
    UINT8 GetInterruptVector() const;
    void AcknowledgeInterrupt() { PeriodicInterruptPending_ = false; }
    bool UseAutoVector(UINT8 level) const;

private:
    static UINT16 ReadWord(const std::array<UINT8, RegisterSize>& registers, UINT16 offset);
    static UINT32 ReadLong(const std::array<UINT8, RegisterSize>& registers, UINT16 offset);
    static void WriteWord(std::array<UINT8, RegisterSize>& registers, UINT16 offset, UINT16 value);
    static void WriteLong(std::array<UINT8, RegisterSize>& registers, UINT16 offset, UINT32 value);
    static UINT8 ByteFromLong(UINT32 value, UINT8 byte);
    static void SetLongByte(UINT32& target, UINT8 byte, UINT8 value);

    struct ChipSelectCache
    {
        UINT32 AddressCompareMask = 0;
        UINT32 BaseAddress = 0;
        UINT32 FunctionCompareMask = 0;
        UINT32 FunctionBase = 0;
        UINT8 WaitStates = 0;
        UINT8 PortWidthBytes = 2;
        bool FastTermination = false;
        bool ExternalDSACK = false;
        bool Valid = false;
    };

    UINT32 AddressMask(UINT8 index) const;
    UINT32 BaseAddress(UINT8 index) const;
    void UpdateChipSelectCache(UINT8 index);
    void UpdateAllChipSelectCaches();
    void UpdateRegister(UINT16 offset);
    void UpdatePeriodicTimer();

    UINT32 ModuleBaseRegister_ = 0;
    std::array<UINT8, RegisterSize> Registers_{};
    std::array<ChipSelectCache, 4> ChipSelectCache_{};
    UINT64 PeriodicTicks_ = 0;
    UINT32 PeriodicPrescale_ = 1600;
    UINT8 PeriodicModulus_ = 0;
    bool PeriodicInterruptPending_ = false;
};
