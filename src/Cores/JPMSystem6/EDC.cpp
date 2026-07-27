#include "stdafx.h"
#include "EDC.h"

#define EDC_ACK 0x06
#define EDC_NAK 0x15
#define EDC_RESEND_BIT 0x80

static const char* EDCFixedMessageName(UINT8 HeaderIn) {
    switch (static_cast<UINT8>(HeaderIn & ~EDC_RESEND_BIT)) {
    case 0x00: return "NULL Message";
    case 0x04: return "Mode Change / EOT";
    case 0x07: return "Idle";
    case 0x17: return "RVI";

    case 0x20: return "Token Payout To Float";
    case 0x21: return "10p Payout To Float";
    case 0x22: return "20p Payout To Float";
    case 0x23: return "50p Payout To Float";
    case 0x24: return "\xA3" "1 Payout To Float";
    case 0x25: return "\xA3" "2 Payout To Float";
    case 0x26: return "\xA3" "5 Payout To Float";
    case 0x27: return "\xA3" "20 Cash In";
    case 0x28: return "\xA3" "50 Cash In";
    case 0x29: return "2p Cash In";
    case 0x2a: return "2p Cash Out";
    case 0x2b: return "Cash Door Open";
    case 0x2c: return "Cash Door Closed";
    case 0x2d: return "Service Door Open";
    case 0x2e: return "Service Door Closed";
    case 0x2f: return "VTP++";

    case 0x30: return "5p Cash In";
    case 0x31: return "10p Cash In";
    case 0x32: return "20p Cash In";
    case 0x33: return "50p Cash In";
    case 0x34: return "\xA3" "1 Cash In";
    case 0x35: return "\xA3" "2 Cash In";
    case 0x36: return "\xA3" "5 Cash In";
    case 0x37: return "\xA3" "10 Cash In";
    case 0x38: return "5p Token In";
    case 0x39: return "10p Token In";
    case 0x3a: return "20p Token In";
    case 0x3b: return "50p Token In";
    case 0x3c: return "\xA3" "1 Token In";
    case 0x3d: return "\xA3" "2 Token In";
    case 0x3e: return "\xA3" "5 Token In";
    case 0x3f: return "\xA3" "10 Token In";

    case 0x40: return "5p Cash Out";
    case 0x41: return "10p Cash Out";
    case 0x42: return "20p Cash Out";
    case 0x43: return "50p Cash Out";
    case 0x44: return "\xA3" "1 Cash Out";
    case 0x45: return "\xA3" "2 Cash Out";
    case 0x46: return "\xA3" "5 Cash Out";
    case 0x47: return "\xA3" "10 Cash Out";
    case 0x48: return "5p Token Out";
    case 0x49: return "10p Token Out";
    case 0x4a: return "20p Token Out";
    case 0x4b: return "50p Token Out";
    case 0x4c: return "\xA3" "1 Token Out";
    case 0x4d: return "\xA3" "2 Token Out";
    case 0x4e: return "\xA3" "5 Token Out";
    case 0x4f: return "\xA3" "10 Token Out";

    case 0x50: return "5p Cash Refill";
    case 0x51: return "10p Cash Refill";
    case 0x52: return "20p Cash Refill";
    case 0x53: return "50p Cash Refill";
    case 0x54: return "\xA3" "1 Cash Refill";
    case 0x55: return "\xA3" "2 Cash Refill";
    case 0x56: return "\xA3" "5 Cash Refill";
    case 0x57: return "\xA3" "10 Cash Refill";
    case 0x58: return "5p Token Refill";
    case 0x59: return "10p Token Refill";
    case 0x5a: return "20p Token Refill";
    case 0x5b: return "50p Token Refill";
    case 0x5c: return "\xA3" "1 Token Refill";
    case 0x5d: return "\xA3" "2 Token Refill";
    case 0x5e: return "\xA3" "5 Token Refill";
    case 0x5f: return "\xA3" "10 Token Refill";
    default: return NULL;
    }
}


EDCUNIT::EDCUNIT() {
    ZeroMemory(EDCBuf, sizeof(EDCBuf));
    ZeroMemory(Message, sizeof(Message));
    ZeroMemory(OutputBuffer, sizeof(OutputBuffer));
    ParserState = PARSER_WAIT_HEADER;
    Header = 0;
    ExpectedPayloadLength = 0;
    PayloadBytesSeen = 0;
    RunningChecksum = 0;
    TextAvailable = 0;
    EDCLength = 0;
    OutputLength = 0;
}

EDCUNIT::~EDCUNIT() {

}

void EDCUNIT::ResetParser() {
    ParserState = PARSER_WAIT_HEADER;
    Header = 0;
    ExpectedPayloadLength = 0;
    PayloadBytesSeen = 0;
    RunningChecksum = 0;
    EDCLength = 0;
    ZeroMemory(Message, sizeof(Message));
}

void EDCUNIT::Reset(LoadSaveClass* LSCIn) {
    LSC = LSCIn;
    ZeroMemory(EDCBuf, sizeof(EDCBuf));
    ZeroMemory(OutputBuffer, sizeof(OutputBuffer));
    OutputLength = 0;
    TextAvailable = 0;
    ResetParser();
}

void EDCUNIT::SaveState() {
    LSC->SaveToBuffer(ParserState);
    LSC->SaveToBuffer(Header);
    LSC->SaveToBuffer(ExpectedPayloadLength);
    LSC->SaveToBuffer(PayloadBytesSeen);
    LSC->SaveToBuffer(RunningChecksum);
    LSC->SaveToBuffer(TextAvailable);
    LSC->SaveToBuffer(EDCLength);
    LSC->SaveToBuffer(OutputLength);

    for (UINT16 i = 0; i < EDCBUFFERSIZE; i++) {
        LSC->SaveToBuffer(EDCBuf[i]);
    }
    for (UINT16 i = 0; i < EDCMESSAGESIZE; i++) {
        LSC->SaveToBuffer(Message[i]);
    }
    for (UINT16 i = 0; i < EDCOUTPUTSIZE; i++) {
        LSC->SaveToBuffer(OutputBuffer[i]);
    }
}

void EDCUNIT::LoadState() {
    LSC->LoadFromBuffer(ParserState);
    LSC->LoadFromBuffer(Header);
    LSC->LoadFromBuffer(ExpectedPayloadLength);
    LSC->LoadFromBuffer(PayloadBytesSeen);
    LSC->LoadFromBuffer(RunningChecksum);
    LSC->LoadFromBuffer(TextAvailable);
    LSC->LoadFromBuffer(EDCLength);
    LSC->LoadFromBuffer(OutputLength);

    for (UINT16 i = 0; i < EDCBUFFERSIZE; i++) {
        LSC->LoadFromBuffer(EDCBuf[i]);
    }
    for (UINT16 i = 0; i < EDCMESSAGESIZE; i++) {
        LSC->LoadFromBuffer(Message[i]);
    }
    for (UINT16 i = 0; i < EDCOUTPUTSIZE; i++) {
        LSC->LoadFromBuffer(OutputBuffer[i]);
    }

    if (ParserState > PARSER_WAIT_PAYLOAD_AND_CHECKSUM || OutputLength >= EDCOUTPUTSIZE) {
        Reset(LSC);
    }
}

UINT8* __fastcall EDCUNIT::getEDCString() {
    if (!TextAvailable) {
        return NULL;
    }
    return reinterpret_cast<UINT8*>(OutputBuffer);
}

UINT8 __fastcall EDCUNIT::GetAvailable() {
    return TextAvailable;
}

bool EDCUNIT::IsVariableLengthHeader(UINT8 HeaderIn) const {
    UINT8 BaseHeader = static_cast<UINT8>(HeaderIn & ~EDC_RESEND_BIT);
    return (BaseHeader >= 0x60 && BaseHeader <= 0x7f);
}

bool EDCUNIT::IsValidHeader(UINT8 HeaderIn) const {
    UINT8 BaseHeader = static_cast<UINT8>(HeaderIn & ~EDC_RESEND_BIT);

    if (BaseHeader == 0x00 || BaseHeader == 0x04 || BaseHeader == 0x07 || BaseHeader == 0x17) {
        return true;
    }
    if (BaseHeader >= 0x20 && BaseHeader <= 0x7f) {
        return true;
    }
    return false;
}

void EDCUNIT::AppendLine(const char* Text) {
    if (!Text) {
        return;
    }

    char Temp[512];
    sprintf_s(Temp, sizeof(Temp), "%s\r\n", Text);

    UINT16 AddLen = static_cast<UINT16>(strlen(Temp));
    if (AddLen >= EDCOUTPUTSIZE) {
        return;
    }

    if ((OutputLength + AddLen + 1) >= EDCOUTPUTSIZE) {
        OutputLength = 0;
        OutputBuffer[0] = 0;
    }

    memcpy(&OutputBuffer[OutputLength], Temp, AddLen);
    OutputLength = static_cast<UINT16>(OutputLength + AddLen);
    OutputBuffer[OutputLength] = 0;
    TextAvailable = 1;

    FILE* EdcFile = NULL;
    fopen_s(&EdcFile, "EDC.txt", "a");
    if (EdcFile) {
        fprintf(EdcFile, "%s\n", Text);
        fclose(EdcFile);
    }
}

void EDCUNIT::AppendHexPayload(const UINT8* Payload, UINT8 Length, char* Out, UINT16 OutSize) const {
    UINT16 Pos = 0;
    if (!Out || OutSize == 0) {
        return;
    }
    Out[0] = 0;

    for (UINT8 i = 0; i < Length; i++) {
        if ((Pos + 4) >= OutSize) {
            break;
        }
        sprintf_s(&Out[Pos], OutSize - Pos, "%02X ", Payload[i]);
        Pos = static_cast<UINT16>(strlen(Out));
    }
}

void EDCUNIT::AppendMessageSummary() {
    char Line[512];
    char PayloadText[256];
    UINT8 BaseHeader = static_cast<UINT8>(Header & ~EDC_RESEND_BIT);
    UINT8 Resend = (Header & EDC_RESEND_BIT) ? 1 : 0;
    UINT8* Payload = &Message[2];
    UINT8 PayloadLen = ExpectedPayloadLength;

    PayloadText[0] = 0;
    AppendHexPayload(Payload, PayloadLen, PayloadText, sizeof(PayloadText));

    const char* FixedMessage = EDCFixedMessageName(Header);
    if (FixedMessage && !IsVariableLengthHeader(Header)) {
        sprintf_s(Line, sizeof(Line), "EDC %s%s", FixedMessage, Resend ? " RESEND" : "");
        AppendLine(Line);
        return;
    }

    switch (BaseHeader) {
    case 0x04:
        sprintf_s(Line, sizeof(Line), "EDC EOT%s", Resend ? " RESEND" : "");
        break;
    case 0x07:
        sprintf_s(Line, sizeof(Line), "EDC IDLE%s", Resend ? " RESEND" : "");
        break;
    case 0x17:
        sprintf_s(Line, sizeof(Line), "EDC RVI%s", Resend ? " RESEND" : "");
        break;
    case 0x2b:
        sprintf_s(Line, sizeof(Line), "EDC Cash Door Open%s", Resend ? " RESEND" : "");
        break;
    case 0x2c:
        sprintf_s(Line, sizeof(Line), "EDC Cash Door Closed%s", Resend ? " RESEND" : "");
        break;
    case 0x2d:
        sprintf_s(Line, sizeof(Line), "EDC Service Door Open%s", Resend ? " RESEND" : "");
        break;
    case 0x2e:
        sprintf_s(Line, sizeof(Line), "EDC Service Door Closed%s", Resend ? " RESEND" : "");
        break;
    case 0x2f:
        sprintf_s(Line, sizeof(Line), "EDC VTP++%s", Resend ? " RESEND" : "");
        break;
    case 0x60:
        if (PayloadLen >= 8) {
            char Man[4] = { static_cast<char>(Payload[0]), static_cast<char>(Payload[1]), static_cast<char>(Payload[2]), 0 };
            char Machine[5] = { static_cast<char>(Payload[4]), static_cast<char>(Payload[5]), static_cast<char>(Payload[6]), static_cast<char>(Payload[7]), 0 };
            sprintf_s(Line, sizeof(Line), "EDC Primary%s: Man=%s Protocol=%c Machine=%s", Resend ? " RESEND" : "", Man, Payload[3], Machine);
        }
        else {
            sprintf_s(Line, sizeof(Line), "EDC Primary%s: %s", Resend ? " RESEND" : "", PayloadText);
        }
        break;
    case 0x61:
        sprintf_s(Line, sizeof(Line), "EDC Tube Level%s: %s", Resend ? " RESEND" : "", PayloadText);
        break;
    case 0x62:
        if (PayloadLen >= 10) {
            char Version[4] = { static_cast<char>(Payload[0]), static_cast<char>(Payload[1]), static_cast<char>(Payload[2]), 0 };
            char Percent[4] = { static_cast<char>(Payload[4]), static_cast<char>(Payload[5]), static_cast<char>(Payload[6]), 0 };
            sprintf_s(Line, sizeof(Line), "EDC Secondary%s: Ver=%s CashToken=%c Percent=%s Type=%c Stake=%u CoinSystem=%c", Resend ? " RESEND" : "", Version, Payload[3], Percent, Payload[7], Payload[8], Payload[9]);
        }
        else {
            sprintf_s(Line, sizeof(Line), "EDC Secondary%s: %s", Resend ? " RESEND" : "", PayloadText);
        }
        break;
    case 0x63:
        sprintf_s(Line, sizeof(Line), "EDC Critical Fault%s: %s", Resend ? " RESEND" : "", PayloadText);
        break;
    case 0x64:
        sprintf_s(Line, sizeof(Line), "EDC Non-Critical Fault%s: %s", Resend ? " RESEND" : "", PayloadText);
        break;
    case 0x65:
        sprintf_s(Line, sizeof(Line), "EDC Game Outcome/Compliance%s: %s", Resend ? " RESEND" : "", PayloadText);
        break;
    case 0x66:
        sprintf_s(Line, sizeof(Line), "EDC Variable Data%s: %s", Resend ? " RESEND" : "", PayloadText);
        break;
    default:
        sprintf_s(Line, sizeof(Line), "EDC Header %02X%s: %s", BaseHeader, Resend ? " RESEND" : "", PayloadText);
        break;
    }

    AppendLine(Line);
}

UINT8 __fastcall EDCUNIT::Write(UINT8 ByteIn) {
    switch (ParserState) {
    case PARSER_WAIT_HEADER:
        if (!IsValidHeader(ByteIn)) {
            return 0;
        }

        ResetParser();
        Header = ByteIn;
        Message[0] = ByteIn;
        EDCLength = 1;
        RunningChecksum = ByteIn;

        if (IsVariableLengthHeader(ByteIn)) {
            ParserState = PARSER_WAIT_LENGTH;
        }
        else {
            ExpectedPayloadLength = 0;
            ParserState = PARSER_WAIT_FIXED_CHECKSUM;
        }
        return 0;

    case PARSER_WAIT_FIXED_CHECKSUM:
        Message[1] = ByteIn;
        EDCLength = 2;
        if (ByteIn == RunningChecksum) {
            AppendMessageSummary();
            ResetParser();
            return EDC_ACK;
        }
        ResetParser();
        return 0;

    case PARSER_WAIT_LENGTH:
        if (ByteIn >= (EDCMESSAGESIZE - 3)) {
            ResetParser();
            return 0;
        }
        ExpectedPayloadLength = ByteIn;
        PayloadBytesSeen = 0;
        Message[1] = ByteIn;
        EDCLength = 2;
        RunningChecksum = static_cast<UINT8>(RunningChecksum + ByteIn);
        ParserState = PARSER_WAIT_PAYLOAD_AND_CHECKSUM;
        return 0;

    case PARSER_WAIT_PAYLOAD_AND_CHECKSUM:
        if (EDCLength >= EDCMESSAGESIZE) {
            ResetParser();
            return 0;
        }

        Message[EDCLength++] = ByteIn;

        if (PayloadBytesSeen < ExpectedPayloadLength) {
            RunningChecksum = static_cast<UINT8>(RunningChecksum + ByteIn);
            PayloadBytesSeen++;
            return 0;
        }

        if (ByteIn == RunningChecksum) {
            AppendMessageSummary();
            ResetParser();
            return EDC_ACK;
        }

        ResetParser();
        return 0;

    default:
        ResetParser();
        return 0;
    }
}
