#ifndef EPOCHH
#define EPOCHH

#include "PA2CoreInterface.h"
#include <string>

#include "H83002.h"
#include "DeviceEpochRTC.h"
#include "AlphaStarburst.h"
#include "AlphaDotMatrix.h"
#include "Reels.h"
#include "SoundMain.h"
#include "CoinMechs.h"
#include "Meters.h"
#include "Hoppers.h"
#include "CashBox.h"
#include "EDC.h"

///////////////////////////////////////////////////////////////////////
//
// 	Definitions
//
///////////////////////////////////////////////////////////////////////
#define NUMMECHS 8
#define NUMREELS 8
#define RAMSIZE 0x10000 //(64KB)
#define ROMSIZE 0x400000
#define SOUNDROMSIZE 0x80000



///////////////////////////////////////////////////////////////////////
//
// 	CPU Emulation Class Definition
//
///////////////////////////////////////////////////////////////////////

class BoardEpoch;

class EPOCHCPU : H83002 {
	friend class BoardEpoch;
private:
	
	
	
	UINT8					IOMAP_LAMPS[512];
	UINT8					IOMAP_LEDS[512];
	UINT8					IOMAP_INPUTS[512];
	UINT8					IOMAP_INPUT_IRQEN[512];
	UINT8					IOMAP_LED_DIM = NULL;
	UINT8					IOMAP_LAMP_DIM = NULL;
	UINT8					IOMAP_LED_TIMER[12];
	UINT8					IOMAP_LAMP_TIMER[12];
	UINT8					IOMAP_DIPS1 = NULL;
	UINT8					IOMAP_DIPS2 = NULL;
	UINT8					IOMAP_MECHDRIVE = NULL;
	UINT8					IOMAP_DIVERTS = NULL;
	UINT8					IOMAP_HOPDRIVE = NULL;

	
	UINT8					last_int_sts_reg = NULL;


	bool					func_sw = NULL;
	UINT8					fReelPatterns[8];
	bool					fReelToggle = NULL;

	UINT8					fStatusLED = NULL;			// Status LED
	UINT8					led_flash_counts[12];
	UINT8					lamp_flash_counts[12];
	bool					led_flash[12];
	bool					lamp_flash[12];
	UINT32					rfsh_int_count = NULL;
	UINT32					sync_int_count = NULL;
	UINT32					lamp_timer_count = NULL;

	

	BoardEpoch 				*fOwner = NULL;

public:

	UINT8  					ROM[ROMSIZE];  // 4mb Program ROM Storage
	UINT8  					RAM[RAMSIZE];	// 128k Onboard RAM
	//CPU R/W/I/O
	UINT8 __fastcall    	program_read_byte(UINT32 address);
	UINT16 __fastcall		program_read_word(UINT32 address);
	void __fastcall			program_write_byte(UINT32 address, UINT8 value);
	void __fastcall			program_write_word(UINT32 address, UINT16 value);
	UINT16 __fastcall		cpu_readop16(UINT32 address);
	UINT8 __fastcall		io_read_byte_8(UINT32 address);
	void __fastcall			io_write_byte_8(UINT32 address, UINT8 value);
	UINT32 					h8_mem_read32(UINT32 address);
	void 					h8_mem_write32(UINT32 address, UINT32 data);
	void					SetDIP(UINT8 Num, UINT8 Value);
	void __fastcall			LoadRAM(const char* FileString);
	void __fastcall			SaveRAM(const char* FileString);
	void __fastcall			ClearRAM(void);
	void SetAudioIRQ(void);
	signed long LoadROM(char*, char*, char*, char*, char FlashSw);
	EPOCHCPU();
	~EPOCHCPU();

};

///////////////////////////////////////////////////////////////////////
//
// 	EPOCH System Board Class Definition
//
///////////////////////////////////////////////////////////////////////

class BoardEpoch
{
	friend class EPOCHCPU;

	private:
		
		EPOCHCPU 				*fMainCPU;
		DeviceEpochRTC			*fRTCDevice;
		AlphaStarburst			*fAlphaStarburst;
		AlphaDotMatrix			*fAlphaDotMatrix;	
		EpochReels				fReels;				// Reels Hardware
		SampledSound			fSound;
		CoinMeter				fMeters;
		CashBoxClass			fCashBox;
		MarsMech		fCoinMech[NUMMECHS];
		HopperPayout			fHoppers;
		
		bool					fProgramLoaded;
		UINT32					fFrameCyclesElapsed;

		UINT32					memoryOffset;
		UINT32					lastMemoryDebug;
		UINT32					lastMemoryScroll;		
		
		UINT8					Stake;
		UINT8					Prize;
		UINT8					Percent;
		UINT8					ReelExt;
		UINT8					PrevValue;
		std::string CFolder;
		std::string CFileName;


	public:

		//Program Subroutines / Functions		
		void Init(void);						//Init Sub	
		void __fastcall		LoadRAM(const char* FileString);
		void __fastcall		SaveRAM(const char* FileString);
		void __fastcall		ClearRAM(void);
		void __fastcall		PowerOnReset(void);


	 	BoardEpoch();
		virtual ~BoardEpoch(void);

		void SetCFolder(const char* Folder);
		void SetCFileName(const char* FileName);
		void SaveState(void);
		void LoadState(void);
		char* getEDCString();
		
		signed long LoadROM(char*, char*, char*, char*, char FlashSw);
		int Execute(int Cycles);
		int	GetAlphaSegs(char);
		UINT8 GetAlphaChar(UINT8 num);
		char GetAlphaBright();
		char GetAlphaDBright();
		UINT8 __fastcall GetAlphaDots(char CharNum, char ColumnNum);
		UINT8 GetAlphaDotComma(char CharIn);
		UINT8 GetAlphaDDotComma(char CharIn);
		void SetStake(char Stake);
		void SetPrize(char Prize);
		void SetPercent(char Percent);
		void TurnSwitchOn(int Num);
		void TurnSwitchOff(int Num);
		UINT8 ReadSwitch(UINT8 Num) const;
		UINT8 GetStatusLED(void);
		//Lamps
		bool GetLampOn(unsigned short);
		float GetLampBright(unsigned short);
		float GetFilamentColorR(unsigned short);
		float GetFilamentColorG(unsigned short);
		float GetFilamentColorB(unsigned short);
		
		//7 Seg Displays
		UINT8 GetSegOn(unsigned short);
		UINT8 GetSegBright(unsigned short);
		
		short GetReelPos(UINT8 num);		
		void SetOptoInvert(UINT8 ReelNum, UINT8 State);
		void SetOptoStart(UINT8 ReelNum, UINT8 Start);
		void SetOptoEnd(UINT8 ReelNum, UINT8 End);
		void SetSteps(UINT8 ReelNum, UINT8 State);
		
		

		signed long LoadSoundROM(char*, char*, char*, char*);
		void SetReelExt(UINT8 Ext);
		//EL Coin Mech
		void SetCommStyle(UINT8 Num, UINT8 Style);
		void SetCommInvert(UINT8 Num, UINT8 Invert);
		void SetCycles(UINT8 Num, unsigned int Cycles);
		void SetEDCEnable(UINT8 Num, UINT8 Enable);
		void SetLockoutVal(UINT8 Num, UINT8 Coin, UINT8 Value);
		void SetLockoutInvert(UINT8 Num, UINT8 Coin, UINT8 Invert);
		UINT8 CoinIn(UINT8 Num, UINT8 Coin, UINT8 CoinValue);
		void SetCoinValue(UINT8 Num, UINT8 CoinNum, UINT8 Value);
		void SetCoinEnable(UINT8 Num, UINT8 CoinNum, UINT8 Value);
		UINT8 GetLampOnOff(UINT8 Num, UINT8 LampNum);
		void SetMeterEnable(UINT8 Num, UINT8 Value);
		void SetMeterCounter(UINT8 num, unsigned int Value);
		unsigned int GetMeterCounter(UINT8 num);
		void SetDIP(UINT8 Num, UINT8 Value);
		UINT8 GetDIP(UINT8 Num) const;
		UINT32 GetOutputSnapshot(PA2_OutputSnapshot& Out);
		UINT32 FillAudioFrames(INT16* OutInterleavedStereo, UINT32 FramesRequired);

		void SetHopperEnable(UINT8 Num, UINT8 Value);
		void SetHopperCoin(UINT8 Num, UINT8 Value);
		void SetHopperCoinsIn(UINT8 Num, UINT32 Value);
		void SetHopperCoinsOut(UINT8 Num, UINT32 Value);
		void SetHopperLevel(UINT8 Num, UINT32 Value);
		void SetHopperFullLevel(UINT8 Num, UINT32 Value);
		void SetHopperLoEnable(UINT8 Num, UINT8 Value);
		void SetHopperLoInvert(UINT8 Num, UINT8 Value);
		void SetHopperLoSwitch(UINT8 Num, UINT8 Value);
		void SetHopperLoLevel(UINT8 Num, UINT32 Value);
		void SetHopperHiEnable(UINT8 Num, UINT8 Value);
		void SetHopperHiInvert(UINT8 Num, UINT8 Value);
		void SetHopperHiSwitch(UINT8 Num, UINT8 Value);
		void SetHopperHiLevel(UINT8 Num, UINT32 Value);
		void SetHopperOptoEnable(UINT8 Num, UINT8 Value);
		void SetHopperOptoReturn(UINT8 Num, UINT8 Value);
		void SetHopperMotorEnable(UINT8 Num, UINT8 Value);
		void SetHopperLoIndicator(UINT8 Num, UINT8 Value);
		void SetHopperHiIndicator(UINT8 Num, UINT8 Value);
		void SetHopperCoinsRefilled(UINT8 Num, UINT32 Value);
		UINT8 GetHopperEnable(UINT8 Num);
		UINT8 GetHopperCoin(UINT8 Num);
		UINT32 GetHopperCoinsIn(UINT8 Num);
		UINT32 GetHopperCoinsOut(UINT8 Num);
		UINT32 GetHopperLevel(UINT8 Num);
		UINT32 GetHopperFullLevel(UINT8 Num);
		UINT8 GetHopperLoEnable(UINT8 Num);
		UINT8 GetHopperLoInvert(UINT8 Num);
		UINT8 GetHopperLoSwitch(UINT8 Num);
		UINT32 GetHopperLoLevel(UINT8 Num);
		UINT8 GetHopperHiEnable(UINT8 Num);
		UINT8 GetHopperHiInvert(UINT8 Num);
		UINT8 GetHopperHiSwitch(UINT8 Num);
		UINT32 GetHopperHiLevel(UINT8 Num);
		UINT8 GetHopperOptoEnable(UINT8 Num);
		UINT8 GetHopperOptoReturn(UINT8 Num);
		UINT8 GetHopperMotorEnable(UINT8 Num);
		UINT32 GetHopperCoinsRefilled(UINT8 Num);
		UINT8 GetHopperHiIndicator(UINT8 Num);
		UINT8 GetHopperLoIndicator(UINT8 Num);
};


#endif // EPOCHH