#include "MPU5ASIC.h"

void MPU5ASIC::Reset()
{
    HighSide_ = 0;
    LowSide_ = 0xFF;
    LowSelect_ = 0;
    LEDs_ = 0;
    LEDSelect_ = 0;
    Coin_ = 0;
    Meters_ = 0;
    StatusLED_ = 0;
    DSPResult_ = 0x99;
    DSPStatus_ = 0;
    DSPInitialised_ = false;
    Checksum_ = 0;
}

UINT8 MPU5ASIC::DecodeHighOneHot(UINT8 value)
{
    if (!value || (value & static_cast<UINT8>(value - 1U))) { return 0; }
    UINT8 result = 1;
    while ((value >>= 1U) != 0) { ++result; }
    return result;
}

UINT8 MPU5ASIC::DecodeLowOneHot(UINT8 value)
{
    return DecodeHighOneHot(static_cast<UINT8>(~value));
}

MPU5ASIC::Changes MPU5ASIC::Write(UINT8 offset, UINT8 value)
{
    Changes changes;
    switch (offset & 0x0F)
    {
    case 0x1: changes.HighSide = HighSide_ != value; HighSide_ = value; break;
    case 0x3: changes.LEDs = LEDs_ != value; LEDs_ = value; LEDSelect_ = DecodeHighOneHot(value); break;
    case 0x5: changes.LowSide = LowSide_ != value; LowSide_ = value; LowSelect_ = DecodeLowOneHot(value); break;
    case 0x7: changes.Coin = Coin_ != value; Coin_ = value; break;
    case 0x9: changes.Meters = Meters_ != value; Meters_ = value; break;
    case 0xB:
        changes.Status = StatusLED_ != (value & 0x30U);
        StatusLED_ = static_cast<UINT8>(value & 0x30U);
        changes.DSPCommand = (value & 0x40U) != 0;
        break;
    case 0xD:
        for (UINT8 count = 0; count < 8; ++count)
        {
            Checksum_ = static_cast<UINT16>(((Checksum_ ^ value) & 1U) ? ((Checksum_ >> 1) ^ 0xA001U) : (Checksum_ >> 1));
            value >>= 1;
        }
        break;
    default:
        break;
    }
    return changes;
}

UINT8 MPU5ASIC::Read(UINT8 offset, UINT8 switchNibble, UINT8 coinInputs, UINT8 pinInputs)
{
    switch (offset & 0x0F)
    {
    case 0x1: return DSPResult_;
    case 0x2: return DSPInitialised_
        ? DSPStatus_
        : static_cast<UINT8>(0x80U | DSPRevision_);
    case 0x8: case 0xA: return static_cast<UINT8>(Checksum_);
    case 0xB: return HardwareRevision_;
    case 0xD: return static_cast<UINT8>((switchNibble << 4) | (pinInputs & 0x0F));
    case 0xE: return 0xFF;
    case 0xF: return static_cast<UINT8>(~coinInputs);
    default: return 0;
    }
}
