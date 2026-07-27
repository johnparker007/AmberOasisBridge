// ###########################################################################
// #
// # FruitSound - Definition of base classes used by various Sound Engines
// # Copyright (C) 2002-2010 Tony Friery [DialTone]
// #
// # ALL RIGHTS RESERVED
// #
// ###########################################################################
//
//  @author 	DialTone
//  @date 		07/08/2007
//  @desc		Initial Version
//  @version	1.01

#ifndef FruitSoundH
#define FruitSoundH


#include <MMreg.h>
#include "dsound.h"

///////////////////////////////////////////////////////////////////////
//
//		Types and Identifiers for the Sound Subsystem
//
///////////////////////////////////////////////////////////////////////

typedef struct {
	int 	fSampleRate;		// Sample Rate (Hz)
	int	fSampleSize;		// Sample Length (samples)
	int	fDuration;			// Sample Duration (mSecs)
	int	fByteOffset;		// ROM Offset (bytes)
	int 	fLoopStart;			// Sample Loop Start Point (samples)
	int 	fLoopEnd;			// Sample Loop End Point (samples)
} FruitSample;

///////////////////////////////////////////////////////////////////////
//
//		Forward declarations
//
///////////////////////////////////////////////////////////////////////

class FruitSoundController;

///////////////////////////////////////////////////////////////////////
//
//		Definition of a sample chip
//
///////////////////////////////////////////////////////////////////////

class FruitSampleChip
{
	friend class FruitSoundController;

	protected:
		int								fBankCount;    		// Num Sample Banks
		int								fSampleCount[64];		// Num Samples in each Bank [Max 64 banks]
		FruitSoundController 		*fController;			// Pointer to master sound control interface

	public:
		int __fastcall 				GetBankCount(void);
		int __fastcall 				GetSampleCount(int Bank);
		int __fastcall					GetTotalSampleCount(void);

		//virtual bool __fastcall					OpenROMList(TStringList *ROMNames) 	= 0;
		virtual FruitSample * __fastcall 	GetSampleInfo(int Bank, int Sample)	= 0;
		virtual void * __fastcall 				DecodeSample(int Bank, int Sample) 	= 0;

		__fastcall 						FruitSampleChip(FruitSoundController *Controller);
		virtual __fastcall 			~FruitSampleChip(void);
};

///////////////////////////////////////////////////////////////////////
//
//		Definition of Sound Controller
//
///////////////////////////////////////////////////////////////////////

class FruitSoundController
{
	friend class FruitSampleChip;

	private:
		FruitSampleChip 		 	*fSampleChips[4];

	protected:
		LPDIRECTSOUND          	lpds;
		LPDIRECTSOUNDBUFFER     lpdsbPrimary;
		int 							fMasterSampleRate;

	public:
		bool 							WriteDataToBuffer(
											LPDIRECTSOUNDBUFFER lpDsb,  // the DirectSound buffer
											DWORD dwOffset,             // our own write cursor
											LPBYTE lpbSoundData,        // start of our data
											DWORD dwSoundBytes);        // size of block to copy
		bool							Initialised;

		bool 							CreateBasicBuffer(
											LPDIRECTSOUNDBUFFER *lplpDsb,
											DWORD freq,
											DWORD length);

		int __fastcall				GetSampleRate(void);

		__fastcall 					FruitSoundController(HWND hwnd, int SampleRate);
		__fastcall 					~FruitSoundController(void);
};

#endif

