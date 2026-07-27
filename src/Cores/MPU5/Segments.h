#pragma once

#include "PA2CoreInterface.h"
#include <array>

class MPU5Segments
{
public:
    static constexpr UINT32 DisplayCount = PA2_NUM_LED_DISPLAYS;

    void Reset();
    void Tick(UINT32 cycles);
    void WriteCommonAnode(UINT8 digitSelect, UINT8 segments, UINT8 segmentNumber, UINT8 bank);
    UINT32 WriteBuffer(const UINT8* data, UINT32 available, UINT16 base, UINT8 mux);
    UINT8 GetSegments(UINT16 digit) const;
    UINT8 GetBrightness(UINT16 digit) const;

private:
    static UINT8 ToAmberOrder(UINT8 value);
    std::array<UINT8, DisplayCount> Digits_{};
    // Only the ASIC's eight multiplexed digits need per-segment persistence.
    // Barbus LED packets are complete state updates and remain latched.
    std::array<std::array<UINT8, 8>, 8> OnboardPersistence_{};
    UINT32 RefreshCounter_ = 0;
};
