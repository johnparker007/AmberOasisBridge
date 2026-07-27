#include "stdafx.h"
#include "MarsMech.h"



namespace
{
	static bool ValidCoin(UINT8 coin)
	{
		return coin < NUMCOINS;
	}

	static bool ValidLockoutBit(UINT8 bit)
	{
		return bit < 8;
	}
}

void MarsMech::SaveState() {
	UINT32 loop;

	LSC->SaveToBuffer(InputCounter);
	LSC->SaveToBuffer(LockCounter);
	LSC->SaveToBuffer(CommStyle);
	LSC->SaveToBuffer(CommInvert);
	LSC->SaveToBuffer(PulseCycles);
	LSC->SaveToBuffer(EDCEnable);

	for (loop = 0; loop < NUMCOINS; loop++) {
		LSC->SaveToBuffer(LockoutVal[loop]);
		LSC->SaveToBuffer(LockoutInvert[loop]);
		LSC->SaveToBuffer(CoinValue[loop]);
		LSC->SaveToBuffer(CoinEnable[loop]);
	}

	LSC->SaveToBuffer(LampOnOff[0]);
	LSC->SaveToBuffer(LampOnOff[1]);
	LSC->SaveToBuffer(CoinsIn2p);
	LSC->SaveToBuffer(CoinsIn5p);
	LSC->SaveToBuffer(CoinsIn10p);
	LSC->SaveToBuffer(CoinsIn20p);
	LSC->SaveToBuffer(CoinsIn50p);
	LSC->SaveToBuffer(CoinsIn100p);
	LSC->SaveToBuffer(CoinsIn200p);
	LSC->SaveToBuffer(TokensIn5p);
	LSC->SaveToBuffer(TokensIn10p);
	LSC->SaveToBuffer(TokensIn20p);
	LSC->SaveToBuffer(TokensIn50p);
	LSC->SaveToBuffer(TokensIn100p);
	LSC->SaveToBuffer(TokensIn200p);
	LSC->SaveToBuffer(TokensIn);
	LSC->SaveToBuffer(CoinsIn);
	LSC->SaveToBuffer(TotalIn);
}

void MarsMech::LoadState() {
	UINT32 loop;

	LSC->LoadFromBuffer(InputCounter);
	LSC->LoadFromBuffer(LockCounter);
	LSC->LoadFromBuffer(CommStyle);
	LSC->LoadFromBuffer(CommInvert);
	LSC->LoadFromBuffer(PulseCycles);
	LSC->LoadFromBuffer(EDCEnable);

	for (loop = 0; loop < NUMCOINS; loop++) {
		LSC->LoadFromBuffer(LockoutVal[loop]);
		LSC->LoadFromBuffer(LockoutInvert[loop]);
		LSC->LoadFromBuffer(CoinValue[loop]);
		LSC->LoadFromBuffer(CoinEnable[loop]);
	}

	LSC->LoadFromBuffer(LampOnOff[0]);
	LSC->LoadFromBuffer(LampOnOff[1]);
	LSC->LoadFromBuffer(CoinsIn2p);
	LSC->LoadFromBuffer(CoinsIn5p);
	LSC->LoadFromBuffer(CoinsIn10p);
	LSC->LoadFromBuffer(CoinsIn20p);
	LSC->LoadFromBuffer(CoinsIn50p);
	LSC->LoadFromBuffer(CoinsIn100p);
	LSC->LoadFromBuffer(CoinsIn200p);
	LSC->LoadFromBuffer(TokensIn5p);
	LSC->LoadFromBuffer(TokensIn10p);
	LSC->LoadFromBuffer(TokensIn20p);
	LSC->LoadFromBuffer(TokensIn50p);
	LSC->LoadFromBuffer(TokensIn100p);
	LSC->LoadFromBuffer(TokensIn200p);
	LSC->LoadFromBuffer(TokensIn);
	LSC->LoadFromBuffer(CoinsIn);
	LSC->LoadFromBuffer(TotalIn);
}

MarsMech::~MarsMech() {

}

MarsMech::MarsMech(UINT32 cpuSpeed)
{
	ZeroMemory(LockoutVal, NUMCOINS * sizeof(UINT8));
	ZeroMemory(LockoutDrive, NUMCOINS * sizeof(UINT8));
	ZeroMemory(LockoutInvert, NUMCOINS * sizeof(UINT8));
	ZeroMemory(CoinValue, NUMCOINS * sizeof(UINT8));
	ZeroMemory(CoinEnable, NUMCOINS * sizeof(UINT8));
	ZeroMemory(LampOnOff, 2 * sizeof(UINT8));
	CpuSpeed = cpuSpeed;
}

UINT8 MarsMech::GetCoinByte()
{
	return CoinByte;
}

UINT8 MarsMech::GetLampOnOff(UINT8 Num) {
	if (Num >= 2) {
		return 0;
	}
	return LampOnOff[Num];
}

void MarsMech::SetCoinValue(UINT8 Num, UINT8 Value) {
	if (!ValidCoin(Num)) {
		return;
	}
	CoinValue[Num] = Value;
}

void MarsMech::SetCoinEnable(UINT8 Num, UINT8 Value) {
	if (!ValidCoin(Num)) {
		return;
	}
	CoinEnable[Num] = Value;
}

bool MarsMech::IsCoinUnlocked(UINT8 coin) const
{
	if (!ValidCoin(coin)) {
		return 0;
	}

	UINT8 val = LockoutVal[coin];
	UINT8 invert = LockoutInvert[coin];

	if (invert)
	{
		if (val)
		{
			return false;
		}
		else
		{
			return true;
		}
	}
	else
	{
		if (val)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
}


UINT8 MarsMech::CoinIn(UINT8 Coin, UINT8 CoinValue) {

	if (!ValidCoin(Coin) || !ValidLockoutBit(LockoutVal[Coin]) || !CoinEnable[Coin]) {
		return false;
	}

	if (InputCounter == 0) {
		if (LockCounter == 0) {
			if (IsCoinUnlocked(Coin)) {
				if (PulseCycles < 1) {
					return false;
				}
				InputCounter = PulseCycles;
				switch (CoinValue) {
				case 0:
					CoinsIn2p += 1; CoinsIn += 2;
					BCD = 0x02;
					break;
				case 1:
					CoinsIn5p += 1; CoinsIn += 5;
					BCD = 0x05;
					break;
				case 2:
					CoinsIn10p += 1; CoinsIn += 10;
					BCD = 0x0A;
					break;
				case 3:
					CoinsIn20p += 1; CoinsIn += 20;
					BCD = 0x14;
					break;
				case 4:
					CoinsIn50p += 1; CoinsIn += 50;
					BCD = 0x32;
					break;
				case 5:
					CoinsIn100p += 1; CoinsIn += 100;
					BCD = 0x3A;
					break;
				case 6:
					CoinsIn200p += 1; CoinsIn += 200;
					BCD = 0xC8;
					break;
				case 7:
					TokensIn5p += 1; TokensIn += 5;
					BCD = 0x5;
					break;
				case 8:
					TokensIn10p += 1; TokensIn += 10;
					BCD = 0xA;
					break;
				case 9:
					TokensIn20p += 1; TokensIn += 20;
					BCD = 0x14;
					break;
				case 10:
					TokensIn50p += 1; TokensIn += 50;
					BCD = 0x32;
					break;
				case 11:
					TokensIn100p += 1; TokensIn += 100;
					BCD = 0x64;
					break;
				case 12:
					TokensIn200p += 1; TokensIn += 200;
					BCD = 0xC8;
					break;
				}

				switch (CommStyle)
				{
				case 0: //Parallel
					CoinByte = Coin + 1;
					break;
				case 2: //BCD
					CoinByte = BCD;
					break;
				}

				TotalIn = (CoinsIn + TokensIn);
				return true;//Coin Accepted
			}
		}
	}

	return false; //Coin Rejected

}

UINT8 MarsMech::Run(UINT32 Cycles) {

	UINT8 ret = 0;

	if (InputCounter > 0) {
		ret = 1;
		if (Cycles >= static_cast<UINT32>(InputCounter)) {
			InputCounter = 0;
			LockCounter = (UINT32)((float)CpuSpeed * 0.5f); //Half second = 1,720,000 / 4
			CoinByte = 0;
		}
		else {
			InputCounter -= static_cast<INT32>(Cycles);
		}
	}

	if (LockCounter > 0) {
		if (Cycles >= static_cast<UINT32>(LockCounter)) {
			LockCounter = 0;
		}
		else {
			LockCounter -= static_cast<INT32>(Cycles);
		}
	}

	return ret;
}
void MarsMech::Init(LoadSaveClass* LSCIn) {
	LSC = LSCIn;
}

void MarsMech::Reset()
{
	InputCounter = 0;
	LockCounter = 0;
}

void MarsMech::SetCommStyle(UINT8 Style) {
	CommStyle = Style;
}
void MarsMech::SetCommInvert(UINT8 Invert) {
	CommInvert = Invert;
}
void MarsMech::SetCycles(UINT32 Cycles) {
	PulseCycles = Cycles;
}
void MarsMech::SetEDCEnable(UINT8 Enable) {
	EDCEnable = Enable;
}

void MarsMech::SetLockoutVal(UINT8 index, UINT8 data) {

	for (UINT8 cnt = 0; cnt < NUMCOINS; cnt++) {
		UINT8 drive = LockoutDrive[cnt];
		if ((drive >= index) && (drive <= (index + 7))) {
			UINT val = (drive - index);
			LockoutVal[cnt] = (data & (1 << val));
		}
	}

}
void MarsMech::SetLockoutDrive(UINT8 Coin, UINT8 drive)
{
	if (!ValidCoin(Coin)) {
		return;
	}
	LockoutDrive[Coin] = drive;
}

void MarsMech::SetLockoutInvert(UINT8 Coin, UINT8 Invert)
{
	if (!ValidCoin(Coin)) {
		return;
	}
	LockoutInvert[Coin] = Invert ? 1 : 0;
}