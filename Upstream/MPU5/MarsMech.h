#pragma once

#include "PA2CoreInterface.h"
#include <array>

class MPU5MarsMech
{
public:
    static constexpr UINT8 CoinCount = 6;

    explicit MPU5MarsMech(UINT32 cpuCyclesPerSecond = 16000000U);
    void Reset();
    void Tick(UINT32 cycles);
    UINT8 CoinIn(UINT8 coin, UINT8 coinValue);
    bool CanAcceptCoin(UINT8 coin, UINT8 coinValue) const;

    void SetCommStyle(UINT8 value) { CommStyle_ = value; }
    void SetCommInvert(UINT8 value) { CommInvert_ = value ? 1U : 0U; }
    void SetPulseCycles(UINT32 value) { PulseCycles_ = value; }
    void SetEDCEnable(UINT8 value) { EDCEnable_ = value ? 1U : 0U; }
    void SetLockoutValue(UINT8 coin, UINT8 value);
    void SetCoinOutputs(UINT8 outputs);
    void SetLockoutInvert(UINT8 coin, UINT8 value);
    void SetCoinValue(UINT8 coin, UINT8 value);
    void SetCoinEnable(UINT8 coin, UINT8 value);

    // Coin pulses are held for real emulated time. ASIC reads are
    // non-destructive so polling frequency cannot shorten the pulse.
    UINT8 ReadCoinByte();
    UINT8 GetCoinByte() const;
    UINT8 GetLockoutState() const;
    UINT8 GetCoinValue(UINT8 coin) const
    {
        return coin < CoinValue_.size() ? CoinValue_[coin] : 0U;
    }
    UINT8 GetCoinEnable(UINT8 coin) const
    {
        return coin < CoinEnable_.size() ? CoinEnable_[coin] : 0U;
    }
    UINT8 GetCoinLamp(UINT8 lamp) const { return lamp < Lamps_.size() ? Lamps_[lamp] : 0U; }

private:
    static UINT8 ParallelCode(UINT8 inputLine);
    static UINT8 BinaryCode(UINT8 coinValue, UINT8 fallbackChannel);
    static UINT8 BCDCode(UINT8 coinValue);
    bool IsCoinUnlocked(UINT8 coin) const;
    UINT8 PresentedCoinByte() const;
    UINT32 EffectivePulseCycles() const;

    UINT32 CpuCyclesPerSecond_;
    UINT32 PulseCycles_ = 0U;
    INT64 InputCounter_ = 0;
    INT64 LockCounter_ = 0;
    UINT8 CommStyle_ = 0;
    UINT8 CommInvert_ = 0;
    UINT8 EDCEnable_ = 0;
    UINT8 CoinByte_ = 0;
    UINT8 CoinOutputs_ = 0xFFU;
    std::array<UINT8, CoinCount> LockoutDrive_{};
    std::array<UINT8, CoinCount> LockoutInvert_{};
    std::array<UINT8, CoinCount> CoinValue_{};
    std::array<UINT8, CoinCount> CoinEnable_{};
    std::array<UINT8, 2> Lamps_{};
};
