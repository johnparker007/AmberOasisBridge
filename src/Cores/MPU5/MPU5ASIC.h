#pragma once

#include "PA2CoreInterface.h"

class MPU5ASIC
{
public:
    struct Changes
    {
        bool HighSide = false;
        bool LowSide = false;
        bool LEDs = false;
        bool Coin = false;
        bool Meters = false;
        bool Status = false;
        bool DSPCommand = false;
    };

    void Reset();
    Changes Write(UINT8 offset, UINT8 value);
    UINT8 Read(UINT8 offset, UINT8 switchNibble, UINT8 coinInputs, UINT8 pinInputs);

    UINT8 GetHighSide() const { return HighSide_; }
    UINT8 GetLowSide() const { return LowSide_; }
    UINT8 GetLowSelect() const { return LowSelect_; }
    UINT8 GetLEDs() const { return LEDs_; }
    UINT8 GetLEDSelect() const { return LEDSelect_; }
    UINT8 GetCoin() const { return Coin_; }
    UINT8 GetMeters() const { return Meters_; }
    UINT8 GetStatusLED() const { return StatusLED_; }
    void SetDSPResult(UINT8 value) { DSPResult_ = value; }
    void SetDSPStatus(UINT8 value) { DSPStatus_ = value; }
    void SetDSPInitialised(bool value) { DSPInitialised_ = value; }
    bool GetDSPInitialised() const { return DSPInitialised_; }
    void SetHardwareRevision(UINT8 value) { HardwareRevision_ = static_cast<UINT8>(value & 0x1FU); }
    void SetDSPRevision(UINT8 value) { DSPRevision_ = static_cast<UINT8>(value & 0x3FU); }

private:
    static UINT8 DecodeHighOneHot(UINT8 value);
    static UINT8 DecodeLowOneHot(UINT8 value);

    UINT8 HighSide_ = 0;
    UINT8 LowSide_ = 0xFF;
    UINT8 LowSelect_ = 0;
    UINT8 LEDs_ = 0;
    UINT8 LEDSelect_ = 0;
    UINT8 Coin_ = 0;
    UINT8 Meters_ = 0;
    UINT8 StatusLED_ = 0;
    UINT8 DSPResult_ = 0x99;
    UINT8 DSPStatus_ = 0;
    bool DSPInitialised_ = false;
    UINT8 HardwareRevision_ = 0;
    UINT8 DSPRevision_ = 5;
    UINT16 Checksum_ = 0;
};
