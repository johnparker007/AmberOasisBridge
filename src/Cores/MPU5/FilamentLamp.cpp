#include "stdafx.h"
#include "FilamentLamp.h"
#include <math.h>

#define FILAMENT_STEFAN_BOLTZMANN 5.670374419e-8f
#define FILAMENT_MAX_SUBSTEP_SECONDS 0.001f
#define FILAMENT_MAX_INTEGRATION_STEPS 4096U
#define FILAMENT_LIT_THRESHOLD 0.003f

// Lookup tables keep the same physical model but avoid repeated powf/logf work
// while many lamps are being advanced.  The JPM core currently uses one lamp
// model for all matrix lamps, so one parameter-specific table cache is enough.
#define FILAMENT_LOOKUP_MIN_K 250.0f
#define FILAMENT_LOOKUP_MAX_K 3700.0f
#define FILAMENT_LOOKUP_STEP_K 1.0f
#define FILAMENT_LOOKUP_COUNT 3451U

static bool gLookupValid = false;
static FilamentLampParams gLookupParams;
static float gLookupColdResistanceOhms = 0.0f;
static float gLookupEffectiveRadiatingAreaM2 = 0.0f;
static float gLookupResistanceOhms[FILAMENT_LOOKUP_COUNT];
static float gLookupRadiationLossW[FILAMENT_LOOKUP_COUNT];
static float gLookupBrightness[FILAMENT_LOOKUP_COUNT];
static float3 gLookupColour[FILAMENT_LOOKUP_COUNT];

static float __fastcall FilamentClamp(float value, float minValue, float maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static float __fastcall FilamentPow(float base, float exponent)
{
    if (base <= 0.0f) return 0.0f;
    return powf(base, exponent);
}

static bool __fastcall FloatExactlyEqual(float a, float b)
{
    return a == b;
}

static bool __fastcall LookupParamsMatch(const FilamentLampParams& params, float coldResistanceOhms, float effectiveRadiatingAreaM2)
{
    if (!gLookupValid) return false;

    return
        FloatExactlyEqual(gLookupParams.AmbientK, params.AmbientK) &&
        FloatExactlyEqual(gLookupParams.NominalVoltageRMS, params.NominalVoltageRMS) &&
        FloatExactlyEqual(gLookupParams.NominalPowerW, params.NominalPowerW) &&
        FloatExactlyEqual(gLookupParams.NominalFilamentK, params.NominalFilamentK) &&
        FloatExactlyEqual(gLookupParams.ResistanceTemperatureExponent, params.ResistanceTemperatureExponent) &&
        FloatExactlyEqual(gLookupParams.Emissivity, params.Emissivity) &&
        FloatExactlyEqual(gLookupParams.RadiativeLossFractionAtNominal, params.RadiativeLossFractionAtNominal) &&
        FloatExactlyEqual(gLookupParams.ThermalMassJPerK, params.ThermalMassJPerK) &&
        FloatExactlyEqual(gLookupParams.VisibleStartK, params.VisibleStartK) &&
        FloatExactlyEqual(gLookupParams.VisibleFullK, params.VisibleFullK) &&
        FloatExactlyEqual(gLookupParams.BrightnessGamma, params.BrightnessGamma) &&
        FloatExactlyEqual(gLookupParams.BrightnessScale, params.BrightnessScale) &&
        FloatExactlyEqual(gLookupParams.MaxBrightness, params.MaxBrightness) &&
        FloatExactlyEqual(gLookupParams.MaxTemperatureK, params.MaxTemperatureK) &&
        FloatExactlyEqual(gLookupParams.SeriesResistanceOhms, params.SeriesResistanceOhms) &&
        FloatExactlyEqual(gLookupParams.DriverVoltageDropV, params.DriverVoltageDropV) &&
        FloatExactlyEqual(gLookupColdResistanceOhms, coldResistanceOhms) &&
        FloatExactlyEqual(gLookupEffectiveRadiatingAreaM2, effectiveRadiatingAreaM2);
}

static float3 __fastcall CalculateBlackBodyColour01(float temperatureK, float brightness)
{
    if (brightness <= 0.0f) {
        return float3();
    }

    float temperature = temperatureK / 100.0f;
    if (temperature < 1.0f) temperature = 1.0f;

    float r, g, b;

    if (temperature <= 66.0f) {
        r = 255.0f;
        g = 99.4708025861f * logf(temperature) - 161.1195681661f;
    }
    else {
        float shifted = temperature - 60.0f;
        if (shifted < 0.1f) shifted = 0.1f;
        r = 329.698727446f * powf(shifted, -0.1332047592f);
        g = 288.1221695283f * powf(shifted, -0.0755148492f);
    }

    if (temperature >= 66.0f) {
        b = 255.0f;
    }
    else if (temperature <= 19.0f) {
        b = 0.0f;
    }
    else {
        b = 138.5177312231f * logf(temperature - 10.0f) - 305.0447927307f;
    }

    r = FilamentClamp(r, 0.0f, 255.0f);
    g = FilamentClamp(g, 0.0f, 255.0f);
    b = FilamentClamp(b, 0.0f, 255.0f);

    return float3(r / 255.0f, g / 255.0f, b / 255.0f);
}

static float __fastcall CalculateVisibleBrightness(const FilamentLampParams& params, float temperatureK)
{
    float visible = (temperatureK - params.VisibleStartK) / (params.VisibleFullK - params.VisibleStartK);
    if (visible < 0.0f) visible = 0.0f;

    visible = FilamentPow(visible, params.BrightnessGamma) * params.BrightnessScale;
    return FilamentClamp(visible, 0.0f, params.MaxBrightness);
}

static void __fastcall BuildLookupTables(const FilamentLampParams& params, float coldResistanceOhms, float effectiveRadiatingAreaM2)
{
    float ambient2 = params.AmbientK * params.AmbientK;
    float ambient4 = ambient2 * ambient2;

    for (UINT32 idx = 0; idx < FILAMENT_LOOKUP_COUNT; idx++) {
        float temperatureK = FILAMENT_LOOKUP_MIN_K + (static_cast<float>(idx) * FILAMENT_LOOKUP_STEP_K);

        float resistanceRatio = FilamentPow(temperatureK / params.AmbientK, params.ResistanceTemperatureExponent);
        if (resistanceRatio < 1.0f) resistanceRatio = 1.0f;

        float resistanceOhms = coldResistanceOhms * resistanceRatio;
        if (resistanceOhms < 0.01f) resistanceOhms = 0.01f;
        gLookupResistanceOhms[idx] = resistanceOhms;

        float t2 = temperatureK * temperatureK;
        float t4 = t2 * t2;
        float radiationLoss = params.Emissivity * FILAMENT_STEFAN_BOLTZMANN * effectiveRadiatingAreaM2 * (t4 - ambient4);
        if (radiationLoss < 0.0f) radiationLoss = 0.0f;
        gLookupRadiationLossW[idx] = radiationLoss;

        float brightness = CalculateVisibleBrightness(params, temperatureK);
        gLookupBrightness[idx] = brightness;
        gLookupColour[idx] = CalculateBlackBodyColour01(temperatureK, brightness);
    }

    gLookupParams = params;
    gLookupColdResistanceOhms = coldResistanceOhms;
    gLookupEffectiveRadiatingAreaM2 = effectiveRadiatingAreaM2;
    gLookupValid = true;
}

static void __fastcall EnsureLookupTables(const FilamentLampParams& params, float coldResistanceOhms, float effectiveRadiatingAreaM2)
{
    if (!LookupParamsMatch(params, coldResistanceOhms, effectiveRadiatingAreaM2)) {
        BuildLookupTables(params, coldResistanceOhms, effectiveRadiatingAreaM2);
    }
}

static UINT32 __fastcall LookupIndexAndFraction(float temperatureK, float& fraction)
{
    if (temperatureK <= FILAMENT_LOOKUP_MIN_K) {
        fraction = 0.0f;
        return 0;
    }

    if (temperatureK >= FILAMENT_LOOKUP_MAX_K) {
        fraction = 0.0f;
        return FILAMENT_LOOKUP_COUNT - 1;
    }

    float position = (temperatureK - FILAMENT_LOOKUP_MIN_K) / FILAMENT_LOOKUP_STEP_K;
    UINT32 index = static_cast<UINT32>(position);
    if (index >= (FILAMENT_LOOKUP_COUNT - 1)) {
        fraction = 0.0f;
        return FILAMENT_LOOKUP_COUNT - 1;
    }

    fraction = position - static_cast<float>(index);
    return index;
}

static float __fastcall LookupFloatValue(const float* table, float temperatureK)
{
    float fraction;
    UINT32 index = LookupIndexAndFraction(temperatureK, fraction);
    if (index >= (FILAMENT_LOOKUP_COUNT - 1)) {
        return table[FILAMENT_LOOKUP_COUNT - 1];
    }

    return table[index] + ((table[index + 1] - table[index]) * fraction);
}

static float3 __fastcall LookupColourValue(float temperatureK)
{
    float fraction;
    UINT32 index = LookupIndexAndFraction(temperatureK, fraction);
    if (index >= (FILAMENT_LOOKUP_COUNT - 1)) {
        return gLookupColour[FILAMENT_LOOKUP_COUNT - 1];
    }

    float3 a = gLookupColour[index];
    float3 b = gLookupColour[index + 1];
    return float3(
        a.x + ((b.x - a.x) * fraction),
        a.y + ((b.y - a.y) * fraction),
        a.z + ((b.z - a.z) * fraction));
}

FilamentLamp::FilamentLamp()
{
    ConfigureDefault();
    ResetRuntime();
}

void FilamentLamp::ConfigureDefault(void)
{
    FilamentLampParams defaults;

    // Fruit-machine style wedge bulb default.  This is intentionally a bulb
    // definition, not a JPM matrix definition.  The matrix driver feeds the
    // instantaneous voltage/duty; the lamp model handles filament physics.
    defaults.AmbientK = 293.15f;
    // CML 1112LF: T-3 1/4 wedge lamp, 12 V, 0.1 A = 1.2 W.
    defaults.NominalVoltageRMS = 12.0f;
    defaults.NominalPowerW = 1.2f;
    defaults.NominalFilamentK = 2600.0f;
    defaults.ResistanceTemperatureExponent = 1.214f;
    defaults.Emissivity = 0.42f;
    defaults.RadiativeLossFractionAtNominal = 0.68f;
    defaults.ThermalMassJPerK = 0.000045f;
    defaults.VisibleStartK = 800.0f;
    defaults.VisibleFullK = 2600.0f;
    defaults.BrightnessGamma = 3.3f;
    defaults.BrightnessScale = 1.0f;
    defaults.MaxBrightness = 5.0f;
    defaults.MaxTemperatureK = 3600.0f;

    defaults.SeriesResistanceOhms = 6.5f;
    defaults.DriverVoltageDropV = 0.0f;

    Configure(defaults);
}

void FilamentLamp::Configure(const FilamentLampParams& params)
{
    Params = params;

    if (Params.AmbientK < 1.0f) Params.AmbientK = 293.15f;
    if (Params.NominalVoltageRMS < 0.1f) Params.NominalVoltageRMS = 12.0f;
    if (Params.NominalPowerW < 0.01f) Params.NominalPowerW = 1.2f;
    if (Params.NominalFilamentK <= Params.AmbientK) Params.NominalFilamentK = 2700.0f;
    if (Params.ResistanceTemperatureExponent < 0.1f) Params.ResistanceTemperatureExponent = 1.214f;
    if (Params.Emissivity < 0.01f) Params.Emissivity = 0.42f;
    if (Params.RadiativeLossFractionAtNominal < 0.0f) Params.RadiativeLossFractionAtNominal = 0.0f;
    if (Params.RadiativeLossFractionAtNominal > 1.0f) Params.RadiativeLossFractionAtNominal = 1.0f;
    if (Params.ThermalMassJPerK < 0.000001f) Params.ThermalMassJPerK = 0.00008f;
    if (Params.VisibleStartK < Params.AmbientK) Params.VisibleStartK = Params.AmbientK;
    if (Params.VisibleFullK <= Params.VisibleStartK) Params.VisibleFullK = Params.VisibleStartK + 1000.0f;
    if (Params.BrightnessGamma < 0.1f) Params.BrightnessGamma = 3.5f;
    if (Params.BrightnessScale < 0.0f) Params.BrightnessScale = 1.0f;
    if (Params.MaxBrightness < 1.0f) Params.MaxBrightness = 1.0f;
    if (Params.MaxTemperatureK < Params.NominalFilamentK) Params.MaxTemperatureK = Params.NominalFilamentK + 100.0f;
    if (Params.SeriesResistanceOhms < 0.0f) Params.SeriesResistanceOhms = 0.0f;
    if (Params.DriverVoltageDropV < 0.0f) Params.DriverVoltageDropV = 0.0f;

    RecalculateDerivedParams();
}

void FilamentLamp::RecalculateDerivedParams(void)
{
    HotResistanceOhms = (Params.NominalVoltageRMS * Params.NominalVoltageRMS) / Params.NominalPowerW;

    float ratioAtNominal = FilamentPow(Params.NominalFilamentK / Params.AmbientK, Params.ResistanceTemperatureExponent);
    if (ratioAtNominal < 1.0f) ratioAtNominal = 1.0f;
    ColdResistanceOhms = HotResistanceOhms / ratioAtNominal;
    if (ColdResistanceOhms < 0.01f) ColdResistanceOhms = 0.01f;

    float nominalLoss = Params.NominalPowerW;
    float radiationLoss = nominalLoss * Params.RadiativeLossFractionAtNominal;
    float conductionLoss = nominalLoss - radiationLoss;

    float t4 = (Params.NominalFilamentK * Params.NominalFilamentK * Params.NominalFilamentK * Params.NominalFilamentK) -
               (Params.AmbientK * Params.AmbientK * Params.AmbientK * Params.AmbientK);
    if (t4 <= 0.0f) t4 = 1.0f;

    EffectiveRadiatingAreaM2 = radiationLoss / (Params.Emissivity * FILAMENT_STEFAN_BOLTZMANN * t4);
    if (EffectiveRadiatingAreaM2 < 0.0000000001f) EffectiveRadiatingAreaM2 = 0.0000000001f;

    ConductanceWPerK = conductionLoss / (Params.NominalFilamentK - Params.AmbientK);
    if (ConductanceWPerK < 0.0f) ConductanceWPerK = 0.0f;

    EnsureLookupTables(Params, ColdResistanceOhms, EffectiveRadiatingAreaM2);
}

void FilamentLamp::ResetRuntime(void)
{
    TemperatureK = Params.AmbientK;
    ResistanceOhms = LookupFloatValue(gLookupResistanceOhms, TemperatureK);
    ElectricalPowerW = 0.0f;
    Brightness01 = 0.0f;
    Colour01 = float3();
}

void FilamentLamp::IntegrateStep(float dtSeconds, float voltageRMS)
{
    if (dtSeconds <= 0.0f) return;

    if (TemperatureK < Params.AmbientK) TemperatureK = Params.AmbientK;
    if (TemperatureK > Params.MaxTemperatureK) TemperatureK = Params.MaxTemperatureK;

    ResistanceOhms = LookupFloatValue(gLookupResistanceOhms, TemperatureK);
    if (ResistanceOhms < 0.01f) ResistanceOhms = 0.01f;

    if (voltageRMS < 0.0f) voltageRMS = 0.0f;

    // Matrix power path: the matrix supplies the applied RMS row voltage, while
    // the actual filament heating is limited by the lamp's hot/cold resistance
    // and by the non-ideal drive path.  This is more physical than feeding the
    // filament from a perfect zero-ohm voltage source.
    float effectiveVoltageRMS = voltageRMS - Params.DriverVoltageDropV;
    if (effectiveVoltageRMS < 0.0f) effectiveVoltageRMS = 0.0f;

    float totalSeriesResistance = ResistanceOhms + Params.SeriesResistanceOhms;
    if (totalSeriesResistance < 0.01f) totalSeriesResistance = 0.01f;

    float currentRMS = effectiveVoltageRMS / totalSeriesResistance;
    ElectricalPowerW = currentRMS * currentRMS * ResistanceOhms;

    float radiationLoss = LookupFloatValue(gLookupRadiationLossW, TemperatureK);

    float conductionLoss = ConductanceWPerK * (TemperatureK - Params.AmbientK);
    if (conductionLoss < 0.0f) conductionLoss = 0.0f;

    float netPower = ElectricalPowerW - radiationLoss - conductionLoss;
    TemperatureK += (netPower / Params.ThermalMassJPerK) * dtSeconds;

    if (TemperatureK < Params.AmbientK) TemperatureK = Params.AmbientK;
    if (TemperatureK > Params.MaxTemperatureK) TemperatureK = Params.MaxTemperatureK;
}

void FilamentLamp::Advance(float dtSeconds, float voltageRMS)
{
    if (dtSeconds <= 0.0f) return;
    if (voltageRMS < 0.0f) voltageRMS = 0.0f;

    // Accuracy-preserving fast path: once an unpowered lamp is effectively at
    // ambient and visibly dark, further cooling integration cannot change the
    // displayed result.  Snap the small numerical tail to ambient and skip until
    // the matrix powers the lamp again.
    if (voltageRMS <= 0.0f && IsColdAndDark()) {
        TemperatureK = Params.AmbientK;
        ResistanceOhms = LookupFloatValue(gLookupResistanceOhms, TemperatureK);
        ElectricalPowerW = 0.0f;
        Brightness01 = 0.0f;
        Colour01 = float3();
        return;
    }

    UINT32 steps = static_cast<UINT32>((dtSeconds / FILAMENT_MAX_SUBSTEP_SECONDS) + 1.0f);
    if (steps < 1) steps = 1;
    if (steps > FILAMENT_MAX_INTEGRATION_STEPS) steps = FILAMENT_MAX_INTEGRATION_STEPS;

    float stepSeconds = dtSeconds / static_cast<float>(steps);
    for (UINT32 loop = 0; loop < steps; loop++) {
        IntegrateStep(stepSeconds, voltageRMS);
    }

    UpdateOptical();
}

void FilamentLamp::UpdateOptical(void)
{
    Brightness01 = LookupFloatValue(gLookupBrightness, TemperatureK);
    Colour01 = LookupColourValue(TemperatureK);
}

void __fastcall FilamentLamp::VoltageDrop(bool enable)
{
    if (enable)
    {
        Params.DriverVoltageDropV = 6.f;
    }
    else
    {
        Params.DriverVoltageDropV = 0.0f;
    }
}

float FilamentLamp::GetTemperatureK(void) const
{
    return TemperatureK;
}

float FilamentLamp::GetBrightness(void) const
{
    return Brightness01;
}

float FilamentLamp::GetResistanceOhms(void) const
{
    return ResistanceOhms;
}

float FilamentLamp::GetElectricalPowerW(void) const
{
    return ElectricalPowerW;
}

bool FilamentLamp::IsLit(void) const
{
    return Brightness01 > FILAMENT_LIT_THRESHOLD;
}

bool FilamentLamp::IsColdAndDark(void) const
{
    // 0.5 K is far below any visible difference, but avoids repeatedly
    // integrating lamps through an asymptotic ambient-temperature tail.
    return (TemperatureK <= (Params.AmbientK + 0.5f)) &&
           (Brightness01 <= FILAMENT_LIT_THRESHOLD);
}

float3 FilamentLamp::GetColour(void) const
{
    return Colour01;
}
