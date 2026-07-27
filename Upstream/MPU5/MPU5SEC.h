#pragma once

#include "PA2CoreInterface.h"

#include <array>
#include <cstddef>

class MPU5SEC
{
public:
    static constexpr UINT8 CounterCount = 31U;

    MPU5SEC();

    void Reset();
    void Enable(UINT8 enabled);
    void SetData(UINT8 data);
    void SetClock(UINT8 clock);
    UINT8 ReadData() const { return ReceiveData_; }

    UINT32 GetCounter(UINT8 counter) const;
    const std::array<char, 8>& GetCounterText(UINT8 counter) const;
    bool ConsumeUpdated();

private:
    static constexpr UINT8 DataResponse = 0x60U;
    static constexpr UINT8 AckResponse = 0x61U;
    static constexpr UINT8 NakResponse = 0x62U;
    static constexpr size_t BufferSize = 60U;

    void ResetProtocol();
    void ReceiveByte(UINT8 value);
    void ProcessCommand();
    void BuildResponse(UINT8 code, UINT8 id, const UINT8* data, UINT8 length);

    UINT8 Clock_ = 1U;
    UINT8 Character_ = 0U;
    UINT8 Clocks_ = 0U;
    UINT8 TransmitData_ = 0U;
    std::array<UINT8, BufferSize> Command_{};
    std::array<UINT8, BufferSize> Response_{};
    UINT8 Position_ = 0U;
    UINT8 ReceivePosition_ = 0U;
    UINT8 ReceiveClock_ = 0U;
    UINT8 ReceiveData_ = 1U;
    UINT8 ReceiveLength_ = 0U;
    UINT8 BytesLeft_ = 0U;
    UINT8 LastID_ = 0U;
    UINT8 Status_ = 0x20U;
    UINT8 MarketType_ = 0U;
    UINT8 LastError_ = 0U;
    UINT8 NumberOfCounters_ = 0U;
    bool Enabled_ = false;
    bool Updated_ = false;

    std::array<UINT32, CounterCount> Counters_{};
    std::array<std::array<char, 8>, CounterCount> CounterText_{};
};
