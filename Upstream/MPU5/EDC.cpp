#include "stdafx.h"
#include "EDC.h"

namespace
{
const char* FixedMessageName(UINT8 header)
{
    switch (static_cast<UINT8>(header & ~0x80U))
    {
    case 0x00: return "NULL Message";
    case 0x04: return "Mode Change / EOT";
    case 0x07: return "Idle";
    case 0x17: return "RVI";
    case 0x20: return "Token Payout To Float";
    case 0x21: return "10p Payout To Float";
    case 0x22: return "20p Payout To Float";
    case 0x23: return "50p Payout To Float";
    case 0x24: return "GBP1 Payout To Float";
    case 0x25: return "GBP2 Payout To Float";
    case 0x26: return "GBP5 Payout To Float";
    case 0x27: return "GBP20 Cash In";
    case 0x28: return "GBP50 Cash In";
    case 0x29: return "2p Cash In";
    case 0x2A: return "2p Cash Out";
    case 0x2B: return "Cash Door Open";
    case 0x2C: return "Cash Door Closed";
    case 0x2D: return "Service Door Open";
    case 0x2E: return "Service Door Closed";
    case 0x2F: return "VTP++";
    case 0x30: return "5p Cash In";
    case 0x31: return "10p Cash In";
    case 0x32: return "20p Cash In";
    case 0x33: return "50p Cash In";
    case 0x34: return "GBP1 Cash In";
    case 0x35: return "GBP2 Cash In";
    case 0x36: return "GBP5 Cash In";
    case 0x37: return "GBP10 Cash In";
    case 0x38: return "5p Token In";
    case 0x39: return "10p Token In";
    case 0x3A: return "20p Token In";
    case 0x3B: return "50p Token In";
    case 0x3C: return "GBP1 Token In";
    case 0x3D: return "GBP2 Token In";
    case 0x3E: return "GBP5 Token In";
    case 0x3F: return "GBP10 Token In";
    case 0x40: return "5p Cash Out";
    case 0x41: return "10p Cash Out";
    case 0x42: return "20p Cash Out";
    case 0x43: return "50p Cash Out";
    case 0x44: return "GBP1 Cash Out";
    case 0x45: return "GBP2 Cash Out";
    case 0x46: return "GBP5 Cash Out";
    case 0x47: return "GBP10 Cash Out";
    case 0x48: return "5p Token Out";
    case 0x49: return "10p Token Out";
    case 0x4A: return "20p Token Out";
    case 0x4B: return "50p Token Out";
    case 0x4C: return "GBP1 Token Out";
    case 0x4D: return "GBP2 Token Out";
    case 0x4E: return "GBP5 Token Out";
    case 0x4F: return "GBP10 Token Out";
    case 0x50: return "5p Cash Refill";
    case 0x51: return "10p Cash Refill";
    case 0x52: return "20p Cash Refill";
    case 0x53: return "50p Cash Refill";
    case 0x54: return "GBP1 Cash Refill";
    case 0x55: return "GBP2 Cash Refill";
    case 0x56: return "GBP5 Cash Refill";
    case 0x57: return "GBP10 Cash Refill";
    case 0x58: return "5p Token Refill";
    case 0x59: return "10p Token Refill";
    case 0x5A: return "20p Token Refill";
    case 0x5B: return "50p Token Refill";
    case 0x5C: return "GBP1 Token Refill";
    case 0x5D: return "GBP2 Token Refill";
    case 0x5E: return "GBP5 Token Refill";
    case 0x5F: return "GBP10 Token Refill";
    default: return nullptr;
    }
}
}

EDCUNIT::EDCUNIT()
{
    Reset();
}

void EDCUNIT::ResetParser()
{
    ParserState_ = ParserMode::WaitHeader;
    Header_ = 0;
    ExpectedPayloadLength_ = 0;
    PayloadBytesSeen_ = 0;
    RunningChecksum_ = 0;
    MessageLength_ = 0;
    Message_.fill(0);
}

void EDCUNIT::Reset()
{
    OutputBuffer_.fill(0);
    OutputLength_ = 0;
    TextAvailable_ = 0;
    ValidMessageCount_ = 0;
    ChecksumErrorCount_ = 0;
    MessageQueue_.clear();
    ResetParser();
}

UINT8* EDCUNIT::GetString()
{
    return TextAvailable_ ? OutputBuffer_.data() : nullptr;
}

UINT32 EDCUNIT::PopMessage(char* output, UINT32 outputSize)
{
    if (!output || outputSize == 0U || MessageQueue_.empty()) { return 0U; }

    const std::string message = MessageQueue_.front();
    MessageQueue_.pop_front();

    const UINT32 copyLength = static_cast<UINT32>(
        std::min<size_t>(message.size(), static_cast<size_t>(outputSize - 1U)));
    if (copyLength > 0U)
    {
        std::memcpy(output, message.data(), copyLength);
    }
    output[copyLength] = 0;
    return copyLength;
}

bool EDCUNIT::IsVariableLengthHeader(UINT8 header) const
{
    const UINT8 base = static_cast<UINT8>(header & ~ResendBit);
    return base >= 0x60U && base <= 0x7FU;
}

bool EDCUNIT::IsValidHeader(UINT8 header) const
{
    const UINT8 base = static_cast<UINT8>(header & ~ResendBit);
    return base == 0x00U || base == 0x04U || base == 0x07U || base == 0x17U ||
        (base >= 0x20U && base <= 0x7FU);
}

void EDCUNIT::AppendLine(const char* text)
{
    if (!text) { return; }

    if (MessageQueue_.size() >= MessageQueueDepth)
    {
        MessageQueue_.pop_front();
    }
    MessageQueue_.emplace_back(text);

    char line[512]{};
    const int written = std::snprintf(line, sizeof(line), "%s\r\n", text);
    if (written <= 0) { return; }

    const UINT16 addLength = static_cast<UINT16>(
        std::min<int>(written, static_cast<int>(sizeof(line) - 1U)));
    if (addLength + 1U >= OutputSize) { return; }

    if (static_cast<UINT32>(OutputLength_) + addLength + 1U >= OutputSize)
    {
        OutputLength_ = 0;
        OutputBuffer_[0] = 0;
    }

    std::memcpy(OutputBuffer_.data() + OutputLength_, line, addLength);
    OutputLength_ = static_cast<UINT16>(OutputLength_ + addLength);
    OutputBuffer_[OutputLength_] = 0;
    TextAvailable_ = 1;

#if defined(_MSC_VER)
    FILE* file = nullptr;
    fopen_s(&file, "EDC.txt", "a");
#else
    FILE* file = std::fopen("EDC.txt", "a");
#endif
    if (file)
    {
        std::fprintf(file, "%s\n", text);
        std::fclose(file);
    }
}

void EDCUNIT::AppendHexPayload(const UINT8* payload, UINT8 length, char* out, UINT16 outSize) const
{
    if (!payload || !out || outSize == 0U) { return; }

    UINT16 position = 0;
    out[0] = 0;
    for (UINT8 index = 0; index < length && position + 4U < outSize; ++index)
    {
        const int written = std::snprintf(out + position, outSize - position, "%02X ", payload[index]);
        if (written <= 0) { break; }
        position = static_cast<UINT16>(position + written);
    }
}

void EDCUNIT::AppendMessageSummary()
{
    char line[512]{};
    char payloadText[256]{};
    const UINT8 base = static_cast<UINT8>(Header_ & ~ResendBit);
    const bool resend = (Header_ & ResendBit) != 0U;
    const UINT8* payload = Message_.data() + 2U;
    AppendHexPayload(payload, ExpectedPayloadLength_, payloadText, sizeof(payloadText));

    const char* fixed = FixedMessageName(Header_);
    if (fixed && !IsVariableLengthHeader(Header_))
    {
        std::snprintf(line, sizeof(line), "EDC %s%s", fixed, resend ? " RESEND" : "");
        AppendLine(line);
        return;
    }

    switch (base)
    {
    case 0x60:
        if (ExpectedPayloadLength_ >= 8U)
        {
            char manufacturer[4] = {
                static_cast<char>(payload[0]), static_cast<char>(payload[1]),
                static_cast<char>(payload[2]), 0 };
            char machine[5] = {
                static_cast<char>(payload[4]), static_cast<char>(payload[5]),
                static_cast<char>(payload[6]), static_cast<char>(payload[7]), 0 };
            std::snprintf(line, sizeof(line),
                "EDC Primary%s: Man=%s Protocol=%c Machine=%s",
                resend ? " RESEND" : "", manufacturer, payload[3], machine);
        }
        else
        {
            std::snprintf(line, sizeof(line), "EDC Primary%s: %s",
                resend ? " RESEND" : "", payloadText);
        }
        break;

    case 0x61:
        std::snprintf(line, sizeof(line), "EDC Tube Level%s: %s",
            resend ? " RESEND" : "", payloadText);
        break;

    case 0x62:
        if (ExpectedPayloadLength_ >= 10U)
        {
            char version[4] = {
                static_cast<char>(payload[0]), static_cast<char>(payload[1]),
                static_cast<char>(payload[2]), 0 };
            char percent[4] = {
                static_cast<char>(payload[4]), static_cast<char>(payload[5]),
                static_cast<char>(payload[6]), 0 };
            std::snprintf(line, sizeof(line),
                "EDC Secondary%s: Ver=%s CashToken=%c Percent=%s Type=%c Stake=%u CoinSystem=%c",
                resend ? " RESEND" : "", version, payload[3], percent,
                payload[7], payload[8], payload[9]);
        }
        else
        {
            std::snprintf(line, sizeof(line), "EDC Secondary%s: %s",
                resend ? " RESEND" : "", payloadText);
        }
        break;

    case 0x63:
        std::snprintf(line, sizeof(line), "EDC Critical Fault%s: %s",
            resend ? " RESEND" : "", payloadText);
        break;
    case 0x64:
        std::snprintf(line, sizeof(line), "EDC Non-Critical Fault%s: %s",
            resend ? " RESEND" : "", payloadText);
        break;
    case 0x65:
        std::snprintf(line, sizeof(line), "EDC Game Outcome/Compliance%s: %s",
            resend ? " RESEND" : "", payloadText);
        break;
    case 0x66:
        std::snprintf(line, sizeof(line), "EDC Variable Data%s: %s",
            resend ? " RESEND" : "", payloadText);
        break;
    default:
        std::snprintf(line, sizeof(line), "EDC Header %02X%s: %s",
            base, resend ? " RESEND" : "", payloadText);
        break;
    }

    AppendLine(line);
}

UINT8 EDCUNIT::Write(UINT8 byteIn)
{
    switch (ParserState_)
    {
    case ParserMode::WaitHeader:
        if (!IsValidHeader(byteIn)) { return 0; }

        ResetParser();
        Header_ = byteIn;
        Message_[0] = byteIn;
        MessageLength_ = 1;
        RunningChecksum_ = byteIn;
        ParserState_ = IsVariableLengthHeader(byteIn)
            ? ParserMode::WaitLength
            : ParserMode::WaitFixedChecksum;
        return 0;

    case ParserMode::WaitFixedChecksum:
        Message_[1] = byteIn;
        MessageLength_ = 2;
        if (byteIn == RunningChecksum_)
        {
            ++ValidMessageCount_;
            AppendMessageSummary();
            ResetParser();
            return Ack;
        }
        ++ChecksumErrorCount_;
        ResetParser();
        return 0;

    case ParserMode::WaitLength:
        if (byteIn >= MessageSize - 3U)
        {
            ResetParser();
            return 0;
        }
        ExpectedPayloadLength_ = byteIn;
        PayloadBytesSeen_ = 0;
        Message_[1] = byteIn;
        MessageLength_ = 2;
        RunningChecksum_ = static_cast<UINT8>(RunningChecksum_ + byteIn);
        ParserState_ = ParserMode::WaitPayloadAndChecksum;
        return 0;

    case ParserMode::WaitPayloadAndChecksum:
        if (MessageLength_ >= MessageSize)
        {
            ResetParser();
            return 0;
        }

        Message_[MessageLength_++] = byteIn;
        if (PayloadBytesSeen_ < ExpectedPayloadLength_)
        {
            RunningChecksum_ = static_cast<UINT8>(RunningChecksum_ + byteIn);
            ++PayloadBytesSeen_;
            return 0;
        }

        if (byteIn == RunningChecksum_)
        {
            ++ValidMessageCount_;
            AppendMessageSummary();
            ResetParser();
            return Ack;
        }

        ++ChecksumErrorCount_;
        ResetParser();
        return 0;
    }

    ResetParser();
    return 0;
}
