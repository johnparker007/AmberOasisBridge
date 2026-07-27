#pragma once

#include "PA2CoreInterface.h"

#include <array>
#include <cstddef>
#include <mutex>
#include <vector>

class MPU5Sound
{
public:
    static constexpr UINT32 OutputSampleRate = 48000U;
    static constexpr UINT8 PlaybackChannelCount = 6U;

    void Reset();
    void ClearSamples();
    UINT32 LoadROM(const std::vector<UINT8>& rom, UINT32 addressBase = 0U);

    bool Start(UINT32 sampleAddress, UINT8 channel);
    void Stop(UINT8 channel);
    void StopAll();

    UINT8 Status() const;
    UINT32 FillAudioFrames(INT16* outInterleavedStereo, UINT32 framesRequired);
    UINT32 GetSampleCount() const;

private:
    static constexpr UINT32 StartRampFrames = 24U;

    struct Sample
    {
        UINT32 HeaderAddress = 0;
        UINT32 DataAddress = 0;
        UINT32 Rate = 0;
        std::vector<INT16> PCM;
    };

    struct Channel
    {
        bool Playing = false;
        size_t SampleIndex = 0;
        UINT32 Position = 0;
        UINT32 Remainder = 0;
        UINT32 RampRemaining = 0;
    };

    static UINT32 ReadBigEndian32(const UINT8* source);
    static UINT16 ReadBigEndian16(const UINT8* source);
    static INT16 DecodeNibble(UINT8 nibble, INT32& last, INT32& stepIndex);
    size_t FindSample(UINT32 address) const;
    static UINT8 NormalizeChannel(UINT8 channel);
    void StopAllUnlocked();

    mutable std::mutex Mutex_;
    std::vector<Sample> Samples_;
    std::array<Channel, PlaybackChannelCount> Channels_{};
};
