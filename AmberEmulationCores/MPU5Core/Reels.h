#pragma once

#include "PA2CoreInterface.h"

#include <array>

class MPU5Reels
{
public:
    static constexpr UINT8 ReelCount = 10;

    MPU5Reels();
    void Reset();
    void Tick(UINT32 cycles);
    UINT32 ProcessMessage(UINT8 controller, UINT8* message, UINT32 length);

    INT32 GetPosition(UINT8 reel) const;
    UINT8 GetReelLamp(UINT8 reel) const;
    void SetOptoInvert(UINT8 reel, UINT8 value);
    void SetOptoStart(UINT8 reel, UINT8 value);
    void SetOptoEnd(UINT8 reel, UINT8 value);
    void SetSteps(UINT8 reel, UINT8 value);
    void SetJumperProfile(UINT8 controller, UINT8 profile);

private:
    struct Reel
    {
        INT32 Delay = 0;
        UINT8 BufferPosition = 0xFF;
        std::array<UINT8, 50> Buffer{};
        UINT16 PhasePosition = 0;
        INT32 MotorPosition = 0;
        INT32 OutputPosition = 0;
        UINT8 LastCommand = 0;
        UINT8 Ramp = 0;
        UINT8 OldRamp = 0;
        bool SetDelay = false;
        bool Opto = false;
        bool Changed = false;
        UINT8 OptoSeen = 0;
        UINT8 Lamp = 0;
    };

    struct Controller
    {
        std::array<Reel, 5> Reels{};
        UINT8 LastOpto = 0;
        UINT8 Opto = 0;
        bool LampChanged = false;
    };

    static void RollChecksum(UINT16& total, UINT8 value);
    static void WriteChecksum(UINT8* message, UINT32 length);
    static void SynchronisePhaseToMotor(Reel& reel);
    static void ResetController(Controller& controller, bool resetPhysical);
    static UINT32 AdjustRepeatCount(Controller& controller, UINT8* message, UINT32 length, UINT32 offset);
    void UpdateController(UINT8 controller);
    void StepReel(UINT8 globalReel, Reel& reel, UINT8 difference);
    void DriveReel(UINT8 globalReel, Reel& reel, UINT8 phase);
    void RefreshOptos();

    std::array<Controller, 2> Controllers_{};
    std::array<UINT8, ReelCount> Steps_{};
    std::array<UINT8, ReelCount> OptoInvert_{};
    std::array<UINT8, ReelCount> OptoStart_{};
    std::array<UINT8, ReelCount> OptoEnd_{};
    // 0 = established early REEL5 response, 1 = later controller response.
    // The later response is the third MFME controller-profile selection and
    // is required by PIC3 games such as Alien for their full ramp tables.
    std::array<UINT8, 2> JumperProfile_{};
    UINT32 TickCycles_ = 0;
};
