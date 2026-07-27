#pragma once

#include "PA2CoreInterface.h"
#include <array>

class MPU5PICDevice
{
public:
    virtual ~MPU5PICDevice() = default;
    virtual void Reset(UINT8 stake, UINT8 dip1, UINT8 dip2, UINT8 percent, UINT8 prize) = 0;
    virtual void SetKeys(UINT8 stake, UINT8 dip1, UINT8 dip2, UINT8 percent, UINT8 prize) = 0;
    virtual void SetTestSwitch(UINT8 pressed) = 0;
    virtual void SetCharacteriserID(const std::array<UINT8, 4>& id, bool present) = 0;
    virtual void Write(UINT8 offset, UINT8 value) = 0;
    virtual UINT8 Read() const = 0;
    virtual bool HasCharacteriser() const = 0;
    virtual const std::array<UINT8, 4>& GetCharacteriserID() const = 0;
    virtual void Tick(UINT32) {}
};

class MPU5PIC1 final : public MPU5PICDevice
{
public:
    void Reset(UINT8 stake, UINT8 dip1, UINT8 dip2, UINT8 percent, UINT8 prize) override;
    void SetKeys(UINT8 stake, UINT8 dip1, UINT8 dip2, UINT8 percent, UINT8 prize) override;
    void SetTestSwitch(UINT8 pressed) override;
    void SetCharacteriserID(const std::array<UINT8, 4>& id, bool present) override;
    void Write(UINT8 offset, UINT8 value) override;
    UINT8 Read() const override { return InputBit_; }
    bool HasCharacteriser() const override { return CharacteriserPresent_; }
    const std::array<UINT8, 4>& GetCharacteriserID() const override { return CharacteriserID_; }

private:
    static UINT8 ReverseNibble(UINT8 value);
    void RebuildKeyMap();
    void SelectBootstrap();
    void SelectKeyMap();
    void AdvanceInput();

    UINT8 Clock_ = 0;
    UINT8 Bit_ = 0;
    UINT8 Data_ = 0;
    UINT8 Input_ = 0;
    UINT8 InputBit_ = 0;
    UINT8 Clocks_ = 0;
    bool Start_ = false;

    UINT8 Stake_ = 0;
    UINT8 Dip2_ = 0;
    UINT8 Percent_ = 0;
    UINT8 Prize_ = 0;
    UINT8 Position_ = 0;
    bool ReadingKeyMap_ = false;
    bool CharacteriserPresent_ = false;

    std::array<UINT8, 4> CharacteriserID_{{ 'O', 'L', 'D', ' ' }};
    std::array<UINT8, 4> Bootstrap_{{ 0x57U, 0x01U, 0x52U, 0x00U }};
    std::array<UINT8, 5> KeyMap_{};
};

class MPU5PIC2 final : public MPU5PICDevice
{
public:
    void Reset(UINT8 stake, UINT8 dip1, UINT8 dip2, UINT8 percent, UINT8 prize) override;
    void SetKeys(UINT8 stake, UINT8 dip1, UINT8 dip2, UINT8 percent, UINT8 prize) override;
    void SetTestSwitch(UINT8 pressed) override;
    void SetCharacteriserID(const std::array<UINT8, 4>& id, bool present) override;
    void Write(UINT8 offset, UINT8 value) override;
    UINT8 Read() const override { return InputBit_; }
    bool HasCharacteriser() const override { return CharacteriserPresent_; }
    const std::array<UINT8, 4>& GetCharacteriserID() const override { return CharacteriserID_; }

private:
    static UINT8 ReverseNibble(UINT8 value);
    void RebuildConfigurationMap();
    std::array<UINT8, 3> ConfigurationPacket() const;
    void BeginTransaction();
    void CompleteByte(UINT8 value);
    UINT8 ResponseForCommand(UINT8 transactionClass, UINT8 command) const;

    UINT8 Clock_ = 0;
    UINT8 Bit_ = 0;
    UINT8 Data_ = 0;
    UINT8 Input_ = 0;
    UINT8 InputBit_ = 0;
    UINT8 Clocks_ = 0;
    bool Start_ = false;

    UINT8 TransactionClass_ = 0;
    UINT8 TransactionCommand_ = 0;
    UINT8 TransactionPosition_ = 0;

    UINT8 Stake_ = 0;
    UINT8 Dip2_ = 0;
    UINT8 Percent_ = 0;
    UINT8 Prize_ = 0;
    UINT8 TestSwitch_ = 0;
    bool CharacteriserPresent_ = false;

    std::array<UINT8, 4> CharacteriserID_{{ 'P', 'I', 'C', '2' }};
    std::array<UINT8, 5> ConfigurationMap_{};
};

class MPU5PIC3 final : public MPU5PICDevice
{
public:
    void Reset(UINT8 stake, UINT8 dip1, UINT8 dip2, UINT8 percent, UINT8 prize) override;
    void SetKeys(UINT8 stake, UINT8 dip1, UINT8 dip2, UINT8 percent, UINT8 prize) override;
    void SetTestSwitch(UINT8 pressed) override;
    void SetCharacteriserID(const std::array<UINT8, 4>& id, bool present) override;
    void Write(UINT8 offset, UINT8 value) override;
    UINT8 Read() const override { return InputBit_; }
    bool HasCharacteriser() const override { return CharacteriserPresent_; }
    const std::array<UINT8, 4>& GetCharacteriserID() const override { return CharacteriserID_; }
    void Tick(UINT32 cycles) override;
    UINT8 ReadSecurityTestForDSP();

private:
    static UINT8 ReverseNibble(UINT8 value);
    void RebuildConfigurationMap();
    std::array<UINT8, 3> ConfigurationPacket() const;
    void BeginTransaction();
    void CompleteByte(UINT8 value);
    UINT8 ResponseForCommand(UINT8 transactionClass, UINT8 command) const;
    UINT8 PairSecuritySample(UINT8 source);


    UINT8 Clock_ = 0;
    UINT8 Bit_ = 0;
    UINT8 Data_ = 0;
    UINT8 Input_ = 0;
    UINT8 InputBit_ = 0;
    UINT8 Clocks_ = 0;
    bool Start_ = false;

    UINT8 TransactionClass_ = 0;
    UINT8 TransactionCommand_ = 0;
    UINT8 TransactionPosition_ = 0;

    UINT8 Stake_ = 0;
    UINT8 Dip2_ = 0;
    UINT8 Percent_ = 0;
    UINT8 Prize_ = 0;
    UINT8 TestSwitch_ = 0;
    UINT8 SecurityPairSample_ = 0;
    UINT8 SecurityPairSource_ = 0; // 0=none, 1=PIC class 4, 2=DSP engine 7
    UINT8 SecurityPulseQueue_ = 0;
    bool SecurityReleasePending_ = false;
    bool CharacteriserPresent_ = false;

    std::array<UINT8, 4> CharacteriserID_{{ 'P', 'I', 'C', '3' }};
    std::array<UINT8, 5> ConfigurationMap_{};
    std::array<UINT8, 256> SecurityRAM_{};
    UINT32 RealTimeCounter_ = 0;
    UINT32 RTCCycleAccumulator_ = 0;
    bool RTCInitialised_ = false;
    mutable bool PrimaryProbeRejected_ = false;
};

class MPU5PIC
{
public:
    enum class Mode : UINT8
    {
        PIC1 = 0,
        PIC2 = 1,
        PIC3 = 2,
        LegacyMFME = PIC1,
        Programmable = PIC2
    };

    void Reset(UINT8 stake, UINT8 dip1, UINT8 dip2, UINT8 percent, UINT8 prize);
    void SetKeys(UINT8 stake, UINT8 dip1, UINT8 dip2, UINT8 percent, UINT8 prize);
    void SetTestSwitch(UINT8 pressed);
    void SetCharacteriserID(const std::array<UINT8, 4>& id, bool present);
    void SetMode(Mode mode) { Mode_ = mode; }
    Mode GetMode() const { return Mode_; }
    void Write(UINT8 offset, UINT8 value);
    UINT8 Read() const;
    void Tick(UINT32 cycles);
    UINT8 ReadSecurityTestForDSP(UINT8 fallback);
    bool HasCharacteriser() const;
    const std::array<UINT8, 4>& GetCharacteriserID() const;

private:
    MPU5PICDevice& Active();
    const MPU5PICDevice& Active() const;

    Mode Mode_ = Mode::PIC1;
    MPU5PIC1 PIC1_{};
    MPU5PIC2 PIC2_{};
    MPU5PIC3 PIC3_{};
};
