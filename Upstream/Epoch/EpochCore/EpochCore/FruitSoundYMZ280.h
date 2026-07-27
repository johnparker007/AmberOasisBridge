// ###########################################################################
// #
// # FruitSoundYMZ280 - Definition of Yamaha YMZ280 emulation class
// # Copyright (C) 2002-2012 Tony Friery [DialTone]
// #
// # ALL RIGHTS RESERVED
// #
// # Based upon code from MAME:
// #
// # Yamaha YMZ280B driver
// #  by Aaron Giles
// #
// #  YMZ280B 8-Channel PCMD8 PCM/ADPCM Decoder
// #
// # Features as listed in LSI-4MZ280B3 data sheet:
// #  Voice data stored in external memory can be played back simultaneously for up to eight voices
// #  Voice data format can be selected from 4-bit ADPCM, 8-bit PCM and 16-bit PCM
// #  Control of voice data external memory
//	# Up to 16M bytes of ROM or SRAM (x 8 bits, access time 150ms max) can be connected
//	# Continuous access is possible
//	# Loop playback between selective addresses is possible
// #  Voice data playback frequency control
//	# 4-bit ADPCM ................ 0.172 to 44.1kHz in 256 steps
//	# 8-bit PCM, 16-bit PCM ...... 0.172 to 88.2kHz in 512 steps
// #  256 steps total level and 16 steps panpot can be set
// #  Voice signal is output in stereo 16-bit 2's complement MSB-first format
// ###########################################################################
//
//  @author 	DialTone
//  @date 		25/04/2011
//  @desc		Initial Version
//  @version	1.01

#ifndef FruitSoundYMZ280H
#define FruitSoundYMZ280H

#include "FruitSound.h"

class FruitSoundYMZ280B : public FruitSampleChip {
	friend class FruitSoundController;

	private:
		// struct describing a single playing ADPCM voice
		struct YMZ280BVoice
		{
			UINT8 	playing;				// 1 if we are actively playing

			UINT8 	keyon;				// 1 if the key is on
			UINT8 	looping;				// 1 if looping is enabled
			UINT8 	mode;					// current playback mode
			UINT16 	fnum;					// frequency
			UINT8 	level;				// output level
			UINT8 	pan;					// panning

			UINT32 	start;				// start address, in nibbles
			UINT32 	stop;					// stop address, in nibbles
			UINT32 	loop_start;			// loop start address, in nibbles
			UINT32 	loop_end;			// loop end address, in nibbles
			UINT32 	position;			// current position, in nibbles

			INT32 	signal;				// current ADPCM signal
			INT32 	step;					// current ADPCM step

			INT32 	loop_signal;		// signal at loop start
			INT32 	loop_step;			// step at loop start
			UINT32 	loop_count;			// number of loops so far

			INT32 	output_left;		// output volume (left)
			INT32 	output_right;		// output volume (right)
			INT32 	output_step;		// step value for frequency conversion
			INT32 	output_pos;			// current fractional position
			INT16 	last_sample;		// last sample output
			INT16 	curr_sample;		// current sample target
			UINT8 	irq_schedule;		// 1 if the IRQ state is updated by timer

			LPDIRECTSOUNDBUFFER buffer;
		};

		UINT8 						fCurrentRegister;					// currently accessible register
		UINT8 						fStatusRegister;					// current status register
		UINT8 						fIrqState;							// current IRQ state
		UINT8 						fIrqMask;							// current IRQ mask
		UINT8 						fIrqEnable;							// current IRQ enable
		UINT8 						fKeyOnEnable;						// key on enable
		double 						fMasterClock;						// master clock frequency
		struct YMZ280BVoice		fVoice[8];							// the 8 voices
		UINT32 						fROMReadbackAddr;					// where the CPU can read the ROM
		unsigned int				fWriteCursor;

		INT16 						*fScratch;
		LPDIRECTSOUNDBUFFER		fOutputBuffer;						// Playback Buffer
		//TMemoryStream				*fCombinedROMs;

		void __fastcall 			ComputeTables(void);

		void __fastcall 			UpdateStream(short *buffer, int length, unsigned int cycles);
		inline int __fastcall 	GenerateADPCM(struct YMZ280BVoice *voice, UINT8 *base, INT16 *buffer, int samples);
		inline int __fastcall 	GeneratePCM8(struct YMZ280BVoice *voice, UINT8 *base, INT16 *buffer, int samples);
		inline int __fastcall 	GeneratePCM16(struct YMZ280BVoice *voice, UINT8 *base, INT16 *buffer, int samples);
		inline void __fastcall 	UpdateVolumes(struct YMZ280BVoice *voice);
		inline void __fastcall 	UpdateStep(struct YMZ280BVoice *voice);
		inline void __fastcall 	UpdateIrqState(void);

	public:
		//bool __fastcall				OpenROMList(TStringList *ROMNames);
		FruitSample * __fastcall 	GetSampleInfo(int Bank, int Sample);
		void * __fastcall 			DecodeSample(int Bank, int Sample);

		UINT8 __fastcall 				ReadRegister(UINT8 reg);
		void __fastcall 				WriteRegister(UINT8 reg, UINT8 data);

		virtual void 					Update(unsigned int cycles);

		__fastcall 						FruitSoundYMZ280B(FruitSoundController *Controller);
		__fastcall 						~FruitSoundYMZ280B(void);
};


//---------------------------------------------------------------------------
#endif
