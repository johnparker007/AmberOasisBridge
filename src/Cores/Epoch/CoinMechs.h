#ifndef CoinMechsH
#define CoinMechsH

//#include "LoadSaveCompressDLLClass.h"

#define NUMCOINS 6

class MarsMech {
private:

	int InputCounter = 0;
	int LockCounter = 0;
	UINT8 CommStyle = 0;
	UINT8 CommInvert = 0;
	unsigned int PulseCycles = 0;
	UINT8 EDCEnable = 0;
	UINT8 LockoutVal[NUMCOINS] = { (0, 0, 0, 0, 0, 0) };
	UINT8 LockoutInvert[NUMCOINS] = {(0, 0, 0, 0, 0, 0)};
	UINT8 CoinValue[NUMCOINS] = { (0, 0, 0, 0, 0, 0) };
	UINT8 CoinEnable[NUMCOINS] = { (0, 0, 0, 0, 0, 0) };
	UINT8 LockoutPort = 0;
	UINT8 SelectedCoin = 0;
	UINT8 LampOnOff[2] = { (0, 0) };

	UINT32	CoinsIn2p = 0;
	UINT32	CoinsIn5p = 0;
	UINT32	CoinsIn10p = 0;
	UINT32	CoinsIn20p = 0;
	UINT32	CoinsIn50p = 0;
	UINT32	CoinsIn100p = 0;
	UINT32	CoinsIn200p = 0;
	UINT32	TokensIn5p = 0;
	UINT32	TokensIn10p = 0;
	UINT32	TokensIn20p = 0;
	UINT32	TokensIn50p = 0;
	UINT32	TokensIn100p = 0;
	UINT32	TokensIn200p = 0;
	UINT32  TokensIn = 0, CoinsIn = 0, TotalIn = 0;

	//LoadSaveCompressDLLClass * LSC;

public:	

	~MarsMech();
	MarsMech();

	UINT8 CoinIn(UINT8 Coin);
	UINT8 Run(unsigned short Cycles);	
	//void Init(LoadSaveCompressDLLClass * LSCIn);	
	void SetCommStyle(UINT8 Style);
	void SetCommInvert(UINT8 Invert);
	void SetCycles(unsigned int Cycles);
	void SetEDCEnable(UINT8 Enable);
	void SetLockoutVal(UINT8 Coin, UINT8 Value);
	void SetLockoutInvert(UINT8 Coin, UINT8 Invert);
	void SetLockoutPort(UINT8 Port);
	void SetSelectedCoin(UINT8 Coin);
	void SetCoinValue(UINT8 Num, UINT8 Value);
	void SetCoinEnable(UINT8 Num, UINT8 Value);
	UINT8 GetLampOnOff(UINT8 Num);	
	UINT8 GetSelectedCoin();

	//void SaveState();
	//void LoadState();
};

class ElecronicNoteMech {
private:
	
public:	
	
	//char NoteIn(UINT8 Coin);	
	//void Run(unsigned short Cycles);	
	//void Init(void);
};

class ElectroMechanicalCoin {
private:
	
public:	

	//char CoinIn(UINT8 Coin);
	//void Run(unsigned short Cycles);	
	//void Init(void);
};

#endif CoinMechsH