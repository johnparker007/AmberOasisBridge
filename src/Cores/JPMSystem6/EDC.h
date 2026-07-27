#pragma once

#include "LoadSave.h"

#define EDCBUFFERSIZE 256
#define EDCMESSAGESIZE 80
#define EDCOUTPUTSIZE 2048

class EDCUNIT {
public:

    UINT8 __fastcall Write(UINT8 ByteIn);
    void __fastcall Reset(LoadSaveClass* LSCIn);

    UINT8* __fastcall getEDCString();
    UINT8 __fastcall GetAvailable();

    void SaveState();
    void LoadState();

    EDCUNIT();
    ~EDCUNIT();

private:

    enum ParserMode : UINT8 {
        PARSER_WAIT_HEADER = 0,
        PARSER_WAIT_FIXED_CHECKSUM = 1,
        PARSER_WAIT_LENGTH = 2,
        PARSER_WAIT_PAYLOAD_AND_CHECKSUM = 3
    };

    bool IsValidHeader(UINT8 Header) const;
    bool IsVariableLengthHeader(UINT8 Header) const;
    void ResetParser();
    void AppendLine(const char* Text);
    void AppendMessageSummary();
    void AppendHexPayload(const UINT8* Payload, UINT8 Length, char* Out, UINT16 OutSize) const;

    UINT8 EDCBuf[EDCBUFFERSIZE];
    UINT8 Message[EDCMESSAGESIZE];
    UINT8 ParserState;
    UINT8 Header;
    UINT8 ExpectedPayloadLength;
    UINT8 PayloadBytesSeen;
    UINT8 RunningChecksum;
    UINT8 TextAvailable;

    UINT16 EDCLength;
    UINT16 OutputLength;
    UINT8 OutputBuffer[EDCOUTPUTSIZE];

    LoadSaveClass* LSC = NULL;

};
