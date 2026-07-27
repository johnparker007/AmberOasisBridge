#include "stdafx.h"
#include "Meters.h"

#define SEC_DATA 0x60
#define SEC_ACK  0x61
#define SEC_NAK  0x62

UINT8 A = 48;
UINT8 B = 50;
UINT8 C = 69;

UINT8 version[4];

CoinMeter::CoinMeter(){

	int cnt;

	version[0] = A;
	version[1] = B;
	version[2] = C;
	version[3] = NULL;

	for (cnt = 0; cnt < FITTEDMETERS; cnt++){
		On[cnt] = 0;
		TimeOn[cnt] = 0;
		Pin[cnt] = 0;
		PrevPin[cnt] = 0;
		Enable[cnt] = 0;
		Counter[cnt] = 0;
	}	
	//SEC
	Reset();
	SECSwitch = false;


}

CoinMeter::~CoinMeter(){

}

unsigned long CoinMeter::GetCounter(UINT8 Num){
	if (Num >= FITTEDMETERS) return 0;
	return Counter[Num];	
}
void CoinMeter::SetCounter(UINT8 Num, unsigned long Value){
	if (Num >= FITTEDMETERS) return;
	Counter[Num] = Value;
}

void CoinMeter::SetMeterEnable(UINT8 Num, UINT8 Value){
	if (Num >= FITTEDMETERS) return;
	Enable[Num] = Value;
}

void CoinMeter::Write(UINT8 Index, UINT8 PinIn){

	if (Index >= FITTEDMETERS) return;

	if (Enable[Index]){
		if (PinIn){
			Pin[Index] = 1;
		} else {
			Pin[Index] = 0;
		}
	
		if (Pin[Index]){
			if (PrevPin[Index] == 0){
				On[Index] = 1;
			}
		} else {
			if (PrevPin[Index]){
				if (TimeOn[Index] > 80000){
					Counter[Index] += 1;
					if (Counter[Index] > 99999999) {
						Counter[Index] = 0;
					}
				}
				TimeOn[Index] = 0;
				On[Index] = 0;
			}			
		}

		PrevPin[Index] = Pin[Index];

	} else {
		Pin[Index] = 0;
		PrevPin[Index] = 0;
		On[Index] = 0;
		TimeOn[Index] = 0;
	}
	
}

void CoinMeter::Run(unsigned short Cycles){

	int Cnt;

	for (Cnt = 0; Cnt < FITTEDMETERS; Cnt++){
		if (Enable[Cnt]){
			if (On[Cnt]){
				TimeOn[Cnt] += Cycles;
			}
		}
	}

}

UINT8 CoinMeter::Check(void){

	int Cnt;
	UINT8 Ret;

	Ret = 0;

	for (Cnt = 0; Cnt < FITTEDMETERS; Cnt++){
		if (Enable[Cnt]){
			if (On[Cnt]){
				Ret = 1;
			}
		}
	}

	return Ret;
}
// SEC METERS
void CoinMeter::BuildResponse(UINT8 code, UINT8 id, UINT8 len, UINT8 *data)
{
	UINT8 csum = 0;

	if (len > (sizeof(response) - 4)) {
		len = (UINT8)(sizeof(response) - 4);
	}

	ZeroMemory(response, sizeof(response));
	response[0] = code;
	response[1] = id;
	response[2] = len;
	if (len && data){
		memcpy(&response[3], data, len);
	}
	for ( int i = 0; i < 3 + len; i++ ){
		csum += response[i];
	}
	response[3+len] = csum;
	rx_pos = 0;
	rx_clk = 0;
	rx_length = 4 + len;

}

void CoinMeter::ProcessCommand(void)
{

UINT8 csum = 0;
char valbuf[10];
UINT8 val[4] = {0, 0, 0, 0};

	for ( int i = 0; i < pos; i++ ){
		csum += command[i];
	}

	last_id = command[1];
	switch ( command[0] ) {
	case 0x20: // Request Status
		BuildResponse(SEC_DATA, last_id, 1, &status);
		break;
	case 0x21: // Request Market Type
		BuildResponse(SEC_DATA, last_id, 1, &market_type);
		break;
	case 0x22: // Request Last Error
		BuildResponse(SEC_DATA, last_id, 1, &last_error);
		break;
	case 0x23: // Request Version
		BuildResponse(SEC_DATA, last_id, 3, version);
		break;
	case 0x24: // Request Counter Value		
		meter = command[3];
		if (meter < 31) {
			unsigned int counterValue = (unsigned int)SEC_Counters[meter];
			val[0] = (UINT8)((counterValue >> 24) & 0xff);
			val[1] = (UINT8)((counterValue >> 16) & 0xff);
			val[2] = (UINT8)((counterValue >> 8) & 0xff);
			val[3] = (UINT8)(counterValue & 0xff);
		}
		BuildResponse(SEC_DATA, last_id, 4, &val[0]);
		break;
	case 0x25: // Request Last Command ID
		BuildResponse(SEC_DATA, last_id, 4, val);
		break;
	case 0x26: // Request Fingerprint
		BuildResponse(SEC_DATA, last_id, 4, (UINT8 *)&fingerprint);
		break;
	case 0x30: // Set Number of Counters
		nbr_of_counters = command[3];
		BuildResponse(SEC_ACK, last_id, 0, NULL);
	break;
	case 0x31: // Set Market Type
		market_type = command[3];
		BuildResponse(SEC_ACK, last_id, 0, NULL);
	break;
	case 0x32: // Set Counter Text
		meter = command[3];	
		if (meter >= 31) { BuildResponse(SEC_NAK, last_id, 0, NULL); break; }
		memcpy( valbuf, &command[4], 7);
		valbuf[7] = 0;
		BuildResponse(SEC_ACK, last_id, 0, NULL);
		strcpy_s(SEC_Strings[meter], valbuf);
		break;
	case 0x40: // Show Text
		BuildResponse(SEC_ACK, last_id, 0, NULL);
		break;
	case 0x41: // Show Counter Value
		BuildResponse(SEC_ACK, last_id, 0, NULL);
		break;
	case 0x42: // Show Counter Text
		BuildResponse(SEC_ACK, last_id, 0, NULL);
		break;
	case 0x43: // Show Bit Pattern
		BuildResponse(SEC_ACK, last_id, 0, NULL);
		break;
	case 0x50: // Counter Increment (Small)
		meter = command[3];
		if (meter >= 31) { BuildResponse(SEC_NAK, last_id, 0, NULL); break; }
		value = command[4] & 0xf;
		SEC_Counters[meter] += value;		
		BuildResponse(SEC_ACK, last_id, 0, NULL);
		updated = true;
		break;
	case 0x51: // Counter Increment (Medium)
		meter = command[3];
		if (meter >= 31) { BuildResponse(SEC_NAK, last_id, 0, NULL); break; }
		value = command[4];
		SEC_Counters[meter] += value;		
		BuildResponse(SEC_ACK, last_id, 0, NULL);		
		updated = true;
		break;
	case 0x52: // Counter Increment (Large)
		meter = command[3];
		if (meter >= 31) { BuildResponse(SEC_NAK, last_id, 0, NULL); break; }
		value = command[4] + 256 * command[5];
		SEC_Counters[meter] += value;		
		BuildResponse(SEC_ACK, last_id, 0, NULL);		
		updated = true;
		break;
	case 0x54: // Cycle Counter Display
		BuildResponse(SEC_ACK, last_id, 0, NULL);
		break;
	case 0x55: // Stop Cycle
		BuildResponse(SEC_ACK, last_id, 0, NULL);
		break;
	case 0x5C: // Self Test
		BuildResponse(SEC_ACK, last_id, 0, NULL);
		break;
	}
}

void CoinMeter::Reset(void)
{
	clk = 1;		// Clear Clk bit
	clks = 0;
    ch = 0;
    pos = 0;
    status = 32;
    rx_clk = 0;
    rx_length = 0;
    rx_pos = 0;
    fingerprint = 0x11010000;
	bytes_left = 0;
	rx_data = 0;
}

void CoinMeter::SetEnable(UINT8 e)
{
	if ( e ) {
		if ( !enabled ) {
			enabled = true;
			rx_data = 0;
		}
	} else {
		if ( enabled ) {
			rx_data = 1;
		}
		enabled = false;
		Reset();
	}
}

void CoinMeter::SetData(UINT8 d)
{
	data1 = d ? 1 : 0;
}

UINT8 CoinMeter::ReadData(void)
{
	return rx_data;
}

void CoinMeter::SetClock(UINT8 c)
{
	c = c ? 1 : 0;

	if ( (clk ^ c) & 1 ) {

		if ( !c ) {
			ch = ( ch << 1 ) | data1;
			if ( rx_clk == 8 ) {
				rx_clk = 0;
				rx_pos++;
				rx_length--;

			}
			if ( rx_length ){
				rx_data = (response[rx_pos] & 0x80) >> 7;
			} else {
				rx_data = enabled ? 0 : 1;
			}
		} else {
			clks++;
			if ( rx_length ) {
				response[rx_pos] <<= 1;
				rx_clk++;
			}
			if ( clks == 8 ) {
				clks = 0;
				if ( !rx_length ){
					Do_Char(ch);
				} else {

				}
			}
		}
		clk = c;
	}
}

void CoinMeter::Do_Char(UINT8 x)
{
	if (pos >= sizeof(command)) {
		pos = 0;
		bytes_left = 0;
		return;
	}

	command[pos++] = x;
	if ( bytes_left ) {
		bytes_left--;
		if ( !bytes_left ) {
			pos--;
			ProcessCommand();
			pos = 0;
		}
	}
	if ( pos == 3 ){
		bytes_left = x + 1;
		if ((unsigned int)pos + (unsigned int)bytes_left > sizeof(command)) {
			pos = 0;
			bytes_left = 0;
		}
	}
}

/*
void CoinMeter::Init(LoadSaveCompressDLLClass * LSCIn){
	
	LSC = LSCIn;
}

void CoinMeter::SaveState(){

	int loop;

	for (loop = 0; loop < FITTEDMETERS; loop++){
		LSC->SaveToBuffer(Pin[loop]);
		LSC->SaveToBuffer(Enable[loop]);
		LSC->SaveToBuffer(PrevPin[loop]);
		LSC->SaveToBuffer(TimeOn[loop]);
		LSC->SaveToBuffer(On[loop]);
		LSC->SaveToBuffer(Counter[loop]);
		LSC->SaveToBuffer(PortIndex[loop]);
	}

}

void CoinMeter::LoadState(){

	int loop;

	for (loop = 0; loop < FITTEDMETERS; loop++){
		LSC->LoadFromBuffer(Pin[loop]);
		LSC->LoadFromBuffer(Enable[loop]);
		LSC->LoadFromBuffer(PrevPin[loop]);
		LSC->LoadFromBuffer(TimeOn[loop]);
		LSC->LoadFromBuffer(On[loop]);
		LSC->LoadFromBuffer(Counter[loop]);
		LSC->LoadFromBuffer(PortIndex[loop]);
	}

}
*/