// ###########################################################################
// #
// # DeviceEpochRTC - Definition of Epoch Pic-Based RTC
// # Copyright (C) 2002-2012 Tony Friery [DialTone]
// #
// # ALL RIGHTS RESERVED
// #
// ###########################################################################

#include "stdafx.h"
#include "DeviceEpochRTC.h"
#include <time.h>

///////////////////////////////////////////////////////////////////////
//
//		Implementation of RTC Class
//
///////////////////////////////////////////////////////////////////////

void __fastcall DeviceEpochRTC::ProcessCommand(void)
{
	UINT8 cmd = fInputChar;

	int setClock = 1;
	int setSec1 = 1;
	int setSec2 = 1;
	int readSec1 = 1;
	int readSec2 = 1;

	switch (cmd)
	{
	case 0xfd:	// Read Clock Command
		/*
		fReplyBuffer[0] = 0x00;	// Seconds
		fReplyBuffer[1] = 0x00; // Minutes
		fReplyBuffer[2] = 0x00; // Hours
		fReplyBuffer[3] = 0x01; // Day
		fReplyBuffer[4] = 0x01; // MSN: DOW, LSN: Month
		fReplyBuffer[5] = 0x00; // Year
		*/

		BuildReadClockReply();
		fReplySize = 6;
		fReplyOffset = -1;
		fOutBitCount = 0;
		fSendReceive = true;	// Now sending back to CPU
		break;
	case 0xfe: 	// Set Clock Command
		setClock = 1;
	case 0xfb:	// Set Security Output 2 Params
		setSec1 = 1;
	case 0xfc:	// Set Security Output 1 Params
		setSec2 = 1;
	case 0xfa:	// Read Security Output 1
		readSec1 = 1;
	case 0xf9:	// Read Security Output 2
		readSec2 = 1;
	default:	// Unknown Command
		break;
	}
}

void __fastcall DeviceEpochRTC::BuildReadClockReply(void)
{
	time_t now = time(0);	
	
	now = (time_t)((long long)now);	

	tm ltm;
	localtime_s(&ltm, &now);

	// SDK rtc01a.h struct RTC fields are binary values:
	// seconds 0-59, minutes 0-59, hours 0-23, day 1-31,
	// month 1-12, year 0-99, dow 0-6 with Sunday=0.
	fReplyBuffer[0] = (UINT8)ltm.tm_sec;
	fReplyBuffer[1] = (UINT8)ltm.tm_min;
	fReplyBuffer[2] = (UINT8)ltm.tm_hour;
	fReplyBuffer[3] = (UINT8)ltm.tm_mday;
	fReplyBuffer[4] = ((UINT8)(ltm.tm_wday - 1) << 4) | (UINT8)(ltm.tm_mon + 1);
	fReplyBuffer[5] = (UINT8)(ltm.tm_year % 100);	
}

void __fastcall DeviceEpochRTC::Write(bool CPUClock, bool CPUData)
{
	// If the data line from the H8 CPU has changed, store
	// the new value
	if (fLastCPUData != CPUData)
	{
		fLastCPUData = CPUData;
	}

	// H8 CPU controls the clocking in/out of data to/from the PIC
	// Check if the clock state has changed
	if (CPUClock != fLastCPUClock)
	{
		// Store the new clock state
		fLastCPUClock = CPUClock;

		// Check the current (expected) comms direction
		if (!fSendReceive)
		{
			// RTC is currently receiving from CPU
			if (CPUClock)
			{
				// CPU signalling it is ready to send next bit
				// (on rising clock edge) so acknowledge by
				// raising the RTC Clock line back to the CPU
				fRTCClock = true;
			}
			else
			{
				// CPU Signalling that Data Line is correctly asserted,
				// so read the data bit
				fInputChar <<= 1;
				fInputChar &= 0xfe;

				if (CPUData)
				{
					fInputChar |= 0x01;
				}

				fInBitCount++;
				if (fInBitCount == 8)
				{
					// We have a valid command or data byte
					fInBitCount = 0;
					ProcessCommand();
				}

				// Finally, acknowledge the read by dropping
				// the RTC Clock line back to the CPU
				fRTCClock = false;
			}
		}
		else
		{
			// RTC is currently sending to CPU
			if (CPUClock)
			{
				// CPU has acknowledged Ready by raising
				// its clock line - Set next bit
				if (fOutBitCount == 0)
				{
					// Obtain next character
					fOutputChar = fReplyBuffer[fReplyOffset];
					fReplyOffset++;
				}

				fRTCData = (fOutputChar & 0x80) ? true : false;

				// and indicate we are ready
				fRTCClock = false;
			}
			else
			{
				// CPU has confirmed receipt of bit
				fOutBitCount++;
				fOutputChar <<= 1;

				if (fOutBitCount == 8)
				{
					// All bits delivered
					fOutBitCount = 0;

					// Check if any more bytes remain
					if (fReplyOffset < fReplySize)
					{
						// Yes, there's more, so raise the clock
						// to let the CPU know
						fRTCClock = true;
					}
					else
					{
						// Return to receive mode
						fSendReceive = false;
					}
				}
				else
				{
					// Indicate that we're ready for the next bit
					fRTCClock = true;
				}
			}
		}
	}
}

UINT8 __fastcall DeviceEpochRTC::GetPort8(void)
{
	if (fGateID < 3)
	{
		return (fRTCData ? 0x10 : 0x00);
	}

	return (fRTCData ? 0x04 : 0x00);
}

UINT8 __fastcall DeviceEpochRTC::GetPortA(void)
{
	UINT8 rv = 0x00;

	// Set correct lines in response
	rv |= fRTCClock ? 0x08 : 0x00;
	rv |= fLastCPUClock ? 0x02 : 0x00;
	rv |= fLastCPUData ? 0x04 : 0x00;

	if ((fReplyOffset == -1) && (fSendReceive == true))
	{
		// Special case: Make clock high as we're about to enter
		// reply mode
		fRTCClock = true;
		fReplyOffset = 0;
	}

	return rv;
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	DeviceEpochRTC::Reset
//
//  PURPOSE:	(POR) Reset the emulated RTC Hardware
//
//  INPUTS:		void
//
//  OUTPUT:		void
//
///////////////////////////////////////////////////////////////////////
void __fastcall DeviceEpochRTC::Reset()
{
	// Set initial state lines
	fRTCClock = false;
	fRTCData = false;
	fLastCPUClock = false;
	fLastCPUData = false;
	fSendReceive = false;

	fOutBitCount = 0;
	fInBitCount = 0;	
	fReplySize = 0;
	fReplyOffset = 0;
	fOutputChar = 0;
	fInputChar = 0;
	fReplySize = 0;

	for (int i = 0; i < 128; i++)
	{
		fReplyBuffer[i] = 0x00;
	}
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	DeviceEpochRTC::DeviceEpochRTC
//
//  PURPOSE:	Constructor for DeviceEpochRTC Class
//
//  INPUTS:		int						ID of ASIC Gate (Version)
//
//  OUTPUT:		(none)
//
///////////////////////////////////////////////////////////////////////
DeviceEpochRTC::DeviceEpochRTC(int gateID)	
{
	fGateID = gateID;
	Reset();
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	DeviceEpochRTC::~DeviceEpochRTC
//
//  PURPOSE:	Destructor for DeviceEpochRTC Class
//
//  INPUTS:		(none)
//
//  OUTPUT:		(none)
//
///////////////////////////////////////////////////////////////////////
DeviceEpochRTC::~DeviceEpochRTC()
{
}