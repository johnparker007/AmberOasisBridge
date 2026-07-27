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


#include "stdafx.h"
#include "FruitSoundYMZ280.h"

///////////////////////////////////////////////////////////////////////
//
//		Constants and Defines
//
///////////////////////////////////////////////////////////////////////

// Step Value Fractional Bits
#define FRAC_BITS					14
#define FRAC_ONE					(1 << FRAC_BITS)
#define FRAC_MASK					(FRAC_ONE - 1)

#define INTERNAL_BUFFER_SIZE	(1 << 14) /* Was 15 */
#define INTERNAL_SAMPLE_RATE	64000

#define MAX_SAMPLE_CHUNK		10000

///////////////////////////////////////////////////////////////////////
//
//		ADPCM Decode Lookup Tables
//
///////////////////////////////////////////////////////////////////////

// step size index shift table
static const int index_scale[8] =
{
	0x0e6, 0x0e6, 0x0e6, 0x0e6, 0x133, 0x199, 0x200, 0x266
};

// lookup table for the precomputed difference
static int diff_lookup[16];

#if 0
static void update_irq_state_timer_common(int voicenum)
{
	ymz280b_state *chip = (ymz280b_state *)param;
	struct YMZ280BVoice *voice = &chip->voice[voicenum];

	if(!voice->irq_schedule) return;

	voice->playing = 0;
	chip->status_register |= 1 << voicenum;
	update_irq_state(chip);
	voice->irq_schedule = 0;
}
#endif

/**********************************************************************************************

	  ymz280b_update -- update the sound chip so that it is in sync with CPU execution

***********************************************************************************************/

void __fastcall FruitSoundYMZ280B::UpdateStream(short *buffer, int samples, unsigned int cycles)
{
	INT16 *lacc = buffer;
//	INT16 *racc = buffer + 1;

	// clear out the accumulator
	memset(lacc, 0, samples * sizeof(lacc[0]));
//	memset(racc, 0, samples * sizeof(racc[0]));

	// loop over voices
	for (int v = 0; v < 8; v++)
	{
		struct YMZ280BVoice *voice = &fVoice[v];
		INT16 prev = voice->last_sample;
		INT16 curr = voice->curr_sample;
		INT16 *curr_data = fScratch;
		INT16 *ldest = lacc;
//		INT16 *rdest = racc;
		UINT32 new_samples, samples_left;
		UINT32 final_pos;
		int remaining = samples;
		int lvol = 64;//voice->output_left;
		int rvol = 64;//voice->output_right;

		// quick out if we're not playing and we're at 0
		if (!voice->playing && curr == 0)
		{
			continue;
		}

		// finish off the current sample
//      if (voice->output_pos > 0)
		{
			// interpolate
			while (remaining > 0 && voice->output_pos < FRAC_ONE)
			{
				int interp_sample = (((INT32)prev * (FRAC_ONE - voice->output_pos)) + ((INT32)curr * voice->output_pos)) >> FRAC_BITS;
				*ldest++ += (interp_sample * lvol) / 256;
//				*rdest++ += interp_sample * rvol;
//				ldest++;
//				rdest++;
				voice->output_pos += voice->output_step;
				remaining--;
			}

			// if we're over, continue; otherwise, we're done
			if (voice->output_pos >= FRAC_ONE)
				voice->output_pos -= FRAC_ONE;
			else
				continue;
		}

		// compute how many new samples we need
		final_pos = voice->output_pos + remaining * voice->output_step;
		new_samples = (final_pos + FRAC_ONE) >> FRAC_BITS;
		if (new_samples > MAX_SAMPLE_CHUNK)
			new_samples = MAX_SAMPLE_CHUNK;
		samples_left = new_samples;

		// generate them into our buffer
		if (voice->playing)
		{
			switch (voice->mode)
			{
				case 1:
					samples_left = GenerateADPCM(voice, (UINT8 *)fCombinedROMs->Memory, fScratch, new_samples);
					break;
				case 2:
					samples_left = GeneratePCM8(voice, (UINT8 *)fCombinedROMs->Memory, fScratch, new_samples);
					break;
				case 3:
					samples_left = GeneratePCM16(voice, (UINT8 *)fCombinedROMs->Memory, fScratch, new_samples);
					break;
				default:
				case 0:
					samples_left = 0;
					memset(fScratch, 0, new_samples * sizeof(fScratch[0]));
					break;
			}
		}

		// if there are leftovers, ramp back to 0
		if (samples_left)
		{
			int base = new_samples - samples_left;
			int t = (base == 0) ? curr : fScratch[base - 1];
			for (unsigned int i = 0; i < samples_left; i++)
			{
				if (t < 0) t = -((-t * 15) >> 4);
				else if (t > 0) t = (t * 15) >> 4;
				fScratch[base + i] = t;
			}

			// if we hit the end and IRQs are enabled, signal it
			if (base != 0)
			{
				voice->playing = 0;

				// set update_irq_state_timer. IRQ is signaled on next CPU execution.
				//timer_set(chip->device->machine, attotime_zero, chip, 0, update_irq_state_cb[v]);
				voice->irq_schedule = 1;

				fStatusRegister |= 1 << v;
				UpdateIrqState();
				voice->irq_schedule = 0;
			}
		}

		// advance forward one sample
		prev = curr;
		curr = *curr_data++;

		// then sample-rate convert with linear interpolation
		while (remaining > 0)
		{
			// interpolate
			while (remaining > 0 && voice->output_pos < FRAC_ONE)
			{
				int interp_sample = (((INT32)prev * (FRAC_ONE - voice->output_pos)) + ((INT32)curr * voice->output_pos)) >> FRAC_BITS;
				*ldest++ += (interp_sample * lvol) / 256;
//				*rdest++ += interp_sample * rvol;
//				ldest++;
//				rdest++;
				voice->output_pos += voice->output_step;
				remaining--;
			}

			// if we're over, grab the next samples
			if (voice->output_pos >= FRAC_ONE)
			{
				voice->output_pos -= FRAC_ONE;
				prev = curr;
				curr = *curr_data++;
			}
		}

		// remember the last samples
		voice->last_sample = prev;
		voice->curr_sample = curr;
	}
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	FruitSoundYMZ280B::GenerateADPCM
//
//  PURPOSE:	Generate playable sample data from 4-bit encoded ADPCM samples
//
//  INPUTS:		YMZ280BVoice *		Pointer to Yamaha Voice structure
//					UINT8 *				Pointer to base of sample ROM memory
//					INT16 *				Pointer to buffer to receive decoded samples.
//											Note: This must be already allocated and of
//											adequate size to receive the requested samples
//					int					Number of samples to be produced
//
//  OUTPUT:		int					Number of samples not produced (because voice
//											stop address was reached, for example)
//
///////////////////////////////////////////////////////////////////////
inline int __fastcall FruitSoundYMZ280B::GenerateADPCM(struct YMZ280BVoice *voice, UINT8 *base, INT16 *buffer, int samples)
{
	UINT32 position = voice->position;
	int signal = voice->signal;
	int step = voice->step;
	int val;

	if (!voice->looping)
	{
		// two cases: first cases is non-looping
		// loop while we still have samples to generate
		while (samples)
		{
			// compute the new amplitude and update the current step
			val = base[position / 2] >> ((~position & 1) << 2);
			signal += (step * diff_lookup[val & 15]) / 8;

			// clamp to the maximum
			if (signal > 32767)
			{
				signal = 32767;
			}
			else if (signal < -32768)
			{
				signal = -32768;
			}

			// adjust the step size and clamp
			step = (step * index_scale[val & 7]) >> 8;
			if (step > 0x6000)
			{
				step = 0x6000;
			}
			else if (step < 0x7f)
			{
				step = 0x7f;
			}

			// output to the buffer, scaling by the volume
			*buffer++ = signal;
			samples--;

			// next!
			position++;
			if (position >= voice->stop)
			{
				break;
			}
		}
	}
	else
	{
		// second case: looping
		// loop while we still have samples to generate
		while (samples)
		{
			// compute the new amplitude and update the current step
			val = base[position / 2] >> ((~position & 1) << 2);
			signal += (step * diff_lookup[val & 15]) / 8;

			// clamp to the maximum
			if (signal > 32767)
			{
				signal = 32767;
			}
			else if (signal < -32768)
			{
				signal = -32768;
			}

			// adjust the step size and clamp
			step = (step * index_scale[val & 7]) >> 8;

			if (step > 0x6000)
			{
				step = 0x6000;
			}
			else if (step < 0x7f)
			{
				step = 0x7f;
			}

			// output to the buffer, scaling by the volume
			*buffer++ = signal;
			samples--;

			// next!
			position++;
			if (position == voice->loop_start && voice->loop_count == 0)
			{
				voice->loop_signal = signal;
				voice->loop_step = step;
			}

			if (position >= voice->loop_end)
			{
				if (voice->keyon)
				{
					position = voice->loop_start;
					signal = voice->loop_signal;
					step = voice->loop_step;
					voice->loop_count++;
				}
			}

			if (position >= voice->stop)
			{
				break;
			}
		}
	}

	// update the parameters
	voice->position = position;
	voice->signal = signal;
	voice->step = step;

	return samples;
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	FruitSoundYMZ280B::GeneratePCM8
//
//  PURPOSE:	Generate playable sample data from 8-bit encoded PCM samples
//
//  INPUTS:		YMZ280BVoice *		Pointer to Yamaha Voice structure
//					UINT8 *				Pointer to base of sample ROM memory
//					INT16 *				Pointer to buffer to receive decoded samples.
//											Note: This must be already allocated and of
//											adequate size to receive the requested samples
//					int					Number of samples to be produced
//
//  OUTPUT:		int					Number of samples not produced (because voice
//											stop address was reached, for example)
//
///////////////////////////////////////////////////////////////////////
inline int __fastcall FruitSoundYMZ280B::GeneratePCM8(struct YMZ280BVoice *voice, UINT8 *base, INT16 *buffer, int samples)
{
	UINT32 position = voice->position;
	int val;

	if (!voice->looping)
	{
		// two cases: first cases is non-looping
		// loop while we still have samples to generate
		while (samples)
		{
			// fetch the current value
			val = base[position / 2];

			// output to the buffer, scaling by the volume
			*buffer++ = (INT8)val * 256;
			samples--;

			// next!
			position += 2;

			if (position >= voice->stop)
			{
				break;
			}
		}
	}
	else
	{
		// second case: looping
		// loop while we still have samples to generate
		while (samples)
		{
			// fetch the current value
			val = base[position / 2];

			// output to the buffer, scaling by the volume
			*buffer++ = (INT8)val * 256;
			samples--;

			// next!
			position += 2;

			if (position >= voice->loop_end)
			{
				if (voice->keyon)
				{
					position = voice->loop_start;
				}
			}

			if (position >= voice->stop)
			{
				break;
			}
		}
	}

	// update the parameters
	voice->position = position;

	return samples;
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	FruitSoundYMZ280B::GeneratePCM16
//
//  PURPOSE:	Generate playable sample data from 16-bit encoded PCM samples
//
//  INPUTS:		YMZ280BVoice *		Pointer to Yamaha Voice structure
//					UINT8 *				Pointer to base of sample ROM memory
//					INT16 *				Pointer to buffer to receive decoded samples.
//											Note: This must be already allocated and of
//											adequate size to receive the requested samples
//					int					Number of samples to be produced
//
//  OUTPUT:		int					Number of samples not produced (because voice
//											stop address was reached, for example)
//
///////////////////////////////////////////////////////////////////////
inline int __fastcall FruitSoundYMZ280B::GeneratePCM16(struct YMZ280BVoice *voice, UINT8 *base, INT16 *buffer, int samples)
{
	UINT32 position = voice->position;
	int val;

	if (!voice->looping)
	{
		// two cases: first cases is non-looping
		// loop while we still have samples to generate
		while (samples)
		{
			// fetch the current value
			val = (INT16)((base[position / 2 + 1] << 8) + base[position / 2]);

			// output to the buffer, scaling by the volume
			*buffer++ = val;
			samples--;

			// next!
			position += 4;

			if (position >= voice->stop)
			{
				break;
			}
		}
	}
	else
	{
		// second case: looping
		// loop while we still have samples to generate
		while (samples)
		{
			// fetch the current value
			val = (INT16)((base[position / 2 + 1] << 8) + base[position / 2]);

			// output to the buffer, scaling by the volume
			*buffer++ = val;
			samples--;

			// next!
			position += 4;

			if (position >= voice->loop_end)
			{
				if (voice->keyon)
				{
					position = voice->loop_start;
				}
			}

			if (position >= voice->stop)
			{
				break;
			}
		}
	}

	// update the parameters
	voice->position = position;

	return samples;
}

inline void __fastcall FruitSoundYMZ280B::UpdateIrqState(void)
{
	int irq_bits = fStatusRegister & fIrqMask;

	// always off if the enable is off
	if (!fIrqEnable)
	{
		irq_bits = 0;
	}

	// update the state if changed
	if (irq_bits && !fIrqState)
	{
		fIrqState = 1;

//		if (chip->irq_callback)
//			(*chip->irq_callback)(chip->device, 1);
//		else logerror("YMZ280B: IRQ generated, but no callback specified!");
	}
	else if (!irq_bits && fIrqState)
	{
		fIrqState = 0;

//		if (chip->irq_callback)
//			(*chip->irq_callback)(chip->device, 0);
//		else logerror("YMZ280B: IRQ generated, but no callback specified!");
	}
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	FruitSoundYMZ280B::UpdateVolumes
//
//  PURPOSE:	Update the left/right output levels for a voice
//					based on the voice's volume and panning registers
//
//  INPUTS:		YMZ280BVoice *		Pointer to Yamaha Voice structure
//
//  OUTPUT:		none
//
///////////////////////////////////////////////////////////////////////
inline void __fastcall FruitSoundYMZ280B::UpdateVolumes(struct YMZ280BVoice *voice)
{
	if (voice->pan == 8)
	{
		voice->output_left = voice->level;
		voice->output_right = voice->level;
	}
	else if (voice->pan < 8)
	{
		voice->output_left = voice->level;
		voice->output_right = voice->level * voice->pan / 8;
	}
	else
	{
		voice->output_left = voice->level * (15 - voice->pan) / 8;
		voice->output_right = voice->level;
	}
}

inline void __fastcall FruitSoundYMZ280B::UpdateStep(struct YMZ280BVoice *voice)
{
	double frequency;

	// compute the frequency
	if (voice->mode == 1)
	{
		frequency = fMasterClock * (double)((voice->fnum & 0x0ff) + 1) * (1.0 / 256.0);
	}
	else
	{
		frequency = fMasterClock * (double)((voice->fnum & 0x1ff) + 1) * (1.0 / 256.0);
	}

	voice->output_step = (UINT32)(frequency * (double)FRAC_ONE / INTERNAL_SAMPLE_RATE);
	//DiagForm->AddMessage(DIAG_AUDIO, Format(
		//"Voice Freq set to: %f, step %8.8x",
		//OPENARRAY(TVarRec, (frequency, voice->output_step))
	));
}


/**********************************************************************************************

	  compute_tables -- compute the difference tables

***********************************************************************************************/

void __fastcall FruitSoundYMZ280B::ComputeTables(void)
{
	// loop over all nibbles and compute the difference
	for (int nib = 0; nib < 16; nib++)
	{
		int value = (nib & 0x07) * 2 + 1;
		diff_lookup[nib] = (nib & 0x08) ? -value : value;
	}
}

UINT8 __fastcall FruitSoundYMZ280B::ReadRegister(UINT8 reg)
{
	UINT8 result;

	if ((reg & 1) == 0)
	{
		result = 0x00;//devcb_call_read8(&chip->ext_ram_read, chip->rom_readback_addr++ - 1);
	}
	else
	{
		// ROM/RAM readback?
		if (fCurrentRegister == 0x86)
		{
			result = ((UINT8 *)fCombinedROMs->Memory)[fROMReadbackAddr++];
		}
		else
		{
			// force an update
			Update(0);

			result = (fCombinedROMs->Size > 0) ? fStatusRegister : 0xff;

			// clear the IRQ state
			fStatusRegister = 0;
			UpdateIrqState();
		}
	}

	return result;
}

void __fastcall FruitSoundYMZ280B::WriteRegister(UINT8 reg, UINT8 data)
{
	if ((reg & 1) == 0)
	{
		fCurrentRegister = data;
	}
	else
	{
		struct YMZ280BVoice *voice;

		// force an update
		Update(0);

		// lower registers follow a pattern
		if (fCurrentRegister < 0x80)
		{
			int voiceNum = (fCurrentRegister >> 2) & 7;
			voice = &fVoice[voiceNum];

			switch (fCurrentRegister & 0xe3)
			{
				case 0x00:		// pitch low 8 bits
					voice->fnum = (voice->fnum & 0x100) | (data & 0xff);
					UpdateStep(voice);
					//DiagForm->AddMessage(DIAG_AUDIO, Format("Voice %d Pitch set to: 0x%4.4x", OPENARRAY(TVarRec, (voiceNum, voice->fnum))));
					break;
				case 0x01:		// pitch upper 1 bit, loop, key on, mode
				{
					voice->fnum = (voice->fnum & 0xff) | ((data & 0x01) << 8);
					//DiagForm->AddMessage(DIAG_AUDIO, Format(
						//"Voice %d Pitch set to: 0x%4.4x",
						//OPENARRAY(TVarRec, (voiceNum, voice->fnum))
					));

					voice->looping = (data & 0x10) >> 4;
					voice->mode = (data & 0x60) >> 5;

					if (!voice->keyon && (data & 0x80) && fKeyOnEnable)
					{
						//DiagForm->AddMessage(DIAG_AUDIO, Format(
							//"Voice %d Playing (%s)",
							//OPENARRAY(TVarRec, (voiceNum, voice->looping ? "looped" : "not looped"))
						//));
						voice->playing = 1;
						voice->position = voice->start;
						voice->signal = voice->loop_signal = 0;
						voice->step = voice->loop_step = 0x7f;
						voice->loop_count = 0;

						// if update_irq_state_timer is set, cancel it.
						voice->irq_schedule = 0;
					}

					if (voice->keyon && !(data & 0x80) && !voice->looping)
					{
						//DiagForm->AddMessage(DIAG_AUDIO, Format(
							//"Voice %d Stop",
							//OPENARRAY(TVarRec, (voiceNum))
						//));
						voice->playing = 0;

						// if update_irq_state_timer is set, cancel it.
						voice->irq_schedule = 0;
					}

					voice->keyon = (data & 0x80) >> 7;
					UpdateStep(voice);
				}
					break;
				case 0x02:		// total level
					voice->level = data;
					UpdateVolumes(voice);
					//DiagForm->AddMessage(DIAG_AUDIO, Format(
					//	"Voice %d Volume set to: 0x%2.2x",
					//	OPENARRAY(TVarRec, (voiceNum, voice->level))
					//));
					break;
				case 0x03:		// pan
					voice->pan = data & 0x0f;
					UpdateVolumes(voice);

					break;
				case 0x20:		// start address high
					voice->start = (voice->start & (0x00ffff << 1)) | (data << 17);

					break;
				case 0x21:		// loop start address high
					voice->loop_start = (voice->loop_start & (0x00ffff << 1)) | (data << 17);

					break;
				case 0x22:		// loop end address high
					voice->loop_end = (voice->loop_end & (0x00ffff << 1)) | (data << 17);

					break;
				case 0x23:		// stop address high
					voice->stop = (voice->stop & (0x00ffff << 1)) | (data << 17);

					break;
				case 0x40:		// start address middle
					voice->start = (voice->start & (0xff00ff << 1)) | (data << 9);

					break;
				case 0x41:		// loop start address middle
					voice->loop_start = (voice->loop_start & (0xff00ff << 1)) | (data << 9);

					break;
				case 0x42:		// loop end address middle
					voice->loop_end = (voice->loop_end & (0xff00ff << 1)) | (data << 9);

					break;
				case 0x43:		// stop address middle
					voice->stop = (voice->stop & (0xff00ff << 1)) | (data << 9);

					break;
				case 0x60:		// start address low
					voice->start = (voice->start & (0xffff00 << 1)) | (data << 1);

					break;
				case 0x61:		// loop start address low
					voice->loop_start = (voice->loop_start & (0xffff00 << 1)) | (data << 1);

					break;
				case 0x62:		// loop end address low
					voice->loop_end = (voice->loop_end & (0xffff00 << 1)) | (data << 1);

					break;
				case 0x63:		// stop address low
					voice->stop = (voice->stop & (0xffff00 << 1)) | (data << 1);

					break;
				default:

					break;
			}
		}
		else
		{
			// upper registers are special
			switch (fCurrentRegister)
			{
				case 0x84:		// ROM readback / RAM write (high)
					fROMReadbackAddr &= 0xffff;
					fROMReadbackAddr |= (data << 16);

					break;
				case 0x85:		// ROM readback / RAM write (med)
					fROMReadbackAddr &= 0xff00ff;
					fROMReadbackAddr |= (data << 8);

					break;
				case 0x86:		// ROM readback / RAM write (low)
					fROMReadbackAddr &= 0xffff00;
					fROMReadbackAddr |= data;

					break;
				case 0x87:		// RAM write
					
					break;
				case 0xfe:		// IRQ mask
					fIrqMask = data;
					UpdateIrqState();
					
					break;

				case 0xff:		/* IRQ enable, test, etc */
					fIrqEnable = (data & 0x10) >> 4;
					UpdateIrqState();

					if (fKeyOnEnable && !(data & 0x80))
					{
						for (int i = 0; i < 8; i++)
						{
							fVoice[i].playing = 0;

							// if update_irq_state_timer is set, cancel it.
							fVoice[i].irq_schedule = 0;
						}
					}
					else if (!fKeyOnEnable && (data & 0x80))
					{
						for (int i = 0; i < 8; i++)
						{
							if (fVoice[i].keyon && fVoice[i].looping)
							{
								fVoice[i].playing = 1;
							}
						}
					}
					fKeyOnEnable = (data & 0x80) >> 7;

					
					break;
				default:
					
					break;
			}
		}
	}
}

void FruitSoundYMZ280B::Update(unsigned int cycles)
{
	DWORD	playPos, unusedWriteCursor;
	DWORD	writeLen;
	LPVOID p1, p2;
	DWORD	l1, l2;
	HRESULT hRes;

	hRes = fOutputBuffer->GetCurrentPosition(&playPos, &unusedWriteCursor);

	if (hRes != DS_OK)
	{
		playPos = 0;
	}

	if (fWriteCursor < playPos)
	{
		writeLen = playPos - fWriteCursor;
	}
	else
	{
		writeLen = INTERNAL_BUFFER_SIZE - (fWriteCursor - playPos);
	}

	while (DS_OK != fOutputBuffer->Lock(fWriteCursor, writeLen, &p1, &l1, &p2, &l2, 0))
	{
		fOutputBuffer->Restore();
		fOutputBuffer->Play(0, 0, DSBPLAY_LOOPING);
	}

	if ((p1) && (l1 > 0))
	{
		UpdateStream((short *)p1, l1 / 2, cycles);
	}

	if ((p2) && (l2 > 0))
	{
		UpdateStream((short *)p2, l2 / 2, cycles);
	}

	fOutputBuffer->Unlock(p1, l1, p2, l2);
	fWriteCursor += writeLen;

	if (fWriteCursor >= INTERNAL_BUFFER_SIZE)
	{
		fWriteCursor -= INTERNAL_BUFFER_SIZE;
	}
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	FruitSoundYMZ280B::OpenROMList
//
//  PURPOSE:	Load one or more Sound ROM files
//
//  INPUTS:		TStringList *		Pointer to list of filenames to open.
//											Note: the list should already by sorted
//											into the correct order (usually in
//											ascending alphabetical order)
//
//  OUTPUT:		bool					Success (true = OK)
//
///////////////////////////////////////////////////////////////////////
bool __fastcall FruitSoundYMZ280B::OpenROMList(TStringList *ROMNames)
{
	if (ROMNames->Count)
	{
		for (int i = 0; i < ROMNames->Count; i++)
		{
			DiagForm->AddMessage(DIAG_AUDIO, "Loading Sample ROM: " + ROMNames->Strings[i]);
			TMemoryStream *tempStream = new TMemoryStream();
			try
			{
				tempStream->LoadFromFile(ROMNames->Strings[i]);
			}
			catch(...)
			{
				delete tempStream;
				return false;
			}

			fCombinedROMs->WriteBuffer(tempStream->Memory, tempStream->Size);
			delete tempStream;
		}

		DiagForm->AddMessage(DIAG_AUDIO, Format(
			"Total Sound Length: %8.8x",
			OPENARRAY(TVarRec, (fCombinedROMs->Size))
		));
	}

	return true;
}

FruitSample * __fastcall FruitSoundYMZ280B::GetSampleInfo(int Bank, int Sample)
{
	return NULL;
}

void * __fastcall FruitSoundYMZ280B::DecodeSample(int Bank, int Sample) {
	return NULL;
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	FruitSoundYMZ280B::FruitSoundYMZ280B
//
//  PURPOSE:	Constructor for the Yamaha YMZ280B Emulation
//
//  INPUTS:		FruitSoundController *		Pointer to Sound Controller
//
//  OUTPUT:		none
//
///////////////////////////////////////////////////////////////////////
__fastcall FruitSoundYMZ280B::FruitSoundYMZ280B(FruitSoundController *Controller) :
	FruitSampleChip(Controller)
{

	fCombinedROMs = new TMemoryStream();

	// Compute ADPCM tables
	ComputeTables();

	fMasterClock = (double)16934400 / 384.0;

	// Allocate memory for building sample data
	fScratch = new INT16[MAX_SAMPLE_CHUNK];

	if (!fController->CreateBasicBuffer(&fOutputBuffer, INTERNAL_SAMPLE_RATE, INTERNAL_BUFFER_SIZE))
	{
		DiagForm->AddMessage(DIAG_AUDIO, "Failed to create buffer for Yamaha");
	}
	else
	{
		fWriteCursor = 0;
		fOutputBuffer->Play(0, 0, DSBPLAY_LOOPING);
	}

	for (int i = 0; i < 8; i++)
	{
		memset(&fVoice[i], 0, sizeof(YMZ280BVoice));
	}
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	FruitSoundYMZ280B::~FruitSoundYMZ280B
//
//  PURPOSE:	Destructor for the Yamaha YMZ280B Emulation
//
//  INPUTS:		none
//
//  OUTPUT:		none
//
///////////////////////////////////////////////////////////////////////
__fastcall FruitSoundYMZ280B::~FruitSoundYMZ280B(void)
{
	

	if (fCombinedROMs)
	{
		delete fCombinedROMs;
		fCombinedROMs = NULL;
	}

	if (fScratch)
	{
		delete[] fScratch;
		fScratch = NULL;
	}
}

//---------------------------------------------------------------------------

#pragma package(smart_init)

