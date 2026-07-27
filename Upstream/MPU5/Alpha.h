#pragma once

#include "PA2CoreInterface.h"
#include <array>

class MPU5Alpha
{
public:
    static constexpr UINT8 CharacterCount = 16;

    MPU5Alpha();
    void Reset();
    void Enable(UINT8 enabled);
    void WriteClock(UINT8 clock, UINT8 data);
    void WriteByte(UINT8 data);
    void WriteBuffer(const UINT8* data, UINT32 length);

    UINT16 GetSegments(UINT8 character) const;
    UINT8 GetDotComma(UINT8 character) const;
    UINT8 GetBrightness() const { return Intensity_; }
    UINT8 GetCharacter(UINT8 character) const;
    bool IsEnabled() const { return Enabled_; }

private:
    static UINT16 CharacterToSegments(UINT8 character);

    std::array<UINT16, CharacterCount> Characters_{};
    UINT8 Clock_ = 0;
    UINT8 Shift_ = 0;
    UINT8 BitCount_ = 0;
    UINT8 Position_ = 15;
    UINT8 DigitCount_ = CharacterCount;
    UINT8 Intensity_ = 0;
    bool Enabled_ = false;
};
