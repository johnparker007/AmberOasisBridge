#include "stdafx.h"
#include "Lamps.h"
#include <math.h>

#define SQUARE_ROOT_OF_TWO 1.414213562373095f
#define LAMP_MAX_COUNTER 0xffffffffUL
#define JPM_NORMAL_SCAN_DUTY (1.0f / static_cast<float>(NUMSTROBELINES))


static UINT32 __fastcall ClampAddCycles(UINT32 current, UINT64 add)
{
    if (add >= LAMP_MAX_COUNTER) {
        return LAMP_MAX_COUNTER;
    }
    if (LAMP_MAX_COUNTER - current < static_cast<UINT32>(add)) {
        return LAMP_MAX_COUNTER;
    }
    return current + static_cast<UINT32>(add);
}

static float __fastcall ClampFloat(float value, float minValue, float maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}


static FilamentLampParams __fastcall BuildJpmImpactLampParams(float matrixSupplyRMSVoltage)
{
    FilamentLampParams params;

    // CML 1112LF actual lamp: T-3 1/4 wedge, 12 V, 0.1 A, 1.2 W.
    // The bulb parameters describe the bulb only.  The electrical power comes
    // from the JPM matrix voltage/timing when the lamp is advanced.  With a
    // 50 V RMS selected-row supply and a 16-row scan, the matrix naturally
    // drives a normal full-on lamp at roughly the 12 V class, while still
    // allowing short over-bright pulses when the matrix overdrives it.
    (void)matrixSupplyRMSVoltage;

    params.AmbientK = 293.15f;
    params.NominalVoltageRMS = 12.0f;
    params.NominalPowerW = 1.2f;
    params.NominalFilamentK = 2640.0f; // tuned warmer/slightly dimmer than 2680 K normal running point
    params.ResistanceTemperatureExponent = 1.214f;
    params.Emissivity = 0.42f;
    params.RadiativeLossFractionAtNominal = 0.68f;
    params.ThermalMassJPerK = 0.000085f; // more thermal inertia to further smooth matrix flicker while preserving flash headroom
    params.VisibleStartK = 800.0f;
    params.VisibleFullK = 2700.0f;
    params.BrightnessGamma = 3.3f;
    params.BrightnessScale = 1.0f;
    params.MaxBrightness = 5.0f;
    params.MaxTemperatureK = 3600.0f;

    // Small matrix/driver series loss.  This keeps the input power matrix-
    // derived, but prevents an ideal voltage source from exaggerating cold
    // filament inrush.  At hot resistance this also trims the slightly-too-
    // bright normal lamp level without hurting short super-bright headroom.
    params.SeriesResistanceOhms = 4.5f;
    params.DriverVoltageDropV = 0.0f;

    return params;
}

Lamping::~Lamping(){
}

Lamping::Lamping(){
    ZeroMemory(DataVal, NUMSTROBELINES * sizeof(UINT16));
    InputRMSVoltage = INPUTVOLTAGEAC;
}

void Lamping::SetIntensity(UINT8 Intens) {
    Intensity = Intens;
}

void __fastcall Lamping::VoltageDrop(bool enable)
{
    if (prevVDrop != enable)
    {
        for (UINT16 i = 0; i < 256; i++)
        {
            Bulbs[i].Filament.VoltageDrop(enable);
        }
    }

    prevVDrop = enable;
}

float Lamping::GetPoweredVoltageRMS(void) const
{
    // Intensity is treated as an additional PWM-style dimmer.  Electrical
    // heating follows RMS voltage, so duty scaling maps to sqrt(scale).
    float intensityScale = float(Intensity) / 255.0f;
    intensityScale = ClampFloat(intensityScale, 0.0f, 1.0f);

    float voltage = InputRMSVoltage;
    if (IntensityEnable || Intensity != 255) {
        voltage *= sqrtf(intensityScale);
    }
    return voltage;
}

bool Lamping::RowRequiresThermalUpdate(UINT8 strobe) const
{
    strobe &= 0x0f;

    // If this is the selected row and any data bit is on, the row can still
    // feed energy into a cold lamp, so it must be brought up to date.
    if (strobe == StrobeVal && DataVal[strobe] != 0) {
        return true;
    }

    // Otherwise, only rows with hot/visible filaments need cooling updates.
    for (UINT8 cnt = 0; cnt < NUMDATALINES; cnt++) {
        UINT16 sel = static_cast<UINT16>((strobe * NUMDATALINES) + cnt);
        if (!Bulbs[sel].Filament.IsColdAndDark()) {
            return true;
        }
    }

    return false;
}

void Lamping::ResetLampEntry(UINT16 lampNum)
{
    if (lampNum >= NUMLAMPS) return;

    Bulbs[lampNum].Powered = 0;
    Bulbs[lampNum].OnCycles = 0;
    Bulbs[lampNum].OffCycles = 0;
    Bulbs[lampNum].DutyCycles = 0;
    Bulbs[lampNum].Period = 0;
    Bulbs[lampNum].Filament.Configure(BuildJpmImpactLampParams(InputRMSVoltage));
    Bulbs[lampNum].Filament.ResetRuntime();
}

void Lamping::Reset(LoadSaveClass * LSCIn){

    ZeroMemory(DataVal, sizeof(DataVal));

    InputRMSVoltage = INPUTVOLTAGEAC;

    StrobeVal = 0;
    PrevStrobe = 0;
    Intensity = 255;
    IntensityEnable = 0;
    LampCycleCounter = 0;
    ZeroMemory(LastStrobeUpdate, sizeof(LastStrobeUpdate));

    for (UINT16 loop = 0; loop < NUMLAMPS; loop++) {
        ResetLampEntry(loop);
    }

    LSC = LSCIn;
}

void Lamping::UpdateStrobeToNow(UINT8 strobe){

    strobe &= 0x0f;

    UINT64 LastCycle = LastStrobeUpdate[strobe];
    if (LastCycle >= LampCycleCounter) {
        LastStrobeUpdate[strobe] = LampCycleCounter;
        return;
    }

    UINT64 ElapsedCycles = LampCycleCounter - LastCycle;
    LastStrobeUpdate[strobe] = LampCycleCounter;

    float elapsedSeconds = float(ElapsedCycles) / EMULATEDCYCLESPERSECOND;
    float poweredVoltage = GetPoweredVoltageRMS();

    for (UINT8 cnt = 0; cnt < NUMDATALINES; cnt++){
        UINT16 sel = static_cast<UINT16>((strobe * NUMDATALINES) + cnt);

        bool powered = false;
        if (strobe == StrobeVal){
            powered = ((DataVal[strobe] & (static_cast<UINT16>(1) << cnt)) != 0);
        }

        if (powered){
            Bulbs[sel].Powered = 1;
            Bulbs[sel].OnCycles = ClampAddCycles(Bulbs[sel].OnCycles, ElapsedCycles);
            Bulbs[sel].Filament.Advance(elapsedSeconds, poweredVoltage);
        }
        else {
            Bulbs[sel].Powered = 0;
            Bulbs[sel].OffCycles = ClampAddCycles(Bulbs[sel].OffCycles, ElapsedCycles);
            if (!Bulbs[sel].Filament.IsColdAndDark()) {
                Bulbs[sel].Filament.Advance(elapsedSeconds, 0.0f);
            }
        }
    }
}

void Lamping::FlushLampCounters(void){

    for (UINT8 strobe = 0; strobe < NUMSTROBELINES; strobe++){
        if (LastStrobeUpdate[strobe] >= LampCycleCounter) {
            LastStrobeUpdate[strobe] = LampCycleCounter;
            continue;
        }

        if (RowRequiresThermalUpdate(strobe)) {
            UpdateStrobeToNow(strobe);
        }
        else {
            // Cold, dark, unpowered rows are already visually/physically at
            // their steady state.  Move their lazy timestamp forward without
            // running 16 no-op cooling integrations.
            LastStrobeUpdate[strobe] = LampCycleCounter;
        }
    }
}

void Lamping::WriteStrobe(UINT8 strobe){

    strobe &= 0x0f;

    if (strobe != StrobeVal) {

        // Bring the previously active row and the row being selected up to the
        // current emulated time before changing selection.
        UpdateStrobeToNow(StrobeVal);
        UpdateStrobeToNow(strobe);

        // Keep the old measured duty/period values for debugging/save-state
        // continuity, but visual output now comes from the filament model.
        for (UINT8 cnt = 0; cnt < NUMDATALINES; cnt++) {
            UINT16 sel = static_cast<UINT16>((strobe * NUMDATALINES) + cnt);
            Bulbs[sel].Period = ClampAddCycles(Bulbs[sel].OnCycles, Bulbs[sel].OffCycles);
            Bulbs[sel].DutyCycles = Bulbs[sel].OnCycles;
            Bulbs[sel].OffCycles = 0;
            Bulbs[sel].OnCycles = 0;
        }

        PrevStrobe = StrobeVal;
        StrobeVal = strobe;
        LastStrobeUpdate[StrobeVal] = LampCycleCounter;
    }
}

void Lamping::WriteData(UINT16 data){

    // Account for all cycles run under the previous data value before changing
    // the active row data.
    UpdateStrobeToNow(StrobeVal);
    DataVal[StrobeVal] = data;
}

void Lamping::Run(UINT32 InstructionCycles){

    // Called very frequently. Lazy timing only advances the lamp clock; row
    // counters and filament heat are updated when matrix data/strobe changes
    // or once per frame.
    LampCycleCounter += InstructionCycles;
}

void Lamping::Update(){

    // Bring all lazy row counters and filament temperatures up to date before
    // the front-end reads lamp state.
    FlushLampCounters();
}

float3 Lamping::GetFilamentColour(UINT16 Num){
    if (Num >= NUMLAMPS) return float3();
    return Bulbs[(Num & 0xff)].Filament.GetColour();
}

UINT8 __fastcall Lamping::GetStrobeVal()
{
    return StrobeVal;
}

UINT8 __fastcall Lamping::GetIntensityEnable()
{
    return IntensityEnable;
}

void __fastcall Lamping::SetIntensityEnable(UINT8 value)
{
    IntensityEnable = value;
}

float Lamping::GetLampBrightness(UINT16 Num) {
    if (Num >= NUMLAMPS) return 0.f;
    return Bulbs[(Num & 0xff)].Filament.GetBrightness();
}

bool Lamping::GetLampsOn(UINT16 Num) {
    if (Num >= NUMLAMPS) return false;
    return Bulbs[(Num & 0xff)].Filament.IsLit();
}

void Lamping::SaveState(){

    // The lamp system is lazily evaluated.  Always flush to the current emulated
    // cycle before saving so the filament temperatures/colours and row counters
    // are not stale in the save file.
    FlushLampCounters();

    int loop;

    // Save matrix/global state first so load can configure the filament model
    // before restoring per-lamp physical state.
    LSC->SaveToBuffer(InputRMSVoltage);
    LSC->SaveToBuffer(StrobeVal);
    LSC->SaveToBuffer(PrevStrobe);
    LSC->SaveToBuffer(Intensity);
    LSC->SaveToBuffer(IntensityEnable);
    LSC->SaveToBuffer(LampCycleCounter);
    for (loop = 0; loop < NUMSTROBELINES; loop++) {
        LSC->SaveToBuffer(LastStrobeUpdate[loop]);
    }
    for (loop = 0; loop < NUMSTROBELINES; loop++) {
        LSC->SaveToBuffer(DataVal[loop]);
    }

    for (loop = 0; loop < NUMLAMPS; loop++){
        LSC->SaveToBuffer(Bulbs[loop].DutyCycles);
        LSC->SaveToBuffer(Bulbs[loop].OffCycles);
        LSC->SaveToBuffer(Bulbs[loop].OnCycles);
        LSC->SaveToBuffer(Bulbs[loop].Period);
        LSC->SaveToBuffer(Bulbs[loop].Powered);
        Bulbs[loop].Filament.SaveState(LSC);
    }
}

void Lamping::LoadState(){

    int loop;

    LSC->LoadFromBuffer(InputRMSVoltage);
    LSC->LoadFromBuffer(StrobeVal);
    LSC->LoadFromBuffer(PrevStrobe);
    LSC->LoadFromBuffer(Intensity);
    LSC->LoadFromBuffer(IntensityEnable);
    LSC->LoadFromBuffer(LampCycleCounter);

    if (InputRMSVoltage < 1.0f || InputRMSVoltage > 250.0f) {
        InputRMSVoltage = INPUTVOLTAGEAC;
    }

    for (loop = 0; loop < NUMSTROBELINES; loop++) {
        LSC->LoadFromBuffer(LastStrobeUpdate[loop]);
        if (LastStrobeUpdate[loop] > LampCycleCounter) {
            LastStrobeUpdate[loop] = LampCycleCounter;
        }
    }
    for (loop = 0; loop < NUMSTROBELINES; loop++) {
        LSC->LoadFromBuffer(DataVal[loop]);
    }

    StrobeVal &= 0x0f;
    PrevStrobe &= 0x0f;
    Intensity = static_cast<UINT8>(ClampFloat(float(Intensity), 0.0f, 255.0f));
    IntensityEnable = IntensityEnable ? 1 : 0;

    FilamentLampParams params = BuildJpmImpactLampParams(InputRMSVoltage);

    for (loop = 0; loop < NUMLAMPS; loop++){
        LSC->LoadFromBuffer(Bulbs[loop].DutyCycles);
        LSC->LoadFromBuffer(Bulbs[loop].OffCycles);
        LSC->LoadFromBuffer(Bulbs[loop].OnCycles);
        LSC->LoadFromBuffer(Bulbs[loop].Period);
        LSC->LoadFromBuffer(Bulbs[loop].Powered);
        Bulbs[loop].Powered = Bulbs[loop].Powered ? 1 : 0;
        Bulbs[loop].Filament.Configure(params);
        Bulbs[loop].Filament.LoadState(LSC);
    }
}
