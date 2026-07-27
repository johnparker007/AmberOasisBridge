#pragma once

#include "LoadSave.h"

#define PAGES 8
#define MAXSAMPLES 224 // ???
#define SOUNDMEMORYSIZE 0x800000

#define PA2_AUDIO_OUTPUT_SAMPLE_RATE 48000
#define PA2_AUDIO_OUTPUT_CHANNELS 2
#define PA2_AUDIO_START_RAMP_FRAMES 24

class SampledSound {
public:

    void ExtractROM(void);
    void NECReset(void);
    void NECInit(LoadSaveClass* LSCIn);
    void NECWriteLatch(UINT8 LatchVal);
    void NECWriteControl(UINT8 ResetIn, UINT8 STIn);
    void NECStop(void);
    void NECPlay(void);
    void NECRun(INT32 Cycles);
    void NECSerialWrite(UINT8 Reset, UINT8 Clock, UINT8 Data);
    void NECSerialReset(void);
    void NECSerialDataByte(UINT8 Data);
    void NECSetBank(UINT8 Bank);
    void NECSetTune(UINT8 Tune);
    void NECSetVolumeControl(UINT8 volCtrl);
    INT16 NECDecodeNibble(UINT8 Nibble);
    UINT32 FillAudioFrames(INT16* OutInterleavedStereo, UINT32 FramesRequired);

    UINT8 GetBusy();
    void SetMemory(UINT32 pos, UINT8 value);
    void ClearMemory();
    void CopyROM(UINT32 pos, const UINT8* source, UINT32 length);
    void SetROMSize(UINT32 size);

    // Con/De structors
    SampledSound(void);
    ~SampledSound(void);

    void SaveState();
    void LoadState();

private:

    UINT8 Memory_Space[SOUNDMEMORYSIZE];      // 8MB ROM space
    INT16* Sample_Space[MAXSAMPLES];          // decoded sample data
    // Sample data tables. These were signed long in the original core;
    // use fixed-width signed INT32 to preserve the original arithmetic.
    float SampleSeconds[MAXSAMPLES];
    UINT8 SampleBank[MAXSAMPLES];
    UINT8 SampleIndex[MAXSAMPLES];
    INT32 SampleStart[MAXSAMPLES];
    INT32 SampleEnd[MAXSAMPLES];
    INT32 SampleRate[MAXSAMPLES];
    INT32 SampleRateDivisor[MAXSAMPLES];
    INT32 SampleLengthBytes[MAXSAMPLES];
    INT32 SampleLengthSamples[MAXSAMPLES];
    UINT8 SampleDummy[MAXSAMPLES];
    INT32 TotalSamples;
    INT32 ROMSize;

    // ADPCM conversion
    INT16 StepSizes[49];
    INT32 ADPCMIndex;
    INT32 ADPCMLast;

    // NEC decode tables
    INT32 Step7759[16][16];
    INT32 State[16];
    UINT8 TuneLookup[PAGES][128];
    INT32 SamplesInPage[PAGES];

    // Emulation
    UINT8 BankSwitch;
    UINT8 Busy;
    INT32 BusyTimer;

    // NEC has 1 channel
    UINT8 Looping;
    UINT8 Playing;
    UINT8 Stopped;
    INT32 PositionL;
    INT32 PositionR;
    UINT8 EndOfSample;
    UINT8 Restarted;
    UINT8 NowPlaying;
    INT32 Frequency;
    UINT8 Tune;
    INT32 PlaybackRemainder;
    INT32 PlaybackStartRampRemaining;

    UINT8 volDir = 0;
    UINT8 prevClk = 0;
    UINT8 volClk = 0;
    UINT8 volEnable = 0;

    UINT8 volume = 0;


    CRITICAL_SECTION AudioLock;

    LoadSaveClass* LSC;
};
