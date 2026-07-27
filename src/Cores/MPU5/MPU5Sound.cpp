#include "MPU5Sound.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace
{
constexpr INT32 StepSizes[49] = {
    16, 17, 19, 21, 23, 25, 28, 31, 34, 37,
    41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253,
    279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552
};

constexpr size_t InvalidSample = std::numeric_limits<size_t>::max();
}

void MPU5Sound::Reset()
{
    std::lock_guard<std::mutex> lock(Mutex_);
    StopAllUnlocked();
}

void MPU5Sound::ClearSamples()
{
    std::lock_guard<std::mutex> lock(Mutex_);
    StopAllUnlocked();
    Samples_.clear();
}

UINT32 MPU5Sound::ReadBigEndian32(const UINT8* source)
{
    return (static_cast<UINT32>(source[0]) << 24) |
        (static_cast<UINT32>(source[1]) << 16) |
        (static_cast<UINT32>(source[2]) << 8) |
        static_cast<UINT32>(source[3]);
}

UINT16 MPU5Sound::ReadBigEndian16(const UINT8* source)
{
    return static_cast<UINT16>((static_cast<UINT16>(source[0]) << 8) | source[1]);
}

INT16 MPU5Sound::DecodeNibble(UINT8 nibble, INT32& last, INT32& stepIndex)
{
    nibble &= 0x0FU;
    const INT32 step = StepSizes[stepIndex];
    INT32 magnitude = step / 8;
    if ((nibble & 0x01U) != 0) { magnitude += step / 4; }
    if ((nibble & 0x02U) != 0) { magnitude += step / 2; }
    if ((nibble & 0x04U) != 0) { magnitude += step; }

    last += (nibble & 0x08U) != 0 ? -magnitude : magnitude;
    last = std::clamp(last, -2048, 2048);

    static constexpr INT32 StepAdjust[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };
    stepIndex = std::clamp(stepIndex + StepAdjust[nibble & 0x07U], 0, 48);

    return static_cast<INT16>(last * 8);
}

UINT32 MPU5Sound::LoadROM(const std::vector<UINT8>& rom, UINT32 addressBase)
{
    std::lock_guard<std::mutex> lock(Mutex_);
    StopAllUnlocked();
    Samples_.clear();

    constexpr size_t HeaderSize = 16U;
    size_t cursor = 0;
    while (cursor + HeaderSize <= rom.size())
    {
        const bool signature = std::memcmp(rom.data() + cursor, "SAMP", 4U) == 0;
        const bool supportedVersion =
            (rom[cursor + 4U] == 0x01U && rom[cursor + 5U] == 0x00U) ||
            (rom[cursor + 4U] == 0x00U && rom[cursor + 5U] == 0x01U);
        if (!signature || !supportedVersion)
        {
            ++cursor;
            continue;
        }

        const UINT32 decodedSamples = ReadBigEndian32(rom.data() + cursor + 6U);
        const UINT32 rate = ReadBigEndian16(rom.data() + cursor + 10U);
        const size_t compressedBytes = (static_cast<size_t>(decodedSamples) + 1U) / 2U;
        const size_t dataOffset = cursor + HeaderSize;

        if (decodedSamples == 0U || rate == 0U || rate > 192000U ||
            compressedBytes > rom.size() - dataOffset)
        {
            ++cursor;
            continue;
        }

        const UINT64 headerAddress64 = static_cast<UINT64>(addressBase) + cursor;
        const UINT64 dataAddress64 = static_cast<UINT64>(addressBase) + dataOffset;
        if (headerAddress64 > std::numeric_limits<UINT32>::max() ||
            dataAddress64 > std::numeric_limits<UINT32>::max())
        {
            ++cursor;
            continue;
        }

        Sample sample;
        sample.HeaderAddress = static_cast<UINT32>(headerAddress64);
        sample.DataAddress = static_cast<UINT32>(dataAddress64);
        sample.Rate = rate;
        sample.PCM.resize(decodedSamples);

        INT32 last = 0;
        INT32 stepIndex = 0;
        UINT32 output = 0;
        for (size_t byteIndex = 0; byteIndex < compressedBytes && output < decodedSamples; ++byteIndex)
        {
            const UINT8 packed = rom[dataOffset + byteIndex];
            sample.PCM[output++] = DecodeNibble(static_cast<UINT8>(packed >> 4), last, stepIndex);
            if (output < decodedSamples)
            {
                sample.PCM[output++] = DecodeNibble(packed, last, stepIndex);
            }
        }

        Samples_.push_back(std::move(sample));
        cursor = dataOffset + compressedBytes;
    }

    return static_cast<UINT32>(std::min<size_t>(Samples_.size(), std::numeric_limits<UINT32>::max()));
}

UINT8 MPU5Sound::NormalizeChannel(UINT8 channel)
{
    // MFME maps DSP channel 4 onto playback channel 3.
    if (channel == 4U) { channel = 3U; }
    return channel;
}

size_t MPU5Sound::FindSample(UINT32 address) const
{
    const UINT32 maskedAddress = address & 0x007FFFFFU;
    for (size_t index = 0; index < Samples_.size(); ++index)
    {
        const Sample& sample = Samples_[index];
        if (address == sample.DataAddress || address == sample.HeaderAddress ||
            maskedAddress == (sample.DataAddress & 0x007FFFFFU) ||
            maskedAddress == (sample.HeaderAddress & 0x007FFFFFU))
        {
            return index;
        }
    }
    return InvalidSample;
}

bool MPU5Sound::Start(UINT32 sampleAddress, UINT8 channel)
{
    std::lock_guard<std::mutex> lock(Mutex_);
    channel = NormalizeChannel(channel);
    if (channel == 0U || channel > Channels_.size()) { return false; }

    const size_t sampleIndex = FindSample(sampleAddress);
    if (sampleIndex == InvalidSample) { return false; }

    Channel& playback = Channels_[channel - 1U];
    playback.Playing = true;
    playback.SampleIndex = sampleIndex;
    playback.Position = 0;
    playback.Remainder = 0;
    playback.RampRemaining = StartRampFrames;
    return true;
}

void MPU5Sound::Stop(UINT8 channel)
{
    std::lock_guard<std::mutex> lock(Mutex_);
    channel = NormalizeChannel(channel);
    if (channel == 0U || channel > Channels_.size()) { return; }
    Channels_[channel - 1U] = Channel{};
}

void MPU5Sound::StopAllUnlocked()
{
    for (Channel& channel : Channels_) { channel = Channel{}; }
}

void MPU5Sound::StopAll()
{
    std::lock_guard<std::mutex> lock(Mutex_);
    StopAllUnlocked();
}

UINT8 MPU5Sound::Status() const
{
    std::lock_guard<std::mutex> lock(Mutex_);
    UINT8 result = 0;
    // The recovered MPU5 DSP status register reports the first five channels.
    for (UINT8 channel = 0; channel < 5U && channel < Channels_.size(); ++channel)
    {
        if (Channels_[channel].Playing) { result |= static_cast<UINT8>(1U << channel); }
    }
    return result;
}

UINT32 MPU5Sound::FillAudioFrames(INT16* outInterleavedStereo, UINT32 framesRequired)
{
    if (!outInterleavedStereo || framesRequired == 0U) { return 0; }

    std::lock_guard<std::mutex> lock(Mutex_);
    for (UINT32 frame = 0; frame < framesRequired; ++frame)
    {
        INT32 mixed = 0;
        for (Channel& playback : Channels_)
        {
            if (!playback.Playing || playback.SampleIndex >= Samples_.size()) { continue; }

            const Sample& sample = Samples_[playback.SampleIndex];
            if (sample.PCM.empty() || sample.Rate == 0U || playback.Position >= sample.PCM.size())
            {
                playback = Channel{};
                continue;
            }

            const UINT32 position = playback.Position;
            const UINT32 next = position + 1U < sample.PCM.size() ? position + 1U : position;
            const INT32 first = sample.PCM[position];
            const INT32 second = sample.PCM[next];
            INT32 value = first + static_cast<INT32>(
                (static_cast<INT64>(second - first) * playback.Remainder) / OutputSampleRate);

            if (playback.RampRemaining != 0U)
            {
                const UINT32 rampStep = StartRampFrames - playback.RampRemaining + 1U;
                value = static_cast<INT32>((static_cast<INT64>(value) * rampStep) / StartRampFrames);
                --playback.RampRemaining;
            }
            mixed += value;

            playback.Remainder += sample.Rate;
            const UINT32 advance = playback.Remainder / OutputSampleRate;
            playback.Remainder %= OutputSampleRate;
            if (advance != 0U)
            {
                if (advance >= sample.PCM.size() - playback.Position)
                {
                    playback = Channel{};
                }
                else
                {
                    playback.Position += advance;
                }
            }
        }

        mixed = std::clamp(mixed, -32768, 32767);
        const INT16 output = static_cast<INT16>(mixed);
        const size_t destination = static_cast<size_t>(frame) * 2U;
        outInterleavedStereo[destination] = output;
        outInterleavedStereo[destination + 1U] = output;
    }
    return framesRequired;
}

UINT32 MPU5Sound::GetSampleCount() const
{
    std::lock_guard<std::mutex> lock(Mutex_);
    return static_cast<UINT32>(std::min<size_t>(Samples_.size(), std::numeric_limits<UINT32>::max()));
}
