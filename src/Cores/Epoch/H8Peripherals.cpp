// ###########################################################################
// #
// # h8periph - H8/3002 CPU Emulation Core - Onboard Peripherals
// # Copyright (C) 2002-2010 Tony Friery [DialTone]
// #
// # ALL RIGHTS RESERVED
// #
// # Based on a version originally by The_Author and DynaChicken
// # for the ZiNc emulator, subsequently bug-fixed and ported to
// # MAME by R Belmont
// #
// ###########################################################################
#include "stdafx.h"
#include "h83002.h"
#include <stdio.h>

FILE* DbugFile;
H83002::H83002() {

	h8_cyccnt = 0;
	h8_sleeping = 0;
	h8_suppress_irq_once = 0;

	ZeroMemory(IRQ_Map, sizeof(IRQ_Map));
	ZeroMemory(h8.regs, sizeof(h8.regs));
	ZeroMemory(h8.per_regs, sizeof(h8.per_regs));
	ZeroMemory(dStrings, sizeof(dStrings));

	dasmInitialize(NULL, 10);

	DbugFile = 0;
	fopen_s(&DbugFile, "DataLog.txt", "w");

}

H83002::~H83002() {
	if (DbugFile) fclose(DbugFile);
	dasmDestroyEntries();
}
// SCI Stuff
void H83002::h8_sci_tick(int cycles) {

	//Serial Control Register
	//Bit 0		CKE0	- Clock Enable	
	//Bit 1		CKE1	- Clock Enable					- Selects SCI Clock source
	//Bit 2		TEIE	- Transmit End Interrupt Enable - Enables or disables transmit end interrupts
	//Bit 3		MPIE	- Multi Processor Interrupt		- Enables or disables multiprocessor interrupt
	//Bit 4		RE		- Receive Enable				- Enables or disables the receiver
	//Bit 5		TE		- Transmit Enable				- Enables or disables the transmitter
	//Bit 6		RIE		- Receive Interrupt Enable  	- Enables or disables receive-data-full interrupts (RXI) and receive - error interrupts
	//Bit 7		TIE		- Transmit Interrupt Enable		- Enables or disables the transmit-data-empty interrupt

	//Serial Status Register	
	//Bit 0		MPBT	- Multiprocessor bit transfer	- Value of multiprocessor bit to be transmitted
	//Bit 1		MPB		- Received Multiprocessor Bit	- Read Only
	//Bit 2		TEND	- Transmit End Flag				- Read Only
	//Bit 3		PER		- Parity Error Flag				- *Only 0 can be written
	//Bit 4		FER		- Framing Error Flag			- *Only 0 can be written
	//Bit 5		ORER	- Overrun Error Flag			- *Only 0 can be written
	//Bit 6		RDRF	- Receive Data Register Full	- *Only 0 can be written
	//Bit 7		TDRE	- Transmit Data Empty Flag		- *Only 0 can be written

	//Serial Mode Register
	//Bit 0		CKS0	- Clock Source 0				- Together selects the prescaler value
	//Bit 1		CKS1	- Clock Source 1			  	- Together selects the prescaler value
	//Bit 2		MP		- Multiprocessor Mode			- Selects multiprocessor function
	//Bit 3		STOP	- Stop Bit Length				- Selects 1 or 2 stop bits 
	//Bit 4		O/E		- Parity Mode					- Selects Even or Odd parity
	//Bit 5		PE		- Parity Enable					- Selects whether a parity bit is added
	//Bit 6		CHR		- Character length				- Selects character length in asynchronous mode 0 = 8bit data 1 = 7bit data. Synchronous mode is always 8 bit
	//Bit 7		C/A		- Communication mode			- Selects asynchronous or synchronous mode

	UINT32 pscaleAmt = 0;

	//SCI Device 0
	if (h8.per_regs[SCR0] & TE)//Transmit Enable
	{
		if ((h8.per_regs[SSR0] & TEND) == 0) //Transmitting Data
		{
			//Prescale Clock
			h8.SCIPrescale0 += cycles;
			switch (h8.per_regs[SMR0] & 0x3) {
			case 0: //No Prescaler
				pscaleAmt = h8.SCIPrescale0;
				h8.SCIPrescale0 = 0;
				break;
			case 1: //Clock / 4
				if (h8.SCIPrescale0 & 0xfffc) {
					pscaleAmt = (h8.SCIPrescale0 >> 2); // divide by 4
					h8.SCIPrescale0 &= 0x3;
				}
				break;
			case 2: //Clock / 16
				if (h8.SCIPrescale0 & 0xfff0) {
					pscaleAmt = (h8.SCIPrescale0 >> 4); // divide by 16
					h8.SCIPrescale0 &= 0xf;
				}
				break;
			case 3: //Clock / 64
				if (h8.SCIPrescale0 & 0xffc0) {
					pscaleAmt = (h8.SCIPrescale0 >> 6); // divide by 64
					h8.SCIPrescale0 &= 0x3f;
				}
				break;
			}

			if (pscaleAmt) {
				h8.SCIClock0 += pscaleAmt;
				if (h8.SCIClock0 >= h8.per_regs[BRR0]) { // bit rate register
					h8.SCIClock0 = 0;

					h8.SCIBitPoint0++;
					if (h8.SCIBitPoint0 > h8.SCIBitNum0) {
						//Finished Sending Data

						h8.SCIBitPoint0 = 0; //Reset bit pointer

						if (!(h8.per_regs[SSR0] & TDRE)) { //If TDRE bit is high, no more data to send, this must be zero to send more data
							//More Data to Send 					


							//Calculate num bits to send
							if ((!(h8.per_regs[SMR0] & CA)) && (h8.per_regs[SMR0] & CHR))
							{
								//Synchronous mode & 7 bit CHR length
								h8.SCIBitNum0 = 7;
							}
							else {
								h8.SCIBitNum0 = 8;
							}
							//Add 1 start bit
							h8.SCIBitNum0++;

							if (h8.per_regs[SMR0] & PE) {
								//Add a parity bit
								h8.SCIBitNum0++;
							}

							if (h8.per_regs[SMR0] & STOP) {
								//Add 2 stop bits
								h8.SCIBitNum0 += 2;
							}
							else {
								//Add 1 stop bit
								h8.SCIBitNum0++;
							}

							//Grab the data from the Transmit Data Register and store it in the Transmit shift register
							UINT8 Sent = h8.per_regs[STSR0];//Save for debug

							//Acknowledge Sent Byte
							h8.per_regs[RDR0] = 0x06; 

							//Set the flag to show RDR0 now full
							h8.per_regs[SSR0] |= RDRF;

							h8.per_regs[STSR0] = h8.per_regs[TDR0];
							
							//Set the flag to show TDR0 now empty
							h8.per_regs[SSR0] |= TDRE;

							if (DbugFile) fprintf(DbugFile, "SCI Char TDR0 > STSR0 To Send: %x, %c\n", h8.per_regs[STSR0], h8.per_regs[STSR0]);
							//Assemble the bit buffer here if you really need to send it somewhere, otherwise cheat and use the value in h8.per_regs[STSR0] as the data byte.
							//Or as we do here, do nothing. this is just so the timing works for interrupts
							
							fEDC.Write(Sent);

							if (DbugFile) fprintf(DbugFile, "SCI Send Complete Restarting - TDRE High. Char Sent: %x %c\n", Sent, Sent);

						}
						else {
							//No more data to send

							//Acknowledge Sent Byte
							h8.per_regs[RDR0] = 0x06;

							//Set the flag to show RDR0 now full
							h8.per_regs[SSR0] |= RDRF;

							//Set the Transmit End Flag
							h8.per_regs[SSR0] |= TEND;


							fEDC.Write(h8.per_regs[STSR0]);

							if (DbugFile) fprintf(DbugFile, "SCI Send Complete Ending - TEND High. Char Sent: %x %c\n", h8.per_regs[STSR0], h8.per_regs[STSR0]);
						}
					}
					else {
						//Send the next bit
						//UINT8 nextBit = (h8.SCIBitBuffer0 & (1 << h8.SCIBitPoint0));
						//Do something with the bits being sent here, Epoch doesn't need it so nothing to do.
					}

				}
			}
		}
		else {
			//Not Transmitting atm

			if (!(h8.per_regs[SSR0] & TDRE)) { //If TDRE bit is low, data to send
				//Data to Send 					

				//Clear the Transmit End Flag
				h8.per_regs[SSR0] &= ~TEND;

				//Calculate num bits to send
				if ((!(h8.per_regs[SMR0] & CA)) && (h8.per_regs[SMR0] & CHR))
				{
					//Synchronous mode & 7 bit CHR length
					h8.SCIBitNum0 = 7;
				}
				else {
					h8.SCIBitNum0 = 8;
				}
				//Add 1 start bit
				h8.SCIBitNum0++;

				if (h8.per_regs[SMR0] & PE) {
					//Add a parity bit
					h8.SCIBitNum0++;
				}

				if (h8.per_regs[SMR0] & STOP) {
					//Add 2 stop bits
					h8.SCIBitNum0 += 2;
				}
				else {
					//Add 1 stop bit
					h8.SCIBitNum0++;
				}

				//Grab the data from the Transmit Data Register and store it in the Transmit shift register
				h8.per_regs[STSR0] = h8.per_regs[TDR0];

				if (DbugFile) fprintf(DbugFile, "SCI Char TDR0 > STSR0 To Send: %x, %c - TEND Low\n", h8.per_regs[STSR0], h8.per_regs[STSR0]);
				//Assemble the bit buffer here if you really need to send it somewhere, otherwise cheat and use the value in h8.per_regs[STSR0] as the data byte.
				//Or as we do here, do nothing. this is just so the timing works for interrupts

				//Set the flag to show TDRE now empty
				h8.per_regs[SSR0] |= TDRE;
				


			}
			else {
				//No data in TDR

			}
		}
	}
	else {
		//Transmitting Disabled		
		h8.per_regs[SSR0] |= (TDRE);
	}


	//SCI Device 1
	pscaleAmt = 0;

	if (h8.per_regs[SCR1] & TE)//Transmit Enable
	{
		if ((h8.per_regs[SSR1] & TEND) == 0) //Transmit Data
		{
			//Prescale Clock
			h8.SCIPrescale1 += cycles;
			switch (h8.per_regs[SMR1] & 0x3) {
			case 0: //No Prescaler
				pscaleAmt = h8.SCIPrescale1;
				h8.SCIPrescale1 = 0;
				break;
			case 1: //Clock / 4
				if (h8.SCIPrescale1 & 0xfffc) {
					pscaleAmt = (h8.SCIPrescale1 >> 2); // divide by 4
					h8.SCIPrescale1 &= 0x3;
				}
				break;
			case 2: //Clock / 16
				if (h8.SCIPrescale1 & 0xfff0) {
					pscaleAmt = (h8.SCIPrescale1 >> 4); // divide by 16
					h8.SCIPrescale1 &= 0xf;
				}
				break;
			case 3: //Clock / 64
				if (h8.SCIPrescale1 & 0xffc0) {
					pscaleAmt = (h8.SCIPrescale1 >> 6); // divide by 64
					h8.SCIPrescale1 &= 0x3f;
				}
				break;
			}

			if (pscaleAmt) {
				h8.SCIClock1 += pscaleAmt;
				if (h8.SCIClock1 >= h8.per_regs[BRR1]) { // bit rate register
					h8.SCIClock1 = 0;

					h8.SCIBitPoint1++;
					if (h8.SCIBitPoint1 > h8.SCIBitNum1) {
						//Finished Sending Data

						h8.SCIBitPoint1 = 0; //Reset bit pointer

						if (!(h8.per_regs[SSR1] & TDRE)) { //If TDRE bit is high, no more data to send, this must be zero to send more data
							//Data to Send 					


							//Calculate num bits to send
							if ((!(h8.per_regs[SMR1] & CA)) && (h8.per_regs[SMR1] & CHR))
							{
								//Synchronous mode & 7 bit CHR length
								h8.SCIBitNum1 = 7;
							}
							else {
								h8.SCIBitNum1 = 8;
							}
							//Add 1 start bit
							h8.SCIBitNum1++;

							if (h8.per_regs[SMR1] & PE) {
								//Add a parity bit
								h8.SCIBitNum1++;
							}

							if (h8.per_regs[SMR1] & STOP) {
								//Add 2 stop bits
								h8.SCIBitNum1 += 2;
							}
							else {
								//Add 1 stop bit
								h8.SCIBitNum1++;
							}

							//Grab the data from the Transmit Data Register and store it in the Transmit shift register
							h8.per_regs[STSR1] = h8.per_regs[TDR1];
							if (DbugFile) fprintf(DbugFile, "SCI Char Out %x, %c\n", h8.per_regs[STSR1], h8.per_regs[STSR1]);
							//Assemble the bit buffer here if you really need to send it somewhere, otherwise cheat and use the value in h8.SCITSR1 as the data byte.
							//Or as we do here, do nothing. this is just so the timing works for interrupts

							//Set the flag to show TDRE now empty
							h8.per_regs[SSR1] |= TDRE;

							
						}
						else {
							//No more data to send
							

							//Set the Transmit End Flag
							h8.per_regs[SSR1] |= TEND;

						}
					}
					else {
						//Send the next bit
						//UINT8 nextBit = (h8.SCIBitBuffer1 & (1 << h8.SCIBitPoint1));
						//Do something with the bits being sent here, Epoch doesn't need it so nothing to do.
					}

				}
			}
		}
		else {
			//Not Transmitting atm

			if (!(h8.per_regs[SSR1] & TDRE)) { //If TDRE bit is low, data to send
				//Data to Send 					

				//Clear the Transmit End Flag
				h8.per_regs[SSR1] &= ~TEND;

				//Calculate num bits to send
				if ((!(h8.per_regs[SMR1] & CA)) && (h8.per_regs[SMR1] & CHR))
				{
					//Synchronous mode & 7 bit CHR length
					h8.SCIBitNum1 = 7;
				}
				else {
					h8.SCIBitNum1 = 8;
				}
				//Add 1 start bit
				h8.SCIBitNum1++;

				if (h8.per_regs[SMR1] & PE) {
					//Add a parity bit
					h8.SCIBitNum1++;
				}

				if (h8.per_regs[SMR1] & STOP) {
					//Add 2 stop bits
					h8.SCIBitNum1 += 2;
				}
				else {
					//Add 1 stop bit
					h8.SCIBitNum1++;
				}

				//Grab the data from the Transmit Data Register and store it in the Transmit shift register
				h8.per_regs[STSR1] = h8.per_regs[TDR1];
				if (DbugFile) fprintf(DbugFile, "SCI Char Out %x, %c\n", h8.per_regs[STSR1], h8.per_regs[STSR1]);
				//Assemble the bit buffer here if you really need to send it somewhere, otherwise cheat and use the value in h8.SCITSR1 as the data byte.
				//Or as we do here, do nothing. this is just so the timing works for interrupts

				//Set the flag to show TDRE now empty
				h8.per_regs[SSR1] |= TDRE;


			}
			else {
				//No more data to send

			}
		}
	}
	else {
		h8.per_regs[SSR1] |= (TDRE);
	}

	
	
}
UINT8 H83002::h8_sci_read8(UINT8 reg){

	UINT8 ret = 0;

	/*
	switch (reg) {
	case 0xb2:	// 0xb2 Serial port A Control/Status
		ret = 0x84;
		break;
	case 0xb4: // serial port A status
		ret = h8.per_regs[reg];
		ret |= 0xc4;		// transmit finished, receive ready, no errors
		break;
	case 0xb5: // serial port A receive
		ret = io_read_byte_8(H8_SERIAL_A);
		break;
	case 0xba:	// serial port b control/status
		ret = 0x84;
		break;
	case 0xbc: // serial port B status
		ret = h8.per_regs[reg];
		ret |= 0xc4;		// transmit finished, receive ready, no errors
		break;
	case 0xbd: // serial port B receive
		ret = io_read_byte_8(H8_SERIAL_B);
		break;
	}
	*/
	ret = h8.per_regs[reg];

	switch (reg){	
	case SSR0: //Serial Status Register Chn 0 (0xb4)
		//Bit 0			- Multiprocessor bit transfer	- Value of multiprocessor bit to be transmitted
		//Bit 1			- Received Multiprocessor Bit	- Read Only
		//Bit 2			- Transmit End Flag				- Read Only
		//Bit 3			- Parity Error Flag				- *Only 0 can be written
		//Bit 4			- Framing Error Flag			- *Only 0 can be written
		//Bit 5			- Overrun Error Flag			- *Only 0 can be written
		//Bit 6			- Receive Data Register Full	- *Only 0 can be written
		//Bit 7			- Transmit Data Empty Flag		- *Only 0 can be written		
		h8.h8PREVSSR0 = ret;
		
		if (DbugFile) {
			fprintf(DbugFile, "** SCI Read SSR0 ** :%x, %i\n", h8.per_regs[SSR0], h8.per_regs[SSR0]);
			fprintf(DbugFile, "Send Multiprocessor Bit %x\n", (h8.per_regs[SSR0] & 0x1));
			fprintf(DbugFile, "Receive Multiprocessor Bit %x\n", (h8.per_regs[SSR0] & 0x2));
			fprintf(DbugFile, "Transmit End Flag %x\n", (h8.per_regs[SSR0] & 0x4));
			fprintf(DbugFile, "Parity Error Flag %x\n", (h8.per_regs[SSR0] & 0x8));
			fprintf(DbugFile, "Framing Error Flag %x\n", (h8.per_regs[SSR0] & 0x10));
			fprintf(DbugFile, "Overrun Error Flag %x\n", (h8.per_regs[SSR0] & 0x20));
			fprintf(DbugFile, "Receive Data Full %x\n", (h8.per_regs[SSR0] & 0x40));
			fprintf(DbugFile, "Transmit Data Empty %x\n\n", (h8.per_regs[SSR0] & 0x80));
		}

		break;	
	case SSR1: //Serial Status Register Chn 1 (0xbc)
		//Bit 0			- Multiprocessor bit transfer	- Value of multiprocessor bit to be transmitted
		//Bit 1			- Received Multiprocessor Bit	- Read Only
		//Bit 2			- Transmit End Flag				- Read Only
		//Bit 3			- Parity Error Flag				- *Only 0 can be written
		//Bit 4			- Framing Error Flag			- *Only 0 can be written
		//Bit 5			- Overrun Error Flag			- *Only 0 can be written
		//Bit 6			- Receive Data Register Full	- *Only 0 can be written
		//Bit 7			- Transmit Data Empty Flag		- *Only 0 can be written
		h8.h8PREVSSR1 = ret;
		if (DbugFile) {
			fprintf(DbugFile, "SCI Read SSR1 %x, %i\n\n", h8.per_regs[SSR1], h8.per_regs[SSR1]);
		}
		break;
	}
	return ret;
}



void H83002::h8_sci_write8(UINT8 reg, UINT8 val){
	
	switch (reg){	
	case SMR0: //Serial Mode Register Chn 0 (0xb0)		
		//Bit 0		CKS0	- Clock Source 0				- Together selects the prescaler value
		//Bit 1		CKS1	- Clock Source 1			  	- Together selects the prescaler value
		//Bit 2		MP		- Multiprocessor Mode			- Selects multiprocessor function
		//Bit 3		STOP	- Stop Bit Length				- Selects 1 or 2 stop bits 
		//Bit 4		O/E		- Parity Mode					- Selects Even or Odd parity
		//Bit 5		PE		- Parity Enable					- Selects whether a parity bit is added
		//Bit 6		CHR		- Character length				- Selects character length in asynchronous mode 0 = 8bit data 1 = 7bit data. Synchronous mode is always 8 bit
		//Bit 7		C/A		- Communication mode			- Selects asynchronous or synchronous mode
		h8.per_regs[SMR0] = val;	
		if (DbugFile) {
			fprintf(DbugFile, "** SCI Write SMR0 **:%x, %i\n", h8.per_regs[SMR0], h8.per_regs[SMR0]);
			fprintf(DbugFile, "Clock Prescaler %x\n", (h8.per_regs[SMR0] & 0x3));			
			fprintf(DbugFile, "Multiprocessor Mode %x\n", (h8.per_regs[SMR0] & 0x4));
			fprintf(DbugFile, "Stop Bit Length %x\n", (h8.per_regs[SMR0] & 0x8));
			fprintf(DbugFile, "Parity Mode %x\n", (h8.per_regs[SMR0] & 0x10));
			fprintf(DbugFile, "Parity Enable %x\n", (h8.per_regs[SMR0] & 0x20));
			fprintf(DbugFile, "Character Length %x\n", (h8.per_regs[SMR0] & 0x40));
			fprintf(DbugFile, "Communication Mode %x\n\n", (h8.per_regs[SMR0] & 0x80));
		}
		break;
	case BRR0:
		//Bit Rate Register  Chn 0 (0xb1)
		h8.per_regs[BRR0] = val;
		if (DbugFile) fprintf(DbugFile, "** SCI Write BRR0 **: %x, %i\n\n", h8.per_regs[BRR0], h8.per_regs[BRR0]);
		break;
	case SCR0: //Serial Control Register Chn 0 (0xb2)		
		//Bit 0		CKE0	- Clock Enable					- Together selects the SCI Clock source
		//Bit 1		CKE1	- Clock Enable					- Together selects the SCI Clock source
		//Bit 2		TEIE	- Transmit End Interrupt Enable - Enables or disables transmit end interrupts
		//Bit 3		MPIE	- Multi Processor Interrupt		- Enables or disables multiprocessor interrupt
		//Bit 4		RE		- Receive Enable				- Enables or disables the receiver
		//Bit 5		TE		- Transmit Enable				- Enables or disables the transmitter
		//Bit 6		RIE		- Receive Interrupt Enable  	- Enables or disables receive-data-full interrupts (RXI) and receive - error interrupts
		//Bit 7		TIE		- Transmit Interrupt Enable		- Enables or disables the transmit-data-empty interrupt
		h8.per_regs[SCR0] = val;
		if (DbugFile) {
			fprintf(DbugFile, "** SCI Write SCR0 **:%x, %i\n", h8.per_regs[SCR0], h8.per_regs[SCR0]);			
			fprintf(DbugFile, "Clock Enables %x\n", (h8.per_regs[SCR0] & 0x3));			
			fprintf(DbugFile, "Transmit End Interrupt Enable %x\n", (h8.per_regs[SCR0] & 0x4));
			fprintf(DbugFile, "Multi Processor Interrupt Enable %x\n", (h8.per_regs[SCR0] & 0x8));
			fprintf(DbugFile, "Receive Enable %x\n", (h8.per_regs[SCR0] & 0x10));
			fprintf(DbugFile, "Transmit Enable %x\n", (h8.per_regs[SCR0] & 0x20));
			fprintf(DbugFile, "Receive Interrupt Enable %x\n", (h8.per_regs[SCR0] & 0x40));
			fprintf(DbugFile, "Transmit Interrupt Enable %x\n\n", (h8.per_regs[SCR0] & 0x80));
		}
		break;
	case TDR0: //Transmit Data Register Chn 0 (0xb3)
		h8.per_regs[TDR0] = val;		
		if (DbugFile) {
			fprintf(DbugFile, "** SCI Write TDR0 **:%x, %i\n\n", h8.per_regs[TDR0], h8.per_regs[TDR0]);			
		}
		break;
	case SSR0: //Serial Status Register 0 (0xb4)
		//Bit 0		MPBT	- Multiprocessor bit transfer	- Value of multiprocessor bit to be transmitted
		//Bit 1		MPB		- Received Multiprocessor Bit	- Read Only
		//Bit 2		TEND	- Transmit End Flag				- Read Only
		//Bit 3		PER		- Parity Error Flag				- *Only 0 can be written
		//Bit 4		FER		- Framing Error Flag			- *Only 0 can be written
		//Bit 5		ORER	- Overrun Error Flag			- *Only 0 can be written
		//Bit 6		RDRF	- Receive Data Register Full	- *Only 0 can be written
		//Bit 7		TDRE	- Transmit Data Empty Flag		- *Only 0 can be written
		if ((h8.h8PREVSSR0 & TDRE) && ((val & TDRE) == 0)) {
			h8.per_regs[SSR0] &= ~(TDRE);
			if (DbugFile) fprintf(DbugFile, "SCI TEND 0 Low\n");
		}
		if ((h8.h8PREVSSR0 & RDRF) && ((val & RDRF) == 0))	h8.per_regs[SSR0] &= ~(RDRF);
		if ((h8.h8PREVSSR0 & ORER) && ((val & ORER) == 0))	h8.per_regs[SSR0] &= ~(ORER);
		if ((h8.h8PREVSSR0 & FER) && ((val & FER) == 0))	h8.per_regs[SSR0] &= ~(FER);
		if ((h8.h8PREVSSR0 & PER) && ((val & PER) == 0))	h8.per_regs[SSR0] &= ~(PER);
		if (DbugFile) {
			fprintf(DbugFile, "** SCI Write SSR0 **:%x, %i\n", h8.per_regs[SSR0], h8.per_regs[SSR0]);
			fprintf(DbugFile, "Send Multiprocessor Bit %x\n", (h8.per_regs[SSR0] & 0x1));
			fprintf(DbugFile, "Receive Multiprocessor Bit %x\n", (h8.per_regs[SSR0] & 0x2));
			fprintf(DbugFile, "Transmit End Flag %x\n", (h8.per_regs[SSR0] & 0x4));
			fprintf(DbugFile, "Parity Error Flag %x\n", (h8.per_regs[SSR0] & 0x8));
			fprintf(DbugFile, "Framing Error Flag %x\n", (h8.per_regs[SSR0] & 0x10));
			fprintf(DbugFile, "Overrun Error Flag %x\n", (h8.per_regs[SSR0] & 0x20));
			fprintf(DbugFile, "Receive Data Full %x\n", (h8.per_regs[SSR0] & 0x40));
			fprintf(DbugFile, "Transmit Data Empty %x\n\n", (h8.per_regs[SSR0] & 0x80));
		}
		break;	
	case SMR1: //Serial Mode Register Chn 1 (0xb8)
		//Bit 0		CKS0	- Clock Source 0				- Together selects the prescaler value
		//Bit 1		CKS1	- Clock Source 1			  	- Together selects the prescaler value
		//Bit 2		MP		- Multiprocessor Mode			- Selects multiprocessor function
		//Bit 3		STOP	- Stop Bit Length				- Selects 1 or 2 stop bits 
		//Bit 4		O/E		- Parity Mode					- Selects Even or Odd parity
		//Bit 5		PE		- Parity Enable					- Selects whether a parity bit is added
		//Bit 6		CHR		- Character length				- Selects character length in asynchronous mode 0 = 8bit data 1 = 7bit data. Synchronous mode is always 8 bit
		//Bit 7		C/A		- Communication mode			- Selects asynchronous or synchronous mode
		h8.per_regs[SMR1] = val;
		if (DbugFile) fprintf(DbugFile, "SCI Write SMR1 %x, %i\n", h8.per_regs[SMR1], h8.per_regs[SMR1]);
		break;
	case BRR1:
		//Bit Rate Register  Chn 1 (0xb9)
		h8.per_regs[BRR1] = val;
		if (DbugFile) fprintf(DbugFile, "SCI Write BRR1 %x, %i\n", h8.per_regs[BRR1], h8.per_regs[BRR1]);
		break;
	case SCR1: //Serial Control Register Chn 0 (0xba)
		//Bit 0		CKE0	- Clock Enable	
		//Bit 1		CKE1	- Clock Enable					- Selects SCI Clock source
		//Bit 2		TEIE	- Transmit End Interrupt Enable - Enables or disables transmit end interrupts
		//Bit 3		MPIE	- Multi Processor Interrupt		- Enables or disables multiprocessor interrupt
		//Bit 4		RE		- Receive Enable				- Enables or disables the receiver
		//Bit 5		TE		- Transmit Enable				- Enables or disables the transmitter
		//Bit 6		RIE		- Receive Interrupt Enable  	- Enables or disables receive-data-full interrupts (RXI) and receive - error interrupts
		//Bit 7		TIE		- Transmit Interrupt Enable		- Enables or disables the transmit-data-empty interrupt
		if (DbugFile) fprintf(DbugFile, "SCI Write SCR1 %x, %i\n", h8.per_regs[SCR1], h8.per_regs[SCR1]);
		h8.per_regs[SCR1] = val;
		break;
	case TDR1: //Transmit Data Register Chn 1 (0xbb)
		//Put Data in TDR 1
		h8.per_regs[TDR1] = val;	
		if (DbugFile) fprintf(DbugFile, "SCI Write TDR1 %x, %i\n", h8.per_regs[TDR1], h8.per_regs[TDR1]);
		break;
	case SSR1: //Serial Status Register	1 (0xbc)
		//Bit 0		MPBT	- Multiprocessor bit transfer	- Value of multiprocessor bit to be transmitted
		//Bit 1		MPB		- Received Multiprocessor Bit	- Read Only
		//Bit 2		TEND	- Transmit End Flag				- Read Only
		//Bit 3		PER		- Parity Error Flag				- *Only 0 can be written
		//Bit 4		FER		- Framing Error Flag			- *Only 0 can be written
		//Bit 5		ORER	- Overrun Error Flag			- *Only 0 can be written
		//Bit 6		RDRF	- Receive Data Register Full	- *Only 0 can be written
		//Bit 7		TDRE	- Transmit Data Empty Flag		- *Only 0 can be written
		
		if ((h8.h8PREVSSR1 & TDRE) && ((val & TDRE) == 0)) h8.per_regs[SSR1] &= ~(TDRE);
		if ((h8.h8PREVSSR1 & RDRF) && ((val & RDRF) == 0)) h8.per_regs[SSR1] &= ~(RDRF);
		if ((h8.h8PREVSSR1 & ORER) && ((val & ORER) == 0)) h8.per_regs[SSR1] &= ~(ORER);
		if ((h8.h8PREVSSR1 & FER ) && ((val & FER ) == 0)) h8.per_regs[SSR1] &= ~(FER);
		if ((h8.h8PREVSSR1 & PER)  && ((val & PER ) == 0)) h8.per_regs[SSR1] &= ~(PER);
		if (DbugFile) fprintf(DbugFile, "SCI Write SSR1 %x, %i\n", h8.per_regs[SSR1], h8.per_regs[SSR1]);
		break;
	default: 
		h8.per_regs[reg]= val;
		break;	
	}

}
void H83002::h8_sci_reset(void){
	
	h8.per_regs[BRR0] = 0xff;	//0xb1	
	h8.per_regs[TDR0] = 0xff;	//0xb3
	h8.per_regs[SSR0] = (TDRE | TEND);	//0xb4		
		
	h8.per_regs[BRR1] = 0xff;	//0xb9	
	h8.per_regs[TDR1] = 0xff;	//0xbb
	h8.per_regs[SSR1] = (TDRE | TEND);	//0xbc	
	//others already zero'd

	//Variables to run the SCI devices
	h8.SCIPrescale0 = 0x00;
	h8.SCIPrescale1 = 0x00;

	h8.SCIClock0 = 0x00;
	h8.SCIClock1 = 0x00;

	h8.SCIBitBuffer0 = 0x00;
	h8.SCIBitBuffer1 = 0x00;

	h8.SCIBitPoint0 = 0x00;
	h8.SCIBitPoint1 = 0x00;

	h8.SCIBitNum0 = 0x00;
	h8.SCIBitNum1 = 0x00;

}
///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	H83002::h8_itu_tick
//
//  PURPOSE:	Cause the ITU device to be clocked
//
//  INPUTS:		int					Number of cycles to clock the device by
//
//  OUTPUT:		none
//
///////////////////////////////////////////////////////////////////////
void H83002::h8_itu_tick(int cycles){	
	
	UINT32 pscaleAmt, Timer, GenA, GenB, oldTimer;

	//Timer 0
	if (h8.per_regs[TSTR] & 1)
	{
		pscaleAmt = 0;
		
		// This timer is running
		h8.h8TPrescale0 += cycles;
		switch (h8.per_regs[TCR0] & 0x3) {
		case 0: //No Prescaler
			pscaleAmt = h8.h8TPrescale0;
			h8.h8TPrescale0 = 0;
			break;
		case 1: //Clock / 2
			if (h8.h8TPrescale0 & 0xfffe) {
				pscaleAmt = (h8.h8TPrescale0 >> 1);
				h8.h8TPrescale0 &= 0x1;
			}
			break;
		case 2: //Clock / 4
			if (h8.h8TPrescale0 & 0xfffc) {
				pscaleAmt = (h8.h8TPrescale0 >> 2);
				h8.h8TPrescale0 &= 0x3;
			}
			break;
		case 3: //Clock / 8
			if (h8.h8TPrescale0 & 0xfff8) {
				pscaleAmt = (h8.h8TPrescale0 >> 3);
				h8.h8TPrescale0 &= 0x7;
			}
			break;
		}
		//Timer Unit

		if (pscaleAmt)
		{
			//Get Timer Value in 16 bit
			Timer = ((h8.per_regs[TCNT0H] << 8) + h8.per_regs[TCNT0L]);
			//Keep Old Timer Value
			oldTimer = Timer;
			//Add prescaled cycles to Timer
			Timer += pscaleAmt;
			//Save Timer in Registers
			h8.per_regs[TCNT0H] = (((UINT16)Timer >> 8) & 0xff);
			h8.per_regs[TCNT0L] = ((UINT16)Timer & 0xff);

			//Counter Clear options
			switch (h8.per_regs[TCR0] & 0x60) {
			case 0x20: //Counter Cleared by GRA Match

				GenA = ((h8.per_regs[GRA0H] << 8) + h8.per_regs[GRA0L]);

				if (((oldTimer <= GenA) && (Timer >= GenA)) || (Timer > 0xffff && ((Timer & 0xffff) >= GenA))) {
					//Clear counter (account for extra cycles)
					h8.per_regs[TCNT0H] = (((Timer - GenA) >> 8) & 0xff);
					h8.per_regs[TCNT0L] = ((Timer - GenA) & 0xff);
					//Set Status Reg Bit
					h8.per_regs[TSR0] |= 1;
				}
				break;
			case 0x40: //Counter Cleared by GRB Match

				GenB = ((h8.per_regs[GRB0H] << 8) + h8.per_regs[GRB0L]);

				if (((oldTimer <= GenB) && (Timer >= GenB)) || (Timer > 0xffff && ((Timer & 0xffff) >= GenB))) {
					//Clear counter (account for extra cycles)				
					h8.per_regs[TCNT0H] = (((Timer - GenB) >> 8) & 0xff);
					h8.per_regs[TCNT0L] = ((Timer - GenB) & 0xff);
					//Set Status Reg Bit
					h8.per_regs[TSR0] |= 2;
				}
				break;
			case 0x60: // Synchronous clear
				// The H8/300H ITU synchronous-clear mode clears counters through
				// inter-channel synchronisation.  Epoch software observed so far only
				// needs this mode not to become a fatal emulator error; keep the
				// counter running and report overflow normally below.
				if (Timer > 0xffff) {
					h8.per_regs[TSR0] |= 4;
				}
				break;
			case 0x0: //Counter not cleared

				if (Timer > 0xffff) {
					// Timer overflow

					//Set Status Reg Bit
					h8.per_regs[TSR0] |= 4;
				}
				break;
			}
		}
	}

	//Timer 1
	if (h8.per_regs[TSTR] & 2)
	{
		pscaleAmt = 0;
		// This timer is running
		h8.h8TPrescale1 += cycles;
		switch (h8.per_regs[TCR1] & 0x3) {
		case 0: //No Prescaler
			pscaleAmt = h8.h8TPrescale1;
			h8.h8TPrescale1 = 0;
			break;
		case 1: //Clock / 2
			if (h8.h8TPrescale1 & 0xfffe) {
				pscaleAmt = (h8.h8TPrescale1 >> 1);
				h8.h8TPrescale1 &= 0x1;
			}
			break;
		case 2: //Clock / 4
			if (h8.h8TPrescale1 & 0xfffc) {
				pscaleAmt = (h8.h8TPrescale1 >> 2);
				h8.h8TPrescale1 &= 0x3;
			}
			break;
		case 3: //Clock / 8
			if (h8.h8TPrescale1 & 0xfff8) {
				pscaleAmt = (h8.h8TPrescale1 >> 3);
				h8.h8TPrescale1 &= 0x7;
			}
			break;
		}

		if (pscaleAmt)
		{
			//Get Timer Value in 16 bit
			Timer = ((h8.per_regs[TCNT1H] << 8) + h8.per_regs[TCNT1L]);
			//Keep Old Timer Value
			oldTimer = Timer;
			//Add prescaled cycles to Timer
			Timer += pscaleAmt;
			//Save Timer in Registers
			h8.per_regs[TCNT1H] = (((UINT16)Timer >> 8) & 0xff);
			h8.per_regs[TCNT1L] = ((UINT16)Timer & 0xff);

			//Counter Clear options
			switch (h8.per_regs[TCR1] & 0x60) {
			case 0x20: //Counter Cleared by GRA Match

				GenA = ((h8.per_regs[GRA1H] << 8) + h8.per_regs[GRA1L]);

				if (((oldTimer <= GenA) && (Timer >= GenA)) || (Timer > 0xffff && ((Timer & 0xffff) >= GenA))) {
					//Clear counter (account for extra cycles)
					h8.per_regs[TCNT1H] = (((Timer - GenA) >> 8) & 0xff);
					h8.per_regs[TCNT1L] = ((Timer - GenA) & 0xff);
					//Set Status Reg Bit
					h8.per_regs[TSR1] |= 1;
				}
				break;
			case 0x40: //Counter Cleared by GRB Match

				GenB = ((h8.per_regs[GRB1H] << 8) + h8.per_regs[GRB1L]);

				if (((oldTimer <= GenB) && (Timer >= GenB)) || (Timer > 0xffff && ((Timer & 0xffff) >= GenB))) {
					//Clear counter (account for extra cycles)				
					h8.per_regs[TCNT1H] = (((Timer - GenB) >> 8) & 0xff);
					h8.per_regs[TCNT1L] = ((Timer - GenB) & 0xff);
					//Set Status Reg Bit
					h8.per_regs[TSR1] |= 2;
				}
				break;
			case 0x60: // Synchronous clear
				// Conservative implementation: do not raise an emulator error for
				// the valid H8 ITU synchronous-clear mode.  Treat it as running
				// without local clear until full inter-channel sync is required.
				if (Timer > 0xffff) {
					h8.per_regs[TSR1] |= 4;
				}
				break;
			case 0x0: //Counter not cleared

				if (Timer > 0xffff) {
					// Timer overflow

					//Set Status Reg Bit
					h8.per_regs[TSR1] |= 4;
				}

				break;
			}
		}
	}

	//Timer 2
	if (h8.per_regs[TSTR] & 4)
	{
		pscaleAmt = 0;
		// This timer is running
		h8.h8TPrescale2 += cycles;
		switch (h8.per_regs[TCR2] & 0x3) {
		case 0: //No Prescaler
			pscaleAmt = h8.h8TPrescale2;
			h8.h8TPrescale2 = 0;
			break;
		case 1: //Clock / 2
			if (h8.h8TPrescale2 & 0xfffe) {
				pscaleAmt = (h8.h8TPrescale2 >> 1);
				h8.h8TPrescale2 &= 0x1;
			}
			break;
		case 2: //Clock / 4
			if (h8.h8TPrescale2 & 0xfffc) {
				pscaleAmt = (h8.h8TPrescale2 >> 2);
				h8.h8TPrescale2 &= 0x3;
			}
			break;
		case 3: //Clock / 8
			if (h8.h8TPrescale2 & 0xfff8) {
				pscaleAmt = (h8.h8TPrescale2 >> 3);
				h8.h8TPrescale2 &= 0x7;
			}
			break;
		}

		if (pscaleAmt)
		{
			//Get Timer Value in 16 bit
			Timer = ((h8.per_regs[TCNT2H] << 8) + h8.per_regs[TCNT2L]);
			//Keep Old Timer Value
			oldTimer = Timer;
			//Add prescaled cycles to Timer
			Timer += pscaleAmt;
			//Save Timer in Registers
			h8.per_regs[TCNT2H] = (((UINT16)Timer >> 8) & 0xff);
			h8.per_regs[TCNT2L] = ((UINT16)Timer & 0xff);

			//Counter Clear options
			switch (h8.per_regs[TCR2] & 0x60) {
			case 0x20: //Counter Cleared by GRA Match

				GenA = ((h8.per_regs[GRA2H] << 8) + h8.per_regs[GRA2L]);

				if (((oldTimer <= GenA) && (Timer >= GenA)) || (Timer > 0xffff && ((Timer & 0xffff) >= GenA))) {
					//Clear counter (account for extra cycles)
					h8.per_regs[TCNT2H] = (((Timer - GenA) >> 8) & 0xff);
					h8.per_regs[TCNT2L] = ((Timer - GenA) & 0xff);
					//Set Status Reg Bit
					h8.per_regs[TSR2] |= 1;
				}
				break;
			case 0x40: //Counter Cleared by GRB Match

				GenB = ((h8.per_regs[GRB2H] << 8) + h8.per_regs[GRB2L]);

				if (((oldTimer <= GenB) && (Timer >= GenB)) || (Timer > 0xffff && ((Timer & 0xffff) >= GenB))) {
					//Clear counter (account for extra cycles)				
					h8.per_regs[TCNT2H] = (((Timer - GenB) >> 8) & 0xff);
					h8.per_regs[TCNT2L] = ((Timer - GenB) & 0xff);
					//Set Status Reg Bit
					h8.per_regs[TSR2] |= 2;
				}
				break;
			case 0x60: // Synchronous clear
				// Conservative implementation: do not raise an emulator error for
				// the valid H8 ITU synchronous-clear mode.  Treat it as running
				// without local clear until full inter-channel sync is required.
				if (Timer > 0xffff) {
					h8.per_regs[TSR2] |= 4;
				}
				break;
			case 0x0: //Counter not cleared

				if (Timer > 0xffff) {
					// Timer overflow

					//Set Status Reg Bit
					h8.per_regs[TSR2] |= 4;
				}
				break;
			}
		}
	}

	//Timer 3
	if (h8.per_regs[TSTR] & 8)
	{
		pscaleAmt = 0;
		// This timer is running
		h8.h8TPrescale3 += cycles;
		switch (h8.per_regs[TCR3] & 0x3) {
		case 0: //No Prescaler
			pscaleAmt = h8.h8TPrescale3;
			h8.h8TPrescale3 = 0;
			break;
		case 1: //Clock / 2
			if (h8.h8TPrescale3 & 0xfffe) {
				pscaleAmt = (h8.h8TPrescale3 >> 1);
				h8.h8TPrescale3 &= 0x1;
			}
			break;
		case 2: //Clock / 4
			if (h8.h8TPrescale3 & 0xfffc) {
				pscaleAmt = (h8.h8TPrescale3 >> 2);
				h8.h8TPrescale3 &= 0x3;
			}
			break;
		case 3: //Clock / 8
			if (h8.h8TPrescale3 & 0xfff8) {
				pscaleAmt = (h8.h8TPrescale3 >> 3);
				h8.h8TPrescale3 &= 0x7;
			}
			break;
		}

		if (pscaleAmt)
		{
			//Get Timer Value in 16 bit
			Timer = ((h8.per_regs[TCNT3H] << 8) + h8.per_regs[TCNT3L]);
			//Keep Old Timer Value
			oldTimer = Timer;
			//Add prescaled cycles to Timer
			Timer += pscaleAmt;
			//Save Timer in Registers
			h8.per_regs[TCNT3H] = (((UINT16)Timer >> 8) & 0xff);
			h8.per_regs[TCNT3L] = ((UINT16)Timer & 0xff);

			//Counter Clear options
			switch (h8.per_regs[TCR3] & 0x60) {
			case 0x20: //Counter Cleared by GRA Match

				GenA = ((h8.per_regs[GRA3H] << 8) + h8.per_regs[GRA3L]);

				if (((oldTimer <= GenA) && (Timer >= GenA)) || (Timer > 0xffff && ((Timer & 0xffff) >= GenA))) {
					//Clear counter (account for extra cycles)
					h8.per_regs[TCNT3H] = (((Timer - GenA) >> 8) & 0xff);
					h8.per_regs[TCNT3L] = ((Timer - GenA) & 0xff);
					//Set Status Reg Bit
					h8.per_regs[TSR3] |= 1;
				}
				break;
			case 0x40: //Counter Cleared by GRB Match

				GenB = ((h8.per_regs[GRB3H] << 8) + h8.per_regs[GRB3L]);

				if (((oldTimer <= GenB) && (Timer >= GenB)) || (Timer > 0xffff && ((Timer & 0xffff) >= GenB))) {
					//Clear counter (account for extra cycles)				
					h8.per_regs[TCNT3H] = (((Timer - GenB) >> 8) & 0xff);
					h8.per_regs[TCNT3L] = ((Timer - GenB) & 0xff);
					//Set Status Reg Bit
					h8.per_regs[TSR3] |= 2;
				}
				break;
			case 0x60: // Synchronous clear
				// Conservative implementation: do not raise an emulator error for
				// the valid H8 ITU synchronous-clear mode.  Treat it as running
				// without local clear until full inter-channel sync is required.
				if (Timer > 0xffff) {
					h8.per_regs[TSR3] |= 4;
				}
				break;
			case 0x0: //Counter not cleared

				if (Timer > 0xffff) {
					// Timer overflow

					//Set Status Reg Bit
					h8.per_regs[TSR3] |= 4;				
				}
				break;
			}
		}
	}

	//Timer 4
	if (h8.per_regs[TSTR] & 16)
	{
		pscaleAmt = 0;
		// This timer is running
		h8.h8TPrescale4 += cycles;
		switch (h8.per_regs[TCR4] & 0x3) {
		case 0: //No Prescaler
			pscaleAmt = h8.h8TPrescale4;
			h8.h8TPrescale4 = 0;
			break;
		case 1: //Clock / 2
			if (h8.h8TPrescale4 & 0xfffe) {
				pscaleAmt = (h8.h8TPrescale4 >> 1);
				h8.h8TPrescale4 &= 0x1;
			}
			break;
		case 2: //Clock / 4
			if (h8.h8TPrescale4 & 0xfffc) {
				pscaleAmt = (h8.h8TPrescale4 >> 2);
				h8.h8TPrescale4 &= 0x3;
			}
			break;
		case 3: //Clock / 8
			if (h8.h8TPrescale4 & 0xfff8) {
				pscaleAmt = (h8.h8TPrescale4 >> 3);
				h8.h8TPrescale4 &= 0x7;
			}
			break;
		}

		if (pscaleAmt)
		{
			//Get Timer Value in 16 bit
			Timer = ((h8.per_regs[TCNT4H] << 8) + h8.per_regs[TCNT4L]);			
			//Keep Old Timer Value
			oldTimer = Timer;
			//Add prescaled cycles to Timer
			Timer += pscaleAmt;
			//Save Timer in Registers
			h8.per_regs[TCNT4H] = (((UINT16)Timer >> 8) & 0xff);
			h8.per_regs[TCNT4L] = ((UINT16)Timer & 0xff);

			//Counter Clear options
			switch (h8.per_regs[TCR4] & 0x60) {
			case 0x20: //Counter Cleared by GRA Match

				GenA = ((h8.per_regs[GRA4H] << 8) + h8.per_regs[GRA4L]);

				if (((oldTimer <= GenA) && (Timer >= GenA)) || (Timer > 0xffff && ((Timer & 0xffff) >= GenA))) {
					//Clear counter (account for extra cycles)
					h8.per_regs[TCNT4H] = (((Timer - GenA) >> 8) & 0xff);
					h8.per_regs[TCNT4L] = ((Timer - GenA) & 0xff);
					//Set Status Reg Bit
					h8.per_regs[TSR4] |= 1;
				}
				break;
			case 0x40: //Counter Cleared by GRB Match
				
				GenB = ((h8.per_regs[GRB4H] << 8) + h8.per_regs[GRB4L]);
				
				if (((oldTimer <= GenB) && (Timer >= GenB)) || (Timer > 0xffff && ((Timer & 0xffff) >= GenB))) {
					//Clear counter (account for extra cycles)				
					h8.per_regs[TCNT4H] = (((Timer - GenB) >> 8) & 0xff);
					h8.per_regs[TCNT4L] = ((Timer - GenB) & 0xff);
					//Set Status Reg Bit
					h8.per_regs[TSR4] |= 2;
				}
				break;
			case 0x60: // Synchronous clear
				// Conservative implementation: do not raise an emulator error for
				// the valid H8 ITU synchronous-clear mode.  Treat it as running
				// without local clear until full inter-channel sync is required.
				if (Timer > 0xffff) {
					h8.per_regs[TSR4] |= 4;
				}
				break;
			case 0x0: //Counter not cleared
				
				if (Timer > 0xffff) {
					// Timer overflow

					//Set Status Reg Bit
					h8.per_regs[TSR4] |= 4;
				}
				break;
			}			
		}
	}

}


///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	H83002::h8_itu_read8
//
//  PURPOSE:	Register read of the onboard ITU unit
//
//  INPUTS:		UINT8					Register Number to read
//
//  OUTPUT:		UINT8					Value read from ITU Register
//
///////////////////////////////////////////////////////////////////////
UINT8 H83002::h8_itu_read8(UINT8 reg)
{
	UINT8 val;

	val = h8.per_regs[reg];

	switch (reg)
	{
	case TSR0:	
		h8.h8PREVTSR0 = val;
		break;
	case TSR1:		
		h8.h8PREVTSR1 = val;
		break;
	case TSR2:		
		h8.h8PREVTSR2 = val;
		break;
	case TSR3:		
		h8.h8PREVTSR3 = val;
		break;
	case TSR4:		
		h8.h8PREVTSR4 = val;
		break;
	}

	return val;
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	H83002::h8_itu_write8
//
//  PURPOSE:	Register write of the onboard ITU unit
//
//  INPUTS:		UINT8					Register Number to write
//				UINT8					Value to write to the register
//
//  OUTPUT:		none
//
///////////////////////////////////////////////////////////////////////
void H83002::h8_itu_write8(UINT8 reg, UINT8 val)
{
	switch (reg) {		
	case TSR0:
		if ((h8.h8PREVTSR0 & 1) && ((val & 1) == 0)) h8.per_regs[reg] &= 254;
		if ((h8.h8PREVTSR0 & 2) && ((val & 2) == 0)) h8.per_regs[reg] &= 253;
		if ((h8.h8PREVTSR0 & 4) && ((val & 4) == 0)) h8.per_regs[reg] &= 251;
		break;
	case TSR1:
		if ((h8.h8PREVTSR1 & 1) && ((val & 1) == 0)) h8.per_regs[reg] &= 254;
		if ((h8.h8PREVTSR1 & 2) && ((val & 2) == 0)) h8.per_regs[reg] &= 253;
		if ((h8.h8PREVTSR1 & 4) && ((val & 4) == 0)) h8.per_regs[reg] &= 251;
		break;
	case TSR2:
		if ((h8.h8PREVTSR2 & 1) && ((val & 1) == 0)) h8.per_regs[reg] &= 254;
		if ((h8.h8PREVTSR2 & 2) && ((val & 2) == 0)) h8.per_regs[reg] &= 253;
		if ((h8.h8PREVTSR2 & 4) && ((val & 4) == 0)) h8.per_regs[reg] &= 251;
		break;
	case TSR3:
		if ((h8.h8PREVTSR3 & 1) && ((val & 1) == 0)) h8.per_regs[reg] &= 254;
		if ((h8.h8PREVTSR3 & 2) && ((val & 2) == 0)) h8.per_regs[reg] &= 253;
		if ((h8.h8PREVTSR3 & 4) && ((val & 4) == 0)) h8.per_regs[reg] &= 251;
		break;
	case TSR4:
		if ((h8.h8PREVTSR4 & 1) && ((val & 1) == 0)) h8.per_regs[reg] &= 254;
		if ((h8.h8PREVTSR4 & 2) && ((val & 2) == 0)) h8.per_regs[reg] &= 253;
		if ((h8.h8PREVTSR4 & 4) && ((val & 4) == 0)) h8.per_regs[reg] &= 251;
		break;
	default:
		h8.per_regs[reg] = val;
		break;
	}

	Dbg = 0;
	
	if (Dbg) {
		if (DbugFile) {
			//Global Timer Registers
			switch (reg) {
			case TSTR:
				fprintf(DbugFile, "Timer Start Reg: %x \n", h8.per_regs[reg]);
				if (!(h8.per_regs[reg] & 0x01)) fprintf(DbugFile, "Timer 0 Stopped\n");
				if (h8.per_regs[reg] & 0x01) fprintf(DbugFile, "Timer 0 Started\n");
				if (!(h8.per_regs[reg] & 0x02)) fprintf(DbugFile, "Timer 1 Stopped\n");
				if (h8.per_regs[reg] & 0x02) fprintf(DbugFile, "Timer 1 Started\n");
				if (!(h8.per_regs[reg] & 0x04)) fprintf(DbugFile, "Timer 2 Stopped\n");
				if (h8.per_regs[reg] & 0x04) fprintf(DbugFile, "Timer 2 Started\n");
				if (!(h8.per_regs[reg] & 0x08)) fprintf(DbugFile, "Timer 3 Stopped\n");
				if (h8.per_regs[reg] & 0x08) fprintf(DbugFile, "Timer 3 Started\n");
				if (!(h8.per_regs[reg] & 0x10)) fprintf(DbugFile, "Timer 4 Stopped\n");				
				if (h8.per_regs[reg] & 0x10) fprintf(DbugFile, "Timer 4 Started\n");
				break;
			case TSNC:
				fprintf(DbugFile, "Timer Sync Reg: %x \n", h8.per_regs[reg]);
				if (!(h8.per_regs[reg] & 0x01)) fprintf(DbugFile, "Timer 0 Independant\n");
				if (h8.per_regs[reg] & 0x01) fprintf(DbugFile, "Timer 0 synchronous\n");
				if (!(h8.per_regs[reg] & 0x02)) fprintf(DbugFile, "Timer 1 Independant\n");
				if (h8.per_regs[reg] & 0x02) fprintf(DbugFile, "Timer 1 synchronous\n");
				if (!(h8.per_regs[reg] & 0x04)) fprintf(DbugFile, "Timer 2 Independant\n");
				if (h8.per_regs[reg] & 0x04) fprintf(DbugFile, "Timer 2 synchronous\n");
				if (!(h8.per_regs[reg] & 0x08)) fprintf(DbugFile, "Timer 3 Independant\n");
				if (h8.per_regs[reg] & 0x08) fprintf(DbugFile, "Timer 3 synchronous\n");
				if (!(h8.per_regs[reg] & 0x10)) fprintf(DbugFile, "Timer 4 Independant\n");	
				if (h8.per_regs[reg] & 0x10) fprintf(DbugFile, "Timer 4 synchronous\n");
				break;
			case TMDR:
				fprintf(DbugFile, "Timer Mode Reg: %x \n", h8.per_regs[reg]);
				if (!(h8.per_regs[reg] & 0x01)) fprintf(DbugFile, "Timer 0 Normal (PWM Off)\n");
				if (h8.per_regs[reg] & 0x01) fprintf(DbugFile, "Timer 0 PWM Mode\n");
				if (!(h8.per_regs[reg] & 0x02)) fprintf(DbugFile, "Timer 1 Normal (PWM Off)\n");
				if (h8.per_regs[reg] & 0x02) fprintf(DbugFile, "Timer 1 PWM Mode\n");
				if (!(h8.per_regs[reg] & 0x04)) fprintf(DbugFile, "Timer 2 Normal (PWM Off)\n");
				if (h8.per_regs[reg] & 0x04) fprintf(DbugFile, "Timer 2 PWM Mode\n");
				if (!(h8.per_regs[reg] & 0x08)) fprintf(DbugFile, "Timer 3 Normal (PWM Off)\n");
				if (h8.per_regs[reg] & 0x08) fprintf(DbugFile, "Timer 3 PWM Mode\n");
				if (!(h8.per_regs[reg] & 0x10)) fprintf(DbugFile, "Timer 4 Normal (PWM Off)\n");
				if (h8.per_regs[reg] & 0x10) fprintf(DbugFile, "Timer 4 PWM Mode\n");
				if (!(h8.per_regs[reg] & 0x20)) fprintf(DbugFile, "Timer 2 Status set for overflow or underflow\n");
				if (h8.per_regs[reg] & 0x20) fprintf(DbugFile, "Timer 2 Status set for overflow only\n");
				if (!(h8.per_regs[reg] & 0x40)) fprintf(DbugFile, "Timer 2 Normal (Phase Count off)\n");
				if (h8.per_regs[reg] & 0x40) fprintf(DbugFile, "Timer 2 Phase Count Mode\n");
				
				break;
			case TFCR:
				fprintf(DbugFile, "Timer Function Control Reg: %x \n", h8.per_regs[reg]);

				if (!(h8.per_regs[reg] & 0x01)) fprintf(DbugFile, "Timer 3 GRA Normal (Buffer mode Off)\n");
				if (h8.per_regs[reg] & 0x01) fprintf(DbugFile, "Timer 3 GRA reset by BRA\n");
				if (!(h8.per_regs[reg] & 0x02)) fprintf(DbugFile, "Timer 3 GRB Normal (Buffer mode Off)\n");
				if (h8.per_regs[reg] & 0x02) fprintf(DbugFile, "Timer 3 GRB reset by BRB\n");
				if (!(h8.per_regs[reg] & 0x04)) fprintf(DbugFile, "Timer 4 GRA Normal (Buffer mode Off)\n");
				if (h8.per_regs[reg] & 0x04) fprintf(DbugFile, "Timer 4 GRA reset by BRA\n");
				if (!(h8.per_regs[reg] & 0x08)) fprintf(DbugFile, "Timer 4 GRA Normal (Buffer mode Off)\n");
				if (h8.per_regs[reg] & 0x08) fprintf(DbugFile, "Timer 4 GRB reset by BRB\n");
				
				switch (h8.per_regs[reg] & 0x30){
				case 0x0:
					fprintf(DbugFile, "Timers 3 & 4 Normal (PWM off)\n");
					break;
				case 0x10:
					fprintf(DbugFile, "Timers 3 & 4 Normal (PWM Off)\n");
					break;
				case 0x20:
					fprintf(DbugFile, "Timers 3 & 4 Complementary PWM mode\n");
					break;
				case 0x30:
					fprintf(DbugFile, "Timers 3 & 4 Reset Synchronized PWM mode\n");
					break;
				}
				break;
			
			//Timer 0
			case TCR0:
				fprintf(DbugFile, "Timer Control Reg 0: %x \n", h8.per_regs[reg]);
				switch (h8.per_regs[reg] & 0x07) {
				case 0x0:
					fprintf(DbugFile, "Timer 0 use Internal Clock no prescale\n");
					break;
				case 0x1:
					fprintf(DbugFile, "Timer 0 use Internal Clock prescale /2 \n");
					break;
				case 0x2:
					fprintf(DbugFile, "Timer 0 use Internal Clock prescale /4 \n");
					break;
				case 0x3:
					fprintf(DbugFile, "Timer 0 use Internal Clock prescale /8 \n");
					break;
				case 0x4:
					fprintf(DbugFile, "Timer 0 use External Clock A\n");
					break;
				case 0x5:
					fprintf(DbugFile, "Timer 0 use External Clock B\n");
					break;
				case 0x6:
					fprintf(DbugFile, "Timer 0 use External Clock C\n");
					break;
				case 0x7:
					fprintf(DbugFile, "Timer 0 use External Clock D\n");
					break;
				}
				switch (h8.per_regs[reg] & 0x18) {
				case 0x0:
					fprintf(DbugFile, "Timer 0 External Clock Counts Rising Edges\n");
					break;
				case 0x8:
					fprintf(DbugFile, "Timer 0 External Clock Counts Falling Edges\n");
					break;
				case 0x10:
					fprintf(DbugFile, "Timer 0 External Clock Counts Both Edges\n");
					break;
				case 0x18:
					fprintf(DbugFile, "Timer 0 External Clock Counts Both Edges\n");
					break;
				}
				switch (h8.per_regs[reg] & 0x60) {
				case 0x0:
					fprintf(DbugFile, "Timer 0 TCNT not cleared\n");
					break;
				case 0x20:
					fprintf(DbugFile, "Timer 0 TCNT cleared by GRA compare match or input capture\n");
					break;
				case 0x40:
					fprintf(DbugFile, "Timer 0 TCNT cleared by GRB compare match or input capture\n");
					break;
				case 0x60:
					fprintf(DbugFile, "Timer 0 TCNT Synchronous clear\n");
					break;
				}
				break;
			case TIOR0:
				fprintf(DbugFile, "Timer IO Control Reg 0: %x \n", h8.per_regs[reg]);
				switch (h8.per_regs[reg] & 0x07) {
				case 0x0:
					fprintf(DbugFile, "Timer 0 IO no output GRA\n");
					break;
				case 0x1:
					fprintf(DbugFile, "Timer 0 IO low output at GRA Compare Match\n");
					break;
				case 0x2:
					fprintf(DbugFile, "Timer 0 IO high output at GRA Compare Match\n");
					break;
				case 0x3:
					fprintf(DbugFile, "Timer 0 IO toggle output at GRA Compare Match\n");
					break;
				case 0x4:
					fprintf(DbugFile, "Timer 0 IO Input Capture GRA rising edge\n");
					break;
				case 0x5:
					fprintf(DbugFile, "Timer 0 IO Input Capture GRA falling edge\n");
					break;
				case 0x6:
					fprintf(DbugFile, "Timer 0 IO Input Capture GRA both edges\n");
					break;
				case 0x7:
					fprintf(DbugFile, "Timer 0 IO Input Capture GRA both edges\n");
					break;
				}
				switch (h8.per_regs[reg] & 0x70) {
				case 0x00:
					fprintf(DbugFile, "Timer 0 IO no output GRB\n");
					break;
				case 0x10:
					fprintf(DbugFile, "Timer 0 IO low output at GRB Compare Match\n");
					break;
				case 0x20:
					fprintf(DbugFile, "Timer 0 IO high output at GRB Compare Match\n");
					break;
				case 0x30:
					fprintf(DbugFile, "Timer 0 IO toggle output at GRB Compare Match\n");
					break;
				case 0x40:
					fprintf(DbugFile, "Timer 0 IO Input Capture GRB rising edge\n");
					break;
				case 0x50:
					fprintf(DbugFile, "Timer 0 IO Input Capture GRB falling edge\n");
					break;
				case 0x60:
					fprintf(DbugFile, "Timer 0 IO Input Capture GRB both edges\n");
					break;
				case 0x70:
					fprintf(DbugFile, "Timer 0 IO Input Capture GRB both edges\n");
					break;
				}
				break;
			/*case TIER0:
				fprintf(DbugFile, "Timer Interrupt Enable Reg 0: %x \n", h8.per_regs[reg]);
				if (!(h8.per_regs[reg] & 0x01)) fprintf(DbugFile, "Timer 0 IMFA Interrupt Disabled\n");
				if (h8.per_regs[reg] & 0x01) fprintf(DbugFile, "Timer 0 IMFA Interrupt Enabled\n");
				if (!(h8.per_regs[reg] & 0x02)) fprintf(DbugFile, "Timer 0 IMFB Interrupt Disabled\n");
				if (h8.per_regs[reg] & 0x02) fprintf(DbugFile, "Timer 0 IMFB Interrupt Enabled\n");
				if (!(h8.per_regs[reg] & 0x04)) fprintf(DbugFile, "Timer 0 OVF Interrupt Disabled\n");
				if (h8.per_regs[reg] & 0x04) fprintf(DbugFile, "Timer 0 OVF Interrupt Enabled\n");
				break;
			case TSR0:
				fprintf(DbugFile, "Timer Status Reg 0: %x \n", h8.per_regs[reg]);
				if (!(h8.per_regs[reg] & 0x01)) fprintf(DbugFile, "Timer 0 IMFA attempt to Clear\n");
				if (h8.per_regs[reg] & 0x01) fprintf(DbugFile, "Timer 0 IMFA no attempt to Clear\n");
				if (!(h8.per_regs[reg] & 0x02)) fprintf(DbugFile, "Timer 0 IMFB attempt to Clear\n");
				if (h8.per_regs[reg] & 0x02) fprintf(DbugFile, "Timer 0 IMFB no attempt to Clear\n");
				if (!(h8.per_regs[reg] & 0x04)) fprintf(DbugFile, "Timer 0 OVF attempt to Clear\n");
				if (h8.per_regs[reg] & 0x04) fprintf(DbugFile, "Timer 0 OVF no attempt to Clear\n");
				break;*/
			case TCNT0H:
				fprintf(DbugFile, "Timer Cnt Reg 0H: %x \n", h8.per_regs[reg]);
				break;
			case TCNT0L:
				fprintf(DbugFile, "Timer Cnt Reg 0L: %x \n", h8.per_regs[reg]);
				break;
			case GRA0H:
				fprintf(DbugFile, "Timer General Reg A 0H: %x \n", h8.per_regs[reg]);
				break;
			case GRA0L:
				fprintf(DbugFile, "Timer General Reg A 0L: %x \n", h8.per_regs[reg]);
				break;
			case GRB0H:
				fprintf(DbugFile, "Timer General Reg B 0H: %x \n", h8.per_regs[reg]);
				break;
			case GRB0L:
				fprintf(DbugFile, "Timer General Reg B 0L: %x \n", h8.per_regs[reg]);
				break;
			
			//Timer 1
			case TCR1:
				fprintf(DbugFile, "Timer Control Reg 1: %x \n", h8.per_regs[reg]);
				switch (h8.per_regs[reg] & 0x07) {
				case 0x0:
					fprintf(DbugFile, "Timer 1 use Internal Clock no prescale\n");
					break;
				case 0x1:
					fprintf(DbugFile, "Timer 1 use Internal Clock prescale /2 \n");
					break;
				case 0x2:
					fprintf(DbugFile, "Timer 1 use Internal Clock prescale /4 \n");
					break;
				case 0x3:
					fprintf(DbugFile, "Timer 1 use Internal Clock prescale /8 \n");
					break;
				case 0x4:
					fprintf(DbugFile, "Timer 1 use External Clock A\n");
					break;
				case 0x5:
					fprintf(DbugFile, "Timer 1 use External Clock B\n");
					break;
				case 0x6:
					fprintf(DbugFile, "Timer 1 use External Clock C\n");
					break;
				case 0x7:
					fprintf(DbugFile, "Timer 1 use External Clock D\n");
					break;
				}
				switch (h8.per_regs[reg] & 0x18) {
				case 0x0:
					fprintf(DbugFile, "Timer 1 External Clock Counts Rising Edges\n");
					break;
				case 0x8:
					fprintf(DbugFile, "Timer 1 External Clock Counts Falling Edges\n");
					break;
				case 0x10:
					fprintf(DbugFile, "Timer 1 External Clock Counts Both Edges\n");
					break;
				case 0x18:
					fprintf(DbugFile, "Timer 1 External Clock Counts Both Edges\n");
					break;
				}
				switch (h8.per_regs[reg] & 0x60) {
				case 0x0:
					fprintf(DbugFile, "Timer 1 TCNT not cleared\n");
					break;
				case 0x20:
					fprintf(DbugFile, "Timer 1 TCNT cleared by GRA compare match or input capture\n");
					break;
				case 0x40:
					fprintf(DbugFile, "Timer 1 TCNT cleared by GRB compare match or input capture\n");
					break;
				case 0x60:
					fprintf(DbugFile, "Timer 1 TCNT Synchronous clear\n");
					break;
				}
				break;
			case TIOR1:
				fprintf(DbugFile, "Timer IO Control Reg 1: %x \n", h8.per_regs[reg]);
				switch (h8.per_regs[reg] & 0x07) {
				case 0x0:
					fprintf(DbugFile, "Timer 1 IO no output GRA\n");
					break;
				case 0x1:
					fprintf(DbugFile, "Timer 1 IO low output at GRA Compare Match\n");
					break;
				case 0x2:
					fprintf(DbugFile, "Timer 1 IO high output at GRA Compare Match\n");
					break;
				case 0x3:
					fprintf(DbugFile, "Timer 1 IO toggle output at GRA Compare Match\n");
					break;
				case 0x4:
					fprintf(DbugFile, "Timer 1 IO Input Capture GRA rising edge\n");
					break;
				case 0x5:
					fprintf(DbugFile, "Timer 1 IO Input Capture GRA falling edge\n");
					break;
				case 0x6:
					fprintf(DbugFile, "Timer 1 IO Input Capture GRA both edges\n");
					break;
				case 0x7:
					fprintf(DbugFile, "Timer 1 IO Input Capture GRA both edges\n");
					break;
				}
				switch (h8.per_regs[reg] & 0x70) {
				case 0x0:
					fprintf(DbugFile, "Timer 1 IO no output GRB\n");
					break;
				case 0x10:
					fprintf(DbugFile, "Timer 1 IO low output at GRB Compare Match\n");
					break;
				case 0x20:
					fprintf(DbugFile, "Timer 1 IO high output at GRB Compare Match\n");
					break;
				case 0x30:
					fprintf(DbugFile, "Timer 1 IO toggle output at GRB Compare Match\n");
					break;
				case 0x40:
					fprintf(DbugFile, "Timer 1 IO Input Capture GRB rising edge\n");
					break;
				case 0x50:
					fprintf(DbugFile, "Timer 1 IO Input Capture GRB falling edge\n");
					break;
				case 0x60:
					fprintf(DbugFile, "Timer 1 IO Input Capture GRB both edges\n");
					break;
				case 0x70:
					fprintf(DbugFile, "Timer 1 IO Input Capture GRB both edges\n");
					break;
				}
				break;
			/*case TIER1:
				fprintf(DbugFile, "Timer Interrupt Enable Reg 1: %x \n", h8.per_regs[reg]);
				if (!(h8.per_regs[reg] & 0x01)) fprintf(DbugFile, "Timer 1 IMFA Interrupt Disabled\n");
				if (h8.per_regs[reg] & 0x01) fprintf(DbugFile, "Timer 1 IMFA Interrupt Enabled\n");
				if (!(h8.per_regs[reg] & 0x02)) fprintf(DbugFile, "Timer 1 IMFB Interrupt Disabled\n");
				if (h8.per_regs[reg] & 0x02) fprintf(DbugFile, "Timer 1 IMFB Interrupt Enabled\n");
				if (!(h8.per_regs[reg] & 0x04)) fprintf(DbugFile, "Timer 1 OVF Interrupt Disabled\n");
				if (h8.per_regs[reg] & 0x04) fprintf(DbugFile, "Timer 1 OVF Interrupt Enabled\n");
				break;
			case TSR1:
				fprintf(DbugFile, "Timer Status Reg 1: %x \n", h8.per_regs[reg]);
				if (!(h8.per_regs[reg] & 0x01)) fprintf(DbugFile, "Timer 1 IMFA attempt to Clear\n");
				if (h8.per_regs[reg] & 0x01) fprintf(DbugFile, "Timer 1 IMFA no attempt to Clear\n");
				if (!(h8.per_regs[reg] & 0x02)) fprintf(DbugFile, "Timer 1 IMFB attempt to Clear\n");
				if (h8.per_regs[reg] & 0x02) fprintf(DbugFile, "Timer 1 IMFB no attempt to Clear\n");
				if (!(h8.per_regs[reg] & 0x04)) fprintf(DbugFile, "Timer 1 OVF attempt to Clear\n");
				if (h8.per_regs[reg] & 0x04) fprintf(DbugFile, "Timer 1 OVF no attempt to Clear\n");
				break;*/
			case TCNT1H:
				fprintf(DbugFile, "Timer Cnt Reg 1H: %x \n", h8.per_regs[reg]);
				break;
			case TCNT1L:
				fprintf(DbugFile, "Timer Cnt Reg 1L: %x \n", h8.per_regs[reg]);
				break;
			case GRA1H:
				fprintf(DbugFile, "Timer General Reg A 1H: %x \n", h8.per_regs[reg]);
				break;
			case GRA1L:
				fprintf(DbugFile, "Timer General Reg A 1L: %x \n", h8.per_regs[reg]);
				break;
			case GRB1H:
				fprintf(DbugFile, "Timer General Reg B 1H: %x \n", h8.per_regs[reg]);
				break;
			case GRB1L:
				fprintf(DbugFile, "Timer General Reg B 1L: %x \n", h8.per_regs[reg]);
				break;
			
			//Timer 2
			case TCR2:
				fprintf(DbugFile, "Timer Control Reg 2: %x \n", h8.per_regs[reg]);
				switch (h8.per_regs[reg] & 0x07) {
				case 0x0:
					fprintf(DbugFile, "Timer 2 use Internal Clock no prescale\n");
					break;
				case 0x1:
					fprintf(DbugFile, "Timer 2 use Internal Clock prescale /2 \n");
					break;
				case 0x2:
					fprintf(DbugFile, "Timer 2 use Internal Clock prescale /4 \n");
					break;
				case 0x3:
					fprintf(DbugFile, "Timer 2 use Internal Clock prescale /8 \n");
					break;
				case 0x4:
					fprintf(DbugFile, "Timer 2 use External Clock A\n");
					break;
				case 0x5:
					fprintf(DbugFile, "Timer 2 use External Clock B\n");
					break;
				case 0x6:
					fprintf(DbugFile, "Timer 2 use External Clock C\n");
					break;
				case 0x7:
					fprintf(DbugFile, "Timer 2 use External Clock D\n");
					break;
				}
				switch (h8.per_regs[reg] & 0x18) {
				case 0x0:
					fprintf(DbugFile, "Timer 2 External Clock Counts Rising Edges\n");
					break;
				case 0x8:
					fprintf(DbugFile, "Timer 2 External Clock Counts Falling Edges\n");
					break;
				case 0x10:
					fprintf(DbugFile, "Timer 2 External Clock Counts Both Edges\n");
					break;
				case 0x18:
					fprintf(DbugFile, "Timer 2 External Clock Counts Both Edges\n");
					break;
				}
				switch (h8.per_regs[reg] & 0x60) {
				case 0x0:
					fprintf(DbugFile, "Timer 2 TCNT not cleared\n");
					break;
				case 0x20:
					fprintf(DbugFile, "Timer 2 TCNT cleared by GRA compare match or input capture\n");
					break;
				case 0x40:
					fprintf(DbugFile, "Timer 2 TCNT cleared by GRB compare match or input capture\n");
					break;
				case 0x60:
					fprintf(DbugFile, "Timer 2 TCNT Synchronous clear\n");
					break;
				}
				break;
			case TIOR2:
				fprintf(DbugFile, "Timer IO Control Reg 2: %x \n", h8.per_regs[reg]);
				switch (h8.per_regs[reg] & 0x07) {
				case 0x0:
					fprintf(DbugFile, "Timer 2 IO no output GRA\n");
					break;
				case 0x1:
					fprintf(DbugFile, "Timer 2 IO low output at GRA Compare Match\n");
					break;
				case 0x2:
					fprintf(DbugFile, "Timer 2 IO high output at GRA Compare Match\n");
					break;
				case 0x3:
					fprintf(DbugFile, "Timer 2 IO toggle output at GRA Compare Match\n");
					break;
				case 0x4:
					fprintf(DbugFile, "Timer 2 IO Input Capture GRA rising edge\n");
					break;
				case 0x5:
					fprintf(DbugFile, "Timer 2 IO Input Capture GRA falling edge\n");
					break;
				case 0x6:
					fprintf(DbugFile, "Timer 2 IO Input Capture GRA both edges\n");
					break;
				case 0x7:
					fprintf(DbugFile, "Timer 2 IO Input Capture GRA both edges\n");
					break;
				}
				switch (h8.per_regs[reg] & 0x70) {
				case 0x00:
					fprintf(DbugFile, "Timer 2 IO no output GRB\n");
					break;
				case 0x10:
					fprintf(DbugFile, "Timer 2 IO low output at GRB Compare Match\n");
					break;
				case 0x20:
					fprintf(DbugFile, "Timer 2 IO high output at GRB Compare Match\n");
					break;
				case 0x30:
					fprintf(DbugFile, "Timer 2 IO toggle output at GRB Compare Match\n");
					break;
				case 0x40:
					fprintf(DbugFile, "Timer 2 IO Input Capture GRB rising edge\n");
					break;
				case 0x50:
					fprintf(DbugFile, "Timer 2 IO Input Capture GRB falling edge\n");
					break;
				case 0x60:
					fprintf(DbugFile, "Timer 2 IO Input Capture GRB both edges\n");
					break;
				case 0x70:
					fprintf(DbugFile, "Timer 2 IO Input Capture GRB both edges\n");
					break;
				}
				break;
			/*case TIER2:
				fprintf(DbugFile, "Timer Interrupt Enable Reg 2: %x \n", h8.per_regs[reg]);
				if (!(h8.per_regs[reg] & 0x01)) fprintf(DbugFile, "Timer 2 IMFA Interrupt Disabled\n");
				if (h8.per_regs[reg] & 0x01) fprintf(DbugFile, "Timer 2 IMFA Interrupt Enabled\n");
				if (!(h8.per_regs[reg] & 0x02)) fprintf(DbugFile, "Timer 2 IMFB Interrupt Disabled\n");
				if (h8.per_regs[reg] & 0x02) fprintf(DbugFile, "Timer 2 IMFB Interrupt Enabled\n");
				if (!(h8.per_regs[reg] & 0x04)) fprintf(DbugFile, "Timer 2 OVF Interrupt Disabled\n");
				if (h8.per_regs[reg] & 0x04) fprintf(DbugFile, "Timer 2 OVF Interrupt Enabled\n");
				break;
			case TSR2:
				fprintf(DbugFile, "Timer Status Reg 2: %x \n", h8.per_regs[reg]);
				if (!(h8.per_regs[reg] & 0x01)) fprintf(DbugFile, "Timer 2 IMFA attempt to Clear\n");
				if (h8.per_regs[reg] & 0x01) fprintf(DbugFile, "Timer 2 IMFA no attempt to Clear\n");
				if (!(h8.per_regs[reg] & 0x02)) fprintf(DbugFile, "Timer 2 IMFB attempt to Clear\n");
				if (h8.per_regs[reg] & 0x02) fprintf(DbugFile, "Timer 2 IMFB no attempt to Clear\n");
				if (!(h8.per_regs[reg] & 0x04)) fprintf(DbugFile, "Timer 2 OVF attempt to Clear\n");
				if (h8.per_regs[reg] & 0x04) fprintf(DbugFile, "Timer 2 OVF no attempt to Clear\n");
				break;*/
			case TCNT2H:
				fprintf(DbugFile, "Timer Cnt Reg 2H: %x \n", h8.per_regs[reg]);
				break;
			case TCNT2L:
				fprintf(DbugFile, "Timer Cnt Reg 2L: %x \n", h8.per_regs[reg]);
				break;
			case GRA2H:
				fprintf(DbugFile, "Timer General Reg A 2H: %x \n", h8.per_regs[reg]);
				break;
			case GRA2L:
				fprintf(DbugFile, "Timer General Reg A 2L: %x \n", h8.per_regs[reg]);
				break;
			case GRB2H:
				fprintf(DbugFile, "Timer General Reg B 2H: %x \n", h8.per_regs[reg]);
				break;
			case GRB2L:
				fprintf(DbugFile, "Timer General Reg B 2L: %x \n", h8.per_regs[reg]);
				break;
			
			//Timer 3
			case TCR3:
				fprintf(DbugFile, "Timer Control Reg 3: %x \n", h8.per_regs[reg]);
				switch (h8.per_regs[reg] & 0x07) {
				case 0x0:
					fprintf(DbugFile, "Timer 3 use Internal Clock no prescale\n");
					break;
				case 0x1:
					fprintf(DbugFile, "Timer 3 use Internal Clock prescale /2 \n");
					break;
				case 0x2:
					fprintf(DbugFile, "Timer 3 use Internal Clock prescale /4 \n");
					break;
				case 0x3:
					fprintf(DbugFile, "Timer 3 use Internal Clock prescale /8 \n");
					break;
				case 0x4:
					fprintf(DbugFile, "Timer 3 use External Clock A\n");
					break;
				case 0x5:
					fprintf(DbugFile, "Timer 3 use External Clock B\n");
					break;
				case 0x6:
					fprintf(DbugFile, "Timer 3 use External Clock C\n");
					break;
				case 0x7:
					fprintf(DbugFile, "Timer 3 use External Clock D\n");
					break;
				}
				switch (h8.per_regs[reg] & 0x18) {
				case 0x0:
					fprintf(DbugFile, "Timer 3 External Clock Counts Rising Edges\n");
					break;
				case 0x8:
					fprintf(DbugFile, "Timer 3 External Clock Counts Falling Edges\n");
					break;
				case 0x10:
					fprintf(DbugFile, "Timer 3 External Clock Counts Both Edges\n");
					break;
				case 0x18:
					fprintf(DbugFile, "Timer 3 External Clock Counts Both Edges\n");
					break;
				}
				switch (h8.per_regs[reg] & 0x60) {
				case 0x0:
					fprintf(DbugFile, "Timer 3 TCNT not cleared\n");
					break;
				case 0x20:
					fprintf(DbugFile, "Timer 3 TCNT cleared by GRA compare match or input capture\n");
					break;
				case 0x40:
					fprintf(DbugFile, "Timer 3 TCNT cleared by GRB compare match or input capture\n");
					break;
				case 0x60:
					fprintf(DbugFile, "Timer 3 TCNT Synchronous clear\n");
					break;
				}
				break;
			case TIOR3:
				fprintf(DbugFile, "Timer IO Control Reg 3: %x \n", h8.per_regs[reg]);
				switch (h8.per_regs[reg] & 0x07) {
				case 0x0:
					fprintf(DbugFile, "Timer 3 IO no output GRA\n");
					break;
				case 0x1:
					fprintf(DbugFile, "Timer 3 IO low output at GRA Compare Match\n");
					break;
				case 0x2:
					fprintf(DbugFile, "Timer 3 IO high output at GRA Compare Match\n");
					break;
				case 0x3:
					fprintf(DbugFile, "Timer 3 IO toggle output at GRA Compare Match\n");
					break;
				case 0x4:
					fprintf(DbugFile, "Timer 3 IO Input Capture GRA rising edge\n");
					break;
				case 0x5:
					fprintf(DbugFile, "Timer 3 IO Input Capture GRA falling edge\n");
					break;
				case 0x6:
					fprintf(DbugFile, "Timer 3 IO Input Capture GRA both edges\n");
					break;
				case 0x7:
					fprintf(DbugFile, "Timer 3 IO Input Capture GRA both edges\n");
					break;
				}
				switch (h8.per_regs[reg] & 0x70) {
				case 0x00:
					fprintf(DbugFile, "Timer 3 IO no output GRB\n");
					break;
				case 0x10:
					fprintf(DbugFile, "Timer 3 IO low output at GRB Compare Match\n");
					break;
				case 0x20:
					fprintf(DbugFile, "Timer 3 IO high output at GRB Compare Match\n");
					break;
				case 0x30:
					fprintf(DbugFile, "Timer 3 IO toggle output at GRB Compare Match\n");
					break;
				case 0x40:
					fprintf(DbugFile, "Timer 3 IO Input Capture GRB rising edge\n");
					break;
				case 0x50:
					fprintf(DbugFile, "Timer 3 IO Input Capture GRB falling edge\n");
					break;
				case 0x60:
					fprintf(DbugFile, "Timer 3 IO Input Capture GRB both edges\n");
					break;
				case 0x70:
					fprintf(DbugFile, "Timer 3 IO Input Capture GRB both edges\n");
					break;
				}
				break;
			/*case TIER3:
				fprintf(DbugFile, "Timer Interrupt Enable Reg 3: %x \n", h8.per_regs[reg]);
				if (!(h8.per_regs[reg] & 0x01)) fprintf(DbugFile, "Timer 3 IMFA Interrupt Disabled\n");
				if (h8.per_regs[reg] & 0x01) fprintf(DbugFile, "Timer 3 IMFA Interrupt Enabled\n");
				if (!(h8.per_regs[reg] & 0x02)) fprintf(DbugFile, "Timer 3 IMFB Interrupt Disabled\n");
				if (h8.per_regs[reg] & 0x02) fprintf(DbugFile, "Timer 3 IMFB Interrupt Enabled\n");
				if (!(h8.per_regs[reg] & 0x04)) fprintf(DbugFile, "Timer 3 OVF Interrupt Disabled\n");
				if (h8.per_regs[reg] & 0x04) fprintf(DbugFile, "Timer 3 OVF Interrupt Enabled\n");
				break;
			case TSR3:
				fprintf(DbugFile, "Timer Status Reg 3: %x \n", h8.per_regs[reg]);
				if (!(h8.per_regs[reg] & 0x01)) fprintf(DbugFile, "Timer 3 IMFA attempt to Clear\n");
				if (h8.per_regs[reg] & 0x01) fprintf(DbugFile, "Timer 3 IMFA no attempt to Clear\n");
				if (!(h8.per_regs[reg] & 0x02)) fprintf(DbugFile, "Timer 3 IMFB attempt to Clear\n");
				if (h8.per_regs[reg] & 0x02) fprintf(DbugFile, "Timer 3 IMFB no attempt to Clear\n");
				if (!(h8.per_regs[reg] & 0x04)) fprintf(DbugFile, "Timer 3 OVF attempt to Clear\n");
				if (h8.per_regs[reg] & 0x04) fprintf(DbugFile, "Timer 3 OVF no attempt to Clear\n");
				break;*/
			case TCNT3H:
				fprintf(DbugFile, "Timer Cnt Reg 3H: %x \n", h8.per_regs[reg]);
				break;
			case TCNT3L:
				fprintf(DbugFile, "Timer Cnt Reg 3L: %x \n", h8.per_regs[reg]);
				break;
			case GRA3H:
				fprintf(DbugFile, "Timer General Reg A 3H: %x \n", h8.per_regs[reg]);
				break;
			case GRA3L:
				fprintf(DbugFile, "Timer General Reg A 3L: %x \n", h8.per_regs[reg]);
				break;
			case GRB3H:
				fprintf(DbugFile, "Timer General Reg B 3H: %x \n", h8.per_regs[reg]);
				break;
			case GRB3L:
				fprintf(DbugFile, "Timer General Reg B 3L: %x \n", h8.per_regs[reg]);
				break;
			case BRA3H:
				fprintf(DbugFile, "Timer Buffer Reg A 3H: %x \n", h8.per_regs[reg]);
				break;
			case BRA3L:
				fprintf(DbugFile, "Timer Buffer Reg A 3L: %x \n", h8.per_regs[reg]);
				break;
			case BRB3H:
				fprintf(DbugFile, "Timer Buffer Reg B 3H: %x \n", h8.per_regs[reg]);
				break;
			case BRB3L:
				fprintf(DbugFile, "Timer Buffer Reg B 3L: %x \n", h8.per_regs[reg]);
				break;

				//Timer 4
			case TCR4:
				fprintf(DbugFile, "Timer Control Reg 4: %x \n", h8.per_regs[reg]);
				switch (h8.per_regs[reg] & 0x07) {
				case 0x0:
					fprintf(DbugFile, "Timer 4 use Internal Clock no prescale\n");
					break;
				case 0x1:
					fprintf(DbugFile, "Timer 4 use Internal Clock prescale /2 \n");
					break;
				case 0x2:
					fprintf(DbugFile, "Timer 4 use Internal Clock prescale /4 \n");
					break;
				case 0x3:
					fprintf(DbugFile, "Timer 4 use Internal Clock prescale /8 \n");
					break;
				case 0x4:
					fprintf(DbugFile, "Timer 4 use External Clock A\n");
					break;
				case 0x5:
					fprintf(DbugFile, "Timer 4 use External Clock B\n");
					break;
				case 0x6:
					fprintf(DbugFile, "Timer 4 use External Clock C\n");
					break;
				case 0x7:
					fprintf(DbugFile, "Timer 4 use External Clock D\n");
					break;
				}
				switch (h8.per_regs[reg] & 0x18) {
				case 0x0:
					fprintf(DbugFile, "Timer 4 External Clock Counts Rising Edges\n");
					break;
				case 0x8:
					fprintf(DbugFile, "Timer 4 External Clock Counts Falling Edges\n");
					break;
				case 0x10:
					fprintf(DbugFile, "Timer 4 External Clock Counts Both Edges\n");
					break;
				case 0x18:
					fprintf(DbugFile, "Timer 4 External Clock Counts Both Edges\n");
					break;
				}
				switch (h8.per_regs[reg] & 0x60) {
				case 0x0:
					fprintf(DbugFile, "Timer 4 TCNT not cleared\n");
					break;
				case 0x20:
					fprintf(DbugFile, "Timer 4 TCNT cleared by GRA compare match or input capture\n");
					break;
				case 0x40:
					fprintf(DbugFile, "Timer 4 TCNT cleared by GRB compare match or input capture\n");
					break;
				case 0x60:
					fprintf(DbugFile, "Timer 4 TCNT Synchronous clear\n");
					break;
				}
				break;
			case TIOR4:
				fprintf(DbugFile, "Timer IO Control Reg 4: %x \n", h8.per_regs[reg]);
				switch (h8.per_regs[reg] & 0x07) {
				case 0x0:
					fprintf(DbugFile, "Timer 4 IO no output GRA\n");
					break;
				case 0x1:
					fprintf(DbugFile, "Timer 4 IO low output at GRA Compare Match\n");
					break;
				case 0x2:
					fprintf(DbugFile, "Timer 4 IO high output at GRA Compare Match\n");
					break;
				case 0x3:
					fprintf(DbugFile, "Timer 4 IO toggle output at GRA Compare Match\n");
					break;
				case 0x4:
					fprintf(DbugFile, "Timer 4 IO Input Capture GRA rising edge\n");
					break;
				case 0x5:
					fprintf(DbugFile, "Timer 4 IO Input Capture GRA falling edge\n");
					break;
				case 0x6:
					fprintf(DbugFile, "Timer 4 IO Input Capture GRA both edges\n");
					break;
				case 0x7:
					fprintf(DbugFile, "Timer 4 IO Input Capture GRA both edges\n");
					break;
				}
				switch (h8.per_regs[reg] & 0x70) {
				case 0x00:
					fprintf(DbugFile, "Timer 4 IO no output GRB\n");
					break;
				case 0x10:
					fprintf(DbugFile, "Timer 4 IO low output at GRB Compare Match\n");
					break;
				case 0x20:
					fprintf(DbugFile, "Timer 4 IO high output at GRB Compare Match\n");
					break;
				case 0x30:
					fprintf(DbugFile, "Timer 4 IO toggle output at GRB Compare Match\n");
					break;
				case 0x40:
					fprintf(DbugFile, "Timer 4 IO Input Capture GRB rising edge\n");
					break;
				case 0x50:
					fprintf(DbugFile, "Timer 4 IO Input Capture GRB falling edge\n");
					break;
				case 0x60:
					fprintf(DbugFile, "Timer 4 IO Input Capture GRB both edges\n");
					break;
				case 0x70:
					fprintf(DbugFile, "Timer 4 IO Input Capture GRB both edges\n");
					break;
				}
				break;
			/*case TIER4:
				fprintf(DbugFile, "Timer Interrupt Enable Reg 4: %x \n", h8.per_regs[reg]);
				if (!(h8.per_regs[reg] & 0x01)) fprintf(DbugFile, "Timer 4 IMFA Interrupt Disabled\n");
				if (h8.per_regs[reg] & 0x01) fprintf(DbugFile, "Timer 4 IMFA Interrupt Enabled\n");
				if (!(h8.per_regs[reg] & 0x02)) fprintf(DbugFile, "Timer 4 IMFB Interrupt Disabled\n");
				if (h8.per_regs[reg] & 0x02) fprintf(DbugFile, "Timer 4 IMFB Interrupt Enabled\n");
				if (!(h8.per_regs[reg] & 0x04)) fprintf(DbugFile, "Timer 4 OVF Interrupt Disabled\n");
				if (h8.per_regs[reg] & 0x04) fprintf(DbugFile, "Timer 4 OVF Interrupt Enabled\n");
				break;
			case TSR4:
				fprintf(DbugFile, "Timer Status Reg 4: %x \n", h8.per_regs[reg]);
				if (!(h8.per_regs[reg] & 0x01)) fprintf(DbugFile, "Timer 4 IMFA attempt to Clear\n");
				if (h8.per_regs[reg] & 0x01) fprintf(DbugFile, "Timer 4 IMFA no attempt to Clear\n");
				if (!(h8.per_regs[reg] & 0x02)) fprintf(DbugFile, "Timer 4 IMFB attempt to Clear\n");
				if (h8.per_regs[reg] & 0x02) fprintf(DbugFile, "Timer 4 IMFB no attempt to Clear\n");
				if (!(h8.per_regs[reg] & 0x04)) fprintf(DbugFile, "Timer 4 OVF attempt to Clear\n");
				if (h8.per_regs[reg] & 0x04) fprintf(DbugFile, "Timer 4 OVF no attempt to Clear\n");
				break;*/
			case TCNT4H:
				fprintf(DbugFile, "Timer Cnt Reg 4H: %x \n", h8.per_regs[reg]);
				break;
			case TCNT4L:
				fprintf(DbugFile, "Timer Cnt Reg 4L: %x \n", h8.per_regs[reg]);
				break;
			case GRA4H:
				fprintf(DbugFile, "Timer General Reg A 4H: %x \n", h8.per_regs[reg]);
				break;
			case GRA4L:
				fprintf(DbugFile, "Timer General Reg A 4L: %x \n", h8.per_regs[reg]);
				break;
			case GRB4H:
				fprintf(DbugFile, "Timer General Reg B 4H: %x \n", h8.per_regs[reg]);
				break;
			case GRB4L:
				fprintf(DbugFile, "Timer General Reg B 4L: %x \n", h8.per_regs[reg]);
				break;
			case BRA4H:
				fprintf(DbugFile, "Timer Buffer Reg A 4H: %x \n", h8.per_regs[reg]);
				break;
			case BRA4L:
				fprintf(DbugFile, "Timer Buffer Reg A 4L: %x \n", h8.per_regs[reg]);
				break;
			case BRB4H:
				fprintf(DbugFile, "Timer Buffer Reg B 4H: %x \n", h8.per_regs[reg]);
				break;
			case BRB4L:
				fprintf(DbugFile, "Timer Buffer Reg B 4L: %x \n", h8.per_regs[reg]);
				break;
			}

		}	

	}

}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	H83002::h8_itu_reset
//
//  PURPOSE:	Force a reset of the ITU device
//
//  INPUTS:		none
//
//  OUTPUT:		none
//
///////////////////////////////////////////////////////////////////////
void H83002::h8_itu_reset(void)
{
	//Timer 0
	h8.per_regs[TCR0] = 0x80;	// Timer Control Regs
	h8.per_regs[TIOR0] = 0x88;	// Timer IO Control Regs
	h8.per_regs[TIER0] = 0xf8;	// Timer Interupt Enable Regs
	h8.per_regs[TSR0] = 0xf8;	// Timer Status Regs
	h8.per_regs[TCNT0H] = 0x00;	// Timer Counter Regs (high)
	h8.per_regs[TCNT0L] = 0x00;	// Timer Counter Regs (Low)
	h8.per_regs[GRA0H] = 0xff;	// Timer General Regs A (High)
	h8.per_regs[GRA0L] = 0xff;	// Timer General Regs A (Low)
	h8.per_regs[GRB0H] = 0xff;	// Timer General Regs B (High)
	h8.per_regs[GRB0L] = 0xff;	// Timer General Regs B (Low)

	//Timer 1
	h8.per_regs[TCR1] = 0x80;	// Timer Control Regs
	h8.per_regs[TIOR1] = 0x88;	// Timer IO Control Regs
	h8.per_regs[TIER1] = 0xf8;	// Timer Interupt Enable Regs
	h8.per_regs[TSR1] = 0xf8;	// Timer Status Regs
	h8.per_regs[TCNT1H] = 0x00;	// Timer Counter Regs (high)
	h8.per_regs[TCNT1L] = 0x00;	// Timer Counter Regs (Low)
	h8.per_regs[GRA1H] = 0xff;	// Timer General Regs A (High)
	h8.per_regs[GRA1L] = 0xff;	// Timer General Regs A (Low)
	h8.per_regs[GRB1H] = 0xff;	// Timer General Regs B (High)
	h8.per_regs[GRB1L] = 0xff;	// Timer General Regs B (Low)

	//Timer 2
	h8.per_regs[TCR2] = 0x80;	// Timer Control Regs
	h8.per_regs[TIOR2] = 0x88;	// Timer IO Control Regs
	h8.per_regs[TIER2] = 0xf8;	// Timer Interupt Enable Regs
	h8.per_regs[TSR2] = 0xf8;	// Timer Status Regs
	h8.per_regs[TCNT2H] = 0x00;	// Timer Counter Regs (high)
	h8.per_regs[TCNT2L] = 0x00;	// Timer Counter Regs (Low)
	h8.per_regs[GRA2H] = 0xff;	// Timer General Regs A (High)
	h8.per_regs[GRA2L] = 0xff;	// Timer General Regs A (Low)
	h8.per_regs[GRB2H] = 0xff;	// Timer General Regs B (High)
	h8.per_regs[GRB2L] = 0xff;	// Timer General Regs B (Low)

	//Timer 3
	h8.per_regs[TCR3] = 0x80;	// Timer Control Regs
	h8.per_regs[TIOR3] = 0x88;	// Timer IO Control Regs
	h8.per_regs[TIER3] = 0xf8;	// Timer Interupt Enable Regs
	h8.per_regs[TSR3] = 0xf8;	// Timer Status Regs
	h8.per_regs[TCNT3H] = 0x00;	// Timer Counter Regs (high)
	h8.per_regs[TCNT3L] = 0x00;	// Timer Counter Regs (Low)
	h8.per_regs[GRA3H] = 0xff;	// Timer General Regs A (High)
	h8.per_regs[GRA3L] = 0xff;	// Timer General Regs A (Low)
	h8.per_regs[GRB3H] = 0xff;	// Timer General Regs B (High)
	h8.per_regs[GRB3L] = 0xff;	// Timer General Regs B (Low)
	h8.per_regs[BRA3H] = 0xff;	// Timer Buffer Regs A (High)
	h8.per_regs[BRA3L] = 0xff;	// Timer Buffer Regs A (Low)
	h8.per_regs[BRB3H] = 0xff;	// Timer Buffer Regs B (High)
	h8.per_regs[BRB3L] = 0xff;	// Timer Buffer Regs B (Low)

	//Timer 4
	h8.per_regs[TCR4] = 0x80;	// Timer Control Regs
	h8.per_regs[TIOR4] = 0x88;	// Timer IO Control Regs
	h8.per_regs[TIER4] = 0xf8;	// Timer Interupt Enable Regs
	h8.per_regs[TSR4] = 0xf8;	// Timer Status Regs
	h8.per_regs[TCNT4H] = 0x00;	// Timer Counter Regs (high)
	h8.per_regs[TCNT4L] = 0x00;	// Timer Counter Regs (Low)
	h8.per_regs[GRA4H] = 0xff;	// Timer General Regs A (High)
	h8.per_regs[GRA4L] = 0xff;	// Timer General Regs A (Low)
	h8.per_regs[GRB4H] = 0xff;	// Timer General Regs B (High)
	h8.per_regs[GRB4L] = 0xff;	// Timer General Regs B (Low)
	h8.per_regs[BRA4H] = 0xff;	// Timer Buffer Regs A (High)
	h8.per_regs[BRA4L] = 0xff;	// Timer Buffer Regs A (Low)
	h8.per_regs[BRB4H] = 0xff;	// Timer Buffer Regs B (High)
	h8.per_regs[BRB4L] = 0xff;	// Timer Buffer Regs B (Low)
	
	 // Prescaler Counts
	h8.h8TPrescale0 = 0;
	h8.h8TPrescale1 = 0;
	h8.h8TPrescale2 = 0;
	h8.h8TPrescale3 = 0;
	h8.h8TPrescale4 = 0;	

	// Timer Sync, Start, Mode, Function Ctrl, Output Master Enable, Output Control
	h8.per_regs[TSNC] = 0xE0;
	h8.per_regs[TSTR] = 0xE0;
	h8.per_regs[TMDR] = 0x80;
	h8.per_regs[TFCR] = 0xC0;
	h8.per_regs[TOER] = 0xFF;
	h8.per_regs[TOCR] = 0xFF;

}

void H83002::h8_rc_tick(int tnum) {

}

UINT8 H83002::h8_rc_read8(UINT8 reg) {
	switch (reg) {
	case 0xAC://Refresh Control Register
		return h8.per_regs[RFSHCR];
		break;
	case 0xAD://Refresh Timer Control / Status Reg
		return h8.per_regs[RTMCSR];
		break;
	case 0xAE://Refresh Timer Counter
		return h8.per_regs[RTCNT];
		break;
	case 0xAF://Refresh Timer constant register
		return h8.per_regs[RTCOR];
		break;
	}
	return 0;
}

void H83002::h8_rc_write8(UINT8 reg, UINT8 val){
	switch (reg) {
	case 0xAC://Refresh Control Register
		h8.per_regs[RFSHCR] = val;
		break;
	case 0xAD://Refresh Timer Control / Status Reg
		h8.per_regs[RTMCSR] = val;
		break;
	case 0xAE://Refresh Timer Counter
		h8.per_regs[RTCNT] = val;
		break;
	case 0xAF://Refresh Timer constant register
		h8.per_regs[RTCOR] = val;
		break;
	}
}

void H83002::h8_rc_reset(void) {
	h8.per_regs[RFSHCR] = 0x02;
	h8.per_regs[RTMCSR] = 0x07;
	h8.per_regs[RTCNT] = 0x00;
	h8.per_regs[RTCOR] = 0xff;
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	H83002::h8_wdt_tick
//
//  PURPOSE:	Cause the WDT device to be clocked
//
//  INPUTS:		int					Number of cycles to clock the device by
//
//  OUTPUT:		none
//
///////////////////////////////////////////////////////////////////////
void H83002::h8_wdt_tick(int cycles)
{
	static UINT16 scaleAmounts[8] =
	{
		1, 5, 6, 7, 8, 9, 11, 12
	};


	/*
	Prescaler amounts
	0 0 0 /2 (Initial value)
	0 0 1 /32
	0 1 0 /64
	0 1 1 /128
	1 0 0 /256
	1 0 1 /512
	1 1 0 /2048
	1 1 1 /4096
	*/

	if (h8.per_regs[TCSR] & 0x20)
	{
		// WDT is running
		// integrated div by n counter
		h8.h8WDTPrescale += cycles;

		// Check if Main Prescaler >= 16
		if (h8.h8WDTPrescale & (0xffff << scaleAmounts[h8.per_regs[TCSR] & 7]))
		{
			// And if so, reduce prescaler. Adjust cycles to equal the
			// number of complete main prescale cycles
			cycles = h8.h8WDTPrescale >> scaleAmounts[h8.per_regs[TCSR] & 7];

			// And remove fully processed cycles from the prescaler accordingly
			h8.h8WDTPrescale &= (~(0xffff << scaleAmounts[h8.per_regs[TCSR] & 7]));
		}
		else
		{
			return;
		}

		// Cycles now contains number of counts to make
		UINT8 oldTCNT = h8.per_regs[TCNT];
		h8.per_regs[TCNT] += cycles;
		if (h8.per_regs[TCNT] < oldTCNT)
		{
			// WDT Overflowed
			h8.err = 100;

		}
	}
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	H83002::h8_wdt_read8
//
//  PURPOSE:	Register read of the onboard WDT unit
//
//  INPUTS:		UINT8					Register Number to read
//
//  OUTPUT:		UINT8					Value read from WDT Register
//
///////////////////////////////////////////////////////////////////////
UINT8 H83002::h8_wdt_read8(UINT8 reg)
{
	UINT8 val;

	switch (reg)
	{
		case RSTCSR:
			val = h8.per_regs[RSTCSR] & 0xc0;
			break;
		default:
			val = h8.per_regs[reg];
			break;
	}

	return val;
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	H83002::h8_wdt_write8
//
//  PURPOSE:	Register write of the onboard WDT unit
//
//  INPUTS:		UINT8					Register Number to write
//					UINT8					Value to write to the register
//
//  OUTPUT:		none
//
///////////////////////////////////////////////////////////////////////
void H83002::h8_wdt_write8(UINT8 reg, UINT8 val)
{
	h8.per_regs[reg] = val;
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	H83002::h8_wdt_reset
//
//  PURPOSE:	Force a reset of the WDT device
//
//  INPUTS:		none
//
//  OUTPUT:		none
//
///////////////////////////////////////////////////////////////////////
void H83002::h8_wdt_reset(void)
{
	h8.h8WDTPrescale = 0;
	h8.per_regs[TCNT] = 0;
	h8.per_regs[TCSR] = 0x18;
	h8.per_regs[RSTCSR] = 0x3f;	
}

///////////////////////////////////////////////////////////////////////
//
//  FUNCTION:	H83002::h8_tick
//
//  PURPOSE:	Cause the peripheral devices to be clocked
//
//  INPUTS:		int					Number of cycles to clock the devices by
//
//  OUTPUT:		none
//
///////////////////////////////////////////////////////////////////////
void H83002::h8_tick(int cycles)
{
	h8_itu_tick(cycles);
	h8_wdt_tick(cycles);
	h8_sci_tick(cycles);
}

UINT8 H83002::h8_register_read8(UINT32 address)
{
	UINT8 val = h8.per_regs[(address & 0xff)];
	UINT8 reg = address & 0xff;
	address &= 0xffffff;

	if ((reg >= 0x20) && (reg <= 0x3f))
	{//Direct Memory Access Controller
		return h8_dmac_read8(reg);
	}
	else if ((reg >= 0x60) && (reg <= 0x9f))
	{//Integrated Timer Unit
		return h8_itu_read8(reg);
	}
	else if ((reg >= 0xa0) && (reg <= 0xa7))
	{//Timing Pattern Controller
		return h8_tpc_read8(reg);
	}
	else if ((reg >= 0xa8) && (reg <= 0xab))
	{//Watchdog
		return h8_wdt_read8(reg);
	}
	else if ((reg >= 0xac) && (reg <= 0xaf))
	{//Refresh Controller
		return h8_rc_read8(reg);
	}
	else if ((reg >= 0xb0) && (reg <= 0xbf))
	{//Sci Comms
		return h8_sci_read8(reg);
	}	
	else if ((reg >= 0xc0) && (reg <= 0xdf))
	{//IO Ports
		return h8_io_read8(reg);
	}
	else if ((reg >= 0xe0) && (reg <= 0xe9))
	{//AD Converter
		return h8_adc_read8(reg);
	}
	else if ((reg >= 0xec) && (reg <= 0xef))
	{//Bus Controller
		return h8_bc_read8(reg);
	}
	else if ((reg >= 0xf0) && (reg <= 0xf3))
	{//System regs
		return h8_sys_reg_read8(reg);
	}
	else if ((reg >= 0xf4) && (reg <= 0xf9))
	{//Interrupt Controller
		return h8_ic_read8(reg);
	}

	//Unknown!
	val = h8.per_regs[reg];
	return val;
}

void H83002::h8_register_write8(UINT32 address, UINT8 val)
{
	UINT8 reg = address & 0xff;
	address &= 0xffffff;

	if ((reg >= 0x20) && (reg <= 0x3f))
	{
		//Direct Memory Access Controller
		h8_dmac_write8(reg, val);
	}
	else if ((reg >= 0x60) && (reg <= 0x9f))
	{
		//Integrated Timer Unit
		h8_itu_write8(reg, val);
	}
	else if ((reg >= 0xa0) && (reg <= 0xa7))
	{
		//Timing Pattern Controller
		h8_tpc_write8(reg, val);
	}
	else if ((reg >= 0xa8) && (reg <= 0xab))
	{
		//Watch Dog Timer
		h8_wdt_write8(reg, val);
	}
	else if ((reg >= 0xac) && (reg <= 0xaf))
	{
		//Refresh Controller
		h8_rc_write8(reg, val);
	}
	else if ((reg >= 0xb0) && (reg <= 0xbf))
	{
		//Sci Comms
		h8_sci_write8(reg, val);
	}
	else if ((reg >= 0xc0) && (reg <= 0xdf))
	{
		//IO Controller
		h8_io_write8(reg, val);
	}
	else if ((reg >= 0xe0) && (reg <= 0xe9))
	{
		//ADC Controller
		h8_adc_write8(reg, val);
	}
	else if ((reg >= 0xec) && (reg <= 0xef))
	{
		//Bus Controller
		h8_bc_write8(reg, val);
	}
	else if ((reg >= 0xf0) && (reg <= 0xf3))
	{
		//System Regs
		h8_sys_reg_write8(reg, val);
	}
	else if ((reg >= 0xf4) && (reg <= 0xf9))
	{
		//Interrupt Controller
		h8_ic_write8(reg, val);
	}
	else
	{	
		//Unknown!
		h8.per_regs[reg] = val;	
	}
}

UINT8 H83002::h8_ic_read8(UINT8 reg) {

	UINT8 val = 0;
	switch (reg) {
	case ISCR:	// 0xf4		
		val = h8.per_regs[ISCR];
		break;
	case IER:	// 0xf5		
		val = h8.per_regs[IER];
		break;
	case ISR:	// 0xf6
		val = h8.per_regs[ISR];
		H8PrevISR = val;
		break;
	case IPRA:	// 0xf8
		val = h8.per_regs[IPRA];
		break;
	case IPRB:	// 0xf9
		val = h8.per_regs[IPRB];
		break;
	}

	return val;

}
void H83002::h8_ic_write8(UINT8 reg, UINT8 val) {

	//Interrupt Controller
	switch (reg) {
	case ISCR:	// 0xf4 Interrupt Sense Control Register
		h8.per_regs[ISCR] = val;
		break;
	case IER:	// 0xf5 Interrupt Enable Register
		h8.per_regs[IER] = val;
		break;
	case ISR:	// 0xf6 Interrupt Status Register
			if ((H8PrevISR & IRQ0BIT) && ((val & 0x01) == 0))	h8.per_regs[ISR] &= (~IRQ0BIT);
			if ((H8PrevISR & IRQ1BIT) && ((val & 0x02) == 0))	h8.per_regs[ISR] &= (~IRQ1BIT);
			if ((H8PrevISR & IRQ2BIT) && ((val & 0x04) == 0))	h8.per_regs[ISR] &= (~IRQ2BIT);
			if ((H8PrevISR & IRQ3BIT) && ((val & 0x08) == 0))	h8.per_regs[ISR] &= (~IRQ3BIT);
			if ((H8PrevISR & IRQ4BIT) && ((val & 0x10) == 0))	h8.per_regs[ISR] &= (~IRQ4BIT);
			if ((H8PrevISR & IRQ5BIT) && ((val & 0x20) == 0))	h8.per_regs[ISR] &= (~IRQ5BIT);		
		break;
	case IPRA:	// 0xf8 Interrupt Priority Register A
		h8.per_regs[IPRA] = val;
		break;
	case IPRB:	// 0xf9 Interrupt Priority Register B
		h8.per_regs[IPRB] = val;
		break;
	}

}

void H83002::h8_ic_reset(void) {

	//Reset Interrupt Control Regs
	h8.per_regs[SYSCR] = 0x0B;
	//Others already 0
}

//IO PORTS
#define P4DDR		0xc5 //Port 4 Data Direction Reg
#define P4DR		0xc7 //Port 4 Data Reg
#define P6DDR		0xc9 //Port 6 Data Direction Reg
#define P6DR		0xcb //Port 6 Data Reg
#define P8DDR		0xcd //Port 8 Data Direction Reg
#define P7DR		0xce //Port 7 Data Reg
#define P8DR		0xcf //Port 8 Data Reg
#define P9DDR		0xd0 //Port 9 Data Direction Reg
#define PADDR		0xd1 //Port A Data Direction Reg
#define P9DR		0xd2 //Port 9 Data Reg
#define PADR		0xd3 //Port A Data Reg
#define PBDDR		0xd4 //Port B Data Direction Reg
#define PBDR		0xd6 //Port B Data Reg
#define P4PCR		0xda //Port 4 Input Pull-up control Reg

// IO
UINT8 H83002::h8_io_read8(UINT8 reg) {
	
	UINT8 val = 0;	

	switch (reg) {
	case P4DR:    		// port 4 data
		val = io_read_byte_8(H8_PORT4);
		break;
	case P6DR:    		// port 6 data
		val = io_read_byte_8(H8_PORT6);
		break;
	case P7DR:		// port 7 data
		val = io_read_byte_8(H8_PORT7);
		break;
	case P8DR:		// port 8 data
		val = io_read_byte_8(H8_PORT8);
		break;
	case P9DR:		// port 9 data
		val = io_read_byte_8(H8_PORT9);
		break;
	case PADR:		// port a data
		val = io_read_byte_8(H8_PORTA);
		break;
	case PBDR:		// port b data
		val = io_read_byte_8(H8_PORTB);
		break;
	}
	
	return val;
}


void H83002::h8_io_write8(UINT8 reg, UINT8 val) {

	h8.per_regs[reg] = val;
	int temp = 0;
	switch (reg)
	{
	case P4DR:
		io_write_byte_8(H8_PORT4, val);
		break;
	case P6DR:    	// port 6 data
		io_write_byte_8(H8_PORT6, val);
		break;
	case P7DR:		// port 7 data
		io_write_byte_8(H8_PORT7, val);
		break;
	case P8DR:		// port 8 data
		io_write_byte_8(H8_PORT8, val);
		break;
	case P9DR:		// port 9 data
		io_write_byte_8(H8_PORT9, val);
		break;
	case PADR:		// port a data
		io_write_byte_8(H8_PORTA, val);
		break;
	case PBDR:		// port b data
		io_write_byte_8(H8_PORTB, val);
		break;
	case P8DDR:		// port 8 data direction register
		temp = val;
		break;
	}

}
void H83002::h8_io_reset(void) {
	//IO Ports
	h8.per_regs[P6DDR] = 0x80;
	h8.per_regs[P6DR] = 0x80;
	h8.per_regs[P8DDR] = 0xF0;
	h8.per_regs[P8DR] = 0xE0;
	h8.per_regs[P9DDR] = 0xC0;
	h8.per_regs[P9DR] = 0xC0;
	//Others already zero'd
}

UINT8 H83002::h8_adc_read8(UINT8 reg) {

	UINT8 val = 0;

	switch (reg) {
	case 0xe8:		// adc status reg
		val = 0x80;
		break;
	case 0xe0:
		val = io_read_byte_8(H8_ADC_0_H);
		break;
	case 0xe1:
		val = io_read_byte_8(H8_ADC_0_L);
		break;
	case 0xe2:
		val = io_read_byte_8(H8_ADC_1_H);
		break;
	case 0xe3:
		val = io_read_byte_8(H8_ADC_1_L);
		break;
	case 0xe4:
		val = io_read_byte_8(H8_ADC_2_H);
		break;
	case 0xe5:
		val = io_read_byte_8(H8_ADC_2_L);
		break;
	case 0xe6:
		val = io_read_byte_8(H8_ADC_3_H);
		break;
	case 0xe7:
		val = io_read_byte_8(H8_ADC_3_L);
		break;
	}
	return val;

}
void H83002::h8_adc_write8(UINT8 reg, UINT8 val) {
	h8.per_regs[reg] = val;
}
void H83002::h8_adc_reset(void) {
	//Analogue to Digital Converter (A/D)
	
	//Others already zero'd
}

//Timing Pattern Controller (TPC)
UINT8 H83002::h8_tpc_read8(UINT8 reg) {
	return h8.per_regs[reg];
}
void H83002::h8_tpc_write8(UINT8 reg, UINT8 val) {
	h8.per_regs[reg] = val;
}
void H83002::h8_tpc_reset(void) {
	//Programmable Timing Pattern Controller (PTC)
	h8.per_regs[TPMR] = 0xf0;
	h8.per_regs[TPCR] = 0xff;
	//Others already 0
}

//Bus Controller
UINT8 H83002::h8_bc_read8(UINT8 reg) {
	switch (reg) {
	case ABWCR:
		return h8.per_regs[reg];
		break;
	case ASTCR:
		return h8.per_regs[reg];
		break;
	case WCR:
		return h8.per_regs[reg];
		break;
	case WCER:
		return h8.per_regs[reg];
		break;
	case BRCR:
		return h8.per_regs[reg];
		break;
	default:
		h8.err = 102;
		return h8.per_regs[reg];
		break;
	}
	
}
void H83002::h8_bc_write8(UINT8 reg, UINT8 val) {

	switch (reg) {
	case ABWCR:
		h8.per_regs[reg] = val;
		h8_timing_init();//Reinitialise
		break;
	case ASTCR:
		h8.per_regs[reg] = val;
		h8_timing_init();//Reinitialise
		break;
	case WCR:
		h8.per_regs[reg] = val;
		break;
	case WCER:
		h8.per_regs[reg] = val;
		break;
	case BRCR:
		h8.per_regs[reg] = val;
		break;
	default:
		h8.err = 103;
		h8.per_regs[reg] = val;		
		break;
	}
	
}
void H83002::h8_bc_reset(void) {

	//Reset Bus Controller	
	h8.per_regs[ASTCR] = 0xff;
	h8.per_regs[WCR] = 0xf3;
	h8.per_regs[WCER] = 0xff;
	h8.per_regs[BRCR] = 0xfe;

	switch (INITIALMODE) {
	case 2:
	case 4:
		h8.per_regs[ABWCR] = 0x0;
		break;
	case 1:
	case 3:
		h8.per_regs[ABWCR] = 0xff;
		break;
	default:
		h8.err = 105;
	}	
	
	h8_timing_init();
	//Others already 0
}

//System Regs
UINT8 H83002::h8_sys_reg_read8(UINT8 reg) {
	return h8.per_regs[reg];
}
void H83002::h8_sys_reg_write8(UINT8 reg, UINT8 val) {
	h8.per_regs[reg] = val;
}
void H83002::h8_sys_reg_reset(void) {
	
	//Mode 1 - 1mb address space - 8 bit bus
	//Mode 2 - 1mb address space - 16 bit bus
	//Mode 3 - 16mb address space - 8 bit bus
	//Mode 4 - 16mb address space - 16 bit bus
	UINT8 mode = INITIALMODE; //Epoch is Mode 4
	h8.per_regs[MDCR] = (0xc0 | (mode & 7)); 
	h8.per_regs[ADCR] = 0x7e;
	h8.per_regs[BRCR] = 0xfe;
}


// DMAC 
UINT8 H83002::h8_dmac_read8(UINT8 reg) {
	return h8.per_regs[reg];
}
void H83002::h8_dmac_write8(UINT8 reg, UINT8 val) {
	h8.per_regs[reg] = val;
}
void H83002::h8_dmac_reset(void) {
	//All of these are either undetermined or 0 already
	//May need to set some specifics for EPOCH tech as some of these pins are hard wired high or low on the PCB
}





