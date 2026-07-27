#include "stdafx.h"
#include "CoinMechs.h"
/*
void ElecronicCoinMech::SaveState(){
	int loop;

	LSC->SaveToBuffer(InputCounter);
	LSC->SaveToBuffer(LockCounter);
	LSC->SaveToBuffer(CommStyle);
	LSC->SaveToBuffer(CommInvert);
	LSC->SaveToBuffer(PulseCycles);
	LSC->SaveToBuffer(EDCEnable);

	for (loop = 0; loop < NUMCOINS; loop++){
		LSC->SaveToBuffer(LockoutVal[loop]);
		LSC->SaveToBuffer(LockoutInvert[loop]);
		LSC->SaveToBuffer(CoinValue[loop]);
		LSC->SaveToBuffer(CoinEnable[loop]);
	}

	LSC->SaveToBuffer(LockoutPort);
	LSC->SaveToBuffer(SelectedCoin);
	LSC->SaveToBuffer(LampOnOff[2]);
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

void ElecronicCoinMech::LoadState(){
	int loop;

	LSC->LoadFromBuffer(InputCounter);
	LSC->LoadFromBuffer(LockCounter);
	LSC->LoadFromBuffer(CommStyle);
	LSC->LoadFromBuffer(CommInvert);
	LSC->LoadFromBuffer(PulseCycles);
	LSC->LoadFromBuffer(EDCEnable);

	for (loop = 0; loop < NUMCOINS; loop++){
		LSC->LoadFromBuffer(LockoutVal[loop]);
		LSC->LoadFromBuffer(LockoutInvert[loop]);
		LSC->LoadFromBuffer(CoinValue[loop]);
		LSC->LoadFromBuffer(CoinEnable[loop]);
	}

	LSC->LoadFromBuffer(LockoutPort);
	LSC->LoadFromBuffer(SelectedCoin);
	LSC->LoadFromBuffer(LampOnOff[2]);
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
*/
MarsMech::MarsMech(){
	
	
}
MarsMech::~MarsMech(){

}

void MarsMech::SetSelectedCoin(UINT8 Coin){
	SelectedCoin = Coin;
}
UINT8 MarsMech::GetSelectedCoin(){
	return SelectedCoin;
}

UINT8 MarsMech::GetLampOnOff(UINT8 Num){
	if (Num >= 2) return 0;
	UINT8 ret;
	ret = LampOnOff[Num];
	return ret;
}

void MarsMech::SetCoinValue(UINT8 Num, UINT8 Value){
	if (Num >= NUMCOINS) return;
	CoinValue[Num] = Value;
}

void MarsMech::SetCoinEnable(UINT8 Num, UINT8 Value){
	if (Num >= NUMCOINS) return;
	CoinEnable[Num] = Value;
}
void MarsMech::SetLockoutPort(UINT8 Port){
	
	int loop, tokensEnabled, cashEnabled, tokensLocked, cashLocked;

	LockoutPort = Port;

	cashEnabled = 0;
	cashLocked = 0;
	tokensEnabled = 0;
	tokensLocked = 0;


	for (loop = 0; loop < NUMCOINS; loop++){
		if (CoinEnable[loop]){
			if (CoinValue[loop] < 7){
				cashEnabled += 1;
				if (!((255 ^ LockoutPort) & (1 << LockoutVal[loop]))){
					cashLocked += 1;
				}
			} else {
				tokensEnabled += 1;
				if (!((255 ^ LockoutPort) & (1 << LockoutVal[loop]))){
					tokensLocked += 1;
				}
			}
		}
	}
	
	//Lamp 1
	if (cashLocked == cashEnabled){
		LampOnOff[0] = 0;
	} else {
		LampOnOff[0] = 1;
	}
	//Lamp 2
	if (tokensLocked == tokensEnabled){
		LampOnOff[1] = 0;
	} else {
		LampOnOff[1] = 1;
	}	

}

UINT8 MarsMech::CoinIn(UINT8 Coin){
	
	int LockoutBin;

	if (InputCounter == 0){
		if (LockCounter == 0){
			LockoutBin = (1 << LockoutVal[Coin]);
			if (((255 ^ LockoutPort) & LockoutBin)){
				if (PulseCycles < 1){
					return false;
				}
				InputCounter = PulseCycles;
				switch (CoinValue[Coin]){
				case 0: CoinsIn2p += 1; CoinsIn += 2; break;
				case 1: CoinsIn5p += 1; CoinsIn += 5; break;
				case 2: CoinsIn10p += 1; CoinsIn += 10; break;
				case 3: CoinsIn20p += 1; CoinsIn += 20; break;
				case 4: CoinsIn50p += 1; CoinsIn += 50; break;
				case 5: CoinsIn100p += 1; CoinsIn += 100; break;
				case 6: CoinsIn200p += 1; CoinsIn += 200; break;
				case 7: TokensIn5p += 1; TokensIn += 5; break;
				case 8: TokensIn10p += 1; TokensIn += 10; break;
				case 9: TokensIn20p += 1; TokensIn += 20; break;
				case 10: TokensIn50p += 1; TokensIn += 50; break;
				case 11: TokensIn100p += 1; TokensIn += 100; break;
				case 12: TokensIn200p += 1; TokensIn += 200; break;
				}
				TotalIn = (CoinsIn + TokensIn);
				return true;//Coin Accepted
			}
		}
	}

	return false; //Coin Rejected

}
UINT8 MarsMech::Run(unsigned short Cycles){

	UINT8 ret = 0;

	if (InputCounter){
		ret = 1;
		InputCounter -= Cycles;
		if (InputCounter < 0){
			InputCounter = 0;
			LockCounter = 8000000; //Half a second
		}
	}

	if (LockCounter){
		LockCounter -= Cycles;
		if (LockCounter < 0){
			LockCounter = 0;			
		}
	}

	return ret;
}	
/*
void ElecronicCoinMech::Init(LoadSaveCompressDLLClass * LSCIn){
	LSC = LSCIn;
}
*/
void MarsMech::SetCommStyle(UINT8 Style){
	CommStyle = Style;
}
void MarsMech::SetCommInvert(UINT8 Invert){
	CommInvert = Invert;
}
void MarsMech::SetCycles(unsigned int Cycles){
	PulseCycles = Cycles;
}
void MarsMech::SetEDCEnable(UINT8 Enable){
	EDCEnable = Enable;
}
void MarsMech::SetLockoutVal(UINT8 Coin, UINT8 Value){
	if (Coin >= NUMCOINS) return;
	LockoutVal[Coin] = Value;
}
void MarsMech::SetLockoutInvert(UINT8 Coin, UINT8 Invert){
	if (Coin >= NUMCOINS) return;
	LockoutInvert[Coin] = Invert;
}