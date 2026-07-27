#include "StdAfx.h"
#include "Hoppers.h"
#include "iostream"

HopperPayout::HopperPayout(){

	ZeroMemory(Motor, FITTEDHOPPERS * sizeof(UINT8));
	ZeroMemory(PrevMotor, FITTEDHOPPERS * sizeof(UINT8));
	ZeroMemory(OptoEnable, FITTEDHOPPERS * sizeof(UINT8));
	ZeroMemory(OptoFlag, FITTEDHOPPERS * sizeof(UINT8));
	ZeroMemory(Timer, FITTEDHOPPERS * sizeof(long));
	ZeroMemory(State, FITTEDHOPPERS * sizeof(UINT8));
	ZeroMemory(Enable, FITTEDHOPPERS * sizeof(UINT8));
	ZeroMemory(CoinSelect, FITTEDHOPPERS * sizeof(UINT8));
	ZeroMemory(LowSwitch, FITTEDHOPPERS * sizeof(UINT8));
	ZeroMemory(LowEnable, FITTEDHOPPERS * sizeof(UINT8));
	ZeroMemory(LowInvert, FITTEDHOPPERS * sizeof(UINT8));
	ZeroMemory(LowIndicator, FITTEDHOPPERS * sizeof(UINT8));
	ZeroMemory(LowLevel, FITTEDHOPPERS * sizeof(UINT32));
	ZeroMemory(Level, FITTEDHOPPERS * sizeof(UINT32));
	ZeroMemory(FullLevel, FITTEDHOPPERS * sizeof(UINT32));
	ZeroMemory(CounterIn, FITTEDHOPPERS * sizeof(UINT32));
	ZeroMemory(CounterOut, FITTEDHOPPERS * sizeof(UINT32));
	ZeroMemory(CounterRefill, FITTEDHOPPERS * sizeof(UINT32));
	ZeroMemory(HiSwitch, FITTEDHOPPERS * sizeof(UINT8));
	ZeroMemory(HiEnable, FITTEDHOPPERS * sizeof(UINT8));
	ZeroMemory(HiInvert, FITTEDHOPPERS * sizeof(UINT8));
	ZeroMemory(HiIndicator, FITTEDHOPPERS * sizeof(UINT8));
	ZeroMemory(HiLevel, FITTEDHOPPERS * sizeof(UINT32));
	ZeroMemory(MotorEnablePort, FITTEDHOPPERS * sizeof(UINT8));
	ZeroMemory(OptoReturnPort, FITTEDHOPPERS * sizeof(UINT8));
	ZeroMemory(OptoEnablePort, FITTEDHOPPERS * sizeof(UINT8));

}

HopperPayout::~HopperPayout(){

}

void HopperPayout::Reset(UINT8 HNum){

	//Protect Array
	if (HNum >= FITTEDHOPPERS){
		return;
	}

	Motor[HNum] = 0;
	PrevMotor[HNum] = 0;
	OptoEnable[HNum] = 0;
	OptoFlag[HNum] = 0;
	Timer[HNum] = 0;
	State[HNum] = 0;


}
void HopperPayout::WriteMotor(UINT8 HNum, UINT8 MotorOn){
	
	
	if (HNum >= FITTEDHOPPERS) return;

	if (Enable[HNum]){
		
		//Set Motor
		Motor[HNum] = MotorOn;

		if ((Motor[HNum]) && (PrevMotor[HNum] == 0)){
			State[HNum] = 1;		
		} else if ((Motor[HNum] == 0) && (PrevMotor[HNum])){
			State[HNum] = 0;		
		}
		PrevMotor[HNum] = Motor[HNum];
	}

}
void HopperPayout::WriteOptoEnable(UINT8 HNum, UINT8 OptoIn){

	if (HNum >= FITTEDHOPPERS) return;

	if (Enable[HNum]){
	
		if (OptoIn){
			OptoEnable[HNum] = 1;
		} else {
			OptoEnable[HNum] = 0;
		}
	}

}
UINT8 HopperPayout::ReadOpto(UINT8 HNum){

	UINT8 Ret = 0;
	
	if (HNum >= FITTEDHOPPERS) return 0;

	if (Enable[HNum]){
		
		if (OptoEnable[HNum]){
			Ret = OptoFlag[HNum] ^ 1;
		} else {
			Ret = OptoFlag[HNum];
		}
	}
	
	return Ret;
}
void HopperPayout::Update(UINT8 HNum, unsigned short Cycles){

	if (HNum >= FITTEDHOPPERS) return;

	if (Enable[HNum]){
	
		if (Motor[HNum]){
			//Play Motor Sound Here
			switch (State[HNum]){
			case 1://Motor Spins a bit.
				if (Timer[HNum] <= 0){
					Timer[HNum] = STAGE1TIME;
				}
				Timer[HNum] -= Cycles;
				if (Timer[HNum] <= 0){
					if (Level[HNum] == 0){
						State[HNum] = 0;
					} else {
						State[HNum] = 2;
					}
				} else {				
					OptoFlag[HNum] = 0;				
				}
				break;
			case 2://Motor Kicks a coin out past opto
				if (Timer[HNum] <= 0){
					Timer[HNum] = STAGE2TIME;
					CounterOut[HNum] += 1;				
					if (Level[HNum] > 0){					
						Level[HNum] -= 1;
					}
					//Play Coin Out Sound Here
				}
				Timer[HNum] -= Cycles;
				if (Timer[HNum] <= 0){
					State[HNum] = 1;				
				} else {
					OptoFlag[HNum] = 1;
				}
				break;		
			default://Safety			
				Timer[HNum] = 0;
				OptoFlag[HNum] = 0;
				State[HNum] = 0;
				break;
			}
		} else {
			Timer[HNum] = 0;
			OptoFlag[HNum] = 0;
			State[HNum] = 0;
		}

		if (LowEnable[HNum]){
			if (Level[HNum] <= LowLevel[HNum]){
				LowIndicator[HNum] = 1;
			} else {
				LowIndicator[HNum] = 0;
			}
		} else {
			LowIndicator[HNum] = 0;
		}
	}

}

UINT8 HopperPayout::CoinIn(UINT8 CoinCode){
	
	/* EDC CODES
	Case &H29: TStr = "2p Cash In"
	Case &H30: TStr = "5p Cash In"
    Case &H31: TStr = "10p Cash In"
    Case &H32: TStr = "20p Cash In"
    Case &H33: TStr = "50p Cash In"
    Case &H34: TStr = "1 Cash In"
    Case &H35: TStr = "2 Cash In"
    Case &H36: TStr = "5 Cash In"
    Case &H37: TStr = "10 Cash In"
    Case &H38: TStr = "5p Token In"
    Case &H39: TStr = "10p Token In"
    Case &H3A: TStr = "20p Token In"
    Case &H3B: TStr = "50p Token In"
    Case &H3C: TStr = "1 Token In"
    Case &H3D: TStr = "2 Token In"
    Case &H3E: TStr = "5 Token In"
    Case &H3F: TStr = "10 Token In"
	*/

	signed short cnt;
	UINT8 HopIndex;
	UINT8 CoinDrop;
	HopIndex = 0xff;
	CoinDrop = 0;

	if (CoinInputMode == 0){
		//non EDC Coin In
		HopIndex = 0xff;
		CoinDrop = 0;
	
		//Valid Coin Input
		if ((CoinCode >= 0) && (CoinCode < 13)){		
			//Step Through Each Hopper
			for (cnt = 0; cnt < FITTEDHOPPERS; cnt++){
				//Check Enabled
				if (Enable[cnt]){
					//Set Hopper Index
					if (CoinCode == CoinSelect[cnt]){
						HopIndex = (cnt & 0xff);				
						//Check Solindex is Valid
						if (HopIndex != 255){
							//Check Level agains Full Level
							if (Level[HopIndex] < FullLevel[HopIndex]){
								//Not Full
								break;
							} else {
								//Hopper Full, set HopIndex to invalid
								HopIndex = 0xff;
								//Increment times coin has dropped
								CoinDrop++;
							}
						}
					}	
				}
			}

			if (HopIndex != 0xff){//We found a Hopper with this coin
				//Increment tube level
				Level[HopIndex] += 1;	
				//Increment Coins In Counter
				CounterIn[HopIndex] += 1;											
			} else {	
				//No Tube found or Tubes full

				//Drop To Cashbox
			}	
		}
	} else {		
		//EDC Coin In
		if (((CoinCode >= 0x30) && (CoinCode <= 0x3F)) || (CoinCode == 0x29)){			
			//Valid Coin Input
			for (cnt = 0; cnt < FITTEDHOPPERS; cnt++){
				//Step Through Each Hopper
				if (Enable[cnt]){
					//Check Enabled
					if ((CoinCode == 0x29) && (CoinSelect[cnt] == 0)){HopIndex = (cnt & 0xff);}	//2p Coin
					if ((CoinCode == 0x30) && (CoinSelect[cnt] == 1)){HopIndex = (cnt & 0xff);}	//5p Coin
					if ((CoinCode == 0x31) && (CoinSelect[cnt] == 2)){HopIndex = (cnt & 0xff);}	//10p Coin
					if ((CoinCode == 0x32) && (CoinSelect[cnt] == 3)){HopIndex = (cnt & 0xff);}	//20p Coin
					if ((CoinCode == 0x33) && (CoinSelect[cnt] == 4)){HopIndex = (cnt & 0xff);}	//50p Coin
					if ((CoinCode == 0x34) && (CoinSelect[cnt] == 5)){HopIndex = (cnt & 0xff);}	//1 Coin
					if ((CoinCode == 0x35) && (CoinSelect[cnt] == 6)){HopIndex = (cnt & 0xff);}	//2 Coin
					if ((CoinCode == 0x36) && (CoinSelect[cnt] == 7)){HopIndex = (cnt & 0xff);}	//5 Coin
					if ((CoinCode == 0x37) && (CoinSelect[cnt] == 8)){HopIndex = (cnt & 0xff);}	//10 Coin
					if ((CoinCode == 0x38) && (CoinSelect[cnt] == 9)){HopIndex = (cnt & 0xff);}	//5p Token
					if ((CoinCode == 0x39) && (CoinSelect[cnt] == 10)){HopIndex = (cnt & 0xff);}//10p Token
					if ((CoinCode == 0x3a) && (CoinSelect[cnt] == 11)){HopIndex = (cnt & 0xff);}//20p Token
					if ((CoinCode == 0x3b) && (CoinSelect[cnt] == 12)){HopIndex = (cnt & 0xff);}//50p Token
					if ((CoinCode == 0x3c) && (CoinSelect[cnt] == 13)){HopIndex = (cnt & 0xff);}//1 Token
					if ((CoinCode == 0x3d) && (CoinSelect[cnt] == 14)){HopIndex = (cnt & 0xff);}//2 Token
					if ((CoinCode == 0x3e) && (CoinSelect[cnt] == 15)){HopIndex = (cnt & 0xff);}//5 Token
					if ((CoinCode == 0x3f) && (CoinSelect[cnt] == 16)){HopIndex = (cnt & 0xff);}//10 Token					
				}
			}

			if (HopIndex != 255){
				//We found a hopper with this coin
				if (Level[HopIndex] < FullLevel[HopIndex]){
					Level[HopIndex] += 1;
					CounterIn[HopIndex] += 1;
					//Update Coin Levels
					if (LowEnable[HopIndex]){
						if (Level[HopIndex] < LowLevel[HopIndex]){
							LowIndicator[HopIndex] = 1;
						} else {
							LowIndicator[HopIndex] = 0;
						}
					}
				} else {
					HopIndex = 0xff;
					CoinDrop += 1;
				}													
			}			
		} else {
			if ((CoinCode >= 0x50) && (CoinCode <= 0x5F)){
				//Valid Coin Refill Input
				for (cnt = 0; cnt < FITTEDHOPPERS; cnt++){
					//Step Through Each Hopper
					if (Enable[cnt]){
						//Check Enabled						
						if ((CoinCode == 0x50) && (CoinSelect[cnt] == 1)){HopIndex = (cnt & 0xff);}	//5p Coin Refill
						if ((CoinCode == 0x51) && (CoinSelect[cnt] == 2)){HopIndex = (cnt & 0xff);}	//10p Coin Refill
						if ((CoinCode == 0x52) && (CoinSelect[cnt] == 3)){HopIndex = (cnt & 0xff);}	//20p Coin Refill
						if ((CoinCode == 0x53) && (CoinSelect[cnt] == 4)){HopIndex = (cnt & 0xff);}	//50p Coin Refill
						if ((CoinCode == 0x54) && (CoinSelect[cnt] == 5)){HopIndex = (cnt & 0xff);}	//1 Coin Refill
						if ((CoinCode == 0x55) && (CoinSelect[cnt] == 6)){HopIndex = (cnt & 0xff);}	//2 Coin Refill
						if ((CoinCode == 0x56) && (CoinSelect[cnt] == 7)){HopIndex = (cnt & 0xff);}	//5 Coin Refill
						if ((CoinCode == 0x57) && (CoinSelect[cnt] == 8)){HopIndex = (cnt & 0xff);}	//10 Coin Refill
						if ((CoinCode == 0x58) && (CoinSelect[cnt] == 9)){HopIndex = (cnt & 0xff);}	//5p Token Refill
						if ((CoinCode == 0x59) && (CoinSelect[cnt] == 10)){HopIndex = (cnt & 0xff);}//10p Token Refill
						if ((CoinCode == 0x5a) && (CoinSelect[cnt] == 11)){HopIndex = (cnt & 0xff);}//20p Token Refill
						if ((CoinCode == 0x5b) && (CoinSelect[cnt] == 12)){HopIndex = (cnt & 0xff);}//50p Token Refill
						if ((CoinCode == 0x5c) && (CoinSelect[cnt] == 13)){HopIndex = (cnt & 0xff);}//1 Token Refill
						if ((CoinCode == 0x5d) && (CoinSelect[cnt] == 14)){HopIndex = (cnt & 0xff);}//2 Token Refill
						if ((CoinCode == 0x5e) && (CoinSelect[cnt] == 15)){HopIndex = (cnt & 0xff);}//5 Token Refill
						if ((CoinCode == 0x5f) && (CoinSelect[cnt] == 16)){HopIndex = (cnt & 0xff);}//10 Token Refill						
					}
				}

				if (HopIndex != 255){					
					//We found a hopper with this coin
					if (Level[HopIndex] < FullLevel[HopIndex]){
						Level[HopIndex] += 1;
						CounterRefill[HopIndex] += 1;
						//Update Coin Levels
						if (LowEnable[HopIndex]){
							if (Level[HopIndex] < LowLevel[HopIndex]){
								LowIndicator[HopIndex] = 1;
							} else {
								LowIndicator[HopIndex] = 0;
							}
						}
					} else {
						HopIndex = 0xff;
						CoinDrop += 1;
					}					
				} else {
					CoinDrop += 1;					
				}			
			}
		}
	

	}

	return HopIndex;

}

 void HopperPayout::SetHopperEnable(UINT8 Num, UINT8 Value){
	if (Num >= FITTEDHOPPERS) return;
	Enable[Num] = Value;
}
 void HopperPayout::SetHopperCoin(UINT8 Num, UINT8 Value){
	if (Num >= FITTEDHOPPERS) return;
	CoinSelect[Num] = Value;
}
 void HopperPayout::SetHopperCoinsIn(UINT8 Num, UINT32 Value){
	if (Num >= FITTEDHOPPERS) return;
	CounterIn[Num] = Value;
}
 void HopperPayout::SetHopperCoinsOut(UINT8 Num, UINT32 Value){
	if (Num >= FITTEDHOPPERS) return;
	CounterOut[Num] = Value;
}
 void HopperPayout::SetHopperCoinsRefilled(UINT8 Num, UINT32 Value){
	if (Num >= FITTEDHOPPERS) return;
	CounterRefill[Num] = Value;
}
 void HopperPayout::SetHopperLevel(UINT8 Num, UINT32 Value){
	if (Num >= FITTEDHOPPERS) return;
	Level[Num] = Value;
}
 void HopperPayout::SetHopperFullLevel(UINT8 Num, UINT32 Value){
	if (Num >= FITTEDHOPPERS) return;
	FullLevel[Num] = Value;
}
 void HopperPayout::SetHopperLoEnable(UINT8 Num, UINT8 Value){
	if (Num >= FITTEDHOPPERS) return;
	LowEnable[Num] = Value;
}
 void HopperPayout::SetHopperLoInvert(UINT8 Num, UINT8 Value){
	if (Num >= FITTEDHOPPERS) return;
	LowInvert[Num] = Value;
}
 void HopperPayout::SetHopperLoSwitch(UINT8 Num, UINT8 Value){
	if (Num >= FITTEDHOPPERS) return;
	LowSwitch[Num] = Value;
}
 void HopperPayout::SetHopperLoIndicator(UINT8 Num, UINT8 Value){
	if (Num >= FITTEDHOPPERS) return;
	LowIndicator[Num] = Value;
}
 void HopperPayout::SetHopperLoLevel(UINT8 Num, UINT32 Value){
	if (Num >= FITTEDHOPPERS) return;
	LowLevel[Num] = Value;
}
 void HopperPayout::SetHopperHiEnable(UINT8 Num, UINT8 Value){
	if (Num >= FITTEDHOPPERS) return;
	HiEnable[Num] = Value;
}
 void HopperPayout::SetHopperHiInvert(UINT8 Num, UINT8 Value){
	if (Num >= FITTEDHOPPERS) return;
	HiInvert[Num] = Value;
}
 void HopperPayout::SetHopperHiSwitch(UINT8 Num, UINT8 Value){
	if (Num >= FITTEDHOPPERS) return;
	HiSwitch[Num] = Value;
}
 void HopperPayout::SetHopperHiIndicator(UINT8 Num, UINT8 Value){
	if (Num >= FITTEDHOPPERS) return;
	HiIndicator[Num] = Value;
}
 void HopperPayout::SetHopperHiLevel(UINT8 Num, UINT32 Value){
	if (Num >= FITTEDHOPPERS) return;
	HiLevel[Num] = Value;
}
 void HopperPayout::SetHopperOptoEnable(UINT8 Num, UINT8 Value){
	if (Num >= FITTEDHOPPERS) return;
	OptoEnablePort[Num] = Value;
}
 void HopperPayout::SetHopperOptoReturn(UINT8 Num, UINT8 Value){
	if (Num >= FITTEDHOPPERS) return;
	OptoReturnPort[Num] = Value;
}
 void HopperPayout::SetHopperMotorEnable(UINT8 Num, UINT8 Value){
	if (Num >= FITTEDHOPPERS) return;
	MotorEnablePort[Num] = Value;
}


 UINT8 HopperPayout::GetHopperEnable(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	return Enable[Num];
}
 UINT8 HopperPayout::GetHopperCoin(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	 return CoinSelect[Num];
}
 UINT32 HopperPayout::GetHopperCoinsIn(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	 return CounterIn[Num];
}
 UINT32 HopperPayout::GetHopperCoinsOut(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	 return CounterOut[Num];
}
  UINT32 HopperPayout::GetHopperCoinsRefilled(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	 return CounterRefill[Num];
}
 UINT32 HopperPayout::GetHopperLevel(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	return Level[Num];
}
 UINT32 HopperPayout::GetHopperFullLevel(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	return FullLevel[Num];
}
 UINT8 HopperPayout::GetHopperLoEnable(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	return LowEnable[Num];
}
 UINT8 HopperPayout::GetHopperLoInvert(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	return LowInvert[Num];
}
  UINT8 HopperPayout::GetHopperLoIndicator(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	return LowIndicator[Num];
}
 UINT8 HopperPayout::GetHopperLoSwitch(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	return LowSwitch[Num];
}
 UINT32 HopperPayout::GetHopperLoLevel(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	return LowLevel[Num];
}
 UINT8 HopperPayout::GetHopperHiEnable(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	return HiEnable[Num];
}
 UINT8 HopperPayout::GetHopperHiInvert(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	return HiInvert[Num];
}
 UINT8 HopperPayout::GetHopperHiSwitch(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	return HiSwitch[Num];
}
   UINT8 HopperPayout::GetHopperHiIndicator(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	return HiIndicator[Num];
}
 UINT32 HopperPayout::GetHopperHiLevel(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	return HiLevel[Num];
}
 UINT8 HopperPayout::GetHopperOptoEnable(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	return OptoEnablePort[Num];
}
 UINT8 HopperPayout::GetHopperOptoReturn(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	return OptoReturnPort[Num];
}
 UINT8 HopperPayout::GetHopperMotorEnable(UINT8 Num){
	if (Num >= FITTEDHOPPERS) return 0;
	return MotorEnablePort[Num];
}