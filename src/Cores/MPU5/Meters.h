#pragma once

#include "PA2CoreInterface.h"
#include <array>

class MPU5Meters
{
public:
    static constexpr UINT32 HardwareCount = 8;
    static constexpr UINT32 FrontendCount = PA2_NUM_METERS;

    void Reset();
    void Write(UINT8 outputs);
    void Tick(UINT32 cycles);
    UINT8 Sense() const;
    UINT32 GetCounter(UINT8 meter) const;

private:
    static constexpr UINT32 MinimumPulseCycles = 80000U; // 5 ms at 16 MHz
    std::array<UINT8, HardwareCount> On_{};
    std::array<UINT8, HardwareCount> Counted_{};
    std::array<UINT32, HardwareCount> OnCycles_{};
    std::array<UINT32, HardwareCount> Counters_{};
};
