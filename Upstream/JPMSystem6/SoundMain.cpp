#include "stdafx.h"
#include "SoundMain.h"
#include "iostream"
#include <new>

namespace
{
    const INT32 NEC_PAGE_SIZE = 131072;
    const INT32 NEC_PAGE_LAST_OFFSET = NEC_PAGE_SIZE - 1;
    const UINT32 SAMPLE_TEMP_CAPACITY = 0x80000;
    const INT32 NEC_VOLUME_MAX = 99;
    const INT32 NEC_VOLUME_MIN = 0;

}

SampledSound::SampledSound(void)
{
    Playing = 0;
    Looping = 0;
    Stopped = 0;
    EndOfSample = 1;
    Restarted = 0;
    NowPlaying = 0;
    PositionL = 0;
    PositionR = 0;
    Frequency = 0;
    BankSwitch = 0;
    Busy = 0;
    BusyTimer = 0;
    Tune = 0;
    TotalSamples = 0;
    ROMSize = 0;
    ADPCMIndex = 0;
    ADPCMLast = 0;
    PlaybackRemainder = 0;
    PlaybackStartRampRemaining = 0;
    LSC = NULL;
    InitializeCriticalSection(&AudioLock);

    ZeroMemory(SampleBank, MAXSAMPLES * sizeof(UINT8));
    ZeroMemory(SampleIndex, MAXSAMPLES * sizeof(UINT8));
    ZeroMemory(SampleDummy, MAXSAMPLES * sizeof(UINT8));
    ZeroMemory(SampleStart, MAXSAMPLES * sizeof(INT32));
    ZeroMemory(SampleEnd, MAXSAMPLES * sizeof(INT32));
    ZeroMemory(SampleRate, MAXSAMPLES * sizeof(INT32));
    ZeroMemory(SampleRateDivisor, MAXSAMPLES * sizeof(INT32));
    ZeroMemory(SampleLengthBytes, MAXSAMPLES * sizeof(INT32));
    ZeroMemory(SampleLengthSamples, MAXSAMPLES * sizeof(INT32));
    ZeroMemory(SampleSeconds, MAXSAMPLES * sizeof(float));
    ZeroMemory(Sample_Space, sizeof(Sample_Space));
    ZeroMemory(Memory_Space, SOUNDMEMORYSIZE * sizeof(UINT8));
    ZeroMemory(TuneLookup, PAGES * 128 * sizeof(UINT8));
    ZeroMemory(SamplesInPage, PAGES * sizeof(INT32));
    ZeroMemory(Step7759, sizeof(Step7759));
    ZeroMemory(State, sizeof(State));

    const INT16 InitialStepSizes[49] = {
        16, 17, 19, 21, 23, 25, 28, 31, 34, 37,
        41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
        107, 118, 130, 143, 157, 173, 190, 209, 230, 253,
        279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
        724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552
    };

    for (UINT32 cnt = 0; cnt < 49; cnt++) {
        StepSizes[cnt] = InitialStepSizes[cnt];
    }

	Step7759[0][0] = 0;
	Step7759[0][1] = 0;
	Step7759[0][2] = 1;
	Step7759[0][3] = 2;
	Step7759[0][4] = 3;
	Step7759[0][5] = 5;
	Step7759[0][6] = 7;
	Step7759[0][7] = 10;
	Step7759[0][8] = 0;
	Step7759[0][9] = 0;
	Step7759[0][10] = -1;
	Step7759[0][11] = -2;
	Step7759[0][12] = -3;
	Step7759[0][13] = -5;
	Step7759[0][14] = -7;
	Step7759[0][15] = -10;

	Step7759[1][0] = 0;
	Step7759[1][1] = 1;
	Step7759[1][2] = 2;
	Step7759[1][3] = 3;
	Step7759[1][4] = 4;
	Step7759[1][5] = 6;
	Step7759[1][6] = 8;
	Step7759[1][7] = 13;
	Step7759[1][8] = 0;
	Step7759[1][9] = -1;
	Step7759[1][10] = -2;
	Step7759[1][11] = -3;
	Step7759[1][12] = -4;
	Step7759[1][13] = -6;
	Step7759[1][14] = -8;
	Step7759[1][15] = -13;

	Step7759[2][0] = 0;
	Step7759[2][1] = 1;
	Step7759[2][2] = 2;
	Step7759[2][3] = 4;
	Step7759[2][4] = 5;
	Step7759[2][5] = 7;
	Step7759[2][6] = 10;
	Step7759[2][7] = 15;
	Step7759[2][8] = 0;
	Step7759[2][9] = -1;
	Step7759[2][10] = -2;
	Step7759[2][11] = -4;
	Step7759[2][12] = -5;
	Step7759[2][13] = -7;
	Step7759[2][14] = -10;
	Step7759[2][15] = -15;

	Step7759[3][0] = 0;
	Step7759[3][1] = 1;
	Step7759[3][2] = 3;
	Step7759[3][3] = 4;
	Step7759[3][4] = 6;
	Step7759[3][5] = 9;
	Step7759[3][6] = 13;
	Step7759[3][7] = 19;
	Step7759[3][8] = 0;
	Step7759[3][9] = -1;
	Step7759[3][10] = -3;
	Step7759[3][11] = -4;
	Step7759[3][12] = -6;
	Step7759[3][13] = -9;
	Step7759[3][14] = -13;
	Step7759[3][15] = -19;

	Step7759[4][0] = 0;
	Step7759[4][1] = 2;
	Step7759[4][2] = 3;
	Step7759[4][3] = 5;
	Step7759[4][4] = 8;
	Step7759[4][5] = 11;
	Step7759[4][6] = 15;
	Step7759[4][7] = 23;
	Step7759[4][8] = 0;
	Step7759[4][9] = -2;
	Step7759[4][10] = -3;
	Step7759[4][11] = -5;
	Step7759[4][12] = -8;
	Step7759[4][13] = -11;
	Step7759[4][14] = -15;
	Step7759[4][15] = -23;

	Step7759[5][0] = 0;
	Step7759[5][1] = 2;
	Step7759[5][2] = 4;
	Step7759[5][3] = 7;
	Step7759[5][4] = 10;
	Step7759[5][5] = 14;
	Step7759[5][6] = 19;
	Step7759[5][7] = 29;
	Step7759[5][8] = 0;
	Step7759[5][9] = -2;
	Step7759[5][10] = -4;
	Step7759[5][11] = -7;
	Step7759[5][12] = -10;
	Step7759[5][13] = -14;
	Step7759[5][14] = -19;
	Step7759[5][15] = -29;

	Step7759[6][0] = 0;
	Step7759[6][1] = 3;
	Step7759[6][2] = 5;
	Step7759[6][3] = 8;
	Step7759[6][4] = 12;
	Step7759[6][5] = 16;
	Step7759[6][6] = 22;
	Step7759[6][7] = 33;
	Step7759[6][8] = 0;
	Step7759[6][9] = -3;
	Step7759[6][10] = -5;
	Step7759[6][11] = -7;
	Step7759[6][12] = -12;
	Step7759[6][13] = -16;
	Step7759[6][14] = -22;
	Step7759[6][15] = -33;

	Step7759[7][0] = 1;
	Step7759[7][1] = 4;
	Step7759[7][2] = 7;
	Step7759[7][3] = 10;
	Step7759[7][4] = 15;
	Step7759[7][5] = 20;
	Step7759[7][6] = 29;
	Step7759[7][7] = 43;
	Step7759[7][8] = -1;
	Step7759[7][9] = -4;
	Step7759[7][10] = -7;
	Step7759[7][11] = -10;
	Step7759[7][12] = -15;
	Step7759[7][13] = -20;
	Step7759[7][14] = -29;
	Step7759[7][15] = -43;

	Step7759[8][0] = 1;
	Step7759[8][1] = 4;
	Step7759[8][2] = 8;
	Step7759[8][3] = 13;
	Step7759[8][4] = 18;
	Step7759[8][5] = 25;
	Step7759[8][6] = 35;
	Step7759[8][7] = 53;
	Step7759[8][8] = -1;
	Step7759[8][9] = -4;;
	Step7759[8][10] = -8;
	Step7759[8][11] = -13;
	Step7759[8][12] = -18;
	Step7759[8][13] = -25;
	Step7759[8][14] = -35;
	Step7759[8][15] = -53;

	Step7759[9][0] = 1;
	Step7759[9][1] = 6;
	Step7759[9][2] = 10;
	Step7759[9][3] = 16;
	Step7759[9][4] = 22;
	Step7759[9][5] = 31;
	Step7759[9][6] = 43;
	Step7759[9][7] = 64;
	Step7759[9][8] = -1;
	Step7759[9][9] = -6;
	Step7759[9][10] = -10;
	Step7759[9][11] = -16;
	Step7759[9][12] = -22;
	Step7759[9][13] = -31;
	Step7759[9][14] = -43;
	Step7759[9][15] = -64;

	Step7759[10][0] = 2;
	Step7759[10][1] = 7;
	Step7759[10][2] = 12;
	Step7759[10][3] = 19;
	Step7759[10][4] = 27;
	Step7759[10][5] = 37;
	Step7759[10][6] = 51;
	Step7759[10][7] = 76;
	Step7759[10][8] = -2;
	Step7759[10][9] = -7;
	Step7759[10][10] = -12;
	Step7759[10][11] = -19;
	Step7759[10][12] = -27;
	Step7759[10][13] = -37;
	Step7759[10][14] = -51;
	Step7759[10][15] = -76;

	Step7759[11][0] = 2;
	Step7759[11][1] = 9;
	Step7759[11][2] = 16;
	Step7759[11][3] = 24;
	Step7759[11][4] = 34;
	Step7759[11][5] = 46;
	Step7759[11][6] = 64;
	Step7759[11][7] = 96;
	Step7759[11][8] = -2;
	Step7759[11][9] = -9;
	Step7759[11][10] = -16;
	Step7759[11][11] = -24;
	Step7759[11][12] = -34;
	Step7759[11][13] = -46;
	Step7759[11][14] = -64;
	Step7759[11][15] = -96;

	Step7759[12][0] = 3;
	Step7759[12][1] = 11;
	Step7759[12][2] = 19;
	Step7759[12][3] = 29;
	Step7759[12][4] = 41;
	Step7759[12][5] = 57;
	Step7759[12][6] = 79;
	Step7759[12][7] = 117;
	Step7759[12][8] = -3;
	Step7759[12][9] = -11;
	Step7759[12][10] = -19;
	Step7759[12][11] = -29;
	Step7759[12][12] = -41;
	Step7759[12][13] = -57;
	Step7759[12][14] = -79;
	Step7759[12][15] = -117;

	Step7759[13][0] = 4;
	Step7759[13][1] = 13;
	Step7759[13][2] = 24;
	Step7759[13][3] = 36;
	Step7759[13][4] = 50;
	Step7759[13][5] = 69;
	Step7759[13][6] = 96;
	Step7759[13][7] = 143;
	Step7759[13][8] = -4;
	Step7759[13][9] = -13;
	Step7759[13][10] = -24;
	Step7759[13][11] = -36;
	Step7759[13][12] = -50;
	Step7759[13][13] = -69;
	Step7759[13][14] = -96;
	Step7759[13][15] = -143;

	Step7759[14][0] = 4;
	Step7759[14][1] = 16;
	Step7759[14][2] = 29;
	Step7759[14][3] = 44;
	Step7759[14][4] = 62;
	Step7759[14][5] = 85;
	Step7759[14][6] = 118;
	Step7759[14][7] = 175;
	Step7759[14][8] = -4;
	Step7759[14][9] = -16;
	Step7759[14][10] = -29;
	Step7759[14][11] = -44;
	Step7759[14][12] = -62;
	Step7759[14][13] = -85;
	Step7759[14][14] = -118;
	Step7759[14][15] = -175;

	Step7759[15][0] = 6;
	Step7759[15][1] = 20;
	Step7759[15][2] = 36;
	Step7759[15][3] = 54;
	Step7759[15][4] = 76;
	Step7759[15][5] = 104;
	Step7759[15][6] = 144;
	Step7759[15][7] = 214;
	Step7759[15][8] = -6;
	Step7759[15][9] = -20;
	Step7759[15][10] = -36;
	Step7759[15][11] = -54;
	Step7759[15][12] = -76;
	Step7759[15][13] = -104;
	Step7759[15][14] = -144;
	Step7759[15][15] = -214;

	State[0] = -1;
	State[1] = -1;
	State[2] = 0;
	State[3] = 0;
	State[4] = 1;
	State[5] = 2;
	State[6] = 2;
	State[7] = 3;
	State[8] = -1;
	State[9] = -1;
	State[10] = 0;
	State[11] = 0;
	State[12] = 1;
	State[13] = 2;
	State[14] = 2;
	State[15] = 3;
}

SampledSound::~SampledSound(void)
{
    for (UINT32 cnt = 0; cnt < MAXSAMPLES; cnt++) {
        if (Sample_Space[cnt]) {
            delete[] Sample_Space[cnt];
            Sample_Space[cnt] = NULL;
        }
    }


    DeleteCriticalSection(&AudioLock);
}

void SampledSound::ExtractROM(void)
{
    for (UINT32 cnt = 0; cnt < MAXSAMPLES; cnt++) {
        if (Sample_Space[cnt]) {
            delete[] Sample_Space[cnt];
            Sample_Space[cnt] = NULL;
        }
    }

    ZeroMemory(SampleBank, MAXSAMPLES * sizeof(UINT8));
    ZeroMemory(SampleIndex, MAXSAMPLES * sizeof(UINT8));
    ZeroMemory(SampleDummy, MAXSAMPLES * sizeof(UINT8));
    ZeroMemory(SampleStart, MAXSAMPLES * sizeof(INT32));
    ZeroMemory(SampleEnd, MAXSAMPLES * sizeof(INT32));
    ZeroMemory(SampleRate, MAXSAMPLES * sizeof(INT32));
    ZeroMemory(SampleRateDivisor, MAXSAMPLES * sizeof(INT32));
    ZeroMemory(SampleLengthBytes, MAXSAMPLES * sizeof(INT32));
    ZeroMemory(SampleLengthSamples, MAXSAMPLES * sizeof(INT32));
    ZeroMemory(SampleSeconds, MAXSAMPLES * sizeof(float));
    ZeroMemory(TuneLookup, PAGES * 128 * sizeof(UINT8));
    ZeroMemory(SamplesInPage, PAGES * sizeof(INT32));

    TotalSamples = 0;
    Tune = 0;
    NowPlaying = 0;
    Playing = 0;
    Looping = 0;
    EndOfSample = 1;
    PositionL = 0;
    PositionR = 0;
    PlaybackRemainder = 0;
    PlaybackStartRampRemaining = 0;

    if (ROMSize <= 0) {
        return;
    }

    if (ROMSize > SOUNDMEMORYSIZE) {
        ROMSize = SOUNDMEMORYSIZE;
    }

    INT32 pageCount = ROMSize / NEC_PAGE_SIZE;
    if (pageCount > PAGES) {
        pageCount = PAGES;
    }

    INT16* sampleTemp = new (std::nothrow) INT16[SAMPLE_TEMP_CAPACITY];
    if (!sampleTemp) {
        return;
    }

    for (INT32 pageIndex = 0; pageIndex < pageCount; pageIndex++) {
        const INT32 pageBase = pageIndex * NEC_PAGE_SIZE;
        const INT32 pageEnd = pageBase + NEC_PAGE_SIZE;

        if ((pageBase + 4) >= ROMSize) {
            break;
        }

        INT32 position = pageBase;
        INT32 samplesInPage = Memory_Space[position];
        position += 1;

        UINT32 header = Memory_Space[position];
        header = (header << 8);
        position += 1;
        header |= Memory_Space[position];
        header = (header << 8);
        position += 1;
        header |= Memory_Space[position];
        header = (header << 8);
        position += 1;
        header |= Memory_Space[position];
        position += 1;

        if (header != 0x5AA56955) {
            continue;
        }

        if (samplesInPage <= 0) {
            continue;
        }

        if (samplesInPage > 127) {
            samplesInPage = 127;
        }

        SamplesInPage[pageIndex] = samplesInPage;

        for (INT32 sampleIndex = 0; sampleIndex <= samplesInPage; sampleIndex++) {
            if (TotalSamples >= (MAXSAMPLES - 1)) {
                delete[] sampleTemp;
                return;
            }

            TotalSamples += 1;
            const INT32 sampleNum = TotalSamples;

            ZeroMemory(sampleTemp, SAMPLE_TEMP_CAPACITY * sizeof(INT16));

            UINT8 repeat = 0;
            INT32 repeatOffset = 0;
            UINT8 validHeader = 0;
            UINT16 nibbles = 0;
            INT32 byteCount = 0;
            INT32 sampleCount = 0;
            INT32 myRate = 8000;

            position = pageBase + 5 + (sampleIndex * 2);
            if ((position + 1) >= ROMSize || (position + 1) >= pageEnd) {
                continue;
            }

            SampleStart[sampleNum] = Memory_Space[position];
            SampleStart[sampleNum] = (SampleStart[sampleNum] << 8);
            position += 1;
            SampleStart[sampleNum] |= Memory_Space[position];
            position += 1;
            SampleStart[sampleNum] = (SampleStart[sampleNum] << 1);
            SampleStart[sampleNum] += pageBase;

            if (sampleIndex < samplesInPage) {
                position = pageBase + 5 + ((sampleIndex + 1) * 2);
                if ((position + 1) >= ROMSize || (position + 1) >= pageEnd) {
                    continue;
                }

                SampleEnd[sampleNum] = Memory_Space[position];
                SampleEnd[sampleNum] = (SampleEnd[sampleNum] << 8);
                position += 1;
                SampleEnd[sampleNum] |= Memory_Space[position];
                position += 1;
                SampleEnd[sampleNum] = (SampleEnd[sampleNum] << 1);
                SampleEnd[sampleNum] += pageBase;
            }
            else {
                SampleEnd[sampleNum] = pageBase + NEC_PAGE_LAST_OFFSET;
            }

            if (SampleEnd[sampleNum] > ROMSize) {
                SampleEnd[sampleNum] = ROMSize;
            }
            if (SampleEnd[sampleNum] > pageEnd) {
                SampleEnd[sampleNum] = pageEnd;
            }
            if (SampleStart[sampleNum] < pageBase || SampleStart[sampleNum] >= SampleEnd[sampleNum]) {
                continue;
            }

            position = SampleStart[sampleNum] + 1;
            ADPCMIndex = 0;
            ADPCMLast = 0;

            while (position < SampleEnd[sampleNum] && position < ROMSize && sampleCount < static_cast<INT32>(SAMPLE_TEMP_CAPACITY)) {
                if (repeat) {
                    repeat -= 1;
                    position = repeatOffset;
                }

                if (position >= SampleEnd[sampleNum] || position >= ROMSize) {
                    break;
                }

                UINT8 value = Memory_Space[position];
                position += 1;

                switch (value & 0xC0) {
                case 0x00: // Silence
                    if (((value & 0x3F) == 0) && validHeader) {
                        position = SampleEnd[sampleNum];
                    }
                    else {
                        validHeader = 1;
                        const INT32 silenceLength = ((value & 0x3F) * 20);
                        ADPCMIndex = 0;
                        ADPCMLast = 0;
                        for (INT32 cnt = 0; cnt < silenceLength; cnt++) {
                            if (sampleCount >= static_cast<INT32>(SAMPLE_TEMP_CAPACITY)) {
                                position = SampleEnd[sampleNum];
                                break;
                            }
                            sampleTemp[sampleCount] = NECDecodeNibble(0);
                            sampleCount += 1;
                        }
                    }
                    nibbles = 0;
                    break;

                case 0x40: // 256 nibbles
                    myRate = (160000 / ((value & 0x1F) + 1));
                    nibbles = 256;
                    validHeader = 1;
                    break;

                case 0x80: // n nibbles
                    myRate = (160000 / ((value & 0x1F) + 1));
                    if (position >= SampleEnd[sampleNum] || position >= ROMSize) {
                        position = SampleEnd[sampleNum];
                        nibbles = 0;
                        break;
                    }
                    nibbles = static_cast<UINT16>(Memory_Space[position] + 1);
                    position += 1;
                    validHeader = 1;
                    break;

                case 0xC0: // Repeat loop
                    repeat = static_cast<UINT8>((value & 0x07) + 1);
                    repeatOffset = position;
                    validHeader = 1;
                    nibbles = 0;
                    break;
                }

                if (nibbles) {
                    for (UINT16 nibbleCount = 0; nibbleCount < nibbles; nibbleCount++) {
                        if (sampleCount >= static_cast<INT32>(SAMPLE_TEMP_CAPACITY)) {
                            position = SampleEnd[sampleNum];
                            break;
                        }

                        if (nibbleCount & 1) {
                            sampleTemp[sampleCount] = NECDecodeNibble(value & 0x0F);
                        }
                        else {
                            if (position >= SampleEnd[sampleNum] || position >= ROMSize) {
                                position = SampleEnd[sampleNum];
                                break;
                            }
                            value = Memory_Space[position];
                            position += 1;
                            sampleTemp[sampleCount] = NECDecodeNibble((value & 0xF0) >> 4);
                            byteCount += 1;
                        }
                        sampleCount += 1;
                    }
                }
            }

            if (sampleCount > 0) {
                SampleRate[sampleNum] = myRate;
                SampleLengthBytes[sampleNum] = byteCount;
                SampleLengthSamples[sampleNum] = sampleCount;
            }
            else {
                SampleLengthBytes[sampleNum] = 0;
                SampleLengthSamples[sampleNum] = 0;
                SampleRate[sampleNum] = 8000;
            }

            SampleEnd[sampleNum] = SampleStart[sampleNum] + byteCount;
            SampleBank[sampleNum] = static_cast<UINT8>(pageIndex & 0xFF);
            SampleIndex[sampleNum] = static_cast<UINT8>(sampleIndex & 0xFF);
            TuneLookup[SampleBank[sampleNum]][SampleIndex[sampleNum]] = static_cast<UINT8>(sampleNum & 0xFF);

            if (SampleRate[sampleNum]) {
                SampleSeconds[sampleNum] = static_cast<float>(SampleLengthSamples[sampleNum]) / static_cast<float>(SampleRate[sampleNum]);
            }
            else {
                SampleSeconds[sampleNum] = 0.0f;
            }

            if (SampleLengthSamples[sampleNum] > 0) {
                Sample_Space[sampleNum] = new (std::nothrow) INT16[SampleLengthSamples[sampleNum]];
                if (Sample_Space[sampleNum]) {
                    memcpy_s(Sample_Space[sampleNum], SampleLengthSamples[sampleNum] * sizeof(INT16), sampleTemp, SampleLengthSamples[sampleNum] * sizeof(INT16));
                }
            }

        }
    }

    delete[] sampleTemp;
}

INT16 SampledSound::NECDecodeNibble(UINT8 Nibble)
{
    if (Nibble > 15) {
        Nibble &= 0x0F;
    }

    if (ADPCMIndex < 0) {
        ADPCMIndex = 0;
    }
    else if (ADPCMIndex > 15) {
        ADPCMIndex = 15;
    }

    INT32 sample = ADPCMLast + Step7759[ADPCMIndex][Nibble];
    ADPCMIndex += State[Nibble];

    if (ADPCMIndex < 0) {
        ADPCMIndex = 0;
    }
    else if (ADPCMIndex > 15) {
        ADPCMIndex = 15;
    }

    if (sample > 255) {
        sample = 255;
    }
    else if (sample < -255) {
        sample = -255;
    }

    ADPCMLast = sample;
    sample = (sample << 7);

    if (sample > 32767) {
        sample = 32767;
    }
    else if (sample < -32768) {
        sample = -32768;
    }

    return static_cast<INT16>(sample);
}

void SampledSound::NECReset(void)
{
    EnterCriticalSection(&AudioLock);
    BusyTimer = 15;
    Busy = 0;
    PlaybackRemainder = 0;
    PlaybackStartRampRemaining = 0;
    PositionL = 0;
    PositionR = 0;
    Playing = 0;
    Looping = 0;
    EndOfSample = 1;
    LeaveCriticalSection(&AudioLock);
}

void SampledSound::NECInit(LoadSaveClass* LSCIn)
{
    EnterCriticalSection(&AudioLock);
    LSC = LSCIn;
    BankSwitch = 0;
    Tune = 0;
    Busy = 0;
    BusyTimer = 15;
    PlaybackRemainder = 0;
    PlaybackStartRampRemaining = 0;
    LeaveCriticalSection(&AudioLock);
}

void SampledSound::NECStop(void)
{
    EnterCriticalSection(&AudioLock);
    PositionL = 0;
    PositionR = 0;
    Playing = 0;
    Looping = 0;
    EndOfSample = 1;
    PlaybackRemainder = 0;
    PlaybackStartRampRemaining = 0;
    LeaveCriticalSection(&AudioLock);
}

void SampledSound::NECPlay(void)
{
    EnterCriticalSection(&AudioLock);

    NowPlaying = Tune;
    PositionL = 0;
    PositionR = 0;
    PlaybackRemainder = 0;
    PlaybackStartRampRemaining = PA2_AUDIO_START_RAMP_FRAMES;
    Looping = 0;

    if (NowPlaying >= MAXSAMPLES || SampleLengthSamples[NowPlaying] <= 0 || SampleRate[NowPlaying] <= 0 || !Sample_Space[NowPlaying]) {
        EndOfSample = 1;
        Playing = 0;
        Busy = 1;
        Frequency = 8000;
        LeaveCriticalSection(&AudioLock);
        return;
    }

    EndOfSample = 0;
    Playing = 1;
    Busy = 0;
    Frequency = SampleRate[NowPlaying];

    LeaveCriticalSection(&AudioLock);
}

void SampledSound::NECRun(INT32 Cycles)
{
    if (Cycles <= 0) {
        return;
    }

    EnterCriticalSection(&AudioLock);
    if (BusyTimer > 0) {
        if (Cycles >= BusyTimer) {
            BusyTimer = 0;
            Busy = 1;
        }
        else {
            BusyTimer -= Cycles;
        }
    }
    LeaveCriticalSection(&AudioLock);
}

void SampledSound::NECSetTune(UINT8 TuneIn)
{
    EnterCriticalSection(&AudioLock);
    if (BankSwitch >= PAGES) {
        Tune = 0;
        LeaveCriticalSection(&AudioLock);
        return;
    }

    Tune = TuneLookup[BankSwitch][TuneIn & 0x7F];
    LeaveCriticalSection(&AudioLock);
}

void SampledSound::NECSetVolumeControl(UINT8 volCtrl)
{
    // Volume Step 0x1, Volume Dir 0x2, Vol Control 0x4,
    // Volume Disable 0x4 should possibly be 0x8.
    //
    // The emulation thread updates the controller while the front-end audio
    // thread reads volume in FillAudioFrames(), so protect both with AudioLock.
    EnterCriticalSection(&AudioLock);

    volClk = (volCtrl & 1);
    volDir = (volCtrl & 2);
    volEnable = (volCtrl & 4);

    if (prevClk != volClk)
    {
        if (volClk) {
            if (volDir)
            {
                if (volume < NEC_VOLUME_MAX)
                {
                    volume++;
                }
            }
            else
            {
                if (volume > NEC_VOLUME_MIN)
                {
                    volume--;
                }
            }
        }
        prevClk = volClk;
    }

    LeaveCriticalSection(&AudioLock);
}

void SampledSound::NECSetBank(UINT8 BankIn)
{
    EnterCriticalSection(&AudioLock);
    BankSwitch = static_cast<UINT8>(BankIn & 0x07);
    LeaveCriticalSection(&AudioLock);
}

UINT32 SampledSound::FillAudioFrames(INT16* OutInterleavedStereo, UINT32 FramesRequired)
{
    if (!OutInterleavedStereo || FramesRequired == 0) {
        return 0;
    }

    EnterCriticalSection(&AudioLock);

    UINT32 framesGenerated = 0;
    for (UINT32 frame = 0; frame < FramesRequired; frame++) {
        INT32 sample = 0;

        if (Playing) {
            if (NowPlaying >= MAXSAMPLES || SampleLengthSamples[NowPlaying] <= 0 || Frequency <= 0 || !Sample_Space[NowPlaying]) {
                EndOfSample = 1;
                Playing = 0;
                Busy = 1;
                PositionL = 0;
                PositionR = 0;
                PlaybackRemainder = 0;
                PlaybackStartRampRemaining = 0;
            }
            else if (PositionL >= SampleLengthSamples[NowPlaying]) {
                EndOfSample = 1;
                Busy = 1;

                if (Looping) {
                    PositionL = 0;
                    PositionR = 0;
                    EndOfSample = 0;
                    PlaybackRemainder = 0;
                    PlaybackStartRampRemaining = PA2_AUDIO_START_RAMP_FRAMES;
                }
                else {
                    Playing = 0;
                }
            }
            else {
                const INT32 sampleCount = SampleLengthSamples[NowPlaying];
                const INT32 sourcePos = PositionL;
                INT32 nextPos = sourcePos + 1;
                if (nextPos >= sampleCount) {
                    nextPos = sourcePos;
                }

                INT32 fraction = PlaybackRemainder;
                if (fraction < 0) {
                    fraction = 0;
                }
                else if (fraction >= PA2_AUDIO_OUTPUT_SAMPLE_RATE) {
                    fraction = PA2_AUDIO_OUTPUT_SAMPLE_RATE - 1;
                }

                const INT32 s0 = Sample_Space[NowPlaying][sourcePos];
                const INT32 s1 = Sample_Space[NowPlaying][nextPos];
                sample = static_cast<INT32>(static_cast<INT64>(s0) +
                    ((static_cast<INT64>(s1 - s0) * fraction) / PA2_AUDIO_OUTPUT_SAMPLE_RATE));

                if (PlaybackStartRampRemaining > 0) {
                    const INT32 rampStep = PA2_AUDIO_START_RAMP_FRAMES - PlaybackStartRampRemaining + 1;
                    sample = static_cast<INT32>((static_cast<INT64>(sample) * rampStep) / PA2_AUDIO_START_RAMP_FRAMES);
                    PlaybackStartRampRemaining -= 1;
                }

                PlaybackRemainder += Frequency;
                INT32 samplesToAdvance = PlaybackRemainder / PA2_AUDIO_OUTPUT_SAMPLE_RATE;
                PlaybackRemainder = PlaybackRemainder % PA2_AUDIO_OUTPUT_SAMPLE_RATE;

                if (samplesToAdvance > 0) {
                    if ((PositionL + samplesToAdvance) >= sampleCount) {
                        PositionL = sampleCount;
                        PositionR = PositionL;
                        EndOfSample = 1;
                        Busy = 1;

                        if (Looping) {
                            PositionL = 0;
                            PositionR = 0;
                            EndOfSample = 0;
                            PlaybackRemainder = 0;
                            PlaybackStartRampRemaining = PA2_AUDIO_START_RAMP_FRAMES;
                        }
                        else {
                            Playing = 0;
                        }
                    }
                    else {
                        PositionL += samplesToAdvance;
                        PositionR = PositionL;
                    }
                }
            }
        }

        // Apply the emulated 0..99 hardware volume directly to the PCM stream.
        // This replaces the old BASS channel-volume stage.  A value of 99 is
        // unity gain, while 0 produces silence.
        const INT32 volumeLevel = (volume <= NEC_VOLUME_MAX) ? volume : NEC_VOLUME_MAX;
        sample = static_cast<INT32>((static_cast<INT64>(sample) * volumeLevel) / NEC_VOLUME_MAX);

        if (sample > 32767) {
            sample = 32767;
        }
        else if (sample < -32768) {
            sample = -32768;
        }

        const INT16 outSample = static_cast<INT16>(sample);
        const UINT32 destIndex = frame * PA2_AUDIO_OUTPUT_CHANNELS;
        OutInterleavedStereo[destIndex] = outSample;
        OutInterleavedStereo[destIndex + 1] = outSample;
        framesGenerated += 1;
    }

    LeaveCriticalSection(&AudioLock);
    return framesGenerated;
}

UINT8 SampledSound::GetBusy()
{
    EnterCriticalSection(&AudioLock);
    const UINT8 ret = Busy;
    LeaveCriticalSection(&AudioLock);
    return ret;
}

void SampledSound::SetMemory(UINT32 pos, UINT8 value)
{
    if (pos < SOUNDMEMORYSIZE) {
        Memory_Space[pos] = value;
    }
}

void SampledSound::ClearMemory()
{
    ZeroMemory(Memory_Space, SOUNDMEMORYSIZE * sizeof(UINT8));
}

void SampledSound::CopyROM(UINT32 pos, const UINT8* source, UINT32 length)
{
    if (!source || pos >= SOUNDMEMORYSIZE || length == 0) {
        return;
    }

    UINT32 maxLength = SOUNDMEMORYSIZE - pos;
    if (length > maxLength) {
        length = maxLength;
    }

    memcpy(Memory_Space + pos, source, length);
}

void SampledSound::SetROMSize(UINT32 size)
{
    if (size > SOUNDMEMORYSIZE) {
        ROMSize = SOUNDMEMORYSIZE;
    }
    else {
        ROMSize = static_cast<INT32>(size);
    }
}

void SampledSound::SaveState()
{
    if (!LSC) {
        return;
    }

    EnterCriticalSection(&AudioLock);

    LSC->SaveToBuffer(Playing);
    LSC->SaveToBuffer(Looping);
    LSC->SaveToBuffer(Stopped);
    LSC->SaveToBuffer(PositionL);
    LSC->SaveToBuffer(PositionR);
    LSC->SaveToBuffer(EndOfSample);
    LSC->SaveToBuffer(Restarted);
    LSC->SaveToBuffer(NowPlaying);
    LSC->SaveToBuffer(Frequency);
    LSC->SaveToBuffer(BankSwitch);
    LSC->SaveToBuffer(Busy);
    LSC->SaveToBuffer(BusyTimer);
    LSC->SaveToBuffer(Tune);
    LSC->SaveToBuffer(PlaybackRemainder);

    LeaveCriticalSection(&AudioLock);
}

void SampledSound::LoadState()
{
    if (!LSC) {
        return;
    }

    EnterCriticalSection(&AudioLock);

    LSC->LoadFromBuffer(Playing);
    LSC->LoadFromBuffer(Looping);
    LSC->LoadFromBuffer(Stopped);
    LSC->LoadFromBuffer(PositionL);
    LSC->LoadFromBuffer(PositionR);
    LSC->LoadFromBuffer(EndOfSample);
    LSC->LoadFromBuffer(Restarted);
    LSC->LoadFromBuffer(NowPlaying);
    LSC->LoadFromBuffer(Frequency);
    LSC->LoadFromBuffer(BankSwitch);
    LSC->LoadFromBuffer(Busy);
    LSC->LoadFromBuffer(BusyTimer);
    LSC->LoadFromBuffer(Tune);
    LSC->LoadFromBuffer(PlaybackRemainder);

    if (BankSwitch >= PAGES) {
        BankSwitch = 0;
    }
    if (NowPlaying >= MAXSAMPLES) {
        NowPlaying = 0;
    }
    if (Tune >= MAXSAMPLES) {
        Tune = 0;
    }
    if (PositionL < 0) {
        PositionL = 0;
    }
    if (PositionR < 0) {
        PositionR = 0;
    }
    if (Frequency < 0) {
        Frequency = 0;
    }
    if (PlaybackRemainder < 0 || PlaybackRemainder >= PA2_AUDIO_OUTPUT_SAMPLE_RATE) {
        PlaybackRemainder = 0;
    }
    PlaybackStartRampRemaining = 0;

    LeaveCriticalSection(&AudioLock);
}
