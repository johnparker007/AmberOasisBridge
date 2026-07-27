// ###########################################################################
// #
// # DeviceEpochRTC - Definition of Epoch RTC Driver
// # Copyright (C) 2002-2012 Tony Friery [DialTone]
// #
// # ALL RIGHTS RESERVED
// #
// ###########################################################################

#ifndef DeviceEpochRTCH
#define DeviceEpochRTCH

#include <basetsd.h>

class DeviceEpochRTC
{
private:
	int					fReplySize;					// Number of bytes in reply
	int					fReplyOffset;				// Current Reply Byte
	UINT8					fReplyBuffer[128];		// Reply Buffer
	UINT8					fOutputChar;				// Character being Output to CPU
	UINT8					fInputChar;					// Character being Read from CPU
	int					fOutBitCount;				// Bit Count of Output
	int					fInBitCount;				// Bit Count of Input
	int					fGateID;						// ID of the ASIC Gate
	bool					fLastCPUClock;				// Previous Hitachi Clock State
	bool					fLastCPUData;				// Previous Hitachi Data State
	bool					fRTCClock;					// Current RTC Clock State
	bool					fRTCData;					// Current RTC Data State
	bool					fSendReceive;				// Send/Receive Mode (false = RTC receiving)

	void __fastcall	ProcessCommand(void);	// Process currently received command

public:
	void __fastcall	Reset(void);
	void __fastcall Write(bool CPUClock, bool CPUData);
	void __fastcall	BuildReadClockReply(void);

	UINT8 __fastcall	GetPort8(void);
	UINT8 __fastcall	GetPortA(void);

	DeviceEpochRTC(int gateID);
	~DeviceEpochRTC();
};

#endif
