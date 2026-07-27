#include "Meters.h"

void MPU5Meters::Reset()
{
    // Meter totals are non-volatile accounting values. A board reset clears
    // only the live pulse state, not the accumulated counters.
    On_.fill(0);
    Counted_.fill(0);
    OnCycles_.fill(0);
}

void MPU5Meters::Write(UINT8 outputs)
{
    for (UINT32 i = 0; i < HardwareCount; ++i)
    {
        const UINT8 state = static_cast<UINT8>((outputs >> i) & 1U);
        if (state == 0U)
        {
            OnCycles_[i] = 0;
            Counted_[i] = 0;
        }
        On_[i] = state;
    }
}

void MPU5Meters::Tick(UINT32 cycles)
{
    for (UINT32 i = 0; i < HardwareCount; ++i)
    {
        if (On_[i] == 0U) { continue; }
        OnCycles_[i] += cycles;
        if (Counted_[i] == 0U && OnCycles_[i] >= MinimumPulseCycles)
        {
            ++Counters_[i];
            Counted_[i] = 1U;
        }
    }
}

UINT8 MPU5Meters::Sense() const
{
    for (UINT8 state : On_) { if (state != 0U) { return 1U; } }
    return 0U;
}

UINT32 MPU5Meters::GetCounter(UINT8 meter) const
{
    return meter < Counters_.size() ? Counters_[meter] : 0U;
}
