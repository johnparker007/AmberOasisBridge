#include "stdafx.h"
#include "SoundMain.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

namespace
{
    class AudioLockGuard
    {
    public:
        explicit AudioLockGuard(CRITICAL_SECTION* Lock) : Lock_(Lock)
        {
            if (Lock_) { EnterCriticalSection(Lock_); }
        }

        ~AudioLockGuard()
        {
            if (Lock_) { LeaveCriticalSection(Lock_); }
        }

    private:
        CRITICAL_SECTION* Lock_;
    };
}

SampledSound::~SampledSound(void)
{
    if (AudioLockReady) {
        DeleteCriticalSection(&AudioLock);
        AudioLockReady = 0;
    }
}

SampledSound::SampledSound(void)
{
    AudioLockReady = 0;
    InitializeCriticalSection(&AudioLock);
    AudioLockReady = 1;

    for (int cnt = 0; cnt < YMZCHANNELS; cnt++) {
        Steps[cnt] = 0x7f;
    }

    ZeroMemory(Signals, sizeof(Signals));
    ZeroMemory(Nib, sizeof(Nib));
    ZeroMemory(Pitch, sizeof(Pitch));
    ZeroMemory(PrevPitch, sizeof(PrevPitch));
    ZeroMemory(KeyOn, sizeof(KeyOn));
    ZeroMemory(PrevKeyOn, sizeof(PrevKeyOn));
    ZeroMemory(QMode, sizeof(QMode));
    ZeroMemory(Loop, sizeof(Loop));
    ZeroMemory(Level, sizeof(Level));
    ZeroMemory(PrevLevel, sizeof(PrevLevel));
    ZeroMemory(Panpot, sizeof(Panpot));
    ZeroMemory(PrevPanpot, sizeof(PrevPanpot));
    ZeroMemory(StartAddress, sizeof(StartAddress));
    ZeroMemory(EndAddress, sizeof(EndAddress));
    ZeroMemory(LoopStartAddress, sizeof(LoopStartAddress));
    ZeroMemory(LoopEndAddress, sizeof(LoopEndAddress));
    ZeroMemory(Position, sizeof(Position));
    ZeroMemory(Memory_Space, sizeof(Memory_Space));
    ZeroMemory(SampledBuffer, sizeof(SampledBuffer));
    ZeroMemory(ChannelBuffer, sizeof(ChannelBuffer));
    ZeroMemory(ChannelReadPosition, sizeof(ChannelReadPosition));
    ZeroMemory(ChannelWritePosition, sizeof(ChannelWritePosition));
    ZeroMemory(ChannelFramesQueued, sizeof(ChannelFramesQueued));
    ZeroMemory(ChannelReadFraction, sizeof(ChannelReadFraction));
    ZeroMemory(ChannelFrequency, sizeof(ChannelFrequency));
    ZeroMemory(ChannelVolume, sizeof(ChannelVolume));
    ZeroMemory(ChannelPan, sizeof(ChannelPan));
    ZeroMemory(ChannelPlaying, sizeof(ChannelPlaying));

    LeftEnable = 0;
    LeftOutputChannel = 0;
    RightEnable = 0;
    RightOutputChannel = 0;
    DSPEnable = 0;
    DSPData = 0;
    ROMAddress = 0;
    ROMData = 0;
    IRQMask = 0;
    IRQEnable = 0;
    IRQOutFlag = 0;
    KeyOnEnable = 0;
    ROMEnable = 0;
    LSITest = 0;
    Status = 0;
    RegSelect = 0;

    YMZInit();
}

void SampledSound::YMZWriteRegSelect(UINT8 value){
	AudioLockGuard Lock(AudioLockReady ? &AudioLock : NULL);
	RegSelect = value;
}

void SampledSound::YMZWriteReg(UINT8 value){
	AudioLockGuard Lock(AudioLockReady ? &AudioLock : NULL);
	
	int Channel, Section;

	YMZUpdate();

	if (RegSelect <= 0x1f){
		//Block 1
		Channel = (RegSelect / 4);
		Section = (RegSelect - (Channel * 4));
		switch (Section){
		case 0:	//Pitch bits 0-7
			Pitch[Channel] &= 0x100;
			Pitch[Channel] |= value;
			break; 
	    case 1:	//Key On, Q Mode, Loop, Pitch Bit 8
						
			KeyOn[Channel] = ((value & 0x80) >> 7);
			QMode[Channel] = ((value & 0x60) >> 5);
			Loop[Channel] = ((value & 0x10) >> 4);
			Pitch[Channel] &= 0xff;
			Pitch[Channel] |= ((value & 1) << 8);

			if (!PrevKeyOn[Channel]) {
				if (KeyOn[Channel]) {
					//Key Turned On
					Position[Channel] = StartAddress[Channel];
					Steps[Channel] = 0x7f;
					Signals[Channel] = 0;
					Nib[Channel] = 0;
				}
			}
			
			if (!KeyOn[Channel]) {
				//Key Turned Off
				Position[Channel] = EndAddress[Channel] + 1;					
			}
			
			if (PrevPitch[Channel] != Pitch[Channel]){
				//Set Channel Sample Rate
				//Decoding Mode
				switch (QMode[Channel]){
				case 1: //4 Bit ADPCM
					ChannelFrequency[Channel] = (((double(Pitch[Channel] & 0xff)) * 172.265625) + 172.265625);//Capped @ 44.1Khz
					break;
				case 2: //8 Bit Linear PCM
				case 3: //16 Bit Linear PCM
					//Set Channel Sample Rate
					ChannelFrequency[Channel] = ((double(Pitch[Channel]) * 172.265625) + 172.265625);//88.2Khz Max
					break;				
				}				
			}

			PrevPitch[Channel] = Pitch[Channel];
			PrevKeyOn[Channel] = KeyOn[Channel];
			break; 
		case 2: //Total Level
			Level[Channel] = value;
			if (PrevLevel[Channel] != Level[Channel]){				
				//Set Channel Volume
				ChannelVolume[Channel] = (float(Level[Channel]) * 0.00390625f);
			}
			PrevLevel[Channel] = Level[Channel];
			break;
		case 3: //Panning
			Panpot[Channel] = (value & 0xf);
			if (PrevPanpot[Channel] != Panpot[Channel]){	
				//Set Channel Pan
				ChannelPan[Channel] = (float(Panpot[Channel]) * 0.0625f);
			}
			PrevPanpot[Channel] = Panpot[Channel];
			break;
		}
	} else if (RegSelect <= 0x3f){
		//Block 2
		Channel = ((RegSelect - 0x20) / 4);
		Section = ((RegSelect - 0x20) - (Channel * 4));
		switch (Section){
		case 0:	//Start Address High Byte
			StartAddress[Channel] &= 0xffff;
			StartAddress[Channel] |= (value << 16);

			break; 
	    case 1:	//Loop Start Address High Byte
			LoopStartAddress[Channel] &= 0xffff;
			LoopStartAddress[Channel] |= (value << 16);
			break; 
		case 2: //Loop End Address High Byte
			LoopEndAddress[Channel] &= 0xffff;
			LoopEndAddress[Channel] |= (value << 16);
			break;
		case 3: //End Address High Byte
			EndAddress[Channel] &= 0xffff;
			EndAddress[Channel] |= (value << 16);
			break;
		}
	} else if (RegSelect <= 0x5f){
		//Block 3
		Channel = ((RegSelect - 0x40) / 4);
		Section = ((RegSelect - 0x40) - (Channel * 4));
		switch (Section){
		case 0:	//Start Address Mid Byte
			StartAddress[Channel] &= 0xff00ff;
			StartAddress[Channel] |= (value << 8);

			break; 
	    case 1:	//Loop Start Address Mid Byte
			LoopStartAddress[Channel] &= 0xff00ff;
			LoopStartAddress[Channel] |= (value << 8);
			break; 
		case 2: //Loop End Address Mid Byte
			LoopEndAddress[Channel] &= 0xff00ff;
			LoopEndAddress[Channel] |= (value << 8);
			break;
		case 3: //End Address Mid Byte
			EndAddress[Channel] &= 0xff00ff;
			EndAddress[Channel] |= (value << 8);
			break;
		}
	} else if (RegSelect <= 0x7f){
		//Block 4
		Channel = ((RegSelect - 0x60) / 4);
		Section = ((RegSelect - 0x60) - (Channel * 4));
		switch (Section){
		case 0:	//Start Address Low Byte
			StartAddress[Channel] &= 0xffff00;
			StartAddress[Channel] |= (value);

			break; 
	    case 1:	//Loop Start Address Low Byte
			LoopStartAddress[Channel] &= 0xffff00;
			LoopStartAddress[Channel] |= (value);
			break; 
		case 2: //Loop End Address Low Byte
			LoopEndAddress[Channel] &= 0xffff00;
			LoopEndAddress[Channel] |= (value);
			break;
		case 3: //End Address Low Byte
			EndAddress[Channel] &= 0xffff00;
			EndAddress[Channel] |= (value);
			break;
		}
	} else if (RegSelect <= 0x87){
		//Block 5
		switch (RegSelect & 7){
		case 0:	// Left / Right Channel Settings
			LeftEnable = ((value & 0x80) >> 7);
			LeftOutputChannel = ((value & 0x70) >> 4);
			RightEnable = ((value & 0x8) >> 3);
			RightOutputChannel = (value & 7);
			break; 
	    case 1:	// DSP Enable
			DSPEnable = (value & 1);
			break; 
		case 2: // DSP Data
			if (!DSPEnable){
				DSPData = value;
			}
			break;
		case 3: // Nothing?
			
			break; 
		case 4:	// ROM High Address			
			ROMAddress &= 0xffff;
			ROMAddress |= (value << 16);
			break; 
	    case 5:	// ROM Mid Address
			ROMAddress &= 0xff00ff;
			ROMAddress |= (value << 8);
			break; 
		case 6: // ROM Low Address
			ROMAddress &= 0xffff00;
			ROMAddress |= value;
			break;
		case 7: //ROM Data Byte
			//YMZ Can have ROM or RAM attached, if RAM write to it here, but Epoch is set up for ROM so no write can occur
			break;
		}
	} else if (RegSelect == 0xE0){
		// Older code treated 0xE0 as IRQ mask. Keep it as an alias for
		// compatibility with any existing layouts/traces.
		IRQMask = value;
		UpdateIrqState();

	} else if (RegSelect == 0xFE){
		// YMZ280B IRQ mask register
		IRQMask = value;
		UpdateIrqState();

	} else if (RegSelect == 0xFF){
		// Key On Enable, ROM Enable, IRQ Enable, LSI TEST
		KeyOnEnable = ((value & 0x80) >> 7);
		ROMEnable = ((value & 0x40) >> 6);
		IRQEnable = ((value & 0x10) >> 4);
		LSITest = (value & 3);
		UpdateIrqState();
	}

}

UINT8 SampledSound::YMZReadReg(){
	AudioLockGuard Lock(AudioLockReady ? &AudioLock : NULL);
	
	UINT8 ret = 0;
	
	int Channel, Section;

	YMZUpdate();

	if (RegSelect <= 0x1f){
		//Block 1
		Channel = (RegSelect / 4);
		Section = (RegSelect - (Channel * 4));
		switch (Section){
		case 0:	//Pitch bits 0-7			
			ret = (Pitch[Channel] & 0xff);
			break; 
	    case 1:	//Key On, Q Mode, Loop, Pitch Bit 8
						
			ret |= (KeyOn[Channel] << 7);
			ret |= (QMode[Channel] << 5);
			ret |= (Loop[Channel]  << 4);
			ret |= ((Pitch[Channel] & 0x100) >> 8);

			break; 
		case 2: //Total Level
			ret = Level[Channel];
			break;
		case 3: //Pan
			ret = Panpot[Channel];			
			break;
		}
	} else if (RegSelect <= 0x3f){
		//Block 2
		Channel = ((RegSelect - 0x20) / 4);
		Section = ((RegSelect - 0x20) - (Channel * 4));
		switch (Section){
		case 0:	//Start Address High Byte
			ret = (StartAddress[Channel] >> 16);
			break; 
	    case 1:	//Loop Start Address High Byte
			ret = (LoopStartAddress[Channel] >> 16);
			break; 
		case 2: //Loop End Address High Byte
			ret = (LoopEndAddress[Channel] >> 16);
			break;
		case 3: //End Address High Byte
			ret = (EndAddress[Channel] >> 16);
			break;
		}
	} else if (RegSelect <= 0x5f){
		//Block 3
		Channel = ((RegSelect - 0x40) / 4);
		Section = ((RegSelect - 0x40) - (Channel * 4));
		switch (Section){
		case 0:	//Start Address Mid Byte
			ret = (StartAddress[Channel] >> 8);
			break; 
	    case 1:	//Loop Start Address Mid Byte
			ret = (LoopStartAddress[Channel] >> 8);
			break; 
		case 2: //Loop End Address Mid Byte
			ret = (LoopEndAddress[Channel] >> 8);
			break;
		case 3: //End Address Mid Byte
			ret = (EndAddress[Channel] >> 8);
			break;
		}
	} else if (RegSelect <= 0x7f){
		//Block 4
		Channel = ((RegSelect - 0x60) / 4);
		Section = ((RegSelect - 0x60) - (Channel * 4));
		switch (Section){
		case 0:	//Start Address Low Byte
			ret = (StartAddress[Channel] & 0xff);
			break; 
	    case 1:	//Loop Start Address Low Byte
			ret = (LoopStartAddress[Channel] & 0xff);
			break; 
		case 2: //Loop End Address Low Byte
			ret = (LoopEndAddress[Channel] & 0xff);
			break;
		case 3: //End Address Low Byte
			ret = (EndAddress[Channel] & 0xff);
			break;
		}
	} else if (RegSelect <= 0x87){
		//Block 5
		switch (RegSelect & 7){
		case 0:	// Left / Right Channel DSP Settings
			ret |= (LeftEnable << 7);
			ret |= (LeftOutputChannel << 4);
			ret |= (RightEnable << 3);
			ret |= (RightOutputChannel);			
			break; 
	    case 1:	// DSP Enable
			ret = DSPEnable;
			break; 
		case 2: // DSP Data
			ret = DSPData;			
			break;
		case 3: // Nothing?
			
			break; 
		case 4:	// ROM High Address			
			ret = (ROMAddress >> 16);			
			break; 
	    case 5:	// ROM Mid Address
			ret = (ROMAddress >> 8);
			break; 
		case 6: // ROM Low Address
			ret = (ROMAddress & 0xff);
			break;
		case 7: //ROM Data Byte
			ret = Memory_Space[ROMAddress &= 0x1fffff];
			ROMAddress++;
			break;
		}
	} else if (RegSelect == 0xE0){
		// IRQ Mask
		ret = IRQMask;
	} else if (RegSelect == 0xFE){
		// YMZ280B IRQ mask register
		ret = IRQMask;
	} else if (RegSelect == 0xFF){
		// Key On Enable, ROM Enable, IRQ Enable, LSI TEST
		ret |= (KeyOnEnable << 7);
		ret |= (ROMEnable << 6);
		ret |= (IRQEnable << 4);
		ret |= (LSITest);
	} else {
		//Unknown!
		ret = 0;
	}

	return ret;

}
UINT8	SampledSound::YMZReadStatus(){
	AudioLockGuard Lock(AudioLockReady ? &AudioLock : NULL);
	UINT8 ret = Status;
	// Reading the status register acknowledges/clears the channel-end flags.
	Status = 0;
	UpdateIrqState();
	return ret;
}
UINT8	SampledSound::YMZReadMemory(){
	AudioLockGuard Lock(AudioLockReady ? &AudioLock : NULL);
	UINT8 ret = 0;
	ret = Memory_Space[ROMAddress &= 0x1fffff];
	ROMAddress++;
	return ret;
}

void SampledSound::YMZDecodeNibble(UINT8 Channel, UINT8 Nibble){
int Signal, Step;

	Signal = Signals[Channel];
    Step = Steps[Channel];
        
    //Calculate Signal
    Signal = (Signal + ((Step * Diff_LookUp[Nibble & 0xf]) >> 0x3));
        
    //Clamp
    if (Signal > 32767){
        Signal = 32767;
	} else if (Signal < -32768){
        Signal = -32768;
	}
        
    //Adjust Step
    Step = ((Step * Index_Scale[Nibble & 0x7]) >> 0x8);
    //Clamp
    if (Step > 24576){
        Step = 24576;
	} else if (Step < 127){
        Step = 127;
	}
        
    Signals[Channel] = Signal;
    Steps[Channel] = Step;
}

void SampledSound::YMZReset(void){
	AudioLockGuard Lock(AudioLockReady ? &AudioLock : NULL);
	
	int cnt;

	for (cnt = 0; cnt < YMZCHANNELS; cnt++){
		//YMZ Decoding
		Signals[cnt] = 0;
		Steps[cnt] = 0x7f;
		Nib[cnt] = 0;
		//YMZ Function Registers
		Pitch[cnt] = 0;
		KeyOn[cnt] = 0;
		PrevKeyOn[cnt] = 0;
		QMode[cnt] = 0;
		Loop[cnt] = 0;
		Level[cnt] = 0;
		Panpot[cnt] = 0;
		StartAddress[cnt] = 0;
		EndAddress[cnt] = 0;
		LoopStartAddress[cnt] = 0;
		LoopEndAddress[cnt] = 0;
		Position[cnt] = 0;		
	}
	
	
	//YMZ Utility Registers
	LeftEnable = 0;
	LeftOutputChannel = 0;
	RightEnable = 0;
	RightOutputChannel = 0;
	DSPEnable = 0;
	DSPData = 0;
	ROMAddress = 0;
	ROMData = 0;
	IRQMask = 0;
	IRQEnable = 0;
	IRQOutFlag = 0;
	KeyOnEnable = 0;
	ROMEnable = 0;
	LSITest = 0;

	Status = 0;
	
}
void SampledSound::YMZInit(){//(LoadSaveCompressDLLClass * LSCIn)
    AudioLockGuard Lock(AudioLockReady ? &AudioLock : NULL);

    int cnt, Val;

    //Diff_Lookup Table
    for (cnt = 0; cnt < 16; cnt++){
        Val = (((cnt & 7) << 1) + 1);
        if (cnt & 8) {
            Diff_LookUp[cnt] = (0 - Val);
        } else {
            Diff_LookUp[cnt] = Val;
        }
    }

    Index_Scale[0] = 0xe6;
    Index_Scale[1] = 0xe6;
    Index_Scale[2] = 0xe6;
    Index_Scale[3] = 0xe6;
    Index_Scale[4] = 0x133;
    Index_Scale[5] = 0x199;
    Index_Scale[6] = 0x200;
    Index_Scale[7] = 0x266;

    for (cnt = 0; cnt < YMZCHANNELS; cnt++){
        // Replacement for the former external push-stream creation and play calls.
        ClearChannelBuffer((UINT8)cnt);
        ChannelPlaying[cnt] = 1;
        ChannelFrequency[cnt] = 11025.0;
        ChannelVolume[cnt] = 1.0f;
        ChannelPan[cnt] = 0.0f;
    }
}

int SampledSound::LoadSoundROM(char *name1, char *name2, char *name3, char *name4)
{
    const char* Names[4] = { name1, name2, name3, name4 };
    std::vector<UINT8> NewMemory(SoundMemorySize, 0);
    size_t Offset = 0;
    bool AnyFile = false;

    for (const char* Name : Names) {
        if (!Name || !Name[0]) { continue; }
        AnyFile = true;

        std::ifstream Input(Name, std::ios::binary | std::ios::ate);
        if (!Input) { return 0; }

        const std::streamoff Length = Input.tellg();
        if (Length < 0) { return 0; }

        const size_t FileSize = static_cast<size_t>(Length);
        if (FileSize > NewMemory.size() - Offset) { return 0; }

        Input.seekg(0, std::ios::beg);
        if (FileSize && !Input.read(reinterpret_cast<char*>(NewMemory.data() + Offset), Length)) {
            return 0;
        }
        Offset += FileSize;
    }

    if (!AnyFile || Offset == 0 || Offset > 0xffffffffULL) { return 0; }
    return LoadSoundData(NewMemory.data(), static_cast<UINT32>(Offset));
}

int SampledSound::LoadSoundData(const UINT8* Data, UINT32 Size)
{
    if (!Data || Size == 0 || Size > SoundMemorySize) { return 0; }

    AudioLockGuard Lock(AudioLockReady ? &AudioLock : NULL);
    ZeroMemory(Memory_Space, SoundMemorySize);
    CopyMemory(Memory_Space, Data, Size);
    return static_cast<int>(Size);
}

void SampledSound::UpdateIrqState(void) {
	// YMZ280B IRQ is level-style: asserted while an enabled status bit is set.
	IRQOutFlag = (IRQEnable && (Status & IRQMask)) ? 1 : 0;
}

void SampledSound::DoInterrupt(void) {
	UpdateIrqState();
}

bool SampledSound::YMZGetIRQ() {
	AudioLockGuard Lock(AudioLockReady ? &AudioLock : NULL);
	// Do not clear here. The line remains asserted until status/mask/enable changes.
	return IRQOutFlag != 0;
}

void SampledSound::YMZUpdate(void){
    AudioLockGuard Lock(AudioLockReady ? &AudioLock : NULL);

    int BufferUsed, SampleCount, loop, DecodeLoop;
    UINT16 BufferToFill;

    // Run the chip exactly as the original Epoch implementation did. The
    // BASS queue calls are replaced by private per-channel push buffers.
    if (KeyOnEnable){
        for (loop = 0; loop < YMZCHANNELS; loop++){
            BufferToFill = 0;
            // BASS_ChannelGetData(BASS_DATA_AVAILABLE) returned queued bytes;
            // the original Epoch code then multiplied that result by
            // sizeof(INT16). Preserve that exact (slightly unusual) arithmetic
            // so refill thresholds and playback latency remain unchanged.
            const UINT32 AvailableBytes = ChannelFramesQueued[loop] * OUTPUTCHANNELS * sizeof(INT16);
            BufferUsed = static_cast<int>(AvailableBytes * sizeof(INT16));
            if (BufferUsed < BUFFERSIZE){
                BufferToFill = static_cast<UINT16>(BUFFERSIZE - BufferUsed);
                BufferToFill = static_cast<UINT16>(BufferToFill >> 1);
                ZeroMemory(SampledBuffer,
                    static_cast<size_t>(BufferToFill) * OUTPUTCHANNELS * sizeof(INT16));
            }

            if (KeyOn[loop] && QMode[loop]){
                DecodeLoop = 0;
                SampleCount = 0;
                switch (QMode[loop]){
                case 1: //4 Bit ADPCM
                    while (DecodeLoop < BufferToFill){
                        if (Position[loop] <= EndAddress[loop]) {
                            if (Nib[loop] == 0){
                                Nib[loop] = 1;
                                YMZDecodeNibble((UINT8)loop, ((Memory_Space[Position[loop]] & 0xf0) >> 4));
                            } else {
                                Nib[loop] = 0;
                                YMZDecodeNibble((UINT8)loop, (Memory_Space[Position[loop]] & 0xf));
                                Position[loop]++;
                            }
                            SampledBuffer[SampleCount] = Signals[loop];
                            SampleCount++;
                            SampledBuffer[SampleCount] = Signals[loop];
                            SampleCount++;
                            if (Loop[loop]){
                                if (Position[loop] > LoopEndAddress[loop]){
                                    Position[loop] = LoopStartAddress[loop];
                                }
                            } else if (Position[loop] > EndAddress[loop]){
                                Status |= (1 << loop);
                                DoInterrupt();
                            }
                        } else {
                            DecodeLoop = BufferToFill;
                            Status |= (1 << loop);
                            DoInterrupt();
                        }
                        DecodeLoop++;
                    }
                    break;
                case 2: //8 Bit Linear PCM
                    while (DecodeLoop < BufferToFill){
                        if (Position[loop] <= EndAddress[loop]) {
                            char sample = static_cast<char>(Memory_Space[Position[loop]]);
                            SampledBuffer[SampleCount] = sample;
                            SampleCount++;
                            SampledBuffer[SampleCount] = sample;
                            SampleCount++;
                            Position[loop]++;
                            if (Loop[loop]){
                                if (Position[loop] > LoopEndAddress[loop]){
                                    Position[loop] = LoopStartAddress[loop];
                                }
                            } else if (Position[loop] > EndAddress[loop]){
                                Status |= (1 << loop);
                                DoInterrupt();
                            }
                        } else {
                            DecodeLoop = BufferToFill;
                            Status |= (1 << loop);
                            DoInterrupt();
                        }
                        DecodeLoop++;
                    }
                    break;
                case 3: //16 Bit Linear PCM
                    while (DecodeLoop < BufferToFill){
                        if (Position[loop] <= EndAddress[loop]) {
                            short sample = static_cast<short>((Memory_Space[Position[loop]] << 8) |
                                Memory_Space[Position[loop] + 1]);
                            SampledBuffer[SampleCount] = sample;
                            SampleCount++;
                            SampledBuffer[SampleCount] = sample;
                            SampleCount++;
                            Position[loop] += 2;
                            if (Loop[loop]){
                                if (Position[loop] > LoopEndAddress[loop]){
                                    Position[loop] = LoopStartAddress[loop];
                                }
                            } else if (Position[loop] > EndAddress[loop]){
                                Status |= (1 << loop);
                                DoInterrupt();
                            }
                        } else {
                            DecodeLoop = BufferToFill;
                            Status |= (1 << loop);
                            DoInterrupt();
                        }
                        DecodeLoop++;
                    }
                    break;
                }
            }
            if (BufferToFill > 0){
                PushChannelFrames((UINT8)loop, SampledBuffer, BufferToFill);
            }
        }
    }
}

void SampledSound::ClearChannelBuffer(UINT8 Channel)
{
    if (Channel >= YMZCHANNELS) { return; }
    ChannelReadPosition[Channel] = 0;
    ChannelWritePosition[Channel] = 0;
    ChannelFramesQueued[Channel] = 0;
    ChannelReadFraction[Channel] = 0.0;
    ZeroMemory(ChannelBuffer[Channel], sizeof(ChannelBuffer[Channel]));
}

void SampledSound::PushChannelFrames(UINT8 Channel, const signed short* InterleavedStereo, UINT32 Frames)
{
    if (Channel >= YMZCHANNELS || !InterleavedStereo || Frames == 0) { return; }
    const UINT32 Available = ChannelBufferFrames - ChannelFramesQueued[Channel];
    const UINT32 ToWrite = (Frames < Available) ? Frames : Available;
    for (UINT32 Frame = 0; Frame < ToWrite; ++Frame) {
        ChannelBuffer[Channel][ChannelWritePosition[Channel]] =
            InterleavedStereo[Frame * OUTPUTCHANNELS];
        ChannelWritePosition[Channel] = (ChannelWritePosition[Channel] + 1) % ChannelBufferFrames;
    }
    ChannelFramesQueued[Channel] += ToWrite;
}

signed short SampledSound::PeekChannelFrame(UINT8 Channel, UINT32 Offset) const
{
    if (Channel >= YMZCHANNELS || Offset >= ChannelFramesQueued[Channel]) { return 0; }
    const UINT32 PositionInBuffer =
        (ChannelReadPosition[Channel] + Offset) % ChannelBufferFrames;
    return ChannelBuffer[Channel][PositionInBuffer];
}

void SampledSound::ConsumeChannelFrame(UINT8 Channel)
{
    if (Channel >= YMZCHANNELS || ChannelFramesQueued[Channel] == 0) { return; }
    ChannelReadPosition[Channel] = (ChannelReadPosition[Channel] + 1) % ChannelBufferFrames;
    ChannelFramesQueued[Channel]--;
}

INT16 SampledSound::ClampSample(INT64 Value)
{
    if (Value > 32767) { return 32767; }
    if (Value < -32768) { return -32768; }
    return static_cast<INT16>(Value);
}

UINT32 SampledSound::FillAudioFrames(INT16* OutInterleavedStereo, UINT32 FramesRequired)
{
    if (!OutInterleavedStereo || FramesRequired == 0) { return 0; }

    AudioLockGuard Lock(AudioLockReady ? &AudioLock : NULL);
    ZeroMemory(OutInterleavedStereo,
        static_cast<size_t>(FramesRequired) * PA2_AUDIO_OUTPUT_CHANNELS * sizeof(INT16));

    for (UINT32 Frame = 0; Frame < FramesRequired; ++Frame) {
        INT64 LeftMix = 0;
        INT64 RightMix = 0;

        for (UINT8 Channel = 0; Channel < YMZCHANNELS; ++Channel) {
            if (!ChannelPlaying[Channel] || ChannelFramesQueued[Channel] == 0) { continue; }

            const INT32 Current = PeekChannelFrame(Channel, 0);
            INT32 Sample = Current;
            if (ChannelFramesQueued[Channel] > 1 && ChannelReadFraction[Channel] > 0.0) {
                const INT32 Next = PeekChannelFrame(Channel, 1);
                Sample = Current + static_cast<INT32>(
                    static_cast<double>(Next - Current) * ChannelReadFraction[Channel]);
            }

            const double Frequency = (ChannelFrequency[Channel] > 0.0)
                ? ChannelFrequency[Channel] : 11025.0;
            ChannelReadFraction[Channel] += Frequency / PA2_AUDIO_OUTPUT_SAMPLE_RATE;
            while (ChannelReadFraction[Channel] >= 1.0 && ChannelFramesQueued[Channel] > 0) {
                ChannelReadFraction[Channel] -= 1.0;
                ConsumeChannelFrame(Channel);
            }
            if (ChannelFramesQueued[Channel] == 0) {
                ChannelReadFraction[Channel] = 0.0;
            }

			const double Volume = (((0.0) > (static_cast<double>(ChannelVolume[Channel]))) ? (0.0) : (static_cast<double>(ChannelVolume[Channel])));
            double LeftGain = Volume;
            double RightGain = Volume;
			const double Pan = (((-1.0) > ((((1.0) < (static_cast<double>(ChannelPan[Channel]))) ? (1.0) : (static_cast<double>(ChannelPan[Channel]))))) ? (-1.0) : ((((1.0) < (static_cast<double>(ChannelPan[Channel]))) ? (1.0) : (static_cast<double>(ChannelPan[Channel])))));
            if (Pan > 0.0) {
                LeftGain *= (1.0 - Pan);
            } else if (Pan < 0.0) {
                RightGain *= (1.0 + Pan);
            }

            LeftMix += static_cast<INT64>(static_cast<double>(Sample) * LeftGain);
            RightMix += static_cast<INT64>(static_cast<double>(Sample) * RightGain);
        }

        const UINT32 Dest = Frame * PA2_AUDIO_OUTPUT_CHANNELS;
        OutInterleavedStereo[Dest] = ClampSample(LeftMix);
        OutInterleavedStereo[Dest + 1] = ClampSample(RightMix);
    }

    return FramesRequired;
}

/*
void SampledSound::SaveState(){

	int loop;

	for (loop = 0; loop < 8; loop++){

		LSC->SaveToBuffer(Playing[loop]);
		LSC->SaveToBuffer(Stopped[loop]);	
		LSC->SaveToBuffer(PositionL[loop]);
		LSC->SaveToBuffer(PositionR[loop]);
		LSC->SaveToBuffer(EndOfSample[loop]);
		LSC->SaveToBuffer(Restarted[loop]);
		LSC->SaveToBuffer(NowPlaying[loop]);	
		LSC->SaveToBuffer(Frequency[loop]);
	}

	LSC->SaveToBuffer(BankSwitch);
	LSC->SaveToBuffer(Busy);
	LSC->SaveToBuffer(BusyTimer);
	LSC->SaveToBuffer(Tune);

}

void SampledSound::LoadState(){
	
	int loop;

	for (loop = 0; loop < 8; loop++){

		LSC->LoadFromBuffer(Playing[loop]);
		LSC->LoadFromBuffer(Stopped[loop]);	
		LSC->LoadFromBuffer(PositionL[loop]);
		LSC->LoadFromBuffer(PositionR[loop]);
		LSC->LoadFromBuffer(EndOfSample[loop]);
		LSC->LoadFromBuffer(Restarted[loop]);
		LSC->LoadFromBuffer(NowPlaying[loop]);	
		LSC->LoadFromBuffer(Frequency[loop]);
	}

	LSC->LoadFromBuffer(BankSwitch);
	LSC->LoadFromBuffer(Busy);
	LSC->LoadFromBuffer(BusyTimer);
	LSC->LoadFromBuffer(Tune);

}*/

void SampledSound::SaveState() {}
void SampledSound::LoadState() {}
