
#include "stdafx.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "Reels.h"


static const int newjpm[8][16] = {
  { 0,  0,  4,  0,  2,  1,  3,  0, -2, -1, -3,  0,  0,  0,  0,  0 },
  { 0, -1,  3,  0,  1,  0,  2,  0, -3, -2,  4,  0,  0,  0,  0,  0 },
  { 0, -2,  2,  0,  0, -1,  1,  0,  4, -3,  3,  0,  0,  0,  0,  0 },
  { 0, -3,  1,  0, -1, -2,  0,  0,  3,  4,  2,  0,  0,  0,  0,  0 },
  { 0,  4,  0,  0, -2, -3, -1,  0,  2,  3,  1,  0,  0,  0,  0,  0 },
  { 0,  3, -1,  0, -3, -4, -2,  0,  1,  2,  0,  0,  0,  0,  0,  0 },
  { 0,  2, -2,  0,  4,  3, -3,  0,  0,  1, -1,  0,  0,  0,  0,  0 },
  { 0,  1, -3,  0,  3,  2, -4,  0, -1,  0, -2,  0,  0,  0,  0,  0 },
};

EpochReels::EpochReels()
{	
	ZeroMemory(reels, NUMEPOCHREELS * sizeof(reelstruct));
		
	for (int count = 0; count < NUMEPOCHREELS; count++) {
		if (count > 2) {
			reels[count].adjust = 14;
		} else {
			reels[count].adjust = 6;
		}		
	}

}

EpochReels::~EpochReels() {
}

void EpochReels::reset()
{	
}

void EpochReels::write(UINT8 x, UINT8 nbr)
{	
	if (nbr >= NUMEPOCHREELS) return;
	if (x >= 16) return;
	reelstruct*reel = &reels[nbr];
	int loop, Values[16];
			
	reel->now = x;

	if (x){

		//Calculate total cycles
		reel->TotalCnt[reel->writePoint] = reel->EndCnt[reel->writePoint];
		//Reset start and end
		reel->EndCnt[reel->writePoint] = 0;		
		//Store pattern value
		reel->PosVal[reel->writePoint] = x;
		//Increment Pointer
		reel->writePoint++;
		if (reel->writePoint == CYCLESTOAVERAGE){
			reel->writePoint = 0;
		}
		//Clear Values
		for (loop = 0; loop < 16; loop++){
			Values[loop] = 0;
		}		
		//Find total cycles on for each pattern
		for (loop = 0; loop < CYCLESTOAVERAGE; loop++){
			Values[reel->PosVal[loop]] += reel->TotalCnt[loop];
		}		
		//Find Value which was on for the most cycles
		int Highest = 0;
		int HighVal = 0;
		for (loop = 0; loop < 16; loop++){
			if (Values[loop] > Highest){
				Highest = Values[loop];
				HighVal = loop;
			}
		}
		//Set Reels based on Reel Value
		if ( HighVal != 0){
			if ( HighVal != reels[nbr].pat ) {
				state( HighVal, nbr);
				reels[nbr].pat = HighVal;
			}
		}

	}

}

void EpochReels::run(int cycles){

	int loop;

	for (loop = 0; loop < NUMEPOCHREELS; loop++){
		reelstruct * reel = &reels[loop];
		if (reel->now){ //disregard zero
			reel->EndCnt[reel->writePoint] += cycles;
		}
	}

}

void EpochReels::state(UINT8 x, UINT8 nbr)
{
if (nbr >= NUMEPOCHREELS) return;
if (x >= 16) return;
UINT8 phase;
UINT8 opto;
reelstruct * reel = &reels[nbr];

	if (reel->steps <= 0) return;
	phase = reel->pos % 8;
	reel->pos = (reel->pos + newjpm[phase][x] + reel->steps ) % reel->steps;
	reel->reelpos = ((reel->steps + reel->adjust - reel->pos ) % reel->steps);
	opto = 1 << nbr;
	if ((reel->pos > reel->startopto) && (reel->pos < reel->endopto )) {
		if ( reel->inverted ){
			optos |= opto;
		} else {
			optos &= ~opto;
		}
	} else {		
		if ( reel->inverted ){
			optos &= ~opto;
		} else {
			optos |= opto;
		}
	}

}
