// ###########################################################################
// #
// # h83002 - H8/3002 CPU Emulation Core
// # Copyright (C) 2002-2010 Tony Friery [DialTone]
// #
// # ALL RIGHTS RESERVED
// #
// # Based on a version originally by The_Author and DynaChicken
// # for the ZiNc emulator, subsequently bug-fixed and ported to
// # MAME by R Belmont
// #
// ###########################################################################

#include "EDC.h"

#ifndef h83002H
#define h83002H

#define DASMMAXSTRINGS 20
#define DASMSTRINGLENGTH 80
///////////////////////////////////////////////////////////////////////
//
// 	Flag Masks
//
///////////////////////////////////////////////////////////////////////

#define IFLAG     0x80
#define UIFLAG    0x40
#define HFLAG     0x20
#define UFLAG     0x10
#define NFLAG     0x08
#define ZFLAG     0x04
#define VFLAG     0x02
#define CFLAG     0x01

#define INITIALMODE 4 //16 bit bus, 16MB Address Space

///////////////////////////////////////////////////////////////////////
//
// 	On-Board Peripheral Addresses
//
///////////////////////////////////////////////////////////////////////
//0x10 thru 0x1f Epoch specific system registers all others are on the CPU itself

//Epoch System Reg
#define	EPSYSSTT			0x10 // Epoch System status register

#define	GEMODE				0x08	// Game memory mode selection
#define	ENBACC				0x40	// Primary uP access to emulator RAM enabled

//Epoch IO Control / Status
#define	EPIOCNTL			0x11 // Epoch I/O control/status register

#define	TRGRES				0x40	// force primary and boot reset
#define	MASRES				0x80	// force primary, boot and security reset
#define SSLED				0x2		// system status LED
//Epoch IO Mode
#define	EPIOMODE			0x12 // Epoch I/O Mode register

//Epoch System Control
#define	EPSYSCTL			0x13 // Epoch  System control register

#define	WATCHDOG			0x01	// Watchdog bit
#define	ENVRAM				0x02	// NV Ram enable bit
#define	EIOLNK				0x04	// I/O link enable bit

#define	EMDMLT				0x20	// multi-plane matrix enable
#define	EMD2PL				0x40	// 2nd matrix plane enable
#define	EMDSYN				0x80	// force matrix display sync

//Epoch Interrupt Enable 
#define	EPINTENB			0x14 // Epoch Interrupt enable register

//Epoch Status Reg
#define	EPINTSTT			0x15 // Epoch Interrupt status register

#define INTINPUT			0x01	// Inputs Interrupt Flag
#define INTAUDIO			0x02	// Audio Interrupt Flag
#define INTI2C				0x04	// I2C Interrupt Flag
#define	INTSYNC				0x08	// Sync Interrupt Flag
#define	INTFUNCSW			0x10	// Function Switch bit
#define	INTRFRSH			0x20	// Refresh Interrupt Flag
#define INTFRAME			0x40	// Frame Interrupt Flag
#define	INTMATRIX			0x80    // Dot Matrix interrupt flag

//Epoch Debug
#define DEBUGPORT			0x17 // Temp Debug Hack

//Epoch I2C Control
#define	EPI2CCTL			0x18 // Epoch I2C control register

#define	I2CSEL				0x01	// interface select bit
#define	I2CERROR			0x40	// error on interface bit
#define	I2CREADY			0x80	// interface ready bit
//Epoch I2C Data
#define	EPI2CDAT			0x19 // Epoch I2C data register

#define EPOCHUNKOWNREAD		0x1a  //Not sure yet
//Epoch FPGA Version		
#define EPFPGAVER			0x1b // Epoch FPGA Version register -Read Only?

#define FPGAVERSION			0x02 //Version 2


// Direct Memory Access Controller
#define MAR0AR		0x20 //Memory Address Register 0AR
#define MAR0AE		0x21 //Memory Address Register 0AE
#define MAR0AH		0x22 //Memory Address Register 0AH
#define MAR0AL		0x23 //Memory Address Register 0AL
#define ETCR0AH		0x24 //Execute Transfer Count Register
#define ETCR0AL		0x25
#define IOAR0A		0x26
#define DTCR0A		0x27
#define MAR0BR		0x28
#define MAR0BE		0x29
#define MAR0BH		0x2a
#define MAR0BL		0x2b
#define ETCR0BH		0x2c
#define ETCR0BL		0x2d
#define IOAR0B		0x2e
#define DTCR0B		0x2f
#define MAR1AR		0x30
#define MAR1AE		0x31
#define MAR1AH		0x32
#define MAR1AL		0x33
#define ETCR1AH		0x34
#define ETCR1AL		0x35
#define IOAR1A		0x36
#define DTCR1A		0x37
#define MAR1BR		0x38
#define MAR1BE		0x39
#define MAR1BH		0x3a
#define MAR1BL		0x3b
#define ETCR1BH		0x3c
#define ETCR1BL		0x3d
#define IOAR1B		0x3e
#define DTCR1B		0x3f

//Timer Unit
#define TSTR		0x60 //Timer Start
#define TSNC		0x61 //Timer Synchro
#define TMDR		0x62 //Timer Mode
#define TFCR		0x63 //Timer Function Control
// Timer 0
#define TCR0	  	0x64 //Timer Control Reg 0
#define TIOR0		0x65 //Timer IO Control Reg 0
#define TIER0	  	0x66 //Timer Interrupt Enable Reg 0
#define TSR0	  	0x67 //Timer Status Reg 0
#define TCNT0H		0x68 //Timer Counter 0 (High)
#define TCNT0L		0x69 //Timer Counter 0 (Low)
#define GRA0H		0x6a //General Register A0 (High)
#define GRA0L		0x6b //General Register A0 (Low)
#define GRB0H		0x6c //General Register B0 (High)
#define GRB0L		0x6d //General Register B0 (Low)
// Timer 1
#define TCR1		0x6e //Timer Control Reg 1
#define TIOR1		0x6f //Timer IO Control Reg 1
#define TIER1	  	0x70 //Timer Interrupt Enable Reg 1
#define TSR1	  	0x71 //Timer Status Reg 1
#define TCNT1H		0x72 //Timer Counter 1 (High)
#define TCNT1L		0x73 //Timer Counter 1 (Low)
#define GRA1H		0x74 //General Register A1 (High)
#define GRA1L		0x75 //General Register A1 (Low)
#define GRB1H		0x76 //General Register B1 (High)
#define GRB1L		0x77 //General Register B1 (Low)
// Timer 2
#define TCR2		0x78 //Timer Control Reg 2
#define TIOR2		0x79 //Timer IO Control Reg 2
#define TIER2	  	0x7a //Timer Interrupt Enable Reg 2
#define TSR2	  	0x7b //Timer Status Reg 2
#define TCNT2H		0x7c //Timer Counter 2 (High)
#define TCNT2L		0x7d //Timer Counter 2 (Low)
#define GRA2H		0x7e //General Register A2 (High)
#define GRA2L		0x7f //General Register A2 (Low)
#define GRB2H		0x80 //General Register B2 (High)
#define GRB2L		0x81 //General Register B2 (Low)
// Timer 3
#define TCR3		0x82 //Timer Control Reg 3
#define TIOR3		0x83 //Timer IO Control Reg 3
#define TIER3	  	0x84 //Timer Interrupt Enable Reg 3
#define TSR3	  	0x85 //Timer Status Reg 3
#define TCNT3H		0x86 //Timer Counter 3 (High)
#define TCNT3L		0x87 //Timer Counter 3 (Low)
#define GRA3H		0x88 //General Register A3 (High)
#define GRA3L		0x89 //General Register A3 (Low)
#define GRB3H		0x8a //General Register B3 (High)
#define GRB3L		0x8b //General Register B3 (Low)
#define BRA3H		0x8c //Buffer register A3 (High)
#define BRA3L		0x8d //Buffer register A3 (Low)
#define BRB3H		0x8e //Buffer register B3 (High)
#define BRB3L		0x8f //Buffer register B3 (Low)
// Timer Output
#define TOER		0x90 //Timer Output Master Enable Reg
#define TOCR		0x91 //Timer Output Control Reg
// Timer 4
#define TCR4		0x92 //Timer Control Reg 4
#define TIOR4		0x93 //Timer IO Control Reg 4
#define TIER4	  	0x94 //Timer Interrupt Enable Reg 4
#define TSR4	  	0x95 //Timer Status Reg 4
#define TCNT4H		0x96 //Timer Counter 4 (High)
#define TCNT4L		0x97 //Timer Counter 4 (Low)
#define GRA4H		0x98 //General Register A4 (High)
#define GRA4L		0x99 //General Register A4 (Low)
#define GRB4H		0x9a //General Register B4 (High)
#define GRB4L		0x9b //General Register B4 (Low)
#define BRA4H		0x9c //Buffer register A4 (High)
#define BRA4L		0x9d //Buffer register A4 (Low)
#define BRB4H		0x9e //Buffer register B4 (High)
#define BRB4L		0x9f //Buffer register B4 (Low)

//Timer Status Register Bits
#define OVF			0x04 //Overflow Flag
#define IMFB		0x02 //Match B Flag
#define IMFA		0x01 //Match A Flag

//Timer Interrupt Enable Register Bits
#define OVIE		0x04 //Overflow Flag
#define IMIEB		0x02 //Match B Flag
#define IMIEA		0x01 //Match A Flag


//Programmable Timing Pattern Controller - Unused
#define TPMR		0xa0 //Output Mode reg
#define TPCR		0xa1 //Output Control reg
#define NDERB		0xa2 //Next Data Enable Reg B
#define NDERA		0xa3 //Next Data Enable Reg A
#define NDRB		0xa4 //Next Data Reg B * optionally 2 possible addresses, see docs
//#define NDRB		0xa6 //Next Data Reg B * optionally 2 possible addresses, see docs
#define NDRA		0xa5 //Next Data Reg A * optionally 2 possible addresses, see docs
//#define NDRA		0xa7 //Next Data Reg A * optionally 2 possible addresses, see docs

//Watchdog Timer
#define TCSR		0xa8 //Timer Control / Status Reg	
#define TCNT		0xa9 //Timer Counter
#define RSTCSR		0xab //Reset Control / Status Reg

// these are "special" write-protected regs from RSTCSR
#define RSTOE		0xaa //Reset Ouput Enable
#define WRST		0xab //Watchdog Timer Reset

//Refresh Controller
#define RFSHCR		0xac //Refresh control register
#define RTMCSR		0xad //Refresh timer control / status register
#define RTCNT		0xae //Refresh timer counter
#define RTCOR		0xaf //Refresh time constant register

//Serial Control Interface - SCI
#define SMR0		0xb0 //Channel 0 Serial Mode Register
#define BRR0		0xb1 //Channel 0 Bit Rate Register
#define SCR0		0xb2 //Channel 0 Serial Control Register
#define TDR0		0xb3 //Channel 0 Transmit Data Register
#define SSR0		0xb4 //Channel 0 Serial Status Register
#define RDR0		0xb5 //Channel 0 Receive Data Register
//Cannot be written via cpu internal only - adresess may be wrong way round
#define STSR0        0xb6 //Transmit Shift register
#define SRSR0        0xb7 //Receive Shift register

#define SMR1		0xb8 //Channel 1 Serial Mode Register
#define BRR1		0xb9 //Channel 1 Bit Rate Register
#define SCR1		0xba //Channel 1 Serial Control Register
#define TDR1		0xbb //Channel 1 Transmit Data Register
#define SSR1		0xbc //Channel 1 Serial Status Register
#define RDR1		0xbd //Channel 1 Receive Data Register
//Cannot be written via cpu internal only - adresess may be wrong way round
#define STSR1        0xbe //Transmit Shift register
#define SRSR1        0xbf //Receive Shift register

	

//Serial Control Register Bits
#define TIE			0x80 //Transmit Interrupt Enable
#define RIE			0x40 //Receive Interrupt Enable
#define TE			0x20 //Transmit Enable
#define RE			0x10 //Receive Enable
#define MPIE		0x08 //Multiprocessor Interrupt Enable
#define TEIE		0x04 //Transmit End Interrupt Enable
#define CKE1		0x02 //Clock Enable
#define CKE2		0x01 //Clock Enable

//Serial Status Register Bits
#define TDRE		0x80 //Transmit Data Register Empty
#define RDRF		0x40 //Receive Data Register Full
#define ORER		0x20 //Overrun Error
#define FER			0x10 //Framing Error
#define PER			0x08 //Parity Error
#define TEND		0x04 //Transmit End
#define MPB			0x02 //Multiprocessor Bit
#define MPBT		0x01 //Multi Processor Bit Transfer

//Serial Mode Register Bits
#define CA			0x80 //Communication Mode
#define CHR			0x40 //Character Length
#define PE			0x20 //Parity Enable
#define OE			0x10 //Parity Mode
#define STOP		0x08 //Stop Bit Length
#define MP			0x04 //Multiprocessor Mode
#define CKS1		0x02 //Clock Select 1 - Prescale
#define CKS0		0x01 //Clock Select 0

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

// Analogue to Digital Converter
#define ADDRAH		0xe0 //A/D data reg A (High)
#define ADDRAL		0xe1 //A/D data reg A (Low)
#define ADDRBH		0xe2 //A/D data reg B (High)
#define ADDRBL		0xe3 //A/D data reg B (Low)
#define ADDRCH		0xe4 //A/D data reg C (High)
#define ADDRCL		0xe5 //A/D data reg C (Low)
#define ADDRDH		0xe6 //A/D data reg D (High)
#define ADDRDL		0xe7 //A/D data reg D (Low)
#define ADCSR		0xe8 //A/D Control / Status Reg
#define ADCR		0xe9 //A/D Control Reg

// BUS CONTROLLER
#define ABWCR		0xec //Bus Width Control reg
#define ASTCR		0xed //Access State Control reg
#define WCR			0xee //Wait Control reg
#define WCER		0xef //Wait State Controller Enable Reg

//MEMORY ACCESS TYPES (Timing)
#define ON_CHIP_MEMORY 1
#define ON_CHIP_MODULE 2
#define MEMORY_AREA	   3
#define INTERNAL_OP	   4

// SYSTEM REGS
#define MDCR		0xf1 //Mode Control Register

#define SYSCR		0xf2 //System Control Reg
	//SYSTEM CTRL REG PINS
#define RAME		0x1
//#define RESERVED	0x2
#define NMIEG		0x4
#define UE			0x8
#define STS0		0x10
#define STS1		0x20
#define STS2		0x40
#define SSBY		0x80

#define BRCR		0xf3 //Bus Release Control reg - Bus Controller

//Interrupt Controller
#define ISCR		0xf4 //Interrupt Sense Control Register
#define IER			0xf5 //Interrupt Enable Register
#define ISR			0xf6 //Interrupt Status Register
#define IPRA		0xf8 //Interrupt Priority Register A
#define IPRB		0xf9 //Interrupt Priority Register b

//IRQ Helpers
#define IRQ0VAL 12
#define IRQ1VAL 13
#define IRQ2VAL 14
#define IRQ3VAL 15
#define IRQ4VAL 16
#define IRQ5VAL 17

//
#define IRQ0BIT 0x01
#define IRQ1BIT 0x02
#define IRQ2BIT 0x04
#define IRQ3BIT 0x08
#define IRQ4BIT 0x10
#define IRQ5BIT 0x20

///////////////////////////////////////////////////////////////////////
//
// 	Type Definitions
//
///////////////////////////////////////////////////////////////////////

typedef signed char     	INT8;
typedef UINT8   			UINT8;
typedef signed short		INT16;
typedef unsigned short  	UINT16;
typedef signed int			INT32;
typedef unsigned int    	UINT32;
typedef signed __int64  	INT64;
typedef unsigned __int64	UINT64;

///////////////////////////////////////////////////////////////////////
//
// 	CPU Variable Structure
//
///////////////////////////////////////////////////////////////////////

typedef struct CPU {

	// main CPU stuff
	UINT32	err = 0, 
			StackVal = 0,
			regs[8],
			pc = 0,
			ppc = 0;

	UINT8
		ccr = 0,
		h8nflag = 0,
		h8vflag = 0,
		h8cflag = 0,
		h8zflag = 0,
		h8iflag = 0,
		h8hflag = 0,
		h8uflag = 0,
		h8uiflag = 0,

		SPSFlag = 0,  //Stack pointer set flag
		

		// H8/3002 onboard peripheral status bytes
		h8PREVTSR0 = 0,
		h8PREVTSR1 = 0,
		h8PREVTSR2 = 0,
		h8PREVTSR3 = 0,
		h8PREVTSR4 = 0,	// Timer Previous Status Regs

		// SCI Comms byte state (2 Channels)
		h8PREVSSR0 = 0,
		h8PREVSSR1 = 0,			//Previous Serial Status Register
		SCIBitNum0 = 0,
		SCIBitNum1 = 0,			//Number of Bits to send
		SCIBitPoint0 = 0,
		SCIBitPoint1 = 0,		//Bit Pointers

		per_regs[256],
		onchip_ram[0x200];

	UINT16	
		SCIBitBuffer0 = 0,
		SCIBitBuffer1 = 0,	//Output Bit buffer		
			
		h8WDTPrescale = 0;	// Watchdog Timer register

	UINT32
		h8TPrescale0 = 0,
		h8TPrescale1 = 0,
		h8TPrescale2 = 0,
		h8TPrescale3 = 0,
		h8TPrescale4 = 0,	// Timer prescaler cycle accumulators
		SCIClock0 = 0,
		SCIClock1 = 0,			// SCI baud clock counters
		SCIPrescale0 = 0,
		SCIPrescale1 = 0;		// SCI prescale buffers

} h83002_state;

///////////////////////////////////////////////////////////////////////
//
// 	Registers Enumeration
//
///////////////////////////////////////////////////////////////////////

enum {
	H8_E0 = 1,
	H8_E1,
	H8_E2,
	H8_E3,
	H8_E4,
	H8_E5,
	H8_E6,
	H8_E7,
	H8_PC,
	H8_CCR
};


///////////////////////////////////////////////////////////////////////
//
// 	External Interrupt Lines Enumeration
//
///////////////////////////////////////////////////////////////////////

enum {
	H8_IRQ0 = 0,
	H8_IRQ1,
	H8_IRQ2,
	H8_IRQ3,
	H8_IRQ4,
	H8_IRQ5
};

///////////////////////////////////////////////////////////////////////
//
// 	I/O Ports Enumeration
//
///////////////////////////////////////////////////////////////////////

enum {
	// Digital I/O Ports
	H8_PORT4 = 0,			// 0x00
	H8_PORT6,				// 0x01
	H8_PORT7,				// 0x02
	H8_PORT8,				// 0x03
	H8_PORT9,				// 0x04
	H8_PORTA,				// 0x05
	H8_PORTB,				// 0x06

	// Analog Inputs
	H8_ADC_0_L = 0x10,	   // 0x10
	H8_ADC_0_H,				// 0x11
	H8_ADC_1_L,				// 0x12
	H8_ADC_1_H,				// 0x13
	H8_ADC_2_L,				// 0x14
	H8_ADC_2_H,				// 0x15
	H8_ADC_3_L,				// 0x16
	H8_ADC_3_H,				// 0x17

	// Serial Ports
	H8_SERIAL_A = 0x20,	// 0x20
	H8_SERIAL_B			// 0x21
};

///////////////////////////////////////////////////////////////////////
//
// 	Main H83002 Class Definition
//
///////////////////////////////////////////////////////////////////////

class H83002 {
public:
		h83002_state 	h8;
		H83002();
		~H83002();

protected:

		INT32 			
			h8_cyccnt = 0;

		UINT8 
			fetchValue = 0,
			stackValue = 0,
			branchValue = 0,
			byteValue = 0,
			wordValue = 0,
			h8_sleeping = 0,
			h8_suppress_irq_once = 0;
		
		// Used internally for opcode processing
		UINT16 getNextOpcode();

		

		// CPU functions	

		void 			h8_group0new(UINT16 opcode);
		void 			h8_group1new(UINT16 opcode);
		void 			h8_group5new(UINT16 opcode);
		void 			h8_group6new(UINT16 opcode);
		void 			h8_group7new(UINT16 opcode);

		void 			h8_table2(UINT16 opcode);
		void 			h8_table3(UINT16 opcode, UINT16 opcode2);

		inline UINT8 	h8_add8(UINT8 src, UINT8 dst);
		inline UINT16 	h8_add16(UINT16 src, UINT16 dst);
		inline UINT32 	h8_add32(UINT32 src, UINT32 dst);
		inline UINT8 	h8_addx8(UINT8 src, UINT8 dst);
		inline UINT8 	h8_and8(UINT8 src, UINT8 dst);
		inline UINT16 	h8_and16(UINT16 src, UINT16 dst);
		inline UINT32 	h8_and32(UINT32 src, UINT32 dst);
		inline void		h8_band8(UINT8 src, UINT8 dst);		// Result in Carry
		inline UINT8 	h8_bclr8(UINT8 src, UINT8 dst);
		inline void		h8_biand8(UINT8 src, UINT8 dst);	// Result in Carry
		inline void 	h8_bild8(UINT8 src, UINT8 dst); 	// Result in Carry
		inline void 	h8_bior8(UINT8 src, UINT8 dst); 	// Result in Carry
		inline UINT8 	h8_bist8(UINT8 src, UINT8 dst);
		inline void		h8_bixor8(UINT8 src, UINT8 dst);	// Result in Carry
		inline void 	h8_bld8(UINT8 src, UINT8 dst); 		// Result in Carry
		inline UINT8 	h8_bnot8(UINT8 src, UINT8 dst);
		inline void 	h8_bor8(UINT8 src, UINT8 dst); 		// Result in Carry

		inline int 		h8_branch(UINT8 condition);

		inline UINT8 	h8_bset8(UINT8 src, UINT8 dst);
		inline UINT8 	h8_bst8(UINT8 src, UINT8 dst);
		inline void 	h8_btst8(UINT8 src, UINT8 dst);
		inline void		h8_bxor8(UINT8 src, UINT8 dst);		// Result in Carry

		inline void 	h8_cmp8(UINT8 src, UINT8 dst);
		inline void 	h8_cmp16(UINT16 src, UINT16 dst);
		inline void 	h8_cmp32(UINT32 src, UINT32 dst);

		inline UINT8 	h8_dec8(UINT8 src);
		inline UINT16 	h8_dec16(UINT16 src);
		inline UINT32 	h8_dec32(UINT32 src);

		inline UINT16 	h8_divxs8(INT8 src, INT16 dst);
		inline UINT32 	h8_divxs16(INT16 src, INT32 dst);

		inline UINT16 	h8_divxu8(UINT16 dst, UINT8 src);
		inline UINT32 	h8_divxu16(UINT32 dst, UINT16 src);

		inline UINT8	h8_daa8(UINT8 src);
		inline UINT8	h8_das8(UINT8 src);

		inline UINT8 	h8_inc8(UINT8 src);
		inline UINT16 	h8_inc16(UINT16 src, UINT8 incVal);
		inline UINT32 	h8_inc32(UINT32 src, UINT8 incVal);

		inline UINT8 	h8_mov8(UINT8 src);
		inline UINT16 	h8_mov16(UINT16 src);
		inline UINT32 	h8_mov32(UINT32 src);

		inline INT16 	h8_mulxs8(INT8 src, INT8 dst);
		inline INT32 	h8_mulxs16(INT16 src, INT16 dst);

		inline INT8 	h8_neg8(INT8 src);
		inline INT16 	h8_neg16(INT16 src);
		inline INT32 	h8_neg32(INT32 src);

		inline UINT8 	h8_not8(UINT8 src);
		inline UINT16 	h8_not16(UINT16 src);
		inline UINT32 	h8_not32(UINT32 src);

		inline UINT8 	h8_or8(UINT8 src, UINT8 dst);
		inline UINT16 	h8_or16(UINT16 src, UINT16 dst);
		inline UINT32 	h8_or32(UINT32 src, UINT32 dst);

		inline UINT8 	h8_rotl8(UINT8 src);
		inline UINT16 	h8_rotl16(UINT16 src);
		inline UINT32 	h8_rotl32(UINT32 src);

		inline UINT8 	h8_rotr8(UINT8 src);
		inline UINT16 	h8_rotr16(UINT16 src);
		inline UINT32 	h8_rotr32(UINT32 src);

		inline UINT8 	h8_rotxl8(UINT8 src);
		inline UINT16 	h8_rotxl16(UINT16 src);
		inline UINT32 	h8_rotxl32(UINT32 src);

		inline UINT8 	h8_rotxr8(UINT8 src);
		inline UINT16 	h8_rotxr16(UINT16 src);
		inline UINT32 	h8_rotxr32(UINT32 src);

		inline UINT8 	h8_shal8(UINT8 src);
		inline UINT16 	h8_shal16(UINT16 src);
		inline UINT32 	h8_shal32(UINT32 src);

		inline UINT8 	h8_shar8(UINT8 src);
		inline UINT16 	h8_shar16(UINT16 src);
		inline UINT32 	h8_shar32(UINT32 src);

		inline UINT8 	h8_shll8(UINT8 src);
		inline UINT16 	h8_shll16(UINT16 src);
		inline UINT32 	h8_shll32(UINT32 src);

		inline UINT8 	h8_shlr8(UINT8 src);
		inline UINT16 	h8_shlr16(UINT16 src);
		inline UINT32 	h8_shlr32(UINT32 src);

		inline UINT8 	h8_sub8(UINT8 src, UINT8 dst);
		inline UINT16 	h8_sub16(UINT16 src, UINT16 dst);
		inline UINT32 	h8_sub32(UINT32 src, UINT32 dst);

		inline UINT8 	h8_subx8(UINT8 src, UINT8 dst);

		inline UINT8 	h8_xor8(UINT8 src, UINT8 dst);
		inline UINT16 	h8_xor16(UINT16 src, UINT16 dst);
		inline UINT32 	h8_xor32(UINT32 src, UINT32 dst);

		// Register Access etc.		
		void 			h8_set_ccr8(UINT8 data);
		UINT8 			h8_getreg8(UINT8 reg);
		void 			h8_setreg8(UINT8 reg, UINT8 data);
		UINT16 			h8_getreg16(UINT8 reg);
		void 			h8_setreg16(UINT8 reg, UINT16 data);
		UINT32 			h8_getreg32(UINT8 reg);
		void 			h8_setreg32(UINT8 reg, UINT32 data);
		//int 			h8_default_irq_callback(int irqline);
		void 			h8_onstateload(void);

		// DMAC 
		UINT8 			h8_dmac_read8(UINT8 reg);
		void 			h8_dmac_write8(UINT8 reg, UINT8 val);
		void 			h8_dmac_reset(void);

		// Integrated Timer
		UINT8 			h8_itu_read8(UINT8 reg);
		void 			h8_itu_write8(UINT8 reg, UINT8 val);
		void 			h8_itu_tick(int cycles);
		void 			h8_itu_reset(void);

		// TPC 
		UINT8 			h8_tpc_read8(UINT8 reg);
		void 			h8_tpc_write8(UINT8 reg, UINT8 val);
		void 			h8_tpc_reset(void);

		// WDT 
		void			h8_wdt_reset(void);
		UINT8			h8_wdt_read8(UINT8 reg);
		void 			h8_wdt_tick(int cycles);
		void			h8_wdt_write8(UINT8 reg, UINT8 val);
		
		// Refresh Controller
		void 			h8_rc_tick(int tnum);
		UINT8 			h8_rc_read8(UINT8 reg);
		void 			h8_rc_write8(UINT8 reg, UINT8 val);
		void 			h8_rc_reset(void);
		
		// SCI 
		void 			h8_sci_tick(int tnum);
		UINT8 			h8_sci_read8(UINT8 reg);
		void 			h8_sci_write8(UINT8 reg, UINT8 val);
		void 			h8_sci_reset(void);

		// IO
		UINT8 			h8_io_read8(UINT8 reg);
		void 			h8_io_write8(UINT8 reg, UINT8 val);
		void 			h8_io_reset(void);

		// ADC
		UINT8 			h8_adc_read8(UINT8 reg);
		void 			h8_adc_write8(UINT8 reg, UINT8 val);
		void 			h8_adc_reset(void);

		//Bus Controller
		UINT8 			h8_bc_read8(UINT8 reg);
		void 			h8_bc_write8(UINT8 reg, UINT8 val);
		void 			h8_bc_reset(void);
		
		//System Regs
		UINT8 			h8_sys_reg_read8(UINT8 reg);
		void 			h8_sys_reg_write8(UINT8 reg, UINT8 val);
		void 			h8_sys_reg_reset(void);

		//Interrupt Controller
		UINT8 			h8_ic_read8(UINT8 reg);
		void 			h8_ic_write8(UINT8 reg, UINT8 val);
		void 			h8_ic_reset(void);

		UINT8			IRQ_Map[37];
		UINT8			H8PrevISR;
		int 			h8_get_priority_level(UINT8 IRQNum);
		void 			h8_check_irqs(void);
		int 			h8_get_real_irq_Value(int);
		void			h8_get_irq_map(void);
		void 			h8_GenException(UINT8 vectornr);

		//Epoch
		void 			h8_epoch_irq_update();
		

		EDCUNIT					fEDC;

		// dasm & debugging
		int Dbg = 0;

public:
	
	void 			reset();	
	//virtual int 	execute(UINT32 totalCycles);
	virtual int 	executeNew(UINT32 totalCycles);
	
	void			h8_timing_init();
	void			h8_timing_fetch(UINT8 Access, UINT8);
	void			h8_timing_branch(UINT8 Access, UINT8);
	void			h8_timing_stack(UINT8 Access, UINT8);
	void			h8_timing_byte(UINT8 Access, UINT8);
	void			h8_timing_word(UINT8 Access, UINT8);
	void			h8_timing_internal(UINT8);
	void 			h8_tick(int cycles);
	UINT8 			h8_register_read8(UINT32 address);
	void 			h8_register_write8(UINT32 address, UINT8 val);
	UINT8			AccessType = 0;

	virtual UINT16  __fastcall program_read_word(UINT32) = 0;
	virtual UINT8 	__fastcall program_read_byte(UINT32) = 0;
	virtual void 	__fastcall program_write_word(UINT32, UINT16) = 0;
	virtual void 	__fastcall program_write_byte(UINT32, UINT8) = 0;
	virtual UINT16  __fastcall cpu_readop16(UINT32) = 0;
	virtual UINT8 	__fastcall io_read_byte_8(UINT32) = 0;
	virtual void 	__fastcall io_write_byte_8(UINT32, UINT8) = 0;
	virtual UINT32 	 h8_mem_read32(UINT32 address) = 0;
	virtual void 	 h8_mem_write32(UINT32 address, UINT32 data) = 0;

	UINT8  h8_mem_read8(UINT32);
	UINT16  h8_mem_read16(UINT32);
	void  h8_mem_write8(UINT32, UINT8);
	void  h8_mem_write16(UINT32, UINT16);

	UINT8  h8_get_ccr(void);

	//DASM Public functions
	void dasmInitialize(UINT8 Terminator, UINT32 Size);	//Initialize DASM
	void dasmExecute(UINT32 address, UINT32 numIns, UINT32 totalCycles);		//Does all Dasm


private:
	
	//Handling
	void dasmCreateEntry();							//Create a single line
	void dasmDestroyEntries();						//Destroy all line entries

	//Output string functions
	void setDasmOpcode(wchar_t * str);					//Set the opcode mnemonic string
	void setDasmOpcodeBytes(UINT8 num, UINT16 op1, UINT16 op2 = 0, UINT16 op3 = 0, UINT16 op4 = 0, UINT16 op5 = 0);//Set the opcode bytes to be displayed in string
	void setDasmAddress(UINT32 addr);				//Set the opcode address
	void setDasmAddressMode(wchar_t * str);				//Set the opcode addessing mode string
	void setDasmInstructionCycles(UINT32 cycles);	//Set the opcode cycles exectued this instruction
	void setDasmTotalCycles(UINT32 cycles);			//Set the Total Cycles Executed
	void dasmClearLine();

	//Options
	void setDasmTermination(UINT8 term);			//Set the termination value (0 = no termination)
	void setDasmSize(UINT32 size);					//Set the number of dasm lines to be displayed 

	//Dasm Opcode Groups
	void dasmGroup0();	
	void dasmGroup1();
	void dasmGroup2();
	void dasmGroup3();
	void dasmGroup4();
	void dasmGroup5();
	void dasmGroup6();
	void dasmGroup7();

	
	UINT32 
		dSize = 0, 
		dTerminator = 0, 
		dCurrent = 0, 
		dCycles = 0,
		dInCycles = 0,
		dasmPC;
	
	UINT8
		
		dasmOpBytes[8];
		
	bool enableDASM = false;
	wchar_t* dStrings[DASMMAXSTRINGS];
};
#endif
