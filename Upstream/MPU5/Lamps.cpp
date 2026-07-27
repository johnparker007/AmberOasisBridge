#include "Lamps.h"

#include <algorithm>
#include <cmath>

void MPU5Lamps::Reset()
{
    ActiveDriveMasks_.fill(0);
    for (auto& columns : ExternalFullColumns_) columns.fill(0);
    for (auto& columns : ExternalDimColumns_) columns.fill(0);
    for (auto& columns : ExternalTestDimColumns_) columns.fill(0);
    CurrentTestActive_.fill(false);
    NormalDimSuppressed_.fill(false);
    DrivenCycles_.fill(0);
    LastDuty_.fill(0.0f);
    LastVoltageRMS_.fill(0.0f);
    MatrixDriveStartCycles_.fill(0);

    TotalCycles_ = 0;
    NextDimOffCycle_ = DimPulseCycles;
    NextSelectCycle_ = SelectPeriodCycles;
    ExternalSelect_ = 0;
    DimPulseActive_ = true;

    for (FilamentLamp& lamp : Filaments_)
    {
        lamp.ConfigureDefault();
        lamp.ResetRuntime();
    }
}

void MPU5Lamps::AdvanceMatrixUntil(UINT32 matrix, UINT64 cycle)
{
    if (matrix >= MatrixCount || cycle <= MatrixDriveStartCycles_[matrix]) return;

    const UINT64 elapsed = cycle - MatrixDriveStartCycles_[matrix];
    const float seconds = static_cast<float>(elapsed) /
        static_cast<float>(CPUCyclesPerSecond);
    const UINT64 driveMask = ActiveDriveMasks_[matrix];
    const UINT32 firstLamp = matrix * LampsPerMatrix;

    for (UINT32 bit = 0; bit < LampsPerMatrix; ++bit)
    {
        const UINT32 lampIndex = firstLamp + bit;
        FilamentLamp& lamp = Filaments_[lampIndex];
        const bool driven = (driveMask & (UINT64(1) << bit)) != 0U;

        if (driven)
        {
            DrivenCycles_[lampIndex] += elapsed;
            lamp.Advance(seconds, LampSupplyVoltage);
        }
        else if (!lamp.IsColdAndDark())
        {
            lamp.Advance(seconds, 0.0f);
        }
    }

    MatrixDriveStartCycles_[matrix] = cycle;
}

void MPU5Lamps::AdvanceExternalUntil(UINT64 cycle)
{
    for (UINT32 matrix = 1U; matrix < MatrixCount; ++matrix)
        AdvanceMatrixUntil(matrix, cycle);
}

void MPU5Lamps::CompleteElectricalFrame(UINT64 boundaryCycle)
{
    // The external matrices have already been advanced to this scan boundary.
    // Bring the independently driven onboard matrix to the same point before
    // publishing one complete 16 ms set of electrical diagnostics.
    AdvanceMatrixUntil(0U, boundaryCycle);

    const float frameCycles = static_cast<float>(ElectricalFrameCycles);
    for (UINT32 lamp = 0; lamp < LampCount; ++lamp)
    {
        const float duty = std::min(1.0f,
            static_cast<float>(DrivenCycles_[lamp]) / frameCycles);
        LastDuty_[lamp] = duty;
        LastVoltageRMS_[lamp] = LampSupplyVoltage * std::sqrt(duty);
        DrivenCycles_[lamp] = 0;
    }
}

void MPU5Lamps::RefreshExternalDriveMasks()
{
    const UINT8 rowBit = static_cast<UINT8>(1U << ExternalSelect_);
    for (UINT32 matrix = 0; matrix < ExternalMatrixCount; ++matrix)
    {
        UINT64 mask = 0;
        for (UINT32 column = 0; column < 8U; ++column)
        {
            const bool active =
                (ExternalFullColumns_[matrix][column] & rowBit) != 0U;
            const UINT32 mux = matrix / 2U;
            const UINT8 normalDimData =
                mux < NormalDimSuppressed_.size() && NormalDimSuppressed_[mux]
                ? 0U : ExternalDimColumns_[matrix][column];
            const bool normalDimBit = (normalDimData & rowBit) != 0U;
            const bool testDimBit =
                (ExternalTestDimColumns_[matrix][column] & rowBit) != 0U;

            bool full = active;
            bool dim = false;
            switch (ExternalProtocol_)
            {
            case ExternalProtocol::SinglePlane:
                break;

            case ExternalProtocol::IndependentDimPlane:
                // PIC2: either latch can drive the lamp. Full drive wins while
                // both are asserted.
                dim = normalDimBit || testDimBit;
                break;

            case ExternalProtocol::BrightnessMask:
                // PIC3: the first byte is the on/off state and the second byte
                // marks a subset of those active lamps for reduced duty. It is
                // not an independent plane and must never keep an off lamp lit.
                dim = (active && normalDimBit) || testDimBit;
                full = active && !normalDimBit;
                break;
            }

            if (full || (DimPulseActive_ && dim))
            {
                mask |= UINT64(1) <<
                    (static_cast<UINT32>(ExternalSelect_) * 8U + column);
            }
        }
        ActiveDriveMasks_[matrix + 1U] = mask;
    }
}

void MPU5Lamps::Tick(UINT32 cycles)
{
    if (cycles == 0U) return;

    const UINT64 target = TotalCycles_ + cycles;
    while (true)
    {
        const UINT64 nextEvent = std::min(NextDimOffCycle_, NextSelectCycle_);
        if (target < nextEvent) break;

        AdvanceExternalUntil(nextEvent);

        if (NextDimOffCycle_ <= NextSelectCycle_)
        {
            // Dim plane: a genuine 1 ms pulse in every 2 ms row slot. This
            // avoids alternate-frame aliasing and gives diagnostics a stable
            // 1/16 duty every completed electrical frame.
            DimPulseActive_ = false;
            RefreshExternalDriveMasks();
            NextDimOffCycle_ = NextSelectCycle_ + DimPulseCycles;
        }
        else
        {
            ExternalSelect_ = static_cast<UINT8>((ExternalSelect_ + 1U) & 7U);
            if (ExternalSelect_ == 0U)
                CompleteElectricalFrame(nextEvent);

            DimPulseActive_ = true;
            RefreshExternalDriveMasks();
            NextSelectCycle_ += SelectPeriodCycles;
        }
    }

    TotalCycles_ = target;
}


void MPU5Lamps::SetExternalProtocol(ExternalProtocol protocol)
{
    if (ExternalProtocol_ == protocol) return;

    // Finish the waveform produced by the previous interpretation before
    // changing how the second sparse field is decoded. Clear the auxiliary
    // latches so data from one PIC generation cannot leak into another after
    // ROM auto-detection changes the effective program-card type.
    AdvanceExternalUntil(TotalCycles_);
    ExternalProtocol_ = protocol;
    for (auto& columns : ExternalDimColumns_) columns.fill(0U);
    for (auto& columns : ExternalTestDimColumns_) columns.fill(0U);
    CurrentTestActive_.fill(false);
    NormalDimSuppressed_.fill(false);
    RefreshExternalDriveMasks();
}

void MPU5Lamps::SetDimPlaneEnabled(bool enabled)
{
    SetExternalProtocol(enabled
        ? ExternalProtocol::IndependentDimPlane
        : ExternalProtocol::SinglePlane);
}

void MPU5Lamps::BeginCurrentTest(UINT8 mux)
{
    if (mux >= CurrentTestActive_.size()) return;

    // Starting the next test also removes any temporary preheat left by an
    // interrupted previous transaction. Preserve the exact waveform up to the
    // latch transition before clearing the overlay.
    const UINT32 firstMatrix = static_cast<UINT32>(mux) * 2U;
    for (UINT32 index = 0; index < 2U; ++index)
    {
        const UINT32 matrix = firstMatrix + index;
        if (matrix >= ExternalMatrixCount) continue;
        AdvanceMatrixUntil(matrix + 1U, TotalCycles_);
        ExternalTestDimColumns_[matrix].fill(0U);
    }
    CurrentTestActive_[mux] = true;
    // Only PIC2 has an independent normal dim latch capable of keeping a lamp
    // energised while the current-test overlay is active. PIC3 dim metadata is
    // already gated by the active-state byte, so suppressing it would turn
    // legitimate dim lamps bright after test 3.1.
    NormalDimSuppressed_[mux] =
        ExternalProtocol_ == ExternalProtocol::IndependentDimPlane;
    RefreshExternalDriveMasks();
}

void MPU5Lamps::EndCurrentTest(UINT8 mux)
{
    if (mux >= CurrentTestActive_.size()) return;

    const UINT32 firstMatrix = static_cast<UINT32>(mux) * 2U;
    for (UINT32 index = 0; index < 2U; ++index)
    {
        const UINT32 matrix = firstMatrix + index;
        if (matrix >= ExternalMatrixCount) continue;
        AdvanceMatrixUntil(matrix + 1U, TotalCycles_);
        ExternalTestDimColumns_[matrix].fill(0U);
    }
    CurrentTestActive_[mux] = false;
    RefreshExternalDriveMasks();
}

void MPU5Lamps::SetMatrixDrive(UINT8 highSide, UINT8 lowSelect)
{
    AdvanceMatrixUntil(0U, TotalCycles_);

    UINT64 mask = 0;
    if (lowSelect >= 1U && lowSelect <= 8U)
    {
        const UINT32 base = static_cast<UINT32>(lowSelect - 1U) * 8U;
        for (UINT32 drive = 0; drive < 8U; ++drive)
        {
            if ((highSide & (1U << drive)) != 0U)
                mask |= UINT64(1) << (base + drive);
        }
    }

    ActiveDriveMasks_[0] = mask;
}

void MPU5Lamps::WriteExternalColumn(
    UINT32 matrix, UINT32 column, UINT8 fullData, UINT8 dimData)
{
    if (matrix >= ExternalMatrixCount || column >= 8U) return;

    // Preserve the exact old waveform up to the Barbus latch transition.
    AdvanceMatrixUntil(matrix + 1U, TotalCycles_);

    // PIC2 has two independent lamp latches. Full drive wins during the second
    // half of the row without deleting the reduced-duty command. PIC1's second
    // sparse field is consumed for packet alignment but is not a lamp plane.
    ExternalFullColumns_[matrix][column] = fullData;
    ExternalDimColumns_[matrix][column] =
        ExternalProtocol_ == ExternalProtocol::SinglePlane ? 0U : dimData;
    RefreshExternalDriveMasks();
}

UINT32 MPU5Lamps::WriteBuffer(
    const UINT8* data, UINT32 available, UINT16 base, UINT8 mux)
{
    if (!data || available < 2U) return 0U;

    const UINT8 fullMask = data[0];
    const UINT8 dimMask = data[1];
    UINT32 consumed = 2U;
    const UINT32 outputBase = static_cast<UINT32>(base) +
        static_cast<UINT32>(mux) * 128U;
    if (outputBase < OnboardLampCount || outputBase >= LampCount)
        return consumed;

    const UINT32 matrix =
        (outputBase - OnboardLampCount) / LampsPerMatrix;

    // MUX5 serialises the sparse planes interleaved by column: full byte first,
    // then dim byte for that same column. Read all selected bytes before
    // changing the latch so a packet is electrically atomic.
    std::array<UINT8, 8> full{};
    std::array<UINT8, 8> dim{};
    std::array<bool, 8> fullChanged{};
    std::array<bool, 8> dimChanged{};

    for (UINT32 column = 0; column < 8U; ++column)
    {
        const UINT8 bit = static_cast<UINT8>(0x80U >> column);
        if ((fullMask & bit) != 0U)
        {
            if (consumed >= available) return consumed;
            full[column] = data[consumed++];
            fullChanged[column] = true;
        }
        if ((dimMask & bit) != 0U)
        {
            if (consumed >= available) return consumed;
            dim[column] = data[consumed++];
            dimChanged[column] = true;
        }
    }

    for (UINT32 column = 0; column < 8U; ++column)
    {
        if (!fullChanged[column] && !dimChanged[column]) continue;
        const UINT8 newFull = fullChanged[column]
            ? full[column] : ExternalFullColumns_[matrix][column];

        if (mux < CurrentTestActive_.size() && CurrentTestActive_[mux])
        {
            // Current-test preheat is a temporary overlay. Full-only sample and
            // clear writes must not erase the normal PIC3 brightness metadata;
            // only a supplied second-field byte changes the test overlay.
            AdvanceMatrixUntil(matrix + 1U, TotalCycles_);
            ExternalFullColumns_[matrix][column] = newFull;
            if (dimChanged[column])
            {
                ExternalTestDimColumns_[matrix][column] =
                    ExternalProtocol_ == ExternalProtocol::SinglePlane
                    ? 0U : dim[column];
            }
            RefreshExternalDriveMasks();
        }
        else
        {
            UINT8 newDim = 0U;
            if (ExternalProtocol_ == ExternalProtocol::IndependentDimPlane)
            {
                // PIC2 sparse fields are independent latches: an omitted dim
                // column retains its previous reduced-duty state.
                newDim = dimChanged[column]
                    ? dim[column] : ExternalDimColumns_[matrix][column];
            }
            else if (ExternalProtocol_ == ExternalProtocol::BrightnessMask)
            {
                // PIC3's second mask is optional brightness metadata attached
                // to the active-state update. If it is omitted for a changed
                // column the lamps are bright, not a stale copy of an earlier
                // dim mask. A dim marker is meaningful only while that lamp is
                // active in the first byte.
                newDim = dimChanged[column] ? dim[column] : 0U;
                newDim = static_cast<UINT8>(newDim & newFull);
            }

            // A non-test nonzero dim command marks the return to normal game
            // output and restores the ordinary dim data after lamp testing.
            if (dimChanged[column] && dim[column] != 0U &&
                mux < NormalDimSuppressed_.size())
            {
                NormalDimSuppressed_[mux] = false;
            }
            WriteExternalColumn(matrix, column, newFull, newDim);
        }
    }

    return consumed;
}

void MPU5Lamps::SetBroken(UINT16 lamp, bool broken)
{
    if (lamp < Broken_.size()) Broken_[lamp] = broken;
}

bool MPU5Lamps::BufferHasHealthyActiveLamp(
    const UINT8* data, UINT32 available, UINT16 base, UINT8 mux) const
{
    if (!data || available < 2U) return false;

    const UINT32 outputBase = static_cast<UINT32>(base) +
        static_cast<UINT32>(mux) * 128U;
    if (outputBase < OnboardLampCount || outputBase >= LampCount)
        return false;

    const UINT8 fullMask = data[0];
    const UINT8 dimMask = data[1];
    UINT32 consumed = 2U;

    for (UINT32 column = 0; column < 8U; ++column)
    {
        const UINT8 maskBit = static_cast<UINT8>(0x80U >> column);
        for (UINT32 plane = 0; plane < 2U; ++plane)
        {
            const UINT8 planeMask = plane == 0U ? fullMask : dimMask;
            if ((planeMask & maskBit) == 0U) continue;
            if (consumed >= available) return false;

            const UINT8 rows = data[consumed++];
            for (UINT32 row = 0; row < 8U; ++row)
            {
                if ((rows & (1U << row)) == 0U) continue;
                const UINT32 lamp = outputBase + row * 8U + column;
                if (lamp < Broken_.size() && !Broken_[lamp]) return true;
            }
        }
    }

    return false;
}

UINT8 MPU5Lamps::IsOn(UINT16 lamp) const
{
    return lamp < Filaments_.size() && Filaments_[lamp].IsLit() ? 1U : 0U;
}

float MPU5Lamps::GetBrightness(UINT16 lamp) const
{
    return lamp < Filaments_.size() ? Filaments_[lamp].GetBrightness() : 0.0f;
}

float3 MPU5Lamps::GetColour(UINT16 lamp) const
{
    return lamp < Filaments_.size() ? Filaments_[lamp].GetColour() : float3();
}

float MPU5Lamps::GetTemperatureK(UINT16 lamp) const
{
    return lamp < Filaments_.size() ? Filaments_[lamp].GetTemperatureK() : 0.0f;
}

float MPU5Lamps::GetResistanceOhms(UINT16 lamp) const
{
    return lamp < Filaments_.size() ? Filaments_[lamp].GetResistanceOhms() : 0.0f;
}

float MPU5Lamps::GetElectricalPowerW(UINT16 lamp) const
{
    return lamp < Filaments_.size() ? Filaments_[lamp].GetElectricalPowerW() : 0.0f;
}

float MPU5Lamps::GetDuty(UINT16 lamp) const
{
    return lamp < LastDuty_.size() ? LastDuty_[lamp] : 0.0f;
}

float MPU5Lamps::GetVoltageRMS(UINT16 lamp) const
{
    return lamp < LastVoltageRMS_.size() ? LastVoltageRMS_[lamp] : 0.0f;
}
