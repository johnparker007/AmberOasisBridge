#pragma once

#include "LoadSave.h"

struct float3 {
    float x, y, z;

    float3() : x(0.0f), y(0.0f), z(0.0f) {}
    float3(float scalar) : x(scalar), y(scalar), z(scalar) {}
    float3(float xIn, float yIn, float zIn) : x(xIn), y(yIn), z(zIn) {}

    float3& operator/=(float rhs) {
        x /= rhs;
        y /= rhs;
        z /= rhs;
        return *this;
    }
};

struct FilamentLampParams {
    float AmbientK;
    float NominalVoltageRMS;
    float NominalPowerW;
    float NominalFilamentK;
    float ResistanceTemperatureExponent;
    float Emissivity;
    float RadiativeLossFractionAtNominal;
    float ThermalMassJPerK;
    float VisibleStartK;
    float VisibleFullK;
    float BrightnessGamma;
    float BrightnessScale;
    float MaxBrightness;          // Normal full brightness is 1.0; short overdrive may exceed this.
    float MaxTemperatureK;

    // External electrical losses in the lamp drive path.  This lets the lamp
    // power still come from the matrix voltage while avoiding an ideal, zero-
    // impedance source that would unrealistically exaggerate cold inrush.
    float SeriesResistanceOhms;
    float DriverVoltageDropV;
};

class FilamentLamp {
private:
    FilamentLampParams Params;

    float TemperatureK;
    float ResistanceOhms;
    float ElectricalPowerW;
    float Brightness01;
    float3 Colour01;

    float ColdResistanceOhms;
    float HotResistanceOhms;
    float EffectiveRadiatingAreaM2;
    float ConductanceWPerK;

    void __fastcall RecalculateDerivedParams(void);
    void __fastcall IntegrateStep(float dtSeconds, float voltageRMS);
    void __fastcall UpdateOptical(void);
    
    

public:
    FilamentLamp();

    void __fastcall ConfigureDefault(void);
    void __fastcall Configure(const FilamentLampParams& params);
    void __fastcall ResetRuntime(void);
    void __fastcall Advance(float dtSeconds, float voltageRMS);

    float __fastcall GetTemperatureK(void) const;
    float __fastcall GetBrightness(void) const;
    bool __fastcall IsLit(void) const;
    bool __fastcall IsColdAndDark(void) const;
    float3 __fastcall GetColour(void) const;

    void __fastcall VoltageDrop(bool enable);

    void __fastcall SaveState(LoadSaveClass* LSC);
    void __fastcall LoadState(LoadSaveClass* LSC);
};
