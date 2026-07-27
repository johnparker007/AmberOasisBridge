#include "Segments.h"

#include <algorithm>

UINT8 MPU5Segments::ToAmberOrder(UINT8 value)
{
    // The legacy Project Amber display API packs segment 0 into bit 7 and
    // segment 7 into bit 0. MPU5/MFME stores the standard display lines in
    // the opposite bit order, so reverse the byte at the interface boundary.
    value = static_cast<UINT8>(((value & 0xF0U) >> 4U) | ((value & 0x0FU) << 4U));
    value = static_cast<UINT8>(((value & 0xCCU) >> 2U) | ((value & 0x33U) << 2U));
    value = static_cast<UINT8>(((value & 0xAAU) >> 1U) | ((value & 0x55U) << 1U));
    return value;
}

namespace
{
constexpr UINT32 kMFMERefreshInstructions = 1300U;
constexpr UINT8 kOnboardPersistence = 35U;
}

void MPU5Segments::Reset()
{
    Digits_.fill(0);
    for (auto& digit : OnboardPersistence_) { digit.fill(0); }
    RefreshCounter_ = 0;
}

void MPU5Segments::Tick(UINT32 cycles)
{
    (void)cycles;

    // MFME calls led.update() once per 1300 CPU execute iterations. MPU5 calls
    // this method from its per-instruction hook, so use the same cadence rather
    // than decaying by raw clock cycles (which made the display almost vanish).
    if (++RefreshCounter_ < kMFMERefreshInstructions) { return; }
    RefreshCounter_ = 0;

    for (UINT32 digit = 0; digit < OnboardPersistence_.size(); ++digit)
    {
        for (UINT32 segment = 0; segment < OnboardPersistence_[digit].size(); ++segment)
        {
            UINT8& persistence = OnboardPersistence_[digit][segment];
            if (persistence == 0U) { continue; }
            if (--persistence == 0U)
            {
                Digits_[digit] = static_cast<UINT8>(Digits_[digit] & ~(1U << segment));
            }
        }
    }
}

void MPU5Segments::WriteCommonAnode(UINT8 digitSelect, UINT8 segments, UINT8 segmentNumber, UINT8 bank)
{
    (void)bank;
    if (segmentNumber >= 8U) { return; }

    // Exact MPU5/MFME common-anode path: LEDs_ selects one or more digits,
    // ~LowSide supplies the active segment bit, and LowSelect identifies which
    // persistence counter belongs to that bit.
    for (UINT32 digit = 0; digit < 8U; ++digit)
    {
        if ((digitSelect & (1U << digit)) == 0U) { continue; }
        Digits_[digit] = static_cast<UINT8>(Digits_[digit] | segments);
        OnboardPersistence_[digit][segmentNumber] = kOnboardPersistence;
    }
}

UINT32 MPU5Segments::WriteBuffer(const UINT8* data, UINT32 available, UINT16 base, UINT8 mux)
{
    if (!data || available < 2U) { return 0; }

    const UINT8 segmentMask = data[0];
    UINT32 consumed = 2U; // Byte 1 is the protocol's second sparse mask/header.
    const UINT32 displayBase = static_cast<UINT32>(base) + static_cast<UINT32>(mux) * 8U;

    for (UINT32 segment = 0; segment < 8U; ++segment)
    {
        if ((segmentMask & (0x80U >> segment)) == 0U) { continue; }
        if (consumed >= available) { return consumed; }

        const UINT8 digitBits = data[consumed++];
        const UINT8 segmentBit = static_cast<UINT8>(1U << segment);
        for (UINT32 digit = 0; digit < 8U; ++digit)
        {
            const UINT32 index = displayBase + digit;
            if (index >= Digits_.size()) { continue; }
            if ((digitBits & (1U << digit)) != 0U)
                Digits_[index] = static_cast<UINT8>(Digits_[index] | segmentBit);
            else
                Digits_[index] = static_cast<UINT8>(Digits_[index] & ~segmentBit);
        }
    }
    return consumed;
}

UINT8 MPU5Segments::GetSegments(UINT16 digit) const
{
    return digit < Digits_.size() ? ToAmberOrder(Digits_[digit]) : 0U;
}

UINT8 MPU5Segments::GetBrightness(UINT16 digit) const
{
    if (digit >= Digits_.size() || Digits_[digit] == 0U) { return 0U; }
    if (digit >= OnboardPersistence_.size()) { return 255U; }

    const UINT8 maximum = *std::max_element(
        OnboardPersistence_[digit].begin(), OnboardPersistence_[digit].end());
    return static_cast<UINT8>(static_cast<UINT32>(maximum) * 255U / kOnboardPersistence);
}
