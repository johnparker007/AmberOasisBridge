#include "stdafx.h"
#include "EDC.h"
#include <cstdio>

EDCUNIT::EDCUNIT() {
	ZeroMemory(Buf, EDCBUFFERSIZE * sizeof(char));
}

EDCUNIT::~EDCUNIT() {

}

void EDCUNIT::SaveState() {

}

void EDCUNIT::LoadState() {

}

char* __fastcall EDCUNIT::getEDCString() {
	return NULL;
}

void __fastcall EDCUNIT::Write(UINT8 ByteIn) {
	
	FILE* EdcFile;
	fopen_s(&EdcFile, "EDC.txt", "a");
	bool chk = false;

	if (Checksum == ByteIn)
	{
		chk = true;
	}
	Checksum = ByteIn;

	if (chk) {		
		if (Mode == 0)
		{
			Mode = ByteIn;
		}
		else
		{
			switch (Mode) {
			case 0x2B://Cash Door Open							
				fprintf(EdcFile, "Cash Door Open \n");				
				Mode = 0;
				break;
			case 0x2C://Cash Door Closed				
				fprintf(EdcFile, "Cash Door Closed \n");				
				Mode = 0;
				break;
			case 0x2D://Service Door Open				
				fprintf(EdcFile, "Service Door Open \n");				
				Mode = 0;
				break;
			case 0x2E://Service Door Closed				
				fprintf(EdcFile, "Service Door Closed \n");				
				Mode = 0;
				break;
			case 0x2F://VTP 10p Units
				fprintf(EdcFile, "VTP++ \n");
				Mode = 0;
				break;
			case 0x60://Primary Machine Message
				if (SaveLength == 0) {
					Length = 0;
					SaveLength = ByteIn;
				}
				else {
					UINT8 ManName[4];
					UINT8 MachName[5];
					UINT8 Protocol;

					Buf[Length] = ByteIn;
					Length++;
					if (Length == (SaveLength)) {
						Buf[Length] = 0;
						Mode = 0;
						SaveLength = 0;
						ManName[0] = Buf[0];
						ManName[1] = Buf[1];
						ManName[2] = Buf[2];
						ManName[3] = 0;
						Protocol = Buf[3];
						MachName[0] = Buf[4];
						MachName[1] = Buf[5];
						MachName[2] = Buf[6];
						MachName[3] = Buf[7];
						MachName[4] = 0;

						fprintf(EdcFile, "Primary: \n");
						fprintf(EdcFile, "    Man ID: %s \n", ManName);
						if (Protocol == 'N') {
							fprintf(EdcFile, "    Protocol: No Data \n");
						}
						else if (Protocol == 'P') {
							fprintf(EdcFile, "    Protocol: Data Usable \n");
						}

						fprintf(EdcFile, "    Machine ID: %s \n", MachName);
					}
				}
				break;

			case 0x61://Float level Message
				if (SaveLength == 0) {
					Length = 0;
					SaveLength = ByteIn;
				}
				else {
					Buf[Length] = ByteIn;
					Length++;
					if (Length == (SaveLength)) {
						Buf[Length] = 0;
						Mode = 0;
						SaveLength = 0;
						fprintf(EdcFile, "Float Level: %s \n", Buf);
					}
				}
				break;

			case 0x62://Secondary Machine Message

				if (SaveLength == 0) {
					Length = 0;
					SaveLength = ByteIn;
				}
				else {
					UINT8 Version[4];
					UINT8 Percent[4];

					Buf[Length] = ByteIn;
					Length++;
					if (Length == (SaveLength)) {
						Buf[Length] = 0;
						Mode = 0;
						SaveLength = 0;

						Version[0] = Buf[0];
						Version[1] = Buf[1];
						Version[2] = Buf[2];
						Version[3] = 0;
						Percent[0] = Buf[4];
						Percent[1] = Buf[5];
						Percent[2] = Buf[6];
						Percent[3] = 0;
						fprintf(EdcFile, "Secondary: \n");

						fprintf(EdcFile, "    Version: %s \n", Version);

						switch (Buf[3]) {//Payout Type
						case 'T': fprintf(EdcFile, "    Payout Type: Cash & Token \n");	break;
						case 'C': fprintf(EdcFile, "    Payout Type: Cash Only \n");	break;
						case 'X': fprintf(EdcFile, "    Payout Type: Not Applicable \n"); break;
						default:  fprintf(EdcFile, "    Payout Type: Unknown or Invalid \n"); break;
						}

						fprintf(EdcFile, "    Percent: %s", Percent);
						fprintf(EdcFile, "%% \n");
						switch (Buf[7]) {//Machine Type
						case 'A': fprintf(EdcFile, "    Machine Type: AWP \n");	break;
						case 'B': fprintf(EdcFile, "    Machine Type: All cash \n"); break;
						case 'C': fprintf(EdcFile, "    Machine Type: Club \n"); break;
						case 'D': fprintf(EdcFile, "    Machine Type: Casino \n"); break;
						case 'S': fprintf(EdcFile, "    Machine Type: SWP \n");	break;
						case 'V': fprintf(EdcFile, "    Machine Type: Video \n"); break;
						case 'J': fprintf(EdcFile, "    Machine Type: Jukebox \n");	break;
						case 'P': fprintf(EdcFile, "    Machine Type: Pool \n"); break;
						case 'X': fprintf(EdcFile, "    Machine Type: Other \n"); break;
						default:  fprintf(EdcFile, "    Machine Type: Unknown or Invalid \n"); break;
						}

						fprintf(EdcFile, "    Stake: %i", Buf[8]);
						fprintf(EdcFile, "p \n");

						switch (Buf[9]) {//Payout Type
						case 'P': fprintf(EdcFile, "    Diversion Type: Passive \n"); break;
						case 'A': fprintf(EdcFile, "    Diversion Type: Active \n"); break;
						case 'X': fprintf(EdcFile, "    Diversion Type: Not Applicable \n"); break;
						default:  fprintf(EdcFile, "    Diversion Type: Unknown or Invalid \n"); break;
						}
					}
				}


				break;
			case 0x63://Critical Fault Message
				if (SaveLength == 0) {
					Length = 0;
					SaveLength = ByteIn;
				}
				else {
					if (ByteIn == 0) ByteIn = 0x20; //Convert 0 to [SPACE]
					Buf[Length] = ByteIn;
					Length++;
					if (Length == (SaveLength)) {
						Buf[Length] = 0;
						Mode = 0;
						SaveLength = 0;
						fprintf(EdcFile, "Critical Fault: %s \n", Buf);
					}
				}
				break;
			case 0x64://Non Critical Fault Message
				if (SaveLength == 0) {
					Length = 0;
					SaveLength = ByteIn;
				}
				else {
					if (ByteIn == 0) ByteIn = 0x20; //Convert 0 to [SPACE]
					Buf[Length] = ByteIn;
					Length++;
					if (Length == (SaveLength)) {
						Buf[Length] = 0;
						Mode = 0;
						SaveLength = 0;
						fprintf(EdcFile, "Non-Critical Fault: %s \n", Buf);
					}
				}
				break;
			case 0x65://Compliance Message or Game Outcome
				if (SaveLength == 0) {
					Length = 0;
					SaveLength = ByteIn;
				}
				else {
					if (ByteIn == 0) ByteIn = 0x20; //Convert 0 to [SPACE]
					Buf[Length] = ByteIn;
					Length++;
					if (Length == (SaveLength)) {
						Buf[Length] = 0;
						Mode = 0;
						SaveLength = 0;
						fprintf(EdcFile, "Compliance: %s \n", Buf);
					}
				}
				break;

			case 0x66://Variable Data
				if (SaveLength == 0) {
					Length = 0;
					SaveLength = ByteIn;
				}
				else {
					if (ByteIn == 0) ByteIn = 0x20; //Convert 0 to [SPACE]
					Buf[Length] = ByteIn;
					Length++;
					if (Length == (SaveLength)) {
						Buf[Length] = 0;
						Mode = 0;
						SaveLength = 0;
						fprintf(EdcFile, "Variable: %s \n", Buf);
					}
				}
				break;
			case 0: //NULL Char
				Mode = 0;
				fprintf(EdcFile, "NULL Message \n");
				break;
			case 4: //Mode Change
				Mode = 0;
				fprintf(EdcFile, "Mode Change \n");
				break;
			case 7: //Idle Message
				Mode = 0;
				fprintf(EdcFile, "Idle \n");
				break;
			case 0x20:
				//Token Payout To Float
				Mode = 0;
				fprintf(EdcFile, "Token Payout To Float \n");				
				break;
			case 0x21:				
				//10p Payout To Float
				Mode = 0;
				fprintf(EdcFile, "10p Payout To Float \n");				
				break;
			case 0x22:				
				//20p Payout To Float
				Mode = 0;
				fprintf(EdcFile, "20p Payout To Float \n");
				break;
			case 0x23:				
				//50p Payout To Float
				Mode = 0;
				fprintf(EdcFile, "50p Payout To Float \n");				
				break;
			case 0x24:				
				//£1 Payout To Float
				Mode = 0;
				fprintf(EdcFile, "£1 Payout To Float \n");
				break;
			case 0x25:				
				//£2 Payout To Float
				Mode = 0;
				fprintf(EdcFile, "£2 Payout To Float \n");				
				break;
			case 0x26:				
				//£5 Payout To Float
				Mode = 0;
				fprintf(EdcFile, "£5 Payout To Float \n");				
				break;
			case 0x27:				
				//£20 Cash In
				Mode = 0;
				fprintf(EdcFile, "£20 Cash In \n");				
				break;
			case 0x28:				
				//£50 Cash In
				Mode = 0;
				fprintf(EdcFile, "£50 Cash In \n");				
				break;
			case 0x29:				
				//2p Cash In
				Mode = 0;
				fprintf(EdcFile, "2p Cash In \n");				
				break;
			case 0x2a:				
				//2p Cash Out
				Mode = 0;
				fprintf(EdcFile, "2p Cash Out \n");				
				break;
			case 0x30:				
				//5p Cash In
				Mode = 0;
				fprintf(EdcFile, "5p Cash In \n");				
				break;
			case 0x31:				
				//10p Cash In
				Mode = 0;
				fprintf(EdcFile, "10p Cash In \n");				
				break;
			case 0x32:				
				//20p Cash In
				Mode = 0;
				fprintf(EdcFile, "20p Cash In \n");				
				break;
			case 0x33:				
				//50p Cash In
				Mode = 0;
				fprintf(EdcFile, "50p Cash In \n");				
				break;
			case 0x34:				
				//£1 Cash In
				Mode = 0;
				fprintf(EdcFile, "£1 Cash In \n");				
				break;
			case 0x35:
				//£2 Cash In
				Mode = 0;
				fprintf(EdcFile, "£2 Cash In \n");				
				break;
			case 0x36:
				//£5 Cash In
				Mode = 0;
				fprintf(EdcFile, "£5 Cash In \n");
				break;
			case 0x37:
				//£10 Cash In
				Mode = 0;
				fprintf(EdcFile, "£10 Cash In \n");
				break;
			case 0x38:
				//5p Token In
				Mode = 0;
				fprintf(EdcFile, "5p Token In \n");
				break;
			case 0x39:
				//10p Token In
				Mode = 0;
				fprintf(EdcFile, "10p Token In \n");
				break;
			case 0x3a:
				//20p Token In
				Mode = 0;
				fprintf(EdcFile, "20p Token In \n");
				break;
			case 0x3b:
				//50p Token In
				Mode = 0;
				fprintf(EdcFile, "50p Token In \n");
				break;
			case 0x3c:
				//£1 Token In
				Mode = 0;
				fprintf(EdcFile, "£1 Token In \n");
				break;
			case 0x3d:
				//£2 Token In
				Mode = 0;
				fprintf(EdcFile, "£2 Token In \n");
				break;
			case 0x3e:
				//£5 Token In
				Mode = 0;
				fprintf(EdcFile, "£5 Token In \n");
				break;
			case 0x3f:
				//£10 Token In
				Mode = 0;
				fprintf(EdcFile, "£10 Token In \n");
				break;
			case 0x40:
				//5p Cash Out
				Mode = 0;
				fprintf(EdcFile, "5p Cash Out \n");
				break;
			case 0x41:
				//10p Cash Out
				Mode = 0;
				fprintf(EdcFile, "10p Cash Out \n");
				break;
			case 0x42:
				//20p Cash Out
				Mode = 0;
				fprintf(EdcFile, "20p Cash Out \n");
				break;
			case 0x43:
				//50p Cash Out
				Mode = 0;
				fprintf(EdcFile, "50p Cash Out \n");
				break;
			case 0x44:
				//£1 Cash Out
				Mode = 0;
				fprintf(EdcFile, "£1 Cash Out \n");
				break;
			case 0x45:
				//£2 Cash Out
				Mode = 0;
				fprintf(EdcFile, "£2 Cash Out \n");
				break;
			case 0x46:
				//£5 Cash Out
				Mode = 0;
				fprintf(EdcFile, "£5 Cash Out \n");
				break;
			case 0x47:
				//£10 Cash Out
				Mode = 0;
				fprintf(EdcFile, "£10 Cash Out \n");
				break;
			case 0x48:
				//5p Token Out
				Mode = 0;
				fprintf(EdcFile, "5p Token Out \n");
				break;
			case 0x49:
				//10p Token Out
				Mode = 0;
				fprintf(EdcFile, "10p Token Out \n");
				break;
			case 0x4a:
				//20p Token Out
				Mode = 0;
				fprintf(EdcFile, "20p Token Out \n");
				break;
			case 0x4b:
				//50p Token Out
				Mode = 0;
				fprintf(EdcFile, "50p Token Out \n");
				break;
			case 0x4c:
				//£1 Token Out
				Mode = 0;
				fprintf(EdcFile, "£1 Token Out \n");
				break;
			case 0x4d:
				//£2 Token Out
				Mode = 0;
				fprintf(EdcFile, "£2 Token Out \n");
				break;
			case 0x4e:
				//£5 Token Out
				Mode = 0;
				fprintf(EdcFile, "£5 Token Out \n");
				break;
			case 0x4f:
				//£10 Token Out
				Mode = 0;
				fprintf(EdcFile, "£10 Token Out \n");
				break;
			case 0x50:
				//5p Cash Refill
				Mode = 0;
				fprintf(EdcFile, "5p Cash Refill \n");
				break;
			case 0x51:
				//10p Cash Refill
				Mode = 0;
				fprintf(EdcFile, "10p Cash Refill \n");
				break;
			case 0x52:
				//20p Cash Refill
				Mode = 0;
				fprintf(EdcFile, "20p Cash Refill \n");
				break;
			case 0x53:
				//50p Cash Refill
				Mode = 0;
				fprintf(EdcFile, "50p Cash Refill \n");
				break;
			case 0x54:
				//£1 Cash Refill
				Mode = 0;
				fprintf(EdcFile, "£1 Cash Refill \n");
				break;
			case 0x55:
				//£2 Cash Refill
				Mode = 0;
				fprintf(EdcFile, "£2 Cash Refill \n");
				break;
			case 0x56:
				//£5 Cash Refill
				Mode = 0;
				fprintf(EdcFile, "£5 Cash Refill \n");
				break;
			case 0x57:
				//£10 Cash Refill
				Mode = 0;
				fprintf(EdcFile, "£10 Cash Refill \n");
				break;
			case 0x58:
				//5p Token Refill
				Mode = 0;
				fprintf(EdcFile, "5p Token Refill \n");
				break;
			case 0x59:
				//10p Token Refill
				Mode = 0;
				fprintf(EdcFile, "10p Token Refill \n");
				break;
			case 0x5a:
				//20p Token Refill
				Mode = 0;
				fprintf(EdcFile, "20p Token Refill \n");
				break;
			case 0x5b:
				//50p Token Refill
				Mode = 0;
				fprintf(EdcFile, "50p Token Refill \n");
				break;
			case 0x5c:
				//£1 Token Refill
				Mode = 0;
				fprintf(EdcFile, "£1 Token Refill \n");
				break;
			case 0x5d:
				//£2 Token Refill
				Mode = 0;
				fprintf(EdcFile, "£2 Token Refill \n");
				break;
			case 0x5e:
				//£5 Token Refill
				Mode = 0;
				fprintf(EdcFile, "£5 Token Refill \n");
				break;
			case 0x5f:
				//£10 Token Refill
				Mode = 0;
				fprintf(EdcFile, "£10 Token Refill \n");
				break;
			}
		}
	}

	fclose(EdcFile);
}
//void __fastcall		EDCUNIT::Reset(LoadSaveCompressDLLClass* LSCIn) {
	//LSC = LSCIn;
//}

