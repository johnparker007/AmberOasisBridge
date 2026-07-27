#ifndef MetersH
#define MetersH

//#include "LoadSaveCompressDLLClass.h"


#define FITTEDMETERS 6
class CoinMeter {
private:

	//LoadSaveCompressDLLClass * LSC;

	//Normal
	UINT8 Pin[FITTEDMETERS];	
	UINT8 On[FITTEDMETERS];	
	UINT8 Enable[FITTEDMETERS];	
	UINT8 PrevPin[FITTEDMETERS];	
	unsigned long TimeOn[FITTEDMETERS];
	unsigned long Counter[FITTEDMETERS];
	UINT8 PortIndex[FITTEDMETERS];	

	//SEC
	UINT8    clk;
	UINT8	 ch;
	UINT8	 clks;
    UINT8    data1;
    UINT8    command[60];
    UINT8    response[60];
    UINT8    pos;
    UINT8    rx_pos;
    UINT8    rx_clk;
    UINT8    rx_data;
    UINT8    rx_length;
    UINT8    bytes_left;
    UINT8    last_id;
    UINT8    status;
    UINT8    market_type;
    UINT8    last_error;
    UINT8    nbr_of_counters;
    int              fingerprint;
    bool             enabled;
	char SEC_Strings[31][8];
	int SEC_Counters[31];

    void             ProcessCommand(void);
    void             BuildResponse(UINT8 code, UINT8 id, UINT8 len, UINT8 *data);
	void			 Do_Char(UINT8 ch);

public:	

	CoinMeter();
	~CoinMeter();
	
	void Write(UINT8 Index, UINT8 PinIn);
	void Run(unsigned short Cycles);
	UINT8 Check(void);
	
	unsigned long GetCounter(UINT8 Num);
	void SetCounter(UINT8 Num, unsigned long Value);
	void SetMeterEnable(UINT8 Num, UINT8 Value);
	
	//SEC
	UINT8    meter;
    int              value;
    bool             updated;
	bool			 SECSwitch;
	void			 Reset(void);
	void			 SetEnable(UINT8 e);
    void             SetClock(UINT8 x);
    void             SetData(UINT8 x);
    UINT8    ReadData(void);

	//void Init(LoadSaveCompressDLLClass * LSCIn);

	void SaveState();
	void LoadState();
};

#endif MetersH