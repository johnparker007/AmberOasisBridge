#ifndef DeviceEpochReelsH
#define DeviceEpochReelsH

#define CYCLESTOAVERAGE 30
#define NUMEPOCHREELS 8

class EpochReels {
public:

	struct reelstruct {
		int offset = 0;
		int flag = 0;
		int steps = 0;
		int pos = 0;
		int reelpos = 0;
		int startopto = 0;
		int endopto = 0;
		int adjust = 0;
		UINT8 inverted = 0;
		UINT8 pat = 0;
		UINT8 now = 0;
		UINT8 ReelVal = 0;
		UINT8 writePoint = 0;
		UINT8 PosVal[CYCLESTOAVERAGE];
		UINT32 EndCnt[CYCLESTOAVERAGE];
		UINT32 TotalCnt[CYCLESTOAVERAGE];
	};

protected:
		
	void			state(UINT8 x, UINT8 nbr);

public:
	UINT8			optos = 0;
	reelstruct      reels[NUMEPOCHREELS];
	void            write(UINT8 x, UINT8 nbr);
	void		    reset(void);
	void			run(int cycles);		
	EpochReels();
	~EpochReels();
};

#endif // DeviceEpochReelsH
