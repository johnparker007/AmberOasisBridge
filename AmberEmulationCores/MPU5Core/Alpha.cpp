#include "Alpha.h"

MPU5Alpha::MPU5Alpha() { Reset(); }

void MPU5Alpha::Reset()
{
    Characters_.fill(0x20);
    Clock_ = 0;
    Shift_ = 0;
    BitCount_ = 0;
    Position_ = 15;
    DigitCount_ = CharacterCount;
    Intensity_ = 0;
}

void MPU5Alpha::Enable(UINT8 enabled)
{
    const bool newState = enabled != 0;
    if (!newState && Enabled_) { Reset(); }
    Enabled_ = newState;
}

void MPU5Alpha::WriteClock(UINT8 clock, UINT8 data)
{
    clock = clock ? 1 : 0;
    data = data ? 1 : 0;
    if (Enabled_ && Clock_ && !clock)
    {
        Shift_ = static_cast<UINT8>((Shift_ << 1) | data);
        if (++BitCount_ == 8)
        {
            WriteByte(Shift_);
            Shift_ = 0;
            BitCount_ = 0;
        }
    }
    Clock_ = clock;
}

void MPU5Alpha::WriteBuffer(const UINT8* data, UINT32 length)
{
    if (!data) { return; }
    for (UINT32 i = 0; i < length; ++i) { WriteByte(data[i]); }
}

void MPU5Alpha::WriteByte(UINT8 data)
{
    if (!Enabled_) { return; }
    switch (data & 0xE0)
    {
    case 0xE0:
        Intensity_ = static_cast<UINT8>(data & 0x1F);
        return;
    case 0xA0:
        Position_ = static_cast<UINT8>(data & 0x0F);
        return;
    case 0xC0:
        DigitCount_ = static_cast<UINT8>(data & 0x1F);
        if (DigitCount_ == 0 || DigitCount_ > CharacterCount) { DigitCount_ = CharacterCount; }
        return;
    case 0x80:
        return; // Display test mode is deliberately not forced into the output RAM.
    default:
        break;
    }

    UINT8 character = static_cast<UINT8>(data & 0x3F);
    if (character == 0) { character = 0x20; }
    if (character == '.' || character == ',')
    {
        Characters_[Position_] = static_cast<UINT16>((Characters_[Position_] & 0x00FFU) | (static_cast<UINT16>(character) << 8));
        return;
    }

    Position_ = static_cast<UINT8>((Position_ + 1) & 0x0F);
    Characters_[Position_] = character;
}

UINT8 MPU5Alpha::GetCharacter(UINT8 character) const
{
    return character < CharacterCount ? static_cast<UINT8>(Characters_[character] & 0x3F) : 0x20;
}

UINT8 MPU5Alpha::GetDotComma(UINT8 character) const
{
    return character < CharacterCount ? static_cast<UINT8>(Characters_[character] >> 8) : 0;
}

UINT16 MPU5Alpha::GetSegments(UINT8 character) const
{
    return character < CharacterCount ? CharacterToSegments(GetCharacter(character)) : 0;
}

UINT16 MPU5Alpha::CharacterToSegments(UINT8 c)
{
    static constexpr UINT16 map[64] = {
        0x507F,0x44CF,0x153F,0x00F3,0x113F,0x40F3,0x40C3,0x04FB,
        0x44CC,0x1133,0x007C,0x4AC0,0x00F0,0x82CC,0x88CC,0x00FF,
        0x44C7,0x08FF,0x4CC7,0x44BB,0x1103,0x00FC,0x22C0,0x28CC,
        0xAA00,0x9200,0x2233,0x00E1,0x8800,0x001E,0x2800,0x0030,
        0x0000,0x8121,0x0180,0x553C,0x11BB,0x7799,0xC979,0x0200,
        0x0A00,0xA000,0xFF00,0x5500,0x0000,0x4400,0x0000,0x2200,
        0x22FF,0x1100,0x4477,0x443F,0x448C,0x44BB,0x44FB,0x000F,
        0x44FF,0x44BF,0x0021,0x2001,0x4430,0x4430,0x0312,0x1407
    };
    return map[c & 0x3F];
}
