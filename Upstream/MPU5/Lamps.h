#pragma once

#include "PA2CoreInterface.h"
#include "FilamentLamp.h"
#include <array>

class MPU5Lamps
{
public:
    enum class ExternalProtocol : UINT8
    {
        SinglePlane = 0,
        IndependentDimPlane = 1,
        BrightnessMask = 2
    };

    static constexpr UINT32 LampCount = 320;
    static constexpr UINT32 LampsPerMatrix = 64;
    static constexpr UINT32 MatrixCount = LampCount / LampsPerMatrix;
    static constexpr UINT32 OnboardLampCount = LampsPerMatrix;
    static constexpr UINT32 ExternalMatrixCount = MatrixCount - 1U;

    void Reset();
    void Tick(UINT32 cycles);
    void SetMatrixDrive(UINT8 highSide, UINT8 lowSelect);
    void SetExternalProtocol(ExternalProtocol protocol);
    // Compatibility wrapper retained for local tests and older internal code.
    void SetDimPlaneEnabled(bool enabled);
    void BeginCurrentTest(UINT8 mux);
    void EndCurrentTest(UINT8 mux);
    UINT32 WriteBuffer(const UINT8* data, UINT32 available, UINT16 base, UINT8 mux);
    void SetBroken(UINT16 lamp, bool broken);
    bool BufferHasHealthyActiveLamp(
        const UINT8* data, UINT32 available, UINT16 base, UINT8 mux) const;

    UINT8 IsOn(UINT16 lamp) const;
    float GetBrightness(UINT16 lamp) const;
    float3 GetColour(UINT16 lamp) const;
    float GetTemperatureK(UINT16 lamp) const;
    float GetResistanceOhms(UINT16 lamp) const;
    float GetElectricalPowerW(UINT16 lamp) const;
    float GetDuty(UINT16 lamp) const;
    float GetVoltageRMS(UINT16 lamp) const;

private:
    static constexpr UINT32 CPUCyclesPerSecond = 16000000U;
    static constexpr UINT32 SelectPeriodCycles = CPUCyclesPerSecond * 2U / 1000U;
    static constexpr UINT32 DimPulseCycles = SelectPeriodCycles / 2U;
    static constexpr UINT32 ElectricalFrameCycles = SelectPeriodCycles * 8U;
    static constexpr float LampSupplyVoltage = 34.0f;

    void AdvanceMatrixUntil(UINT32 matrix, UINT64 cycle);
    void AdvanceExternalUntil(UINT64 cycle);
    void CompleteElectricalFrame(UINT64 boundaryCycle);
    void RefreshExternalDriveMasks();
    void WriteExternalColumn(UINT32 matrix, UINT32 column, UINT8 fullData, UINT8 dimData);

    std::array<FilamentLamp, LampCount> Filaments_{};
    // Configuration only: a broken bulb still receives drive and remains visible
    // in the layout, but must not contribute to lamp-current feedback.
    std::array<bool, LampCount> Broken_{};
    std::array<UINT64, MatrixCount> ActiveDriveMasks_{};

    // MUX5 supplies complete 8x8 matrices rather than individual boolean
    // lamps. Each sparse packet updates one or more matrix columns. PIC2 uses
    // the two fields as independent full/dim latches. PIC3 uses the first field
    // as the active-lamp byte and the second as a dim attribute mask for those
    // active lamps.
    std::array<std::array<UINT8, 8>, ExternalMatrixCount> ExternalFullColumns_{};
    std::array<std::array<UINT8, 8>, ExternalMatrixCount> ExternalDimColumns_{};

    // Lamp-current test 3.1 temporarily uses the reduced-duty plane to warm a
    // selected bulb before the full-current sample. Keep that temporary drive
    // separate from the normal dim latch so it can be removed after the test
    // without disturbing legitimate game lamp states.
    std::array<std::array<UINT8, 8>, ExternalMatrixCount> ExternalTestDimColumns_{};
    std::array<bool, 2> CurrentTestActive_{};
    std::array<bool, 2> NormalDimSuppressed_{};

    std::array<UINT64, LampCount> DrivenCycles_{};
    std::array<float, LampCount> LastDuty_{};
    std::array<float, LampCount> LastVoltageRMS_{};
    std::array<UINT64, MatrixCount> MatrixDriveStartCycles_{};

    UINT64 TotalCycles_ = 0;
    UINT64 NextDimOffCycle_ = DimPulseCycles;
    UINT64 NextSelectCycle_ = SelectPeriodCycles;
    UINT8 ExternalSelect_ = 0;
    bool DimPulseActive_ = true;
    ExternalProtocol ExternalProtocol_ = ExternalProtocol::SinglePlane;
};
