#pragma once

#include "LoadSave.h"

#define NUMCOINS 6

//These define the possible ports the input comes from
#define SYS6_COINPORT 0

class MarsMech {
public:

	~MarsMech();
	MarsMech(UINT32 CpuSpeed);

	UINT8 CoinIn(UINT8 Coin, UINT8 CoinValue);
	UINT8 Run(UINT32 Cycles);
	void Init(LoadSaveClass* LSCIn);
	void Reset();

	void SetCommStyle(UINT8 Style);
	void SetCommInvert(UINT8 Invert);
	void SetCycles(UINT32 Cycles);
	void SetEDCEnable(UINT8 Enable);
	void SetLockoutVal(UINT8 Index, UINT8 Value);
	void SetLockoutDrive(UINT8 Coin, UINT8 drive);
	void SetLockoutInvert(UINT8 Coin, UINT8 Invert);

	void SetSelectedCoin(UINT8 Coin);
	void SetCoinValue(UINT8 Num, UINT8 Value);
	void SetCoinEnable(UINT8 Num, UINT8 Value);

	UINT8 GetLampOnOff(UINT8 Num);

	UINT8 GetCoinByte();

	void SaveState();
	void LoadState();

private:

	UINT32 CpuSpeed = 0;
	INT32 InputCounter = 0;
	INT32 LockCounter = 0;
	UINT8 CommStyle = 0;
	UINT8 CommInvert = 0;
	UINT32 PulseCycles = 0;
	UINT8 EDCEnable = 0;
	UINT8 LockoutVal[NUMCOINS];
	UINT8 LockoutInvert[NUMCOINS];
	UINT8 LockoutDrive[NUMCOINS];
	UINT8 CoinValue[NUMCOINS];
	UINT8 CoinEnable[NUMCOINS];
	UINT8 CoinByte = 0;
	UINT8 LampOnOff[2];

	UINT32	CoinsIn2p = 0,
		CoinsIn5p = 0,
		CoinsIn10p = 0,
		CoinsIn20p = 0,
		CoinsIn50p = 0,
		CoinsIn100p = 0,
		CoinsIn200p = 0,
		TokensIn5p = 0,
		TokensIn10p = 0,
		TokensIn20p = 0,
		TokensIn50p = 0,
		TokensIn100p = 0,
		TokensIn200p = 0,
		TokensIn = 0,
		CoinsIn = 0,
		TotalIn = 0;

	UINT8 BCD = 0;

	bool IsCoinUnlocked(UINT8 Coin) const;

	LoadSaveClass* LSC = NULL;
};