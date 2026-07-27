#include "MPU5SEC.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

MPU5SEC::MPU5SEC()
{
    for (auto& text : CounterText_)
        text.fill('\0');
    Reset();
}

void MPU5SEC::ResetProtocol()
{
    Clock_ = 1U;
    Character_ = 0U;
    Clocks_ = 0U;
    TransmitData_ = 0U;
    Command_.fill(0U);
    Response_.fill(0U);
    Position_ = 0U;
    ReceivePosition_ = 0U;
    ReceiveClock_ = 0U;
    ReceiveLength_ = 0U;
    BytesLeft_ = 0U;
    LastID_ = 0U;
    Status_ = 0x20U;
    MarketType_ = 0U;
    LastError_ = 0U;
    NumberOfCounters_ = 0U;
}

void MPU5SEC::Reset()
{
    Enabled_ = false;
    ReceiveData_ = 1U;
    Updated_ = false;
    ResetProtocol();
}

void MPU5SEC::Enable(UINT8 enabled)
{
    const bool newEnabled = enabled != 0U;
    if (newEnabled)
    {
        if (!Enabled_)
        {
            Enabled_ = true;
            ReceiveData_ = 0U;
        }
        return;
    }

    if (Enabled_)
        ReceiveData_ = 1U;
    Enabled_ = false;
    ResetProtocol();
}

void MPU5SEC::SetData(UINT8 data)
{
    TransmitData_ = data != 0U ? 1U : 0U;
}

void MPU5SEC::SetClock(UINT8 clock)
{
    const UINT8 newClock = clock != 0U ? 1U : 0U;
    if (((Clock_ ^ newClock) & 1U) == 0U)
        return;

    if (newClock == 0U)
    {
        Character_ = static_cast<UINT8>((Character_ << 1U) | TransmitData_);

        if (ReceiveClock_ == 8U)
        {
            ReceiveClock_ = 0U;
            if (ReceivePosition_ < Response_.size())
                ++ReceivePosition_;
            if (ReceiveLength_ != 0U)
                --ReceiveLength_;
        }

        if (ReceiveLength_ != 0U && ReceivePosition_ < Response_.size())
            ReceiveData_ = static_cast<UINT8>((Response_[ReceivePosition_] >> 7U) & 1U);
        else
            ReceiveData_ = Enabled_ ? 0U : 1U;
    }
    else
    {
        ++Clocks_;
        if (ReceiveLength_ != 0U && ReceivePosition_ < Response_.size())
        {
            Response_[ReceivePosition_] = static_cast<UINT8>(Response_[ReceivePosition_] << 1U);
            ++ReceiveClock_;
        }

        if (Clocks_ == 8U)
        {
            Clocks_ = 0U;
            if (ReceiveLength_ == 0U)
                ReceiveByte(Character_);
        }
    }

    Clock_ = newClock;
}

void MPU5SEC::ReceiveByte(UINT8 value)
{
    if (Position_ >= Command_.size())
    {
        Position_ = 0U;
        BytesLeft_ = 0U;
    }

    Command_[Position_++] = value;

    if (BytesLeft_ != 0U)
    {
        --BytesLeft_;
        if (BytesLeft_ == 0U)
        {
            // Position currently includes the checksum byte. MFME excludes it
            // from the command length passed to the dispatcher.
            if (Position_ != 0U)
                --Position_;
            ProcessCommand();
            Position_ = 0U;
        }
    }

    if (Position_ == 3U)
    {
        const UINT16 remaining = static_cast<UINT16>(value) + 1U;
        if (remaining >= Command_.size() - Position_)
        {
            Position_ = 0U;
            BytesLeft_ = 0U;
            return;
        }
        BytesLeft_ = static_cast<UINT8>(remaining);
    }
}

void MPU5SEC::BuildResponse(UINT8 code, UINT8 id, const UINT8* data, UINT8 length)
{
    const UINT8 safeLength = static_cast<UINT8>(std::min<size_t>(length, Response_.size() - 4U));
    Response_.fill(0U);
    Response_[0] = code;
    Response_[1] = id;
    Response_[2] = safeLength;
    if (safeLength != 0U && data != nullptr)
        std::memcpy(Response_.data() + 3U, data, safeLength);

    UINT8 checksum = 0U;
    for (UINT8 index = 0U; index < static_cast<UINT8>(3U + safeLength); ++index)
        checksum = static_cast<UINT8>(checksum + Response_[index]);
    Response_[3U + safeLength] = checksum;

    ReceivePosition_ = 0U;
    ReceiveClock_ = 0U;
    ReceiveLength_ = static_cast<UINT8>(4U + safeLength);
}

void MPU5SEC::ProcessCommand()
{
    if (Position_ < 3U)
        return;

    UINT8 checksum = 0U;
    for (UINT8 index = 0U; index < Position_; ++index)
        checksum = static_cast<UINT8>(checksum + Command_[index]);
    const bool checksumValid = Position_ < Command_.size() && checksum == Command_[Position_];

    LastID_ = Command_[1];
    const UINT8 payloadLength = Command_[2];
    const auto hasPayload = [payloadLength](UINT8 required) { return payloadLength >= required; };

    switch (Command_[0])
    {
    case 0x20U: // Request status
        BuildResponse(DataResponse, LastID_, &Status_, 1U);
        break;

    case 0x21U: // Request market type
        BuildResponse(DataResponse, LastID_, &MarketType_, 1U);
        break;

    case 0x22U: // Request last error
        BuildResponse(DataResponse, LastID_, &LastError_, 1U);
        break;

    case 0x23U: // Request version
        {
            static constexpr std::array<UINT8, 3> version{{ '0', '2', 'E' }};
            BuildResponse(DataResponse, LastID_, version.data(), static_cast<UINT8>(version.size()));
        }
        break;

    case 0x24U: // Request counter value
        if (hasPayload(1U) && Command_[3] < CounterCount)
        {
            char digits[9]{};
            const unsigned long long value = static_cast<unsigned long long>(Counters_[Command_[3]]) * 10ULL;
            std::snprintf(digits, sizeof(digits), "%08llu", value % 100000000ULL);
            std::array<UINT8, 4> packed{};
            for (size_t index = 0; index < packed.size(); ++index)
            {
                packed[index] = static_cast<UINT8>(((digits[index * 2U] - '0') << 4U) |
                    (digits[index * 2U + 1U] - '0'));
            }
            BuildResponse(DataResponse, LastID_, packed.data(), static_cast<UINT8>(packed.size()));
        }
        else
        {
            BuildResponse(NakResponse, LastID_, nullptr, 0U);
        }
        break;

    case 0x25U: // Request last command ID (MFME returned four bytes)
        {
            const std::array<UINT8, 4> id{{ LastID_, 0U, 0U, 0U }};
            BuildResponse(DataResponse, LastID_, id.data(), static_cast<UINT8>(id.size()));
        }
        break;

    case 0x26U: // Request fingerprint; preserve MFME's little-endian byte order.
        {
            static constexpr std::array<UINT8, 4> fingerprint{{ 0x00U, 0x00U, 0x01U, 0x11U }};
            BuildResponse(DataResponse, LastID_, fingerprint.data(), static_cast<UINT8>(fingerprint.size()));
        }
        break;

    case 0x30U: // Set number of counters
        if (hasPayload(1U))
        {
            NumberOfCounters_ = Command_[3];
            BuildResponse(AckResponse, LastID_, nullptr, 0U);
        }
        else
            BuildResponse(NakResponse, LastID_, nullptr, 0U);
        break;

    case 0x31U: // Set market type
        if (hasPayload(1U))
        {
            MarketType_ = Command_[3];
            BuildResponse(AckResponse, LastID_, nullptr, 0U);
        }
        else
            BuildResponse(NakResponse, LastID_, nullptr, 0U);
        break;

    case 0x32U: // Set counter text
        if (hasPayload(8U) && Command_[3] < CounterCount)
        {
            auto& text = CounterText_[Command_[3]];
            for (size_t index = 0U; index < 7U; ++index)
                text[index] = static_cast<char>(Command_[4U + index]);
            text[7] = '\0';
            BuildResponse(AckResponse, LastID_, nullptr, 0U);
        }
        else
            BuildResponse(NakResponse, LastID_, nullptr, 0U);
        break;

    case 0x40U: // Show text
    case 0x41U: // Show counter value
    case 0x42U: // Show counter text
    case 0x43U: // Show bit pattern
    case 0x54U: // Cycle counter display
    case 0x55U: // Stop cycle
    case 0x5CU: // Self test
        BuildResponse(AckResponse, LastID_, nullptr, 0U);
        break;

    case 0x50U: // Counter increment, small
        if (hasPayload(2U) && Command_[3] < CounterCount)
        {
            Counters_[Command_[3]] += static_cast<UINT32>(Command_[4] & 0x0FU);
            Updated_ = true;
            BuildResponse(AckResponse, LastID_, nullptr, 0U);
        }
        else
            BuildResponse(NakResponse, LastID_, nullptr, 0U);
        break;

    case 0x51U: // Counter increment, medium
        if (hasPayload(2U) && Command_[3] < CounterCount)
        {
            Counters_[Command_[3]] += Command_[4];
            Updated_ = true;
            BuildResponse(AckResponse, LastID_, nullptr, 0U);
        }
        else
            BuildResponse(NakResponse, LastID_, nullptr, 0U);
        break;

    case 0x52U: // Counter increment, large
        if (hasPayload(3U) && Command_[3] < CounterCount)
        {
            Counters_[Command_[3]] += static_cast<UINT32>(Command_[4]) |
                (static_cast<UINT32>(Command_[5]) << 8U);
            Updated_ = true;
            BuildResponse(AckResponse, LastID_, nullptr, 0U);
        }
        else
            BuildResponse(NakResponse, LastID_, nullptr, 0U);
        break;

    default:
        // Bad checksums and unsupported commands are explicitly rejected;
        // this avoids leaving MPU5 firmware indefinitely waiting for data.
        LastError_ = checksumValid ? 0x01U : 0x02U;
        BuildResponse(NakResponse, LastID_, nullptr, 0U);
        break;
    }
}

UINT32 MPU5SEC::GetCounter(UINT8 counter) const
{
    return counter < Counters_.size() ? Counters_[counter] : 0U;
}

const std::array<char, 8>& MPU5SEC::GetCounterText(UINT8 counter) const
{
    static const std::array<char, 8> empty{};
    return counter < CounterText_.size() ? CounterText_[counter] : empty;
}

bool MPU5SEC::ConsumeUpdated()
{
    const bool updated = Updated_;
    Updated_ = false;
    return updated;
}
