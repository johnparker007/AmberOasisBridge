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


#include "stdafx.h"
#include "FruitSound.h"
//#include "FormDiag.h"

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	FruitSampleChip::GetBankCount
//
//  PURPOSE:	Retrieve the number of Banks in the ROM
//
//  INPUTS:		none
//
//  OUTPUT:		int					Number of Banks in the ROM
//
///////////////////////////////////////////////////////////////////////
int __fastcall FruitSampleChip::GetBankCount(void)
{
	return fBankCount;
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	FruitSampleChip::GetSampleCount
//
//  PURPOSE:	Retrieve the number of samples in specified bank number
//
//  INPUTS:		int					Bank number (0 based)
//
//  OUTPUT:		int					Number of samples in the bank
//
///////////////////////////////////////////////////////////////////////
int __fastcall FruitSampleChip::GetSampleCount(int Bank)
{
	Bank &= 0x0f;
	return fSampleCount[Bank];
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	FruitSampleChip::GetTotalSampleCount
//
//  PURPOSE:	Retrieve the number of samples across all banks
//
//  INPUTS:		none
//
//  OUTPUT:		int					Number of samples in entire ROM
//
///////////////////////////////////////////////////////////////////////
int __fastcall FruitSampleChip::GetTotalSampleCount(void)
{
	int res = 0;

	if (fBankCount) {
		for (int i = 0; i < fBankCount; i++)
			res += fSampleCount[i];
	}

	return res;
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	FruitSampleChip::FruitSampleChip
//
//  PURPOSE:	Constructor for Sampled Sound Chip
//
//  INPUTS:		FruitSoundController *	Pointer to Sound Controller
//
//  OUTPUT:		none
//
///////////////////////////////////////////////////////////////////////
__fastcall FruitSampleChip::FruitSampleChip(FruitSoundController *Controller)
{
	fController = Controller;
	fBankCount = 0;

	for (int i = 0; i < 64; i++)
	{
		fSampleCount[i] = 0;
	}
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	FruitSampleChip::~FruitSampleChip
//
//  PURPOSE:	Destructor for Sampled Sound Chip
//
//  INPUTS:		none
//
//  OUTPUT:		none
//
///////////////////////////////////////////////////////////////////////
__fastcall FruitSampleChip::~FruitSampleChip(void)
{
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	FruitSoundController::CreateBasicBuffer
//
//  PURPOSE:	Create a direct sound buffer
//
//  INPUTS:		LPDIRECTSOUNDBUFFER *	Pointer to receive buffer
//					DWORD							Sample rate of buffer
//					DWORD							Size of buffer
//
//  OUTPUT:		bool							Success (true = OK, false = failed)
//
///////////////////////////////////////////////////////////////////////
bool FruitSoundController::CreateBasicBuffer(
	LPDIRECTSOUNDBUFFER 	*lplpDsb,
	DWORD 					freq,
	DWORD 					length)
{
	WAVEFORMATEX 	wfx;
	DSBUFFERDESC 	dsbdesc;
	HRESULT 			hr;

	// Set up wave format structure.
	memset(&wfx, 0, sizeof(WAVEFORMATEX));
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 1;
	wfx.nSamplesPerSec = freq;
	wfx.wBitsPerSample = 16;
	wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	// Set up DSBUFFERDESC structure.
	memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);

	// Need default controls (pan, volume, frequency).
	dsbdesc.dwFlags =
		DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GETCURRENTPOSITION2 |
		DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE | DSBCAPS_CTRLPAN |
		DSBCAPS_CTRLVOLUME;

	// 3-second buffer.
	dsbdesc.dwBufferBytes = length;
	dsbdesc.lpwfxFormat = &wfx;

	// Create buffer.
	hr = lpds->CreateSoundBuffer( &dsbdesc, lplpDsb, NULL);
	if SUCCEEDED(hr)
	{
		// Valid interface is in *lplpDsb.
		(*lplpDsb)->SetPan(0);
		(*lplpDsb)->SetVolume(0);
		return true;
	}
	else
	{
		// Failed.
		*lplpDsb = NULL;
		return false;
	}
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	FruitSoundController::WriteDataToBuffer
//
//  PURPOSE:	Write sample data into direct sound buffer
//
//  INPUTS:		LPDIRECTSOUNDBUFFER		Pointer to buffer to be written to
//					DWORD							Position to begin writing
//					LPBYTE						Pointer to sample data to write
//					DWORD							Number of bytes to be written
//
//  OUTPUT:		bool							Success (true = OK, false = failed)
//
///////////////////////////////////////////////////////////////////////
bool FruitSoundController::WriteDataToBuffer(
	LPDIRECTSOUNDBUFFER lpDsb,  // the DirectSound buffer
	DWORD dwOffset,             // our own write cursor
	LPBYTE lpbSoundData,        // start of our data
	DWORD dwSoundBytes)         // size of block to copy
{
	LPVOID  	lpvPtr1;
	DWORD   	dwBytes1;
	LPVOID  	lpvPtr2;
	DWORD   	dwBytes2;
	HRESULT 	hr;
	DWORD  	dwAudio1, dwAudio2;

	// Obtain memory address of write block. This will be in two parts
	// if the block wraps around.
	hr = lpDsb->Lock(dwOffset, dwSoundBytes, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0);

	// If DSERR_BUFFERLOST is returned, restore and retry lock.
	if (DSERR_BUFFERLOST == hr)
	{
		lpDsb->Restore();
		hr = lpDsb->Lock(dwOffset, dwSoundBytes,
			&lpvPtr1, &dwAudio1, &lpvPtr2, &dwAudio2, 0);
	}

	if SUCCEEDED(hr)
	{
		// Write to pointers.
		CopyMemory(lpvPtr1, lpbSoundData, dwBytes1);

		if (NULL != lpvPtr2)
		{
			CopyMemory(lpvPtr2, lpbSoundData+dwBytes1, dwBytes2);
		}

		// Release the data back to DirectSound.
		hr = lpDsb->Unlock(lpvPtr1, dwBytes1, lpvPtr2, dwBytes2);

		if SUCCEEDED(hr)
		{
			// Success.
			return true;
		}
	}

	// Lock, Unlock, or Restore failed.
	return false;
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	FruitSoundController::GetSampleRate
//
//  PURPOSE:	Retrieve the master sample rate
//
//  INPUTS:		none
//
//  OUTPUT:		int					Master Sample Rate (Hz)
//
///////////////////////////////////////////////////////////////////////
int __fastcall FruitSoundController::GetSampleRate(void)
{
	return fMasterSampleRate;
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	FruitSoundController::FruitSoundController
//
//  PURPOSE:	Constructor for the Sound Controller
//
//  INPUTS:		HWND              Window Handle of main GUI window
//					int					Desired master sample rate
//
//  OUTPUT:		none
//
///////////////////////////////////////////////////////////////////////
__fastcall FruitSoundController::FruitSoundController(HWND hwnd, int SampleRate)
{
	DSBUFFERDESC 	dsbdesc;
	WAVEFORMATEX  	wfx;
	unsigned long 	ulSampleRate;

	Initialised = true;

	// Create DirectSound
	//DiagForm->AddMessage(DIAG_AUDIO, "Initialising DirectSound");
	if FAILED(DirectSoundCreate(NULL, &lpds, NULL)) {
		//DiagForm->AddMessage(DIAG_AUDIO, "Failed to Initialise DirectSound");
		return; //FALSE;
	}// else
		//DiagForm->AddMessage(DIAG_AUDIO, "Success Initialising DirectSound");

	// Set co-op level
	//DiagForm->AddMessage(DIAG_AUDIO, "Setting DirectSound Co-Operation Level");
	if FAILED(IDirectSound_SetCooperativeLevel(lpds, hwnd, DSSCL_PRIORITY)) {
		//DiagForm->AddMessage(DIAG_AUDIO, "Failed to set Co-Op Level");
		return; // FALSE;
	} else
		//DiagForm->AddMessage(DIAG_AUDIO, "Success setting Co-Op Level");

	// Obtain primary buffer
	ZeroMemory(&dsbdesc, sizeof(DSBUFFERDESC));
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME;

	//DiagForm->AddMessage(DIAG_AUDIO, "Creating Primary Buffer");
	if FAILED(lpds->CreateSoundBuffer(&dsbdesc, &lpdsbPrimary, NULL)) {
		//DiagForm->AddMessage(DIAG_AUDIO, "Failed to Create Primary Buffer");
		return; // FALSE;
	} else
		//DiagForm->AddMessage(DIAG_AUDIO, "Successfully Created Primary Buffer");

	// Set primary buffer format
	fMasterSampleRate = SampleRate;
	memset(&wfx, 0, sizeof(WAVEFORMATEX));
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 1;
	wfx.nSamplesPerSec = (unsigned long)fMasterSampleRate;
	wfx.wBitsPerSample = 16;
	wfx.nBlockAlign = (wfx.wBitsPerSample * wfx.nChannels ) / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	//DiagForm->AddMessage(DIAG_AUDIO, "Requesting " + AnsiString(fMasterSampleRate) + "Hz Sample Rate");
	if (lpdsbPrimary->SetFormat(&wfx) != DS_OK) {
		//DiagForm->AddMessage(DIAG_AUDIO, "Failed to set Primary Buffer Parameters");
		return;
	} else
		//DiagForm->AddMessage(DIAG_AUDIO, "Sample Rate was Set");

	Initialised = true;
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	FruitSoundController::~FruitSoundController
//
//  PURPOSE:	Destructor for the Sound Controller
//
//  INPUTS:		none
//
//  OUTPUT:		none
//
///////////////////////////////////////////////////////////////////////
__fastcall FruitSoundController::~FruitSoundController(void)
{
	if (lpds)
	{
		lpds->Release();
	}

	//DiagForm->AddMessage(DIAG_AUDIO, "Shutting Down");
}

//---------------------------------------------------------------------------

//#pragma package(smart_init)
//
