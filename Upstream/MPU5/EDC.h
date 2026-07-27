#pragma once

#include "PA2CoreInterface.h"
#include <array>
#include <deque>
#include <string>

class EDCUNIT
{
public:
    static constexpr UINT8 Ack = 0x06U;
    static constexpr UINT8 Nak = 0x15U;

    EDCUNIT();

    void Reset();
    UINT8 Write(UINT8 byteIn);

    UINT8* GetString();
    UINT32 PopMessage(char* output, UINT32 outputSize);
    UINT8 GetAvailable() const { return TextAvailable_; }
    UINT32 GetValidMessageCount() const { return ValidMessageCount_; }
    UINT32 GetChecksumErrorCount() const { return ChecksumErrorCount_; }

private:
    enum class ParserMode : UINT8
    {
        WaitHeader = 0,
        WaitFixedChecksum,
        WaitLength,
        WaitPayloadAndChecksum
    };

    static constexpr UINT16 MessageSize = 80U;
    static constexpr UINT16 OutputSize = 2048U;
    static constexpr size_t MessageQueueDepth = 64U;
    static constexpr UINT8 ResendBit = 0x80U;

    bool IsValidHeader(UINT8 header) const;
    bool IsVariableLengthHeader(UINT8 header) const;
    void ResetParser();
    void AppendLine(const char* text);
    void AppendMessageSummary();
    void AppendHexPayload(const UINT8* payload, UINT8 length, char* out, UINT16 outSize) const;

    std::array<UINT8, MessageSize> Message_{};
    std::array<UINT8, OutputSize> OutputBuffer_{};
    ParserMode ParserState_ = ParserMode::WaitHeader;
    UINT8 Header_ = 0;
    UINT8 ExpectedPayloadLength_ = 0;
    UINT8 PayloadBytesSeen_ = 0;
    UINT8 RunningChecksum_ = 0;
    UINT8 TextAvailable_ = 0;
    UINT16 MessageLength_ = 0;
    UINT16 OutputLength_ = 0;
    UINT32 ValidMessageCount_ = 0;
    UINT32 ChecksumErrorCount_ = 0;
    std::deque<std::string> MessageQueue_{};
};
