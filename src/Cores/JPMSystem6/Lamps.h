#pragma once

#include "LoadSave.h"
#include "FilamentLamp.h"

#define EMULATEDCYCLESPERSECOND 8000000.0f                              //8 Mhz Main CPU
#define NUMSTROBELINES          16                                      //16 Strobe Lines
#define NUMDATALINES            16                                      //16 Data Lines
#define NUMLAMPS                (NUMSTROBELINES * NUMDATALINES)         //Total Number of Lamps
#define INPUTVOLTAGEAC          50.0f                                   //50V RMS lamp matrix row supply
#define M_PI                    3.1415926535897932384626433832795f      //Pi Constant

class Lamping {
private:

    LoadSaveClass * LSC = NULL;

    typedef struct LampEntry {
        UINT8 Powered = 0;
        UINT32 OnCycles = 0;
        UINT32 OffCycles = 0;
        UINT32 DutyCycles = 0;
        UINT32 Period = 0;
        FilamentLamp Filament;
    } LampEntry;

    LampEntry Bulbs[NUMLAMPS];

    float InputRMSVoltage = 0.f;                    //RMS voltage applied to a selected matrix row

    //General Stuff
    UINT8 StrobeVal = 0;                            //Current Strobe Value
    UINT8 PrevStrobe = 0;                           //Previous Strobe Value
    UINT16 DataVal[NUMSTROBELINES];                 //Lamp Data

    //Internal Values
    UINT8 Intensity = 255;                          //Internal value part of IMPACT board
    UINT8 IntensityEnable = 0;                      //Enable Intensity

    // Lazy lamp timing. Run() only advances this clock; individual strobe rows
    // are brought up to date when their data/strobe changes or when the front-end
    // asks for a frame update.
    UINT64 LampCycleCounter = 0;
    UINT64 LastStrobeUpdate[NUMSTROBELINES];

    bool prevVDrop = false;

    float __fastcall GetPoweredVoltageRMS(void) const;
    bool __fastcall RowRequiresThermalUpdate(UINT8 strobe) const;
    void __fastcall ResetLampEntry(UINT16 lampNum);
    void __fastcall UpdateStrobeToNow(UINT8 strobe);
    void __fastcall FlushLampCounters(void);

public:

    Lamping();
    ~Lamping();

    //Subroutines / Functions
    void __fastcall Reset(LoadSaveClass * LSCIn);           //Reset Subroutine
    void __fastcall WriteData(UINT16 data);                 //Matrix Data
    void __fastcall WriteStrobe(UINT8 strobe);              //Matrix Strobe
    void __fastcall Run(UINT32 InstructionCycles);          //Run Lamps
    void __fastcall Update(void);                           //Update

    //Input Functions
    void __fastcall SetIntensity(UINT8);                    //Sets internal value
    void __fastcall VoltageDrop(bool);

    //Output Functions
    float __fastcall GetLampBrightness(UINT16 Num);         //Returns brightness of requested lamp
    bool __fastcall GetLampsOn(UINT16 Num);                 //Returns status of requested lamp (On or Off)
    float3 __fastcall GetFilamentColour(UINT16 Num);        //Returns colour of requested lamp filament
    UINT8 __fastcall GetStrobeVal();
    UINT8 __fastcall GetIntensityEnable();
    void __fastcall SetIntensityEnable(UINT8 value);

    //State Save & Load
    void __fastcall SaveState();
    void __fastcall LoadState();

};
