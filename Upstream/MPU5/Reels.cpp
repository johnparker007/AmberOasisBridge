#include "Reels.h"

#include <algorithm>
#include <cstring>

namespace
{
constexpr UINT8 kRamps[8] = { 9, 8, 12, 4, 6, 2, 3, 1 };
constexpr UINT8 kInactiveBufferPosition = 0xFF;
// REEL5 is an independent processor. MFME's recovered model services it once
// per 1300 MPU5 instruction executions. The supplied long timing trace averages
// about 13.53 MPU5 clocks per instruction, giving approximately 17,584 clocks
// per REEL5 service. Real-machine comparison found 17,600 fractionally slow,
// so retain the conservative user-calibrated 17,280-clock period (925.93 Hz).
// This remains elapsed-time based so compiler optimisation and changing opcode
// mix cannot alter the physical reel speed.
constexpr UINT32 kReelUpdateCycles = 17280U;
constexpr INT8 kPhaseSteps[8][16] =
{
    { 0,  1,  3,  2, -3,  0,  0,  0, -1,  0,  0,  0, -2,  0,  0,  0 },
    { 0,  0,  2,  1,  0,  0,  3,  0, -2, -1,  0,  0, -3,  0,  0,  0 },
    { 0, -1,  1,  0,  3,  0,  2,  0, -3, -2,  0,  0,  0,  0,  0,  0 },
    { 0, -2,  0, -1,  2,  0,  1,  0,  0, -3,  0,  0,  3,  0,  0,  0 },
    { 0, -3, -1, -2,  1,  0,  0,  0,  3,  0,  0,  0,  2,  0,  0,  0 },
    { 0,  0, -2, -3,  0,  0, -1,  0,  2,  3,  0,  0,  1,  0,  0,  0 },
    { 0,  3, -3,  0, -1,  0, -2,  0,  1,  2,  0,  0,  0,  0,  0,  0 },
    { 0,  2,  0,  3, -2,  0, -3,  0,  0,  1,  0,  0, -1,  0,  0,  0 }
};
}

MPU5Reels::MPU5Reels()
{
    Steps_.fill(96);
    OptoStart_.fill(94);
    OptoEnd_.fill(2);
    Reset();
}

void MPU5Reels::SynchronisePhaseToMotor(Reel& reel)
{
    // The REEL5 command phase advances in the opposite direction to the native
    // motor position used by the generic stepper decoder. Reconstruct the phase
    // which corresponds to the retained mechanical position so the first 0,
    // +/-1 or +/-2 command after a controller reset cannot create a false step.
    const UINT8 motorPhase = static_cast<UINT8>(reel.MotorPosition & 7);
    const UINT8 commandPhase = static_cast<UINT8>((8U - motorPhase) & 7U);
    reel.PhasePosition = commandPhase;
    reel.Ramp = kRamps[commandPhase];
    reel.OldRamp = reel.Ramp;
}

void MPU5Reels::ResetController(Controller& controller, bool resetPhysical)
{
    controller.Opto = 0;
    controller.LastOpto = 0;
    controller.LampChanged = false;

    for (Reel& reel : controller.Reels)
    {
        const INT32 motorPosition = reel.MotorPosition;
        const INT32 outputPosition = reel.OutputPosition;
        reel = Reel{};
        reel.BufferPosition = kInactiveBufferPosition;
        if (!resetPhysical)
        {
            // A serial controller reset clears only the reel-board state in
            // MFME/hardware. The mechanical reel itself does not teleport.
            reel.MotorPosition = motorPosition;
            reel.OutputPosition = outputPosition;
        }
        SynchronisePhaseToMotor(reel);
    }
}

void MPU5Reels::Reset()
{
    for (Controller& controller : Controllers_)
    {
        ResetController(controller, true);
    }
    TickCycles_ = 0;
}

void MPU5Reels::Tick(UINT32 cycles)
{
    TickCycles_ += cycles;
    while (TickCycles_ >= kReelUpdateCycles)
    {
        TickCycles_ -= kReelUpdateCycles;
        RefreshOptos();
        UpdateController(0);
        UpdateController(1);
    }
}

void MPU5Reels::RefreshOptos()
{
    for (UINT8 controllerIndex = 0; controllerIndex < Controllers_.size(); ++controllerIndex)
    {
        Controller& controller = Controllers_[controllerIndex];
        controller.Opto = 0;

        for (UINT8 local = 0; local < controller.Reels.size(); ++local)
        {
            const UINT8 global = static_cast<UINT8>(controllerIndex * 5U + local);
            Reel& reel = controller.Reels[local];
            const UINT8 steps = Steps_[global] ? Steps_[global] : 96;
            const UINT8 position = static_cast<UINT8>((reel.MotorPosition % steps + steps) % steps);

            bool inOpto;
            if (OptoStart_[global] <= OptoEnd_[global])
            {
                inOpto = position >= OptoStart_[global] && position <= OptoEnd_[global];
            }
            else
            {
                inOpto = position >= OptoStart_[global] || position <= OptoEnd_[global];
            }

            if (OptoInvert_[global]) { inOpto = !inOpto; }
            reel.Opto = inOpto;

            const UINT8 mask = static_cast<UINT8>(8U << local);
            if (inOpto)
            {
                controller.LastOpto = static_cast<UINT8>(controller.LastOpto | mask);
            }
            else
            {
                controller.Opto = static_cast<UINT8>(controller.Opto | mask);
            }
        }
    }
}

void MPU5Reels::DriveReel(UINT8 globalReel, Reel& reel, UINT8 phase)
{
    // The reel-controller's Ramp value is a four-coil phase pattern. MFME does
    // not expose the controller's +/-2 command directly as physical movement;
    // it passes each changed phase through the generic stepper phase decoder.
    // Without this stage the frontend position merely oscillates around the
    // command phase and the reel visibly judders instead of rotating.
    const INT32 oldMotorPosition = reel.MotorPosition;
    const UINT8 phaseIndex = static_cast<UINT8>(oldMotorPosition & 7);
    const INT32 movement = kPhaseSteps[phaseIndex][phase & 0x0FU];
    reel.MotorPosition = (oldMotorPosition + movement + 96) % 96;

    if (reel.MotorPosition == oldMotorPosition) { return; }

    const INT32 configuredSteps = Steps_[globalReel] ? Steps_[globalReel] : 96;
    const INT32 nativePosition = (96 - reel.MotorPosition) % 96;
    reel.OutputPosition = configuredSteps == 96
        ? nativePosition
        : (nativePosition * configuredSteps) / 96;
}

void MPU5Reels::StepReel(UINT8 globalReel, Reel& reel, UINT8 difference)
{
    // This deliberately mirrors MFME's unsigned eight-bit arithmetic. Values
    // 0xFE and 0xFF represent -2 and -1 respectively and alter OptoSeen in the
    // same way as the original reel-controller model.
    UINT16 position = static_cast<UINT16>(reel.PhasePosition + difference);
    if (difference > 0x80U)
    {
        reel.OptoSeen = static_cast<UINT8>(reel.OptoSeen - 1U);
    }
    if (position > 0xFFU)
    {
        reel.OptoSeen = static_cast<UINT8>(reel.OptoSeen + 1U);
    }

    reel.PhasePosition = static_cast<UINT16>(position & 0xFFU);
    reel.Ramp = kRamps[reel.PhasePosition & 7U];
    if (reel.Ramp != reel.OldRamp)
    {
        reel.Changed = true;
        reel.OldRamp = reel.Ramp;
        DriveReel(globalReel, reel, reel.Ramp);
    }
}

void MPU5Reels::UpdateController(UINT8 controllerIndex)
{
    Controller& controller = Controllers_[controllerIndex];

    for (UINT8 local = 0; local < controller.Reels.size(); ++local)
    {
        Reel& reel = controller.Reels[local];
        reel.Changed = false;

        if (reel.Opto && (reel.PhasePosition & 7U) == 0U)
        {
            reel.PhasePosition = 0;
            reel.OptoSeen = 0;
        }

        // MFME latches every active-low opto transition until the controller's
        // opto status command has observed it.
        controller.LastOpto = static_cast<UINT8>(controller.LastOpto | static_cast<UINT8>(~controller.Opto));

        if (reel.BufferPosition == kInactiveBufferPosition) { continue; }

        --reel.Delay;
        if (reel.Delay >= 0) { continue; }

        bool finished = false;
        UINT32 guard = 0;
        do
        {
            if (reel.BufferPosition >= reel.Buffer.size() || guard++ >= reel.Buffer.size() * 4U)
            {
                reel.BufferPosition = kInactiveBufferPosition;
                break;
            }

            UINT8 value = reel.Buffer[reel.BufferPosition++];
            if ((value & 0x80U) != 0U)
            {
                switch (value & 7U)
                {
                case 6: // Repeat
                    if (reel.BufferPosition + 1U >= reel.Buffer.size())
                    {
                        reel.BufferPosition = kInactiveBufferPosition;
                        finished = true;
                        break;
                    }
                    {
                        UINT16 repeat = static_cast<UINT16>(
                            (static_cast<UINT16>(reel.Buffer[reel.BufferPosition]) << 8U) |
                            reel.Buffer[reel.BufferPosition + 1U]);
                        if (repeat != 0U)
                        {
                            --repeat;
                            reel.Buffer[reel.BufferPosition] = static_cast<UINT8>(repeat >> 8U);
                            reel.Buffer[reel.BufferPosition + 1U] = static_cast<UINT8>(repeat);

                            const UINT8 rewind = static_cast<UINT8>((value >> 3U) & 0x0FU);
                            const UINT32 current = reel.BufferPosition;
                            reel.BufferPosition = static_cast<UINT8>(
                                current > static_cast<UINT32>(rewind)
                                    ? current - static_cast<UINT32>(rewind) - 1U
                                    : 0U);
                        }
                        else
                        {
                            reel.BufferPosition = static_cast<UINT8>(reel.BufferPosition + 2U);
                        }
                    }
                    break;

                case 5: // Set ramp
                    reel.LastCommand = value;
                    if (reel.BufferPosition >= reel.Buffer.size())
                    {
                        reel.BufferPosition = kInactiveBufferPosition;
                        finished = true;
                        break;
                    }
                    value = reel.Buffer[reel.BufferPosition++];
                    reel.Ramp = kRamps[value & 7U];
                    break;

                case 7: // Finished
                    reel.BufferPosition = kInactiveBufferPosition;
                    reel.LastCommand = value;
                    finished = true;
                    break;

                default: // Step -2, -1, 0, 1 or 2
                    reel.SetDelay = true;
                    reel.LastCommand = value;
                    StepReel(
                        static_cast<UINT8>(controllerIndex * 5U + local),
                        reel,
                        static_cast<UINT8>((reel.LastCommand & 7U) - 2U));
                    break;
                }
            }
            else
            {
                if (!reel.SetDelay)
                {
                    StepReel(
                        static_cast<UINT8>(controllerIndex * 5U + local),
                        reel,
                        static_cast<UINT8>((reel.LastCommand & 7U) - 2U));
                }

                reel.SetDelay = false;
                if ((value & 0x40U) != 0U)
                {
                    if (reel.BufferPosition >= reel.Buffer.size())
                    {
                        reel.BufferPosition = kInactiveBufferPosition;
                        break;
                    }
                    reel.Delay = static_cast<INT32>(
                        reel.Buffer[reel.BufferPosition++] +
                        256U * static_cast<UINT32>(value & 0x3FU));
                }
                else
                {
                    reel.Delay = value;
                }
                finished = true;
            }
        } while (!finished);
    }
}

void MPU5Reels::RollChecksum(UINT16& total, UINT8 value)
{
    for (UINT8 i = 0; i < 8; ++i)
    {
        total = static_cast<UINT16>(((total ^ value) & 1U) ? ((total >> 1) ^ 0xA001U) : (total >> 1));
        value >>= 1;
    }
}

void MPU5Reels::WriteChecksum(UINT8* message, UINT32 length)
{
    if (!message || length < 3U) { return; }

    UINT16 total = 0;
    RollChecksum(total, 0x9BU);
    RollChecksum(total, 0xD9U);

    UINT8* cursor = message + 1;
    for (UINT32 i = 0; i < length - 3U; ++i)
    {
        RollChecksum(total, *cursor++);
    }

    *cursor++ = static_cast<UINT8>(total);
    RollChecksum(total, static_cast<UINT8>(total));
    *cursor = static_cast<UINT8>(total);
}

UINT32 MPU5Reels::AdjustRepeatCount(Controller& controller, UINT8* message, UINT32 length, UINT32 offset)
{
    // Every REEL5 Barbus frame ends with two checksum bytes. They are not part
    // of the command payload and must never be interpreted as an alternative
    // nudge-step value.
    const UINT32 payloadEnd = length >= 2U ? length - 2U : 0U;
    if (offset >= payloadEnd) { return 0; }

    const UINT8 reelIndex = message[offset++];
    if (reelIndex >= controller.Reels.size()) { return 0; }

    Reel& reel = controller.Reels[reelIndex];
    UINT32 totalSteps = 0;
    UINT32 position = reel.BufferPosition;

    if (position != kInactiveBufferPosition && position < reel.Buffer.size())
    {
        UINT8 command = reel.Buffer[position];
        UINT32 guard = 0;
        while ((command & 7U) != 6U && (command & 7U) != 7U)
        {
            if (guard++ >= reel.Buffer.size()) { return 0; }

            if ((command & 0x80U) == 0U)
            {
                if ((command & 0x40U) != 0U) { ++position; }
                ++position;
            }
            else
            {
                if ((command & 7U) == 5U) { ++position; }
                ++position;
            }

            if (position >= reel.Buffer.size()) { return 0; }
            command = reel.Buffer[position];
        }

        if ((command & 7U) != 7U)
        {
            ++position; // Step over the repeat command.
            if (position + 1U >= reel.Buffer.size() || offset + 1U >= payloadEnd) { return 0; }

            UINT32 steps = static_cast<UINT32>(message[offset + 1U]) |
                           (static_cast<UINT32>(message[offset]) << 8U);
            steps *= 2U;

            UINT32 repeat = static_cast<UINT32>(reel.Buffer[position + 1U]) |
                            (static_cast<UINT32>(reel.Buffer[position]) << 8U);
            if (steps != 0U)
            {
                // Equality is valid and important: an automatic nudge can ask
                // to consume exactly the number of repeats remaining. The old
                // strict comparison returned zero progress and left the reel
                // command active, so the game waited indefinitely.
                while (steps <= repeat)
                {
                    repeat -= steps;
                    totalSteps += steps;
                    reel.Buffer[position + 1U] = static_cast<UINT8>(repeat);
                    reel.Buffer[position] = static_cast<UINT8>(repeat >> 8U);
                }
            }

            offset += 2U;
            while (repeat != 0U && offset + 1U < payloadEnd)
            {
                steps = static_cast<UINT32>(message[offset + 1U]) |
                        (static_cast<UINT32>(message[offset]) << 8U);

                if (steps != 0U && steps <= repeat)
                {
                    repeat -= steps;
                    reel.Buffer[position + 1U] = static_cast<UINT8>(repeat);
                    reel.Buffer[position] = static_cast<UINT8>(repeat >> 8U);
                    totalSteps += steps;
                    break;
                }
                offset += 2U;
            }
        }
    }

    message[2] = 2;
    message[3] = static_cast<UINT8>(totalSteps >> 8U);
    message[4] = static_cast<UINT8>(totalSteps);
    return 7;
}

UINT32 MPU5Reels::ProcessMessage(UINT8 controllerIndex, UINT8* message, UINT32 length)
{
    if (controllerIndex >= Controllers_.size() || !message || length < 4U) { return 0; }

    Controller& controller = Controllers_[controllerIndex];
    UINT32 reply = 0;
    UINT32 offset = ((message[2] & 0x0FU) == 0x0FU) ? 4U : 3U;

    switch (message[2] & 0xF0U)
    {
    case 0x00: // Reset/ignored command
        ResetController(controller, false);
        message[2] = 0xF1;
        message[3] = 0xF0;
        reply = 6;
        break;

    case 0x10:
    case 0x50:
    case 0x60:
        break;

    case 0x20:
    case 0x30:
    case 0x40:
        message[2] = 0xF1;
        message[3] = 0xF1;
        reply = 6;
        break;

    case 0x70: // Opto state
        message[2] = 0x02;
        message[3] = static_cast<UINT8>(controller.Opto >> 3U);
        message[4] = static_cast<UINT8>(controller.LastOpto >> 3U);
        reply = 7;
        break;

    case 0x80: // Adjust repeat count
        reply = AdjustRepeatCount(controller, message, length, offset);
        break;

    case 0x90: // Reel command stream
        if (offset >= length || message[offset] >= controller.Reels.size()) { return 0; }
        {
            const UINT8 reelIndex = message[offset];
            Reel& reel = controller.Reels[reelIndex];
            const UINT32 payloadLength = length > offset + 2U ? length - offset - 2U : 0U;
            const UINT32 copyLength = std::min<UINT32>(reel.Buffer.size(), payloadLength);
            std::fill(reel.Buffer.begin(), reel.Buffer.end(), 0);
            if (copyLength != 0U)
            {
                std::memcpy(reel.Buffer.data(), message + offset + 1U, copyLength);
            }
            reel.BufferPosition = 0;
            reel.SetDelay = true;
            controller.LastOpto = static_cast<UINT8>(controller.LastOpto & ~(8U << reelIndex));
            message[2] = 0;
            reply = 5;
        }
        break;

    case 0xA0: // Current command pointer and reel position
        if (message[3] >= controller.Reels.size()) { return 0; }
        {
            const Reel& reel = controller.Reels[message[3]];
            message[3] = reel.BufferPosition;
            message[2] = 0x02;
            offset = 4;

            if (reel.PhasePosition < 0x7FU)
            {
                if (reel.OptoSeen != 0U)
                {
                    message[4] = reel.OptoSeen;
                    ++offset;
                }
            }
            else if (reel.OptoSeen != 0xFFU)
            {
                message[4] = reel.OptoSeen;
                ++offset;
            }

            message[offset] = static_cast<UINT8>(reel.PhasePosition);
            message[2] = static_cast<UINT8>(offset - 2U);
            reply = offset + 3U;
        }
        break;

    case 0xB0: // Reel lamps
        offset = 3;
        for (INT32 lampBit = 2; lampBit >= 0; --lampBit)
        {
            if (offset >= length) { return 0; }
            UINT8 mask = 0x08;
            for (UINT8 reelIndex = 0; reelIndex < controller.Reels.size(); ++reelIndex)
            {
                Reel& reel = controller.Reels[reelIndex];
                if ((message[offset] & mask) != 0U)
                {
                    reel.Lamp = static_cast<UINT8>(reel.Lamp | (1U << lampBit));
                }
                else
                {
                    reel.Lamp = static_cast<UINT8>(reel.Lamp & ~(1U << lampBit));
                }
                mask = static_cast<UINT8>(mask << 1U);
            }
            offset += 2U;
        }
        controller.LampChanged = true;
        message[2] = 0;
        reply = 5;
        break;

    case 0xC0:
        message[2] = 0;
        reply = 5;
        break;

    case 0xD0:
        message[2] = 1;
        message[3] = 0x00; // A0 is used by MFME for current-sense state.
        reply = 6;
        break;

    case 0xE0:
        message[2] = 0;
        reply = 5;
        break;

    case 0xF0: // Special commands
        switch (message[3])
        {
        case 0x12: // Version number
            message[2] = 0x03;
            message[3] = 0x01;
            message[4] = 0x00;
            message[5] = 0x03;
            reply = 8;
            break;

        case 0x15: // Set USER flags
        case 0x18: // Ignore
        case 0xFB:
        case 0xFC:
        case 0xFE:
            message[2] = 0;
            reply = 5;
            break;

        case 0xFA: // Reel-controller type and jumper state
            message[2] = 0x04;
            // The first returned byte identifies the fitted REEL5/controller
            // profile. 0x00 selects the established early profile. 0x90 is
            // the later profile represented by MFME's third radio option; it
            // makes the game select mechanism type 4 and its complete RAMP
            // tables. The remaining bytes retain the all-"out out" jumper
            // state used by existing MPU5 layouts.
            message[3] = JumperProfile_[controllerIndex] != 0U ? 0x90U : 0x00U;
            message[4] = 0xE0;
            message[5] = 0xD8;
            message[6] = 0xE0;
            reply = 9;
            break;

        case 0xFD:
            message[2] = 0x02;
            message[3] = 0x00;
            message[4] = 0x12;
            reply = 7;
            break;

        case 0x13:
        case 0x14:
        case 0x16:
        case 0x17:
        case 0xFF:
        default:
            break;
        }
        break;

    default:
        break;
    }

    if (reply != 0U) { WriteChecksum(message, reply); }
    return reply;
}

INT32 MPU5Reels::GetPosition(UINT8 reel) const
{
    if (reel >= ReelCount) { return 0; }
    return Controllers_[reel / 5U].Reels[reel % 5U].OutputPosition;
}

UINT8 MPU5Reels::GetReelLamp(UINT8 reel) const
{
    return reel < ReelCount ? Controllers_[reel / 5U].Reels[reel % 5U].Lamp : 0;
}

void MPU5Reels::SetOptoInvert(UINT8 reel, UINT8 value)
{
    if (reel < ReelCount) { OptoInvert_[reel] = value ? 1 : 0; }
}

void MPU5Reels::SetOptoStart(UINT8 reel, UINT8 value)
{
    if (reel < ReelCount) { OptoStart_[reel] = value; }
}

void MPU5Reels::SetOptoEnd(UINT8 reel, UINT8 value)
{
    if (reel < ReelCount) { OptoEnd_[reel] = value; }
}

void MPU5Reels::SetSteps(UINT8 reel, UINT8 value)
{
    if (reel < ReelCount && value) { Steps_[reel] = value; }
}

void MPU5Reels::SetJumperProfile(UINT8 controller, UINT8 profile)
{
    if (controller < JumperProfile_.size())
        JumperProfile_[controller] = profile != 0U ? 1U : 0U;
}
