#ifndef SoundMainH
#define SoundMainH

#include <windows.h>
#include "PA2CoreInterface.h"

// The YMZ has 8 internal channels.
#define YMZCHANNELS 8

// Left + right channels (speakers).
#define OUTPUTCHANNELS 2

// Original Epoch push-buffer target.  This is retained deliberately because
// it is part of the timing and playback behaviour of the existing core.
#define BUFFERSIZE 16000

// Original BASS stream creation frequency, retained for documentation.  The
// per-channel playback frequency is still changed by the YMZ pitch registers.
#define BASSINTERNALFREQUENCY 64000

#define PA2_AUDIO_OUTPUT_SAMPLE_RATE 48000
#define PA2_AUDIO_OUTPUT_CHANNELS 2

class SampledSound {
protected:

private:
    // YMZ decoding.
    signed short Signals[YMZCHANNELS];
    signed short Steps[YMZCHANNELS];

    // YMZ function registers.
    unsigned short Pitch[YMZCHANNELS];
    unsigned short PrevPitch[YMZCHANNELS];
    UINT8 KeyOn[YMZCHANNELS];
    UINT8 PrevKeyOn[YMZCHANNELS];
    UINT8 QMode[YMZCHANNELS];
    UINT8 Nib[YMZCHANNELS];
    UINT8 Loop[YMZCHANNELS];
    UINT8 Level[YMZCHANNELS];
    UINT8 PrevLevel[YMZCHANNELS];
    UINT8 Panpot[YMZCHANNELS];
    UINT8 PrevPanpot[YMZCHANNELS];
    UINT32 StartAddress[YMZCHANNELS];
    UINT32 EndAddress[YMZCHANNELS];
    UINT32 LoopStartAddress[YMZCHANNELS];
    UINT32 LoopEndAddress[YMZCHANNELS];
    UINT32 Position[YMZCHANNELS];

    // 16 MB ROM space, matching the original Epoch implementation.  The
    // Epoch board exposes a 2 MB YMZ window, hence SoundMemorySize below.
    UINT8 Memory_Space[0x1000000];

    // Original temporary interleaved stereo decode buffer.  YMZUpdate fills
    // this exactly as before, then pushes it into the replacement channels.
    signed short SampledBuffer[BUFFERSIZE];

    // Replacement for the eight BASS push streams.  The emulation thread
    // assembles channel PCM in YMZUpdate(); the front-end audio thread only
    // consumes and mixes these already assembled buffers.
    static constexpr UINT32 ChannelBufferFrames = BUFFERSIZE / OUTPUTCHANNELS;
    signed short ChannelBuffer[YMZCHANNELS][ChannelBufferFrames];
    UINT32 ChannelReadPosition[YMZCHANNELS];
    UINT32 ChannelWritePosition[YMZCHANNELS];
    UINT32 ChannelFramesQueued[YMZCHANNELS];
    double ChannelReadFraction[YMZCHANNELS];
    double ChannelFrequency[YMZCHANNELS];
    float ChannelVolume[YMZCHANNELS];
    float ChannelPan[YMZCHANNELS];
    UINT8 ChannelPlaying[YMZCHANNELS];

    CRITICAL_SECTION AudioLock;
    UINT8 AudioLockReady;

    // Lookup tables.
    int Diff_LookUp[16];
    int Index_Scale[8];

    // YMZ utility registers.
    UINT8 LeftEnable = 0;
    UINT8 LeftOutputChannel = 0;
    UINT8 RightEnable = 0;
    UINT8 RightOutputChannel = 0;
    UINT8 DSPEnable = 0;
    UINT8 DSPData = 0;
    UINT32 ROMAddress = 0;
    UINT8 ROMData = 0;
    UINT8 IRQMask = 0;
    UINT8 IRQEnable = 0;
    UINT8 KeyOnEnable = 0;
    UINT8 ROMEnable = 0;
    UINT8 LSITest = 0;

    // Status register and register select.
    UINT8 Status = 0;
    UINT8 RegSelect = 0;
    UINT8 IRQOutFlag = 0;

    void UpdateIrqState();
    void DoInterrupt();

    void ClearChannelBuffer(UINT8 Channel);
    void PushChannelFrames(UINT8 Channel, const signed short* InterleavedStereo, UINT32 Frames);
    signed short PeekChannelFrame(UINT8 Channel, UINT32 Offset) const;
    void ConsumeChannelFrame(UINT8 Channel);
    static INT16 ClampSample(INT64 Value);

public:
    static constexpr UINT32 OutputSampleRate = PA2_AUDIO_OUTPUT_SAMPLE_RATE;
    static constexpr UINT32 OutputChannels = PA2_AUDIO_OUTPUT_CHANNELS;
    static constexpr UINT32 SoundMemorySize = 0x200000U;

    // Original YMZ interface functions.
    void YMZReset(void);
    void YMZInit(void);
    bool YMZGetIRQ();
    void YMZDecodeNibble(UINT8 Channel, UINT8 Nibble);
    void YMZUpdate(void);
    void YMZWriteRegSelect(UINT8 value);
    void YMZWriteReg(UINT8 value);

    UINT8 YMZReadReg();
    UINT8 YMZReadStatus();
    UINT8 YMZReadMemory();

    int LoadSoundROM(char* name1, char* name2, char* name3, char* name4);
    int LoadSoundData(const UINT8* Data, UINT32 Size);

    // The front-end retrieves the PCM already assembled by YMZUpdate().
    UINT32 FillAudioFrames(INT16* OutInterleavedStereo, UINT32 FramesRequired);

    // State save.
    void SaveState();
    void LoadState();

    SampledSound(void);
    ~SampledSound(void);
};

#endif // SoundMainH
