#include "MC68340SIM.h"

#include <algorithm>

namespace
{
constexpr UINT16 MCR = 0x00;
constexpr UINT16 SYNCR = 0x04;
constexpr UINT16 AVR = 0x06;
constexpr UINT16 PPRA1 = 0x15;
constexpr UINT16 PPARB = 0x1F;
constexpr UINT16 SWIV = 0x20;
constexpr UINT16 SYPCR = 0x21;
constexpr UINT16 PICR = 0x22;
constexpr UINT16 PITR = 0x24;
constexpr UINT16 CS0AM = 0x40;
constexpr UINT16 CS0BA = 0x44;
}

UINT16 MC68340SIM::ReadWord(const std::array<UINT8, RegisterSize>& registers, UINT16 offset)
{
    if (offset + 1U >= registers.size()) { return 0; }
    return static_cast<UINT16>((static_cast<UINT16>(registers[offset]) << 8) | registers[offset + 1U]);
}

UINT32 MC68340SIM::ReadLong(const std::array<UINT8, RegisterSize>& registers, UINT16 offset)
{
    if (offset + 3U >= registers.size()) { return 0; }
    return (static_cast<UINT32>(registers[offset]) << 24) |
        (static_cast<UINT32>(registers[offset + 1U]) << 16) |
        (static_cast<UINT32>(registers[offset + 2U]) << 8) |
        registers[offset + 3U];
}

void MC68340SIM::WriteWord(std::array<UINT8, RegisterSize>& registers, UINT16 offset, UINT16 value)
{
    if (offset + 1U >= registers.size()) { return; }
    registers[offset] = static_cast<UINT8>(value >> 8);
    registers[offset + 1U] = static_cast<UINT8>(value);
}

void MC68340SIM::WriteLong(std::array<UINT8, RegisterSize>& registers, UINT16 offset, UINT32 value)
{
    if (offset + 3U >= registers.size()) { return; }
    registers[offset] = static_cast<UINT8>(value >> 24);
    registers[offset + 1U] = static_cast<UINT8>(value >> 16);
    registers[offset + 2U] = static_cast<UINT8>(value >> 8);
    registers[offset + 3U] = static_cast<UINT8>(value);
}

UINT8 MC68340SIM::ByteFromLong(UINT32 value, UINT8 byte)
{
    return static_cast<UINT8>(value >> ((3U - byte) * 8U));
}

void MC68340SIM::SetLongByte(UINT32& target, UINT8 byte, UINT8 value)
{
    const UINT32 shift = (3U - byte) * 8U;
    target = (target & ~(0xFFU << shift)) | (static_cast<UINT32>(value) << shift);
}

void MC68340SIM::Reset()
{
    ModuleBaseRegister_ = 0;
    Registers_.fill(0);
    WriteWord(Registers_, MCR, 0x608F);
    WriteWord(Registers_, SYNCR, 0x3F00);
    Registers_[PPRA1] = 0x0F;
    Registers_[PPARB] = 0x0F;
    Registers_[SWIV] = 0x0F;
    Registers_[SYPCR] = 0;
    WriteWord(Registers_, PICR, 0x000F);
    WriteWord(Registers_, PITR, 0);
    UpdateAllChipSelectCaches();
    PeriodicTicks_ = 0;
    PeriodicPrescale_ = 1600;
    PeriodicModulus_ = 0;
    PeriodicInterruptPending_ = false;
}

UINT32 MC68340SIM::AddressMask(UINT8 index) const
{
    if (index >= 4) { return 0; }
    return ReadLong(Registers_, static_cast<UINT16>(CS0AM + index * 8U));
}

UINT32 MC68340SIM::BaseAddress(UINT8 index) const
{
    if (index >= 4) { return 0; }
    return ReadLong(Registers_, static_cast<UINT16>(CS0BA + index * 8U));
}

void MC68340SIM::UpdateChipSelectCache(UINT8 index)
{
    if (index >= ChipSelectCache_.size()) { return; }

    const UINT32 baseRegister = BaseAddress(index);
    const UINT32 maskRegister = AddressMask(index);
    ChipSelectCache& cache = ChipSelectCache_[index];

    cache.Valid = (baseRegister & 1U) != 0;
    cache.AddressCompareMask = ~((maskRegister & 0xFFFFFF00U) + 0xFFU);
    cache.BaseAddress = baseRegister & 0xFFFFFF00U;
    cache.FunctionCompareMask = ~(maskRegister & 0xF0U);
    cache.FunctionBase = (baseRegister & 0xF0U) & cache.FunctionCompareMask;
    cache.WaitStates = static_cast<UINT8>((maskRegister >> 2U) & 0x03U);
    cache.FastTermination = (baseRegister & 0x04U) != 0U;

    const UINT8 portSize = static_cast<UINT8>(maskRegister & 0x03U);
    cache.ExternalDSACK = portSize == 0x03U;
    cache.PortWidthBytes = portSize == 0x02U ? 1U : 2U;
}

void MC68340SIM::UpdateAllChipSelectCaches()
{
    for (UINT8 index = 0; index < ChipSelectCache_.size(); ++index)
    {
        UpdateChipSelectCache(index);
    }
}

UINT8 MC68340SIM::ChipSelect(UINT32 address, UINT8 functionCode) const
{
    // Before CS0 is made valid the MC68340 globally asserts it for boot ROM access.
    if (!ChipSelectCache_[0].Valid) { return 1; }

    const UINT32 shiftedFunctionCode = static_cast<UINT32>(functionCode & 7U) << 4;
    for (UINT8 index = 0; index < ChipSelectCache_.size(); ++index)
    {
        const ChipSelectCache& cache = ChipSelectCache_[index];
        if (!cache.Valid) { continue; }

        if ((address & cache.AddressCompareMask) == cache.BaseAddress &&
            (shiftedFunctionCode & cache.FunctionCompareMask) == cache.FunctionBase)
        {
            return static_cast<UINT8>(index + 1U);
        }
    }
    return 0;
}

UINT32 MC68340SIM::GetChipSelectBase(UINT8 index) const
{
    return index < ChipSelectCache_.size() ? ChipSelectCache_[index].BaseAddress : 0;
}

MC68340SIM::ExternalAccessTiming MC68340SIM::GetExternalAccessTiming(
    UINT32 address, UINT8 functionCode, UINT8 accessBytes) const
{
    ExternalAccessTiming result{};
    if (accessBytes == 0U || IsModuleAddress(address, functionCode))
    {
        return result;
    }
    if ((functionCode & 7U) == 7U && address >= 0x0003FF00U && address <= 0x0003FF03U)
    {
        return result;
    }

    const UINT8 chipSelect = ChipSelect(address, functionCode);
    if (chipSelect == 0U)
    {
        return result;
    }

    result.ChipSelect = chipSelect;

    // Until CS0 is made valid it is the MC68340 global boot chip select:
    // a 16-bit port with three wait states.
    if (chipSelect == 1U && !ChipSelectCache_[0].Valid)
    {
        result.PortWidthBytes = 2U;
        result.WaitStates = 3U;
        result.FastTermination = 0U;
    }
    else
    {
        const ChipSelectCache& cache = ChipSelectCache_[chipSelect - 1U];
        result.PortWidthBytes = cache.PortWidthBytes;
        result.WaitStates = cache.WaitStates;
        result.FastTermination = cache.FastTermination ? 1U : 0U;

        // The MPU5 board is a 16-bit system.  Where firmware selects external
        // DSACK termination, model the normal three-clock asynchronous cycle.
        // A future board schematic/device-specific override can refine this.
        if (cache.ExternalDSACK)
        {
            result.PortWidthBytes = 2U;
            result.WaitStates = 0U;
            result.FastTermination = 0U;
        }
    }

    const UINT32 portBytes = result.PortWidthBytes == 1U ? 1U : 2U;
    const UINT32 firstOffset = address & (portBytes - 1U);
    result.BusCycles = (firstOffset + accessBytes + portBytes - 1U) / portBytes;

    // CPU32 timing tables assume one two-clock bus access.  Normal MC68340
    // DSACK termination is at least three clocks, with DD adding 0..3 waits.
    // FTE is the only programmed two-clock external termination.  Additional
    // bus cycles required by a 16/8-bit port each contribute their full length.
    const UINT32 clocksPerBusCycle = result.FastTermination != 0U
        ? 2U
        : static_cast<UINT32>(3U + result.WaitStates);
    result.AdditionalCycles = result.BusCycles * (clocksPerBusCycle - 2U);
    if (result.BusCycles > 1U)
    {
        result.AdditionalCycles += (result.BusCycles - 1U) * 2U;
    }
    return result;
}

bool MC68340SIM::IsModuleAddress(UINT32 address, UINT8 functionCode) const
{
    if (!ModulesEnabled() || (address & 0xFFFFF000U) != GetModuleBase()) { return false; }

    UINT32 accessMask = (ModuleBaseRegister_ & 0x03FEU) >> 1;
    if ((ModuleBaseRegister_ & 0x0200U) != 0) { accessMask |= 0xFF00U; }
    return (accessMask & (1U << (functionCode & 0x0FU))) == 0;
}

UINT16 MC68340SIM::ModuleOffset(UINT32 address) const
{
    return static_cast<UINT16>(address - GetModuleBase());
}

UINT8 MC68340SIM::ReadModuleBaseByte(UINT32 address) const
{
    if (address < 0x0003FF00U || address > 0x0003FF03U) { return 0xFF; }
    return ByteFromLong(ModuleBaseRegister_, static_cast<UINT8>(address & 3U));
}

void MC68340SIM::WriteModuleBaseByte(UINT32 address, UINT8 value)
{
    if (address < 0x0003FF00U || address > 0x0003FF03U) { return; }
    SetLongByte(ModuleBaseRegister_, static_cast<UINT8>(address & 3U), value);
}

UINT8 MC68340SIM::ReadByte(UINT16 offset) const
{
    return offset < Registers_.size() ? Registers_[offset] : 0;
}

void MC68340SIM::WriteByte(UINT16 offset, UINT8 value)
{
    if (offset >= Registers_.size()) { return; }
    Registers_[offset] = value;
    UpdateRegister(offset);
}

void MC68340SIM::UpdateRegister(UINT16 offset)
{
    if (offset >= CS0AM && offset < static_cast<UINT16>(CS0AM + 32U))
    {
        UpdateChipSelectCache(static_cast<UINT8>((offset - CS0AM) / 8U));
    }

    const UINT16 registerOffset = static_cast<UINT16>(offset & 0xFFFEU);
    if (registerOffset == SYNCR)
    {
        WriteWord(Registers_, SYNCR, static_cast<UINT16>(ReadWord(Registers_, SYNCR) | 0x0008U));
    }
    else if (registerOffset == PITR)
    {
        UpdatePeriodicTimer();
    }
}

void MC68340SIM::UpdatePeriodicTimer()
{
    const UINT16 pitr = ReadWord(Registers_, PITR);
    PeriodicPrescale_ = (pitr & 0x0100U) ? 819200U : 1600U;
    PeriodicModulus_ = static_cast<UINT8>(pitr);
    PeriodicTicks_ = 0;
    if (PeriodicModulus_ == 0) { PeriodicInterruptPending_ = false; }
}

void MC68340SIM::Tick(UINT32 cycles)
{
    const UINT8 programmedModulus = static_cast<UINT8>(ReadWord(Registers_, PITR));
    if (programmedModulus == 0 || PeriodicPrescale_ == 0) { return; }
    if (PeriodicModulus_ == 0) { PeriodicModulus_ = programmedModulus; }

    PeriodicTicks_ += cycles;
    while (PeriodicTicks_ >= PeriodicPrescale_)
    {
        PeriodicTicks_ -= PeriodicPrescale_;
        if (--PeriodicModulus_ == 0)
        {
            if (GetInterruptLevel() != 0) { PeriodicInterruptPending_ = true; }
            PeriodicModulus_ = programmedModulus;
        }
    }
}

UINT8 MC68340SIM::GetInterruptLevel() const
{
    return static_cast<UINT8>((ReadWord(Registers_, PICR) >> 8) & 7U);
}

UINT8 MC68340SIM::GetInterruptVector() const
{
    return static_cast<UINT8>(ReadWord(Registers_, PICR));
}

bool MC68340SIM::UseAutoVector(UINT8 level) const
{
    return level < 8 && (Registers_[AVR] & static_cast<UINT8>(1U << level)) != 0;
}
