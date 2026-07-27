#include "MPU5.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>

namespace
{
constexpr std::array<UINT8, 8> kHopperStateMagic{{
    'P','A','2','H','O','P','0','1'
}};
constexpr UINT32 kHopperStateVersion = 2U;

void AppendLE32(std::vector<UINT8>& bytes, UINT32 value)
{
    bytes.push_back(static_cast<UINT8>(value & 0xFFU));
    bytes.push_back(static_cast<UINT8>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<UINT8>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<UINT8>((value >> 24U) & 0xFFU));
}

bool ReadLE32(const std::vector<UINT8>& bytes, size_t& offset, UINT32& value)
{
    if (offset + 4U > bytes.size()) return false;
    value = static_cast<UINT32>(bytes[offset]) |
        (static_cast<UINT32>(bytes[offset + 1U]) << 8U) |
        (static_cast<UINT32>(bytes[offset + 2U]) << 16U) |
        (static_cast<UINT32>(bytes[offset + 3U]) << 24U);
    offset += 4U;
    return true;
}

UINT32 HopperStateChecksum(const UINT8* data, size_t size)
{
    UINT32 hash = 2166136261U;
    for (size_t index = 0U; index < size; ++index)
    {
        hash ^= data[index];
        hash *= 16777619U;
    }
    return hash;
}

bool IsPrintablePICID(const std::array<UINT8, 4>& id)
{
    return std::all_of(id.begin(), id.end(), [](UINT8 value)
    {
        return value >= 0x20U && value <= 0x7EU;
    });
}

UINT32 ReadBigEndianLong(const std::vector<UINT8>& rom, size_t offset)
{
    return (static_cast<UINT32>(rom[offset]) << 24U) |
        (static_cast<UINT32>(rom[offset + 1U]) << 16U) |
        (static_cast<UINT32>(rom[offset + 2U]) << 8U) |
        static_cast<UINT32>(rom[offset + 3U]);
}

std::pair<std::array<UINT8, 4>, bool> DetectCharacteriserID(const std::vector<UINT8>& rom)
{
    const std::array<UINT8, 4> fallback{{ 'O', 'L', 'D', ' ' }};
    if (rom.size() < 12U)
        return { fallback, false };

    // Standard MPU5 personality metadata stores the programmable PIC ID as:
    //   05 03 <four printable ID bytes> 0C 00 "TEST"
    // Keep this established detector first so existing supported cartridges
    // retain exactly the same identity and behaviour.
    for (size_t offset = 0; offset + 12U <= rom.size(); ++offset)
    {
        if (rom[offset] != 0x05U || rom[offset + 1U] != 0x03U ||
            rom[offset + 6U] != 0x0CU || rom[offset + 7U] != 0x00U ||
            rom[offset + 8U] != 'T' || rom[offset + 9U] != 'E' ||
            rom[offset + 10U] != 'S' || rom[offset + 11U] != 'T')
            continue;

        std::array<UINT8, 4> id{{
            rom[offset + 2U], rom[offset + 3U],
            rom[offset + 4U], rom[offset + 5U]
        }};
        if (IsPrintablePICID(id))
            return { id, true };
    }

    // The third-generation program card stores the same four printable
    // personality bytes in its later metadata format:
    //   <ID> FF FF 20 00 0C 00 00 00 "TEST"
    // Match the structural suffix rather than a game title, filename, hash or
    // known characteriser value.
    for (size_t offset = 0; offset + 16U <= rom.size(); ++offset)
    {
        if (rom[offset + 4U] != 0xFFU || rom[offset + 5U] != 0xFFU ||
            rom[offset + 6U] != 0x20U || rom[offset + 7U] != 0x00U ||
            rom[offset + 8U] != 0x0CU || rom[offset + 9U] != 0x00U ||
            rom[offset + 10U] != 0x00U || rom[offset + 11U] != 0x00U ||
            rom[offset + 12U] != 'T' || rom[offset + 13U] != 'E' ||
            rom[offset + 14U] != 'S' || rom[offset + 15U] != 'T')
            continue;

        std::array<UINT8, 4> id{{
            rom[offset], rom[offset + 1U], rom[offset + 2U], rom[offset + 3U]
        }};
        if (IsPrintablePICID(id))
            return { id, true };
    }

    // Red Gaming's later MPU5 software does not contain the metadata block
    // above.  Instead it constructs the expected four-character PIC ID from
    // four consecutive ROM bytes and compares that long word with the ID read
    // from PIC command class 1. Match the complete compiler-generated compare
    // sequence and use its embedded ROM address. This extracts the device ID
    // without using a game name, filename, checksum or hard-coded identity.
    static constexpr std::array<UINT8, 64> redGamingSequence{{
        0x14U, 0x39U, 0, 0, 0, 0, 0x49U, 0xC2U, 0x70U, 0x18U, 0xE1U, 0xAAU,
        0x10U, 0x39U, 0, 0, 0, 0, 0x49U, 0xC0U, 0x72U, 0x10U, 0xE3U, 0xA8U,
        0x84U, 0x80U, 0x10U, 0x39U, 0, 0, 0, 0, 0x49U, 0xC0U, 0xE1U, 0x88U,
        0x84U, 0x80U, 0x10U, 0x39U, 0, 0, 0, 0, 0x49U, 0xC0U, 0x84U, 0x80U,
        0x4EU, 0xB9U, 0, 0, 0, 0, 0xB4U, 0x80U, 0x57U, 0xC0U,
        0x02U, 0x80U, 0x00U, 0x00U, 0x00U, 0x01U
    }};
    const auto isVariableByte = [](size_t index)
    {
        return (index >= 2U && index <= 5U) ||
            (index >= 14U && index <= 17U) ||
            (index >= 28U && index <= 31U) ||
            (index >= 40U && index <= 43U) ||
            (index >= 50U && index <= 53U);
    };

    for (size_t offset = 0; offset + redGamingSequence.size() <= rom.size(); ++offset)
    {
        bool matches = true;
        for (size_t index = 0; index < redGamingSequence.size(); ++index)
        {
            if (isVariableByte(index))
                continue;
            if (rom[offset + index] != redGamingSequence[index])
            {
                matches = false;
                break;
            }
        }
        if (!matches)
            continue;

        const UINT32 first = ReadBigEndianLong(rom, offset + 2U);
        const UINT32 second = ReadBigEndianLong(rom, offset + 14U);
        const UINT32 third = ReadBigEndianLong(rom, offset + 28U);
        const UINT32 fourth = ReadBigEndianLong(rom, offset + 40U);
        if (second != first + 1U || third != first + 2U || fourth != first + 3U ||
            first > rom.size() - 4U)
            continue;

        std::array<UINT8, 4> id{{
            rom[first], rom[first + 1U], rom[first + 2U], rom[first + 3U]
        }};
        if (IsPrintablePICID(id))
            return { id, true };
    }

    return { fallback, false };
}

template <size_t N>
bool RangeContains(const std::vector<UINT8>& rom, size_t begin, size_t end,
    const std::array<UINT8, N>& pattern)
{
    if (begin >= end || pattern.empty() || begin >= rom.size()) return false;
    end = std::min(end, rom.size());
    if (end - begin < pattern.size()) return false;
    return std::search(rom.begin() + begin, rom.begin() + end,
        pattern.begin(), pattern.end()) != rom.begin() + end;
}

bool DetectPIC2ProtocolClient(const std::vector<UINT8>& rom)
{
    // PIC2 software performs a class/command serial transaction and recognises
    // command 0x16 returning 0xA5. Detect that protocol client, never a title,
    // filename, checksum or characteriser identity.
    static constexpr std::array<UINT8, 26> knownClient{{
        0x42U, 0x78U, 0xFFU, 0xD4U,
        0x31U, 0xFCU, 0x00U, 0x01U, 0xFFU, 0xD6U,
        0x48U, 0x78U, 0x00U, 0x16U,
        0x4EU, 0xB9U, 0, 0, 0, 0,
        0x58U, 0x8FU,
        0x0CU, 0x00U, 0x00U, 0xA5U
    }};

    for (size_t offset = 0; offset + knownClient.size() <= rom.size(); ++offset)
    {
        bool matches = true;
        for (size_t index = 0; index < knownClient.size(); ++index)
        {
            if (index >= 16U && index <= 19U) continue; // JSR target varies.
            if (rom[offset + index] != knownClient[index])
            {
                matches = false;
                break;
            }
        }
        if (matches) return true;
    }

    // Accept equivalent compiler output as well. Require all three independent
    // pieces of protocol evidence in one small routine-sized window:
    //   * immediate command 0x16;
    //   * comparison with response 0xA5;
    //   * accesses to the PIC data and clock registers at xxFFD4/xxFFD6.
    static constexpr std::array<std::array<UINT8, 4>, 3> commandForms{{
        std::array<UINT8, 4>{{ 0x48U, 0x78U, 0x00U, 0x16U }}, // PEA.W #$16
        std::array<UINT8, 4>{{ 0x30U, 0x3CU, 0x00U, 0x16U }}, // MOVE.W #$16,D0
        std::array<UINT8, 4>{{ 0x10U, 0x3CU, 0x00U, 0x16U }}  // MOVE.B #$16,D0
    }};
    static constexpr std::array<UINT8, 2> moveqCommand{{ 0x70U, 0x16U }};
    static constexpr std::array<std::array<UINT8, 4>, 3> compareForms{{
        std::array<UINT8, 4>{{ 0x0CU, 0x00U, 0x00U, 0xA5U }}, // CMPI.B
        std::array<UINT8, 4>{{ 0x0CU, 0x40U, 0x00U, 0xA5U }}, // CMPI.W
        std::array<UINT8, 4>{{ 0xB0U, 0x3CU, 0x00U, 0xA5U }}  // CMP.B #$A5,D0
    }};
    static constexpr std::array<UINT8, 2> dataRegister{{ 0xFFU, 0xD4U }};
    static constexpr std::array<UINT8, 2> clockRegister{{ 0xFFU, 0xD6U }};

    for (size_t offset = 0; offset + 4U <= rom.size(); ++offset)
    {
        bool compare = false;
        for (const auto& form : compareForms)
        {
            if (std::equal(form.begin(), form.end(), rom.begin() + offset))
            {
                compare = true;
                break;
            }
        }
        if (!compare) continue;

        const size_t begin = offset > 160U ? offset - 160U : 0U;
        const size_t end = std::min(rom.size(), offset + 96U);
        bool command = RangeContains(rom, begin, end, moveqCommand);
        for (const auto& form : commandForms)
        {
            if (command) break;
            command = RangeContains(rom, begin, end, form);
        }

        if (command && RangeContains(rom, begin, end, dataRegister) &&
            RangeContains(rom, begin, end, clockRegister))
            return true;
    }
    return false;
}

bool DetectPIC3ProtocolClient(const std::vector<UINT8>& rom)
{
    // PIC3 retains the PIC2 0x16 -> 0xA5 probe, but additionally implements
    // class-three programming followed by a two-byte class-four security
    // transaction. Require all of those compiler-level protocol signatures in
    // one routine-sized window so a normal PIC2 client is not promoted merely
    // because unrelated constants occur elsewhere in the ROM.
    static constexpr std::array<UINT8, 4> classThree{{ 0x13U, 0xFCU, 0x00U, 0x03U }};
    static constexpr std::array<UINT8, 4> probeCommand{{ 0x06U, 0x00U, 0x00U, 0x16U }};
    static constexpr std::array<UINT8, 4> classFour{{ 0x13U, 0xFCU, 0x00U, 0x04U }};
    static constexpr std::array<UINT8, 2> twoByteLength{{ 0x70U, 0x02U }};

    for (size_t offset = 0; offset + classThree.size() <= rom.size(); ++offset)
    {
        if (!std::equal(classThree.begin(), classThree.end(), rom.begin() + offset))
            continue;

        const size_t begin = offset > 32U ? offset - 32U : 0U;
        const size_t end = std::min(rom.size(), offset + 0x180U);
        if (RangeContains(rom, begin, end, probeCommand) &&
            RangeContains(rom, begin, end, classFour) &&
            RangeContains(rom, begin, end, twoByteLength))
            return true;
    }

    return false;
}


UINT32 DetectLegacyCharacteriserHook(const std::vector<UINT8>& rom)
{
    // MFME's CharAddr is the address of the comparison instruction immediately
    // after the ROM personality has been copied into D0.  The earlier patch
    // incorrectly returned the instruction following the personality JSR,
    // which can precede the actual comparison by a substantial distance.
    static constexpr std::array<UINT8, 14> lookupSuffix{{
        0x4FU, 0xEFU, 0x00U, 0x0CU,
        0x2DU, 0x40U, 0xFFU, 0xFCU,
        0x0CU, 0x80U, 0x20U, 0x20U, 0x20U, 0x20U
    }};
    static constexpr std::array<UINT8, 6> compareSequence{{
        0x20U, 0x2EU, 0xFFU, 0xFCU, // MOVE.L (-4,A6),D0
        0xB0U, 0x94U                // CMP.L (A4),D0
    }};

    if (rom.size() < 20U) { return 0U; }
    for (size_t offset = 0U; offset + 20U <= rom.size(); ++offset)
    {
        if (rom[offset] != 0x4EU || rom[offset + 1U] != 0xB9U)
            continue;
        if (!std::equal(lookupSuffix.begin(), lookupSuffix.end(), rom.begin() + offset + 6U))
            continue;

        const size_t searchBegin = offset + 20U;
        const size_t searchEnd = std::min(rom.size(), offset + 0x100U);
        for (size_t candidate = searchBegin;
            candidate + compareSequence.size() <= searchEnd; ++candidate)
        {
            if (std::equal(compareSequence.begin(), compareSequence.end(), rom.begin() + candidate))
                return static_cast<UINT32>(candidate + 4U);
        }
    }
    return 0U;
}

void RollBarbusChecksum(UINT16& total, UINT8 value)
{
    for (UINT8 bit = 0; bit < 8U; ++bit)
    {
        total = static_cast<UINT16>(((total ^ value) & 1U) != 0
            ? ((total >> 1) ^ 0xA001U)
            : (total >> 1));
        value >>= 1;
    }
}

void WriteBarbusChecksum(UINT8* message, UINT32 finalLength)
{
    if (!message || finalLength < 3U) { return; }

    UINT16 total = 0;
    RollBarbusChecksum(total, 0x9BU);
    RollBarbusChecksum(total, 0xD9U);
    UINT8* cursor = message + 1U;
    for (UINT32 index = 0; index < finalLength - 3U; ++index)
    {
        RollBarbusChecksum(total, *cursor++);
    }
    *cursor++ = static_cast<UINT8>(total);
    RollBarbusChecksum(total, static_cast<UINT8>(total));
    *cursor = static_cast<UINT8>(total);
}

struct SparseLampPlaneInfo
{
    UINT32 Consumed = 0U;
    bool AnyActive = false;
    bool FullNonZero = false;
    bool FullZeroWrite = false;
    bool DimNonZero = false;
};

SparseLampPlaneInfo InspectSparseLampPlanes(
    const UINT8* data, UINT32 available)
{
    SparseLampPlaneInfo result{};
    if (!data || available < 2U) return result;

    const UINT8 fullMask = data[0];
    const UINT8 dimMask = data[1];
    result.Consumed = 2U;

    for (UINT32 column = 0; column < 8U; ++column)
    {
        const UINT8 bit = static_cast<UINT8>(0x80U >> column);
        if ((fullMask & bit) != 0U)
        {
            if (result.Consumed >= available) return result;
            const UINT8 value = data[result.Consumed++];
            result.AnyActive = result.AnyActive || value != 0U;
            result.FullNonZero = result.FullNonZero || value != 0U;
            result.FullZeroWrite = result.FullZeroWrite || value == 0U;
        }
        if ((dimMask & bit) != 0U)
        {
            if (result.Consumed >= available) return result;
            const UINT8 value = data[result.Consumed++];
            result.AnyActive = result.AnyActive || value != 0U;
            result.DimNonZero = result.DimNonZero || value != 0U;
        }
    }
    return result;
}

FILE* OpenDiagnosticFile(const char* fileName, const char* mode)
{
#if defined(_MSC_VER)
    FILE* file = nullptr;
    fopen_s(&file, fileName, mode);
    return file;
#else
    return std::fopen(fileName, mode);
#endif
}
}

MPU5::MPU5() : DMA_(*this), Mars_(CPUCyclesPerSecond)
{
    RAM_.fill(0);
    InternalRegisters_.fill(0);
    Switches_.fill(0);
    Dips_.fill(0);
}

bool MPU5::Initialise()
{
    if (Initialised_) { return true; }
    RAM_.fill(0);
    InternalRegisters_.fill(0);
    Switches_.fill(0);
    Dips_.fill(0);
    Initialised_ = true;
    return Reset();
}

void MPU5::Shutdown()
{
    Initialised_ = false;
}

bool MPU5::Reset()
{
    if (!Initialised_) { Initialised_ = true; }
    ConfigurationResetPending_ = false;
    ApplyEffectivePICMode();
    ApplyRequestedHardwareConfiguration();
    CurrentFunctionCode_ = 0;
    TotalCycles_ = 0;
    RunCycleCarry_ = 0;
    CharacteriserHookPending_ = PICMode_ == MPU5PIC::Mode::LegacyMFME && CharacteriserHookAddress_ != 0U;
    ResetTimingStats();
    if (DUARTTraceEnabled_) { InitialiseDUARTTraceFile(); }
    InternalRegisters_.fill(0);
    AssertedIRQLevel_ = 0;
    SIM_.Reset();
    Timer_.Reset();
    Serial_.Reset();
    DMA_.Reset();
    ASIC_.Reset();
    DSPStatusResponsePending_ = false;

    // A board reset releases the momentary Test switch. The frontend will
    // deliver a new rising edge if the user is still operating the control.
    TestPulseCyclesRemaining_ = 0U;
    TestPulseQueue_ = 0U;
    TestSwitchRequested_ = 0U;
    TestPulseHigh_ = false;
    TestPulseGap_ = false;
    if (AutomaticTestModeSequence_)
        SecondaryTestSwitchState_ = 0U;
    AutomaticTestModeSequence_ = false;
    Switches_[255U] = 0U;
    PIC_.SetTestSwitch(0U);

    // Door, refill and Test 2 are physical cabinet levels. Reapply them after
    // every reset using the effective PIC-generation loom mapping.
    ApplyCabinetInputMappings();

    PIC_.SetMode(PICMode_);
    PIC_.Reset(StakeKeyCode(), DipBank1Byte(), DipBank2Byte(), PercentKeyCode(), PrizeKeyCode());
    SEC_.Reset();
    Sound_.Reset();
    DUART_.Reset();
    EDC_.Reset();
    LampCurrentTestActive_.fill(false);
    LampCurrentTestSawFullDrive_.fill(false);
    LampCurrentSensePulseIssued_.fill(false);
    LampCurrentSensePulseReplies_.fill(0U);
    for (MPU5Alpha& display : Alpha_) { display.Reset(); display.Enable(0); }
    Lamps_.Reset();
    Mars_.Reset();
    Meters_.Reset();
    Reels_.Reset();
    Segments_.Reset();
    Hoppers_.Reset();

    m68k_set_cpu_type(M68K_CPU_TYPE_68020);
    m68k_pulse_reset();

    return true;
}

INT32 MPU5::Run(UINT32 cycles)
{
    if (!Initialised_ || cycles == 0U) { return 0; }

    // Amber loads ROMs before it has necessarily delivered every layout
    // setting.  Startup-critical changes are coalesced and applied through one
    // reset immediately before the next execution slice.  This reproduces the
    // configured power-on state without requiring the user to press Reset a
    // few seconds after loading the game.
    if (ConfigurationResetPending_) { Reset(); }

    ++TimingStats_.RunCalls;
    TimingStats_.RequestedCycles += cycles;

    // Execute only complete, fixed-size internal quanta. Finishing each video
    // frame with a different short Musashi slice made interrupt observation
    // depend on whether Amber was rendering at 60, 120 or 144 Hz. Any small
    // amount executed ahead of the requested 16 MHz timeline is retained as a
    // negative carry and paid back by later Run calls.
    static constexpr INT32 ExecutionQuantum = 4096;
    INT64 budget = static_cast<INT64>(cycles) + RunCycleCarry_;

    while (budget > 0)
    {
        const int ran = m68k_execute(ExecutionQuantum);
        if (ran <= 0) { break; }
        TimingStats_.ExecutedCycles += static_cast<UINT64>(ran);
        budget -= static_cast<INT64>(ran);
    }

    RunCycleCarry_ = budget;
    TimingStats_.RunCycleCarry = RunCycleCarry_;
    if (budget < 0)
    {
        const INT64 ahead64 = -budget;
        const INT32 ahead = ahead64 > std::numeric_limits<INT32>::max()
            ? std::numeric_limits<INT32>::max()
            : static_cast<INT32>(ahead64);
        TimingStats_.MaximumRunAhead = std::max(TimingStats_.MaximumRunAhead, ahead);
    }
    MaybeWriteTimingTrace();

    if (budget > 0) { return 0; }
    return static_cast<INT32>(cycles);
}

bool MPU5::ReadFile(const char* name, std::vector<UINT8>& data)
{
    data.clear();
    if (!name || !name[0]) { return false; }
    std::ifstream input(name, std::ios::binary | std::ios::ate);
    if (!input) { return false; }
    const std::streamoff length = input.tellg();
    if (length < 0 || static_cast<unsigned long long>(length) > std::numeric_limits<UINT32>::max()) { return false; }
    data.resize(static_cast<size_t>(length));
    input.seekg(0, std::ios::beg);
    return data.empty() || static_cast<bool>(input.read(reinterpret_cast<char*>(data.data()), length));
}

bool MPU5::LoadConcatenatedFiles(std::vector<UINT8>& destination,
    const char* name1, const char* name2, const char* name3, const char* name4,
    UINT32& totalBytes)
{
    destination.clear();
    totalBytes = 0;
    const char* names[4] = { name1, name2, name3, name4 };
    bool any = false;
    for (const char* name : names)
    {
        if (!name || !name[0]) { continue; }
        any = true;
        std::vector<UINT8> file;
        if (!ReadFile(name, file) || file.size() > std::numeric_limits<size_t>::max() - destination.size()) { destination.clear(); return false; }
        destination.insert(destination.end(), file.begin(), file.end());
    }
    if (!any || destination.size() > std::numeric_limits<UINT32>::max()) { destination.clear(); return false; }
    totalBytes = static_cast<UINT32>(destination.size());
    return true;
}

UINT32 MPU5::LoadProgramROM(const char* name1, const char* name2, const char* name3, const char* name4)
{
    const char* names[4] = { name1, name2, name3, name4 };
    std::vector<std::vector<UINT8>> files;
    for (const char* name : names)
    {
        if (!name || !name[0]) { continue; }
        std::vector<UINT8> file;
        if (!ReadFile(name, file))
        {
            ProgramROM_.clear();
            ProgramROMLoaded_ = false;
            ROMUsesPIC3Protocol_ = false;
            ROMUsesPIC2Protocol_ = false;
            ROMUsesLegacyPICProtocol_ = false;
            ApplyEffectivePICMode();
            ApplyRequestedHardwareConfiguration();
            return 0;
        }
        files.push_back(std::move(file));
    }
    if (files.empty())
    {
        ProgramROM_.clear();
        ProgramROMLoaded_ = false;
        ROMUsesPIC3Protocol_ = false;
        ROMUsesPIC2Protocol_ = false;
        ROMUsesLegacyPICProtocol_ = false;
        ApplyEffectivePICMode();
        ApplyRequestedHardwareConfiguration();
        return 0;
    }

    ProgramROM_.clear();
    const bool pairInterleave = (files.size() == 2 || files.size() == 4) &&
        files[0].size() == files[1].size() &&
        (files.size() == 2 || files[2].size() == files[3].size());

    if (pairInterleave)
    {
        for (size_t pair = 0; pair < files.size(); pair += 2)
        {
            const size_t oldSize = ProgramROM_.size();
            const size_t pairBytes = files[pair].size() * 2U;
            if (oldSize + pairBytes > MaximumProgramROM)
            {
                ProgramROM_.clear();
                ProgramROMLoaded_ = false;
                ROMUsesPIC3Protocol_ = false;
                ROMUsesPIC2Protocol_ = false;
                ROMUsesLegacyPICProtocol_ = false;
                ApplyEffectivePICMode();
                ApplyRequestedHardwareConfiguration();
                return 0;
            }
            ProgramROM_.resize(oldSize + pairBytes);
            for (size_t index = 0; index < files[pair].size(); ++index)
            {
                ProgramROM_[oldSize + index * 2U] = files[pair][index];
                ProgramROM_[oldSize + index * 2U + 1U] = files[pair + 1U][index];
            }
        }
    }
    else
    {
        for (const auto& file : files)
        {
            if (ProgramROM_.size() + file.size() > MaximumProgramROM)
            {
                ProgramROM_.clear();
                ProgramROMLoaded_ = false;
                ROMUsesPIC3Protocol_ = false;
                ROMUsesPIC2Protocol_ = false;
                ROMUsesLegacyPICProtocol_ = false;
                ApplyEffectivePICMode();
                ApplyRequestedHardwareConfiguration();
                return 0;
            }
            ProgramROM_.insert(ProgramROM_.end(), file.begin(), file.end());
        }
    }

    ProgramROMLoaded_ = !ProgramROM_.empty();
    const auto characteriser = DetectCharacteriserID(ProgramROM_);
    PIC_.SetCharacteriserID(characteriser.first, characteriser.second);
    const UINT32 detectedLegacyHook = DetectLegacyCharacteriserHook(ProgramROM_);
    ROMUsesLegacyPICProtocol_ = detectedLegacyHook != 0U;
    ROMUsesPIC3Protocol_ = !ROMUsesLegacyPICProtocol_ &&
        DetectPIC3ProtocolClient(ProgramROM_);
    ROMUsesPIC2Protocol_ = !ROMUsesLegacyPICProtocol_ && !ROMUsesPIC3Protocol_ &&
        DetectPIC2ProtocolClient(ProgramROM_);

    // Select from the protection protocol implemented by the ROM. A legacy
    // personality call/compare path identifies PIC1; class-three programming
    // plus class-four security identifies PIC3; the remaining class/command
    // client identifies PIC2. No machine identity is used.
    ApplyEffectivePICMode();
    ApplyRequestedHardwareConfiguration();

    if (!CharacteriserAddressOverridden_)
        CharacteriserHookAddress_ = detectedLegacyHook;
    CharacteriserHookPending_ = PICMode_ == MPU5PIC::Mode::LegacyMFME && CharacteriserHookAddress_ != 0U;
    if (SoundROM_.empty()) { RefreshSoundSamples(); }
    if (Initialised_) { Reset(); }
    return static_cast<UINT32>(ProgramROM_.size());
}

UINT32 MPU5::LoadSoundROM(const char* name1, const char* name2, const char* name3, const char* name4)
{
    UINT32 totalBytes = 0;
    if (!LoadConcatenatedFiles(SoundROM_, name1, name2, name3, name4, totalBytes))
    {
        SoundROM_.clear();
        RefreshSoundSamples();
        return 0;
    }
    RefreshSoundSamples();
    return totalBytes;
}

void MPU5::RefreshSoundSamples()
{
    if (!SoundROM_.empty())
    {
        Sound_.LoadROM(SoundROM_);
    }
    else if (!ProgramROM_.empty())
    {
        Sound_.LoadROM(ProgramROM_);
    }
    else
    {
        Sound_.ClearSamples();
    }
}

UINT8 MPU5::DipBank1Byte() const
{
    UINT8 value = 0;
    for (UINT8 i = 0; i < 8U && i < Dips_.size(); ++i)
        if (Dips_[i]) value |= static_cast<UINT8>(1U << i);
    return value;
}

UINT8 MPU5::DipBank2Byte() const
{
    UINT8 value = 0;
    for (UINT8 i = 0; i < 8U && static_cast<size_t>(i + 8U) < Dips_.size(); ++i)
        if (Dips_[i + 8U]) value |= static_cast<UINT8>(1U << i);
    return value;
}

UINT8 MPU5::StakeKeyCode() const
{
    // Amber stores the stake selector as its displayed list index:
    // 0=None, 1=5p, 2=10p, 3=20p, 4=25p, 5=30p, ...
    // The MPU5 PIC code used by MFME is one lower from 10p upward, while
    // None and 5p both select code zero.
    if (Stake_ <= 1U) { return 0U; }
    return static_cast<UINT8>((Stake_ - 1U) & 0x07U);
}

UINT8 MPU5::PrizeKeyCode() const
{
    // Convert the front-end prize selector to the physical MPU5 prize-key
    // wiring code. PIC2 class zero transfers this nibble during cold boot.
    static constexpr std::array<UINT8, 7> codes{{
        0x0U, // None
        0x8U, // GBP 5 cash
        0x6U, // GBP 8 token
        0x5U, // GBP 8 cash
        0x7U, // GBP 10
        0x9U, // GBP 15
        0xAU  // GBP 25
    }};

    // Later selector values are already physical key nibbles. Alien uses
    // key code 0xD for its GBP 70, 25p-to-GBP 1 configuration.
    return Prize_ < codes.size()
        ? codes[Prize_]
        : static_cast<UINT8>(Prize_ & 0x0FU);
}

UINT8 MPU5::PercentKeyCode() const
{
    // 0=None, 1=70%, followed by two-percent steps through 15=98%.
    return static_cast<UINT8>(Percent_ & 0x0FU);
}

void MPU5::UpdatePICKeys()
{
    PIC_.SetKeys(StakeKeyCode(), DipBank1Byte(), DipBank2Byte(), PercentKeyCode(), PrizeKeyCode());
}

void MPU5::ApplyRequestedHardwareConfiguration()
{
    // Coin mechanism and DataPak fitting are cabinet/layout properties, not PIC
    // types and not game identities. Preserve exactly what the front end asks
    // for; PIC auto-detection is deliberately independent of these settings.
    Mars_.SetCommStyle(RequestedCommStyle_);
    Hoppers_.SetSerialCoinMechEnabled(RequestedCommStyle_ == 3U ? 1U : 0U);
    Mars_.SetEDCEnable(RequestedEDCEnabled_ ? 1U : 0U);
    EDCEnabled_ = RequestedEDCEnabled_;
}


void MPU5::ApplyEffectivePICMode()
{
    // Conclusive ROM protocol evidence takes priority over stale per-layout
    // settings. Explicit selection remains available only for an unrecognised
    // ROM, with PIC1 as the safe default.
    if (ROMUsesLegacyPICProtocol_)
        PICMode_ = MPU5PIC::Mode::PIC1;
    else if (ROMUsesPIC3Protocol_)
        PICMode_ = MPU5PIC::Mode::PIC3;
    else if (ROMUsesPIC2Protocol_)
        PICMode_ = MPU5PIC::Mode::PIC2;
    else
        PICMode_ = PICModeExplicit_ ? RequestedPICMode_ : MPU5PIC::Mode::PIC1;

    PIC_.SetMode(PICMode_);
    MPU5Lamps::ExternalProtocol lampProtocol =
        MPU5Lamps::ExternalProtocol::SinglePlane;
    if (PICMode_ == MPU5PIC::Mode::PIC2)
        lampProtocol = MPU5Lamps::ExternalProtocol::IndependentDimPlane;
    else if (PICMode_ == MPU5PIC::Mode::PIC3)
        lampProtocol = MPU5Lamps::ExternalProtocol::BrightnessMask;
    Lamps_.SetExternalProtocol(lampProtocol);

    // PIC3 appeared with the later MPU5 ASIC-D/DSP revision-six program-card
    // environment. Earlier PIC generations retain the established values used
    // by existing layouts.
    ASIC_.SetHardwareRevision(PICMode_ == MPU5PIC::Mode::PIC3 ? 5U : 0U);
    ASIC_.SetDSPRevision(PICMode_ == MPU5PIC::Mode::PIC3 ? 6U : 5U);
    CharacteriserHookPending_ =
        PICMode_ == MPU5PIC::Mode::LegacyMFME && CharacteriserHookAddress_ != 0U;

    ApplyReelJumperProfiles();
    ApplyCabinetInputMappings();
}

void MPU5::ApplyCabinetInputMappings()
{
    // Clear both generations' dedicated cabinet lines before applying the
    // active loom. This prevents a door state set before ROM/PIC detection
    // from remaining on the old switch number after the mode changes.
    Switches_[1U] = 0U;
    Switches_[2U] = 0U;
    Switches_[10U] = 0U;
    Switches_[11U] = 0U;
    Switches_[24U] = 0U;

    if (PICMode_ == MPU5PIC::Mode::PIC3)
    {
        Switches_[10U] = ServiceDoorState_;
        Switches_[2U] = MainDoorState_;
        Switches_[11U] = RefillKeyState_;
        Switches_[24U] = SecondaryTestSwitchState_;
    }
    else
    {
        Switches_[2U] = ServiceDoorState_;
        Switches_[24U] = MainDoorState_;
        Switches_[1U] = RefillKeyState_;
    }
}

void MPU5::ApplyReelJumperProfiles()
{
    for (UINT8 controller = 0U; controller < RequestedReelJumperProfile_.size(); ++controller)
    {
        const UINT8 requested = RequestedReelJumperProfile_[controller];
        const bool lateProfile = requested == 2U ||
            (requested == 0U && PICMode_ == MPU5PIC::Mode::PIC3);
        Reels_.SetJumperProfile(controller, lateProfile ? 1U : 0U);
    }
}

void MPU5::ScheduleConfigurationReset()
{
    if (Initialised_ && ProgramROMLoaded_)
        ConfigurationResetPending_ = true;
}

void MPU5::StartNextTestPulse()
{
    if (TestPulseHigh_ || TestPulseGap_ || TestPulseQueue_ == 0U) { return; }

    --TestPulseQueue_;
    TestPulseHigh_ = true;
    TestPulseCyclesRemaining_ = TestPulseHighCycles;
    Switches_[255U] = 1U;
    PIC_.SetTestSwitch(1U);
}

void MPU5::TickTestInput(UINT32 cycles)
{
    UINT32 elapsed = cycles;
    while (elapsed != 0U && (TestPulseHigh_ || TestPulseGap_))
    {
        if (elapsed < TestPulseCyclesRemaining_)
        {
            TestPulseCyclesRemaining_ -= elapsed;
            return;
        }

        elapsed -= TestPulseCyclesRemaining_;
        TestPulseCyclesRemaining_ = 0U;

        if (TestPulseHigh_)
        {
            TestPulseHigh_ = false;
            TestPulseGap_ = true;
            TestPulseCyclesRemaining_ = TestPulseGapCycles;
            Switches_[255U] = 0U;
            PIC_.SetTestSwitch(0U);
        }
        else
        {
            TestPulseGap_ = false;
            StartNextTestPulse();
        }
    }

    FinishAutomaticTestModeSequence();
}

void MPU5::FinishAutomaticTestModeSequence()
{
    if (!AutomaticTestModeSequence_ || TestPulseHigh_ || TestPulseGap_ ||
        TestPulseQueue_ != 0U)
    {
        return;
    }

    AutomaticTestModeSequence_ = false;
    SetSecondaryTestSwitchState(0U);
}

void MPU5::SetSwitch(UINT8 num, UINT8 value)
{
    const UINT8 state = value != 0U ? 1U : 0U;

    // The dedicated Test input is sampled through both the PIC and the DSP
    // security engine. Preserve every frontend rising edge as one complete
    // hardware-like pulse so a short mouse click cannot occur entirely between
    // those two polling paths. Queued clicks remain separated by a real low
    // interval and therefore cannot collapse into one long press.
    if (num == 255U)
    {
        const UINT8 previousRequest = TestSwitchRequested_;
        TestSwitchRequested_ = state;
        if (state != 0U && previousRequest == 0U)
        {
            // The existing Amber front-end has one semantic Test button and no
            // separate Test-2 control. On PIC3/Genesis machines one click while
            // the service door is open therefore performs the complete physical
            // entry sequence: hold Test 2 and pulse the program-card Test input
            // twice. PIC1/PIC2 retain their existing one-pulse-per-click path.
            if (PICMode_ == MPU5PIC::Mode::PIC3 && ServiceDoorState_ != 0U)
            {
                BeginAutomaticTestModeSequence();
            }
            else
            {
                if (TestPulseQueue_ < 8U) { ++TestPulseQueue_; }
                StartNextTestPulse();
            }
        }
        return;
    }

    Switches_[num] = state;
}

void MPU5::SetServiceDoorState(UINT8 value)
{
    const UINT8 state = value != 0U ? 1U : 0U;
    if (ServiceDoorState_ == state) { return; }
    ServiceDoorState_ = state;

    // The physical machine re-enters its power-on/initialisation path when the
    // service-door state changes. Store the new level first because Reset()
    // deliberately preserves cabinet switch inputs while resetting the boards.
    ApplyCabinetInputMappings();
    Reset();
}

void MPU5::SetMainDoorState(UINT8 value)
{
    MainDoorState_ = value != 0U ? 1U : 0U;
    ApplyCabinetInputMappings();
}

void MPU5::SetRefillKeyState(UINT8 value)
{
    RefillKeyState_ = value != 0U ? 1U : 0U;
    ApplyCabinetInputMappings();
}

void MPU5::SetSecondaryTestSwitchState(UINT8 value)
{
    const UINT8 state = value != 0U ? 1U : 0U;
    SecondaryTestSwitchState_ = state;

    // Do not alias this onto switch 24 for older PIC generations because that
    // line is used as a cabinet-door input by some earlier layouts.
    ApplyCabinetInputMappings();
}

UINT8 MPU5::BeginAutomaticTestModeSequence()
{
    // Opening the service door performs a board reset, so the request must be
    // made after the door-open transition has completed.
    if (ServiceDoorState_ == 0U || AutomaticTestModeSequence_ ||
        TestPulseHigh_ || TestPulseGap_ || TestPulseQueue_ != 0U)
    {
        return 0U;
    }

    AutomaticTestModeSequence_ = true;
    if (PICMode_ == MPU5PIC::Mode::PIC3)
        SetSecondaryTestSwitchState(1U);

    // Later MPU5 cabinets require Test 2 held while the primary Test button
    // is pressed twice. The same helper remains available explicitly through
    // RequestTestMode(); PIC1/PIC2 callers still receive two clean pulses but
    // the ordinary front-end button path for those generations is unchanged.
    TestPulseQueue_ = 2U;
    StartNextTestPulse();
    return 1U;
}

UINT8 MPU5::RequestTestMode()
{
    return BeginAutomaticTestModeSequence();
}

UINT8 MPU5::GetSwitch(UINT8 num) const { return Switches_[num]; }
void MPU5::SetDIP(UINT8 num, UINT8 value)
{
    if (num >= Dips_.size()) { return; }
    const UINT8 state = value != 0U ? 1U : 0U;
    if (Dips_[num] == state) { return; }
    Dips_[num] = state;
    UpdatePICKeys();
}
void MPU5::SetStake(UINT8 value)
{
    if (Stake_ == value) { return; }
    Stake_ = value;
    UpdatePICKeys();
    ScheduleConfigurationReset();
}
void MPU5::SetPrize(UINT8 value)
{
    if (Prize_ == value) { return; }
    Prize_ = value;
    UpdatePICKeys();
    ScheduleConfigurationReset();
}
void MPU5::SetPercent(UINT8 value)
{
    if (Percent_ == value) { return; }
    Percent_ = value;
    UpdatePICKeys();
    ScheduleConfigurationReset();
}
void MPU5::SetCharacteriserAddress(UINT32 address)
{
    CharacteriserHookAddress_ = address;
    CharacteriserAddressOverridden_ = address != 0U;
    if (!CharacteriserAddressOverridden_ && ProgramROMLoaded_)
        CharacteriserHookAddress_ = DetectLegacyCharacteriserHook(ProgramROM_);
    CharacteriserHookPending_ = PICMode_ == MPU5PIC::Mode::LegacyMFME && CharacteriserHookAddress_ != 0U;
}

void MPU5::SetLegacyPICMode(UINT8 enable)
{
    // Preserve the established exported API: one requests PIC1 and zero
    // requests PIC2. Conclusive ROM protocol detection remains authoritative;
    // this setting is only the fallback for an unrecognised implementation.
    const MPU5PIC::Mode requested = enable != 0U
        ? MPU5PIC::Mode::LegacyMFME
        : MPU5PIC::Mode::Programmable;
    const MPU5PIC::Mode oldMode = PICMode_;
    const MPU5PIC::Mode oldRequested = RequestedPICMode_;
    const bool oldExplicit = PICModeExplicit_;
    RequestedPICMode_ = requested;
    PICModeExplicit_ = true;
    ApplyEffectivePICMode();
    PIC_.Reset(StakeKeyCode(), DipBank1Byte(), DipBank2Byte(), PercentKeyCode(), PrizeKeyCode());
    if (oldMode != PICMode_ || oldRequested != RequestedPICMode_ ||
        oldExplicit != PICModeExplicit_)
        ScheduleConfigurationReset();
}

void MPU5::SetPICMode(UINT8 mode)
{
    MPU5PIC::Mode requested = MPU5PIC::Mode::PIC1;
    if (mode == 2U)
        requested = MPU5PIC::Mode::PIC2;
    else if (mode >= 3U)
        requested = MPU5PIC::Mode::PIC3;

    const MPU5PIC::Mode oldMode = PICMode_;
    const MPU5PIC::Mode oldRequested = RequestedPICMode_;
    const bool oldExplicit = PICModeExplicit_;
    RequestedPICMode_ = requested;
    PICModeExplicit_ = true;
    ApplyEffectivePICMode();
    PIC_.Reset(StakeKeyCode(), DipBank1Byte(), DipBank2Byte(), PercentKeyCode(), PrizeKeyCode());
    if (oldMode != PICMode_ || oldRequested != RequestedPICMode_ ||
        oldExplicit != PICModeExplicit_)
        ScheduleConfigurationReset();
}

void MPU5::SetSECFitted(UINT8 fitted)
{
    const bool value = fitted != 0U;
    if (SECFitted_ == value) { return; }
    SECFitted_ = value;
    SEC_.Reset();
    Meters_.Reset();
    ScheduleConfigurationReset();
}

void MPU5::SetHopperType(UINT8 type)
{
    Hoppers_.SetType(type);
}

void MPU5::SetHopperEnable(UINT8 hopper, UINT8 enabled) { Hoppers_.SetEnabled(hopper, enabled); }
void MPU5::SetHopperCoinsIn(UINT8 hopper, UINT32 count) { Hoppers_.SetCoinsIn(hopper, count); }
void MPU5::SetHopperCoinsOut(UINT8 hopper, UINT32 count) { Hoppers_.SetCoinsOut(hopper, count); }
void MPU5::SetHopperOptoEnable(UINT8 hopper, UINT8 port) { Hoppers_.SetOptoEnable(hopper, port); }
void MPU5::SetHopperOptoReturn(UINT8 hopper, UINT8 port) { Hoppers_.SetOptoReturn(hopper, port); }
void MPU5::SetHopperMotorEnable(UINT8 hopper, UINT8 port) { Hoppers_.SetMotorEnable(hopper, port); }
void MPU5::SetHopperCoin(UINT8 hopper, UINT8 coin) { Hoppers_.SetCoin(hopper, coin); }
void MPU5::SetHopperLevel(UINT8 hopper, UINT32 level) { Hoppers_.SetLevel(hopper, level); }
void MPU5::SetHopperFullLevel(UINT8 hopper, UINT32 level) { Hoppers_.SetFullLevel(hopper, level); }
void MPU5::SetHopperLoEnable(UINT8 hopper, UINT8 enabled) { Hoppers_.SetLowEnable(hopper, enabled); }
void MPU5::SetHopperLoInvert(UINT8 hopper, UINT8 inverted) { Hoppers_.SetLowInvert(hopper, inverted); }
void MPU5::SetHopperLoSwitch(UINT8 hopper, UINT8 switchNumber) { Hoppers_.SetLowSwitch(hopper, switchNumber); }
void MPU5::SetHopperLoLevel(UINT8 hopper, UINT32 level) { Hoppers_.SetLowLevel(hopper, level); }
void MPU5::SetHopperHiEnable(UINT8 hopper, UINT8 enabled) { Hoppers_.SetHighEnable(hopper, enabled); }
void MPU5::SetHopperHiInvert(UINT8 hopper, UINT8 inverted) { Hoppers_.SetHighInvert(hopper, inverted); }
void MPU5::SetHopperHiSwitch(UINT8 hopper, UINT8 switchNumber) { Hoppers_.SetHighSwitch(hopper, switchNumber); }
void MPU5::SetHopperHiLevel(UINT8 hopper, UINT32 level) { Hoppers_.SetHighLevel(hopper, level); }
void MPU5::SetHopperLoIndicator(UINT8 hopper, UINT8 lamp) { Hoppers_.SetLowIndicator(hopper, lamp); }
void MPU5::SetHopperHiIndicator(UINT8 hopper, UINT8 lamp) { Hoppers_.SetHighIndicator(hopper, lamp); }
void MPU5::SetHopperCoinsRefilled(UINT8 hopper, UINT32 count) { Hoppers_.SetCoinsRefilled(hopper, count); }
UINT8 MPU5::GetHopperEnable(UINT8 hopper) const { return Hoppers_.GetEnabled(hopper); }
UINT32 MPU5::GetHopperCoinsIn(UINT8 hopper) const { return Hoppers_.GetCoinsIn(hopper); }
UINT32 MPU5::GetHopperCoinsOut(UINT8 hopper) const { return Hoppers_.GetCoinsOut(hopper); }
UINT8 MPU5::GetHopperOptoEnable(UINT8 hopper) const { return Hoppers_.GetOptoEnable(hopper); }
UINT8 MPU5::GetHopperOptoReturn(UINT8 hopper) const { return Hoppers_.GetOptoReturn(hopper); }
UINT8 MPU5::GetHopperMotorEnable(UINT8 hopper) const { return Hoppers_.GetMotorEnable(hopper); }
UINT8 MPU5::GetHopperCoin(UINT8 hopper) const { return Hoppers_.GetCoin(hopper); }
UINT32 MPU5::GetHopperLevel(UINT8 hopper) const { return Hoppers_.GetLevel(hopper); }
UINT32 MPU5::GetHopperFullLevel(UINT8 hopper) const { return Hoppers_.GetFullLevel(hopper); }
UINT8 MPU5::GetHopperLoEnable(UINT8 hopper) const { return Hoppers_.GetLowEnable(hopper); }
UINT8 MPU5::GetHopperLoInvert(UINT8 hopper) const { return Hoppers_.GetLowInvert(hopper); }
UINT8 MPU5::GetHopperLoSwitch(UINT8 hopper) const { return Hoppers_.GetLowSwitch(hopper); }
UINT32 MPU5::GetHopperLoLevel(UINT8 hopper) const { return Hoppers_.GetLowLevel(hopper); }
UINT8 MPU5::GetHopperHiEnable(UINT8 hopper) const { return Hoppers_.GetHighEnable(hopper); }
UINT8 MPU5::GetHopperHiInvert(UINT8 hopper) const { return Hoppers_.GetHighInvert(hopper); }
UINT8 MPU5::GetHopperHiSwitch(UINT8 hopper) const { return Hoppers_.GetHighSwitch(hopper); }
UINT32 MPU5::GetHopperHiLevel(UINT8 hopper) const { return Hoppers_.GetHighLevel(hopper); }
UINT8 MPU5::GetHopperLoIndicator(UINT8 hopper) const { return Hoppers_.GetLowIndicator(hopper); }
UINT8 MPU5::GetHopperHiIndicator(UINT8 hopper) const { return Hoppers_.GetHighIndicator(hopper); }
UINT32 MPU5::GetHopperCoinsRefilled(UINT8 hopper) const { return Hoppers_.GetCoinsRefilled(hopper); }

void MPU5::SetSerialHopperRecoveryState(UINT8 paid, UINT8 unpaid)
{
    Hoppers_.SetSerialRecoveryState(paid, unpaid);
}

void MPU5::SetLampBroken(UINT16 lamp, UINT8 broken)
{
    Lamps_.SetBroken(lamp, broken != 0U);
}

void MPU5::SetOptoInvert(UINT8 reelNum, UINT8 value) { Reels_.SetOptoInvert(reelNum, value); }
void MPU5::SetOptoStart(UINT8 reelNum, UINT8 value) { Reels_.SetOptoStart(reelNum, value); }
void MPU5::SetOptoEnd(UINT8 reelNum, UINT8 value) { Reels_.SetOptoEnd(reelNum, value); }
void MPU5::SetSteps(UINT8 reelNum, UINT8 value) { Reels_.SetSteps(reelNum, value); }

void MPU5::SetReelJumperProfile(UINT8 controller, UINT8 profile)
{
    if (controller >= RequestedReelJumperProfile_.size()) { return; }
    const UINT8 requested = profile <= 2U ? profile : 0U;
    if (RequestedReelJumperProfile_[controller] == requested) { return; }
    RequestedReelJumperProfile_[controller] = requested;
    ApplyReelJumperProfiles();
    ScheduleConfigurationReset();
}

UINT8 MPU5::CoinIn(UINT8 mechNum, UINT8 coin, UINT8 coinValue)
{
    if (mechNum == 0)
    {
        if (RequestedCommStyle_ == 3U)
        {
            const UINT8 configuredValue = coinValue != 0U
                ? coinValue
                : Mars_.GetCoinValue(coin);
            return Hoppers_.SerialCoinIn(coin, configuredValue);
        }
        return Mars_.CoinIn(coin, coinValue);
    }
    return Hoppers_.CoinIn(coinValue);
}

UINT8 MPU5::CanAcceptCoin(UINT8 mechNum, UINT8 coin, UINT8 coinValue) const
{
    if (mechNum != 0U) return 1U;
    if (coin >= MPU5MarsMech::CoinCount || Mars_.GetCoinEnable(coin) == 0U)
        return 0U;

    const UINT8 configuredValue = coinValue != 0U
        ? coinValue
        : Mars_.GetCoinValue(coin);

    if (RequestedCommStyle_ == 3U)
        return Hoppers_.CanAcceptSerialCoin(coin, configuredValue) ? 1U : 0U;

    return Mars_.CanAcceptCoin(coin, configuredValue) ? 1U : 0U;
}

UINT8 MPU5::GetCoinLockoutState() const
{
    if (RequestedCommStyle_ != 3U)
        return Mars_.GetLockoutState();

    UINT8 result = 0U;
    for (UINT8 coin = 0U; coin < MPU5MarsMech::CoinCount; ++coin)
    {
        if (CanAcceptCoin(0U, coin, Mars_.GetCoinValue(coin)) == 0U)
            result |= static_cast<UINT8>(1U << coin);
    }
    return result;
}
void MPU5::SetCommStyle(UINT8 value)
{
    const UINT8 requested = static_cast<UINT8>(value & 0x03U);
    if (RequestedCommStyle_ == requested) return;
    RequestedCommStyle_ = requested;
    ApplyRequestedHardwareConfiguration();
    ScheduleConfigurationReset();
}
void MPU5::SetCommInvert(UINT8 value) { Mars_.SetCommInvert(value); }
void MPU5::SetCoinCycles(UINT32 value) { Mars_.SetPulseCycles(value); }
void MPU5::SetEDCEnable(UINT8 value)
{
    const bool requested = value != 0U;
    if (RequestedEDCEnabled_ == requested) return;
    RequestedEDCEnabled_ = requested;
    ApplyRequestedHardwareConfiguration();
    EDC_.Reset();
    ScheduleConfigurationReset();
}

UINT8* MPU5::GetEDCString()
{
    return EDC_.GetString();
}

UINT32 MPU5::PopEDCMessage(char* output, UINT32 outputSize)
{
    return EDC_.PopMessage(output, outputSize);
}
void MPU5::SetLockoutVal(UINT8 coin, UINT8 value) { Mars_.SetLockoutValue(coin, value); }
void MPU5::SetLockoutInvert(UINT8 coin, UINT8 value) { Mars_.SetLockoutInvert(coin, value); }
void MPU5::SetCoinValue(UINT8 coin, UINT8 value) { Mars_.SetCoinValue(coin, value); }
void MPU5::SetCoinEnable(UINT8 coin, UINT8 value) { Mars_.SetCoinEnable(coin, value); }

UINT8 MPU5::ReadSwitchNibble() const
{
    // The MPU5 board has a 4 x 4 switch matrix. MFME/Amber number the four
    // returns within each eight-entry row as 0-3, 8-11, 16-19 and 24-27.
    // Numbers 32 upward belong to MUX5 and are returned over Barbus instead.
    const UINT8 row = ASIC_.GetLowSelect();
    if (row < 1U || row > 4U) return 0;

    UINT8 result = 0;
    const UINT16 base = static_cast<UINT16>((row - 1U) * 8U);
    for (UINT8 bit = 0; bit < 4U; ++bit)
        if (Switches_[base + bit]) result |= static_cast<UINT8>(1U << bit);
    return result;
}

UINT8 MPU5::ReadIO(UINT32 address)
{
    const UINT32 base = SIM_.GetChipSelectBase(1);
    const UINT8 offset = static_cast<UINT8>(address - base);
    switch (offset & 0xF0)
    {
    case 0xD0:
        return PIC_.Read();
    case 0xE0:
        {
            // The editor stores the physical option-switch state: off is 0
            // and on is 1.  The selected switch is returned on DUART IP0.
            UINT8 input = 0x06U;
            const UINT8 lowSelect = ASIC_.GetLowSelect();
            if (lowSelect >= 1U && lowSelect <= 8U)
            {
                // PIC2 program cards number the directly scanned first DIL bank
                // in the opposite select-line order. Keep Amber's editor
                // numbering natural: DIL 1 is reported as DIL 1, not DIL 8.
                const UINT8 dipIndex =
                    (PICMode_ == MPU5PIC::Mode::PIC2 || PICMode_ == MPU5PIC::Mode::PIC3)
                    ? static_cast<UINT8>(8U - lowSelect)
                    : static_cast<UINT8>(lowSelect - 1U);
                if (Dips_[dipIndex] != 0U)
                    input |= 0x01U;
            }

            if (SECFitted_)
            {
                if (SEC_.ReadData() == 0U) { input |= 0x10; }
            }
            else if (!Meters_.Sense())
            {
                input |= 0x10;
            }
            if ((DUART_.GetOutputPort() & 0x0F) != 0x0F) { input |= 0x20; }
            DUART_.SetInputPort(input);
            return DUART_.Read(static_cast<UINT8>(offset & 0x0F));
        }
    case 0xF0:
        {
            const UINT8 pin = Hoppers_.ReadPins(DUART_.GetOutputPort());
            const UINT8 reg = static_cast<UINT8>(offset & 0x0FU);
            if (reg == 0x02U && !DSPStatusResponsePending_)
                ASIC_.SetDSPStatus(Sound_.Status());

            // MFME decrements the MPU5 parallel-mech cointimer only when the
            // firmware reads ASIC register 0xF.  Calling ReadCoinByte() while
            // evaluating every ASIC access consumed the entire pulse during
            // unrelated DSP, switch and status reads before the coin register
            // was sampled.
            const UINT8 coinInputs = reg == 0x0FU
                ? Mars_.ReadCoinByte()
                : Mars_.GetCoinByte();

            const UINT8 result = ASIC_.Read(reg, ReadSwitchNibble(), coinInputs, pin);
            if (reg == 0x02U)
                DSPStatusResponsePending_ = false;
            return result;
        }
    default:
        return 0;
    }
}

void MPU5::ApplyDUARTOutputs()
{
    const UINT8 changes = DUART_.ConsumeOutputChanges();
    const UINT8 outputs = DUART_.GetOutputPort();
    if (changes & 0x01) { Hoppers_.SetMotor(0, (outputs & 0x01) ? 0 : 1); }
    if (changes & 0x04) { Hoppers_.SetMotor(1, (outputs & 0x04) ? 0 : 1); }
    if (changes & 0x20) { Alpha_[0].Enable(static_cast<UINT8>((outputs >> 5) & 1U)); }
    if (changes & 0xC0) { Alpha_[0].WriteClock(static_cast<UINT8>((outputs >> 6) & 1U), static_cast<UINT8>((outputs >> 7) & 1U)); }
}

void MPU5::WriteIO(UINT32 address, UINT8 value)
{
    const UINT32 base = SIM_.GetChipSelectBase(1);
    const UINT8 offset = static_cast<UINT8>(address - base);
    switch (offset & 0xF0)
    {
    case 0xD0:
        PIC_.Write(offset, value);
        break;
    case 0xE0:
        DUART_.Write(static_cast<UINT8>(offset & 0x0F), value);
        ApplyDUARTOutputs();
        break;
    case 0xF0:
        {
            const MPU5ASIC::Changes changes = ASIC_.Write(static_cast<UINT8>(offset & 0x0F), value);
            if (changes.HighSide || changes.LowSide)
            {
                Lamps_.SetMatrixDrive(ASIC_.GetHighSide(), ASIC_.GetLowSelect());
            }
            if (changes.LowSide && ASIC_.GetLowSelect())
            {
                Segments_.WriteCommonAnode(ASIC_.GetLEDs(), static_cast<UINT8>(~ASIC_.GetLowSide()), static_cast<UINT8>(ASIC_.GetLowSelect() - 1U), 0);
            }
            if (changes.Coin)
            {
                Mars_.SetCoinOutputs(ASIC_.GetCoin());
            }
            if (changes.Meters)
            {
                const UINT8 meters = ASIC_.GetMeters();
                if (SECFitted_)
                {
                    // On SEC-equipped MPU5 cabinets the three active-low
                    // meter lines are SEC data, clock and chip-select.
                    SEC_.Enable(static_cast<UINT8>((~meters) & 0x04U));
                    SEC_.SetData(static_cast<UINT8>((~meters) & 0x01U));
                    SEC_.SetClock(static_cast<UINT8>((~meters) & 0x02U));
                }
                else
                {
                    Meters_.Write(meters);
                }
            }
            if (changes.DSPCommand) { ProcessDSPCommand(); }
        }
        break;
    default:
        break;
    }
}

void MPU5::ProcessDSPCommand()
{
    // MPU5 firmware stores the DSP command-buffer pointer in vector-table
    // entry 0x5C.  The recovered MFME implementation reads the command from
    // that buffer whenever ASIC register 0xB is written with bit 6 set.
    const UINT32 buffer = ReadMemoryLong(0x0000005CU, 6U);
    const UINT8 command = ReadMemoryByte(buffer, 5U);
    UINT8 channel = ReadMemoryByte(buffer + 1U, 5U);
    if (channel == 4U) { channel = 3U; }

    bool preserveCommandStatus = false;
    // Operation 0x0B is encoded in the low five bits. During the power-on
    // self-test the upper three bits select sound engines zero to five, whose
    // index must be returned as status. Engine seven is the independent
    // program-module test input used by PIC3 security.
    if ((command & 0x1FU) == 0x0BU)
    {
        const UINT8 engine = static_cast<UINT8>(command >> 5U);
        ASIC_.SetDSPInitialised(true);
        UINT8 engineStatus = engine;
        if (engine >= 6U)
        {
            const UINT8 rawTestState = Switches_[255U] != 0U ? 1U : 0U;
            engineStatus = PIC_.ReadSecurityTestForDSP(rawTestState);
        }
        ASIC_.SetDSPStatus(engineStatus);
        ASIC_.SetDSPResult(0x03U);
        preserveCommandStatus = true;
        DSPStatusResponsePending_ = true;
    }
    else switch (command)
    {
    case 0x00U: // DSP reset/restart
        // Red Gaming issues this during its software restart path. Reset the
        // result/status to the same power-on state as ASIC/DSP Reset(), rather
        // than retaining the previous command result and causing a false
        // DSP FAIL / error 54 on the next initialisation pass.
        Sound_.StopAll();
        ASIC_.SetDSPInitialised(false);
        ASIC_.SetDSPStatus(0x85U);
        ASIC_.SetDSPResult(0x99U);
        break;

    case 0x0CU: // eDischarge
        ASIC_.SetDSPResult(0x03U);
        break;

    case 0x02U: // Start encoded sample
    case 0x22U:
    case 0x42U:
        {
            const UINT32 sampleAddress = ReadMemoryLong(buffer + 2U, 5U);
            Sound_.Start(sampleAddress, channel);
            ASIC_.SetDSPInitialised(true);
            ASIC_.SetDSPResult(0x03U);
        }
        break;

    case 0x05U:
    case 0x25U:
    case 0x45U:
        Sound_.Stop(channel);
        ASIC_.SetDSPResult(0x03U);
        break;

    case 0x06U: // Global stop
        Sound_.StopAll();
        ASIC_.SetDSPResult(0x03U);
        break;

    default:
        // MFME leaves the result unchanged for unknown commands.
        break;
    }
    if (!preserveCommandStatus)
    {
        DSPStatusResponsePending_ = false;
        ASIC_.SetDSPStatus(Sound_.Status());
    }
}

UINT8 MPU5::ReadInternalByte(UINT16 offset)
{
    if (offset < MC68340SIM::RegisterSize)
    {
        return SIM_.ReadByte(offset);
    }
    if (offset >= 0x0600U && offset < 0x0680U)
    {
        return Timer_.ReadByte(static_cast<UINT16>(offset - 0x0600U));
    }
    if (offset >= 0x0700U && offset < 0x0722U)
    {
        return Serial_.ReadByte(static_cast<UINT16>(offset - 0x0700U));
    }
    if (offset >= 0x0780U && offset < 0x07C0U)
    {
        return DMA_.ReadByte(static_cast<UINT16>(offset - 0x0780U));
    }
    return offset < InternalRegisters_.size() ? InternalRegisters_[offset] : 0;
}

UINT16 MPU5::ReadInternalWord(UINT16 offset)
{
    if (offset < MC68340SIM::RegisterSize)
    {
        return static_cast<UINT16>((static_cast<UINT16>(SIM_.ReadByte(offset)) << 8) |
            SIM_.ReadByte(static_cast<UINT16>(offset + 1U)));
    }
    if (offset >= 0x0600U && offset < 0x0680U)
    {
        return Timer_.ReadWord(static_cast<UINT16>(offset - 0x0600U));
    }
    if (offset >= 0x0700U && offset < 0x0722U)
    {
        // The MC68340 serial module is byte-wide in MFME's implementation.
        return 0;
    }
    if (offset >= 0x0780U && offset < 0x07C0U)
    {
        return DMA_.ReadWord(static_cast<UINT16>(offset - 0x0780U));
    }
    return static_cast<UINT16>((static_cast<UINT16>(ReadInternalByte(offset)) << 8) |
        ReadInternalByte(static_cast<UINT16>(offset + 1U)));
}

UINT32 MPU5::ReadInternalLong(UINT16 offset)
{
    if (offset < MC68340SIM::RegisterSize)
    {
        return (static_cast<UINT32>(SIM_.ReadByte(offset)) << 24) |
            (static_cast<UINT32>(SIM_.ReadByte(static_cast<UINT16>(offset + 1U))) << 16) |
            (static_cast<UINT32>(SIM_.ReadByte(static_cast<UINT16>(offset + 2U))) << 8) |
            SIM_.ReadByte(static_cast<UINT16>(offset + 3U));
    }
    if (offset >= 0x0600U && offset < 0x0680U)
    {
        return Timer_.ReadLong(static_cast<UINT16>(offset - 0x0600U));
    }
    if (offset >= 0x0700U && offset < 0x0722U)
    {
        return 0;
    }
    if (offset >= 0x0780U && offset < 0x07C0U)
    {
        return DMA_.ReadLong(static_cast<UINT16>(offset - 0x0780U));
    }
    return (static_cast<UINT32>(ReadInternalByte(offset)) << 24) |
        (static_cast<UINT32>(ReadInternalByte(static_cast<UINT16>(offset + 1U))) << 16) |
        (static_cast<UINT32>(ReadInternalByte(static_cast<UINT16>(offset + 2U))) << 8) |
        ReadInternalByte(static_cast<UINT16>(offset + 3U));
}

void MPU5::WriteInternalByte(UINT16 offset, UINT8 value)
{
    if (offset < MC68340SIM::RegisterSize)
    {
        SIM_.WriteByte(offset, value);
    }
    else if (offset >= 0x0600U && offset < 0x0680U)
    {
        Timer_.WriteByte(static_cast<UINT16>(offset - 0x0600U), value);
    }
    else if (offset >= 0x0700U && offset < 0x0722U)
    {
        Serial_.WriteByte(static_cast<UINT16>(offset - 0x0700U), value);
    }
    else if (offset >= 0x0780U && offset < 0x07C0U)
    {
        DMA_.WriteByte(static_cast<UINT16>(offset - 0x0780U), value);
    }
    else if (offset < InternalRegisters_.size())
    {
        InternalRegisters_[offset] = value;
    }
}

void MPU5::WriteInternalWord(UINT16 offset, UINT16 value)
{
    if (offset < MC68340SIM::RegisterSize)
    {
        SIM_.WriteByte(offset, static_cast<UINT8>(value >> 8));
        SIM_.WriteByte(static_cast<UINT16>(offset + 1U), static_cast<UINT8>(value));
    }
    else if (offset >= 0x0600U && offset < 0x0680U)
    {
        Timer_.WriteWord(static_cast<UINT16>(offset - 0x0600U), value);
    }
    else if (offset >= 0x0700U && offset < 0x0722U)
    {
        // Word writes to the byte-wide serial module are ignored by the recovered MFME core.
    }
    else if (offset >= 0x0780U && offset < 0x07C0U)
    {
        DMA_.WriteWord(static_cast<UINT16>(offset - 0x0780U), value);
    }
    else
    {
        WriteInternalByte(offset, static_cast<UINT8>(value >> 8));
        WriteInternalByte(static_cast<UINT16>(offset + 1U), static_cast<UINT8>(value));
    }
}

void MPU5::WriteInternalLong(UINT16 offset, UINT32 value)
{
    if (offset < MC68340SIM::RegisterSize)
    {
        SIM_.WriteByte(offset, static_cast<UINT8>(value >> 24));
        SIM_.WriteByte(static_cast<UINT16>(offset + 1U), static_cast<UINT8>(value >> 16));
        SIM_.WriteByte(static_cast<UINT16>(offset + 2U), static_cast<UINT8>(value >> 8));
        SIM_.WriteByte(static_cast<UINT16>(offset + 3U), static_cast<UINT8>(value));
    }
    else if (offset >= 0x0600U && offset < 0x0680U)
    {
        Timer_.WriteLong(static_cast<UINT16>(offset - 0x0600U), value);
    }
    else if (offset >= 0x0700U && offset < 0x0722U)
    {
        // Long writes to the byte-wide serial module are ignored by the recovered MFME core.
    }
    else if (offset >= 0x0780U && offset < 0x07C0U)
    {
        DMA_.WriteLong(static_cast<UINT16>(offset - 0x0780U), value);
    }
    else
    {
        WriteInternalByte(offset, static_cast<UINT8>(value >> 24));
        WriteInternalByte(static_cast<UINT16>(offset + 1U), static_cast<UINT8>(value >> 16));
        WriteInternalByte(static_cast<UINT16>(offset + 2U), static_cast<UINT8>(value >> 8));
        WriteInternalByte(static_cast<UINT16>(offset + 3U), static_cast<UINT8>(value));
    }
}

UINT16 MPU5::ReadProgramWordDirect(UINT32 address) const
{
    const UINT8 high = address < ProgramROM_.size() ? ProgramROM_[address] : 0xFFU;
    const UINT32 lowAddress = address + 1U;
    const UINT8 low = lowAddress < ProgramROM_.size() ? ProgramROM_[lowAddress] : 0xFFU;
    return static_cast<UINT16>((static_cast<UINT16>(high) << 8) | low);
}

UINT32 MPU5::ReadProgramLongDirect(UINT32 address) const
{
    const UINT16 high = ReadProgramWordDirect(address);
    const UINT16 low = ReadProgramWordDirect(address + 2U);
    return (static_cast<UINT32>(high) << 16) | low;
}

UINT16 MPU5::ReadRAMWordDirect(UINT32 address) const
{
    const UINT8 high = RAM_[address & 0xFFFFU];
    const UINT8 low = RAM_[(address + 1U) & 0xFFFFU];
    return static_cast<UINT16>((static_cast<UINT16>(high) << 8) | low);
}

UINT32 MPU5::ReadRAMLongDirect(UINT32 address) const
{
    return (static_cast<UINT32>(ReadRAMWordDirect(address)) << 16) |
        ReadRAMWordDirect(address + 2U);
}

void MPU5::WriteRAMWordDirect(UINT32 address, UINT16 value)
{
    RAM_[address & 0xFFFFU] = static_cast<UINT8>(value >> 8);
    RAM_[(address + 1U) & 0xFFFFU] = static_cast<UINT8>(value);
}

void MPU5::WriteRAMLongDirect(UINT32 address, UINT32 value)
{
    WriteRAMWordDirect(address, static_cast<UINT16>(value >> 16));
    WriteRAMWordDirect(address + 2U, static_cast<UINT16>(value));
}

UINT8 MPU5::ReadMemoryByte(UINT32 address)
{
    return ReadMemoryByte(address, CurrentFunctionCode_);
}

UINT16 MPU5::ReadMemoryWord(UINT32 address)
{
    return ReadMemoryWord(address, CurrentFunctionCode_);
}

UINT32 MPU5::ReadMemoryLong(UINT32 address)
{
    return ReadMemoryLong(address, CurrentFunctionCode_);
}

void MPU5::WriteMemoryByte(UINT32 address, UINT8 value)
{
    WriteMemoryByte(address, value, CurrentFunctionCode_);
}

void MPU5::WriteMemoryWord(UINT32 address, UINT16 value)
{
    WriteMemoryWord(address, value, CurrentFunctionCode_);
}

void MPU5::WriteMemoryLong(UINT32 address, UINT32 value)
{
    WriteMemoryLong(address, value, CurrentFunctionCode_);
}

UINT8 MPU5::ReadMemoryByte(UINT32 address, UINT8 functionCode)
{
    address &= 0xFFFFFFFFU;
    functionCode &= 7U;

    if (functionCode == 7U && address >= 0x0003FF00U && address <= 0x0003FF03U)
    {
        return SIM_.ReadModuleBaseByte(address);
    }
    if (SIM_.IsModuleAddress(address, functionCode))
    {
        return ReadInternalByte(SIM_.ModuleOffset(address));
    }

    switch (SIM_.ChipSelect(address, functionCode))
    {
    case 1:
        return address < ProgramROM_.size() ? ProgramROM_[address] : 0xFF;
    case 2:
        return ReadIO(address);
    case 3:
    case 4:
        return RAM_[address & 0xFFFFU];
    default:
        return 0xFF;
    }
}

UINT16 MPU5::ReadMemoryWord(UINT32 address, UINT8 functionCode)
{
    functionCode &= 7U;
    if (functionCode == 7U && address >= 0x0003FF00U && address <= 0x0003FF02U)
    {
        return static_cast<UINT16>((static_cast<UINT16>(SIM_.ReadModuleBaseByte(address)) << 8) |
            SIM_.ReadModuleBaseByte(address + 1U));
    }
    if (SIM_.IsModuleAddress(address, functionCode))
    {
        return ReadInternalWord(SIM_.ModuleOffset(address));
    }

    const UINT8 chipSelect = SIM_.ChipSelect(address, functionCode);
    switch (chipSelect)
    {
    case 1:
        return ReadProgramWordDirect(address);

    case 2:
        {
            // CS1 peripherals are discrete byte/word devices. A 16-bit PIC
            // access is one characteriser operation and must not be split.
            const UINT8 offset = static_cast<UINT8>(address - SIM_.GetChipSelectBase(1));
            switch (offset & 0xF0U)
            {
            case 0xD0U: return PIC_.Read();
            case 0xE0U: return 0;
            case 0xF0U: return 0;
            default: return 0;
            }
        }

    case 3:
    case 4:
        return ReadRAMWordDirect(address);

    default:
        return 0xFFFFU;
    }
}

UINT32 MPU5::ReadMemoryLong(UINT32 address, UINT8 functionCode)
{
    functionCode &= 7U;
    if (functionCode == 7U && address == 0x0003FF00U)
    {
        return (static_cast<UINT32>(SIM_.ReadModuleBaseByte(address)) << 24) |
            (static_cast<UINT32>(SIM_.ReadModuleBaseByte(address + 1U)) << 16) |
            (static_cast<UINT32>(SIM_.ReadModuleBaseByte(address + 2U)) << 8) |
            SIM_.ReadModuleBaseByte(address + 3U);
    }
    if (SIM_.IsModuleAddress(address, functionCode))
    {
        return ReadInternalLong(SIM_.ModuleOffset(address));
    }

    switch (SIM_.ChipSelect(address, functionCode))
    {
    case 1:
        return ReadProgramLongDirect(address);
    case 2:
        // No MPU5 CS1 peripheral implements a native long-word read.
        return 0;
    case 3:
    case 4:
        return ReadRAMLongDirect(address);
    default:
        return 0xFFFFFFFFU;
    }
}

void MPU5::WriteMemoryByte(UINT32 address, UINT8 value, UINT8 functionCode)
{
    functionCode &= 7U;
    if (functionCode == 7U && address >= 0x0003FF00U && address <= 0x0003FF03U)
    {
        SIM_.WriteModuleBaseByte(address, value);
        return;
    }
    if (SIM_.IsModuleAddress(address, functionCode))
    {
        WriteInternalByte(SIM_.ModuleOffset(address), value);
        return;
    }

    switch (SIM_.ChipSelect(address, functionCode))
    {
    case 2:
        WriteIO(address, value);
        break;
    case 3:
    case 4:
        RAM_[address & 0xFFFFU] = value;
        break;
    default:
        break;
    }
}

void MPU5::WriteMemoryWord(UINT32 address, UINT16 value, UINT8 functionCode)
{
    functionCode &= 7U;
    if (functionCode == 7U && address >= 0x0003FF00U && address <= 0x0003FF02U)
    {
        SIM_.WriteModuleBaseByte(address, static_cast<UINT8>(value >> 8));
        SIM_.WriteModuleBaseByte(address + 1U, static_cast<UINT8>(value));
        return;
    }
    if (SIM_.IsModuleAddress(address, functionCode))
    {
        WriteInternalWord(SIM_.ModuleOffset(address), value);
        return;
    }

    switch (SIM_.ChipSelect(address, functionCode))
    {
    case 2:
        {
            const UINT8 offset = static_cast<UINT8>(address - SIM_.GetChipSelectBase(1));
            if ((offset & 0xF0U) == 0xD0U)
                PIC_.Write(offset, static_cast<UINT8>(value));
            // Native word writes to the DUART and ASIC are not decoded on MPU5.
        }
        return;
    case 3:
    case 4:
        WriteRAMWordDirect(address, value);
        return;
    default:
        return;
    }
}

void MPU5::WriteMemoryLong(UINT32 address, UINT32 value, UINT8 functionCode)
{
    functionCode &= 7U;
    if (functionCode == 7U && address == 0x0003FF00U)
    {
        SIM_.WriteModuleBaseByte(address, static_cast<UINT8>(value >> 24));
        SIM_.WriteModuleBaseByte(address + 1U, static_cast<UINT8>(value >> 16));
        SIM_.WriteModuleBaseByte(address + 2U, static_cast<UINT8>(value >> 8));
        SIM_.WriteModuleBaseByte(address + 3U, static_cast<UINT8>(value));
        return;
    }
    if (SIM_.IsModuleAddress(address, functionCode))
    {
        WriteInternalLong(SIM_.ModuleOffset(address), value);
        return;
    }

    switch (SIM_.ChipSelect(address, functionCode))
    {
    case 2:
        // No MPU5 CS1 peripheral implements a native long-word write.
        return;
    case 3:
    case 4:
        WriteRAMLongDirect(address, value);
        return;
    default:
        return;
    }
}

UINT8 MPU5::DMARead8(UINT32 address)
{
    return ReadMemoryByte(address, 5U);
}

UINT16 MPU5::DMARead16(UINT32 address)
{
    return ReadMemoryWord(address, 5U);
}

UINT32 MPU5::DMARead32(UINT32 address)
{
    return ReadMemoryLong(address, 5U);
}

void MPU5::DMAWrite8(UINT32 address, UINT8 value)
{
    WriteMemoryByte(address, value, 5U);
}

void MPU5::DMAWrite16(UINT32 address, UINT16 value)
{
    WriteMemoryWord(address, value, 5U);
}

void MPU5::DMAWrite32(UINT32 address, UINT32 value)
{
    WriteMemoryLong(address, value, 5U);
}

UINT32 MPU5::DeescapeBarbusMessage(
    std::array<UINT8, MC68340Serial::MaximumMessageLength>& message, UINT32 length)
{
    if (length == 0 || message[0] != 0x7FU) { return 0; }
    UINT32 write = 1;
    for (UINT32 read = 1; read < length; ++read)
    {
        message[write++] = message[read];
        if (message[read] == 0x7FU && read + 1U < length && message[read + 1U] == 0x7FU)
        {
            ++read;
        }
    }
    return write;
}

void MPU5::QueueBarbusReply(const UINT8* message, UINT32 length)
{
    if (!message || length == 0 || length > MC68340Serial::MaximumMessageLength) { return; }

    std::array<UINT8, MC68340Serial::MaximumMessageLength * 2U> escaped{};
    UINT32 output = 0;
    escaped[output++] = message[0];
    for (UINT32 index = 1; index < length && output < escaped.size(); ++index)
    {
        escaped[output++] = message[index];
        if (message[index] == 0x7FU && output < escaped.size())
        {
            escaped[output++] = 0x7FU;
        }
    }
    Serial_.QueueReceive(0, escaped.data(), output);
}

void MPU5::ProcessBarbusMessage(
    std::array<UINT8, MC68340Serial::MaximumMessageLength>& message, UINT32 length)
{
    length = DeescapeBarbusMessage(message, length);
    if (length < 3U) { return; }

    const UINT8 unit = static_cast<UINT8>((message[1] >> 1) & 3U);
    const UINT8 messageClass = static_cast<UINT8>((message[1] >> 4) & 0x0FU);
    const UINT32 offset = (message[2] & 0x0FU) == 0x0FU ? 4U : 3U;
    const UINT32 declaredLength = offset == 4U && length > 3U ? message[3] : (message[2] & 0x0FU);
    const UINT32 expectedLength = offset + declaredLength + 2U;

    // Never execute a partial Barbus command. The serial module normally
    // delivers exactly one complete framed packet, but this check keeps a
    // malformed or interrupted transfer from becoming a shortened REEL5
    // movement program.
    if (length < expectedLength) { return; }
    const UINT32 available = declaredLength;

    if (messageClass == 0)
    {
        const UINT8 command = message[2];
        const UINT8 subcommand = length > 3U ? message[3] : 0U;
        switch (command & 0xF0U)
        {
        case 0xA0:
            if (unit + 1U < Alpha_.size())
            {
                Alpha_[unit + 1U].Enable(1);
                Alpha_[unit + 1U].WriteBuffer(message.data() + offset, available);
            }
            break;

        case 0xB0:
            {
                const UINT8* cursor = message.data() + offset;
                UINT32 remaining = available;
                UINT32 consumed = 0U;
                SparseLampPlaneInfo combined{};
                bool healthyActiveLamp = false;

                if (remaining >= 2U)
                {
                    consumed = cursor[0] != 0U
                        ? Segments_.WriteBuffer(cursor, remaining, 8U, unit)
                        : 2U;
                    consumed = std::min(consumed, remaining);
                    cursor += consumed;
                    remaining -= consumed;
                }

                if (remaining >= 2U)
                {
                    const SparseLampPlaneInfo block =
                        InspectSparseLampPlanes(cursor, remaining);
                    healthyActiveLamp = healthyActiveLamp ||
                        Lamps_.BufferHasHealthyActiveLamp(cursor, remaining, 64U, unit);
                    consumed = (cursor[0] | cursor[1]) != 0U
                        ? Lamps_.WriteBuffer(cursor, remaining, 64U, unit)
                        : 2U;
                    consumed = std::min(consumed, remaining);
                    combined.AnyActive = combined.AnyActive || block.AnyActive;
                    combined.FullNonZero = combined.FullNonZero || block.FullNonZero;
                    combined.FullZeroWrite = combined.FullZeroWrite || block.FullZeroWrite;
                    combined.DimNonZero = combined.DimNonZero || block.DimNonZero;
                    cursor += consumed;
                    remaining -= consumed;
                }

                if (remaining >= 2U)
                {
                    const SparseLampPlaneInfo block =
                        InspectSparseLampPlanes(cursor, remaining);
                    healthyActiveLamp = healthyActiveLamp ||
                        Lamps_.BufferHasHealthyActiveLamp(cursor, remaining, 128U, unit);
                    if ((cursor[0] | cursor[1]) != 0U)
                        Lamps_.WriteBuffer(cursor, remaining, 128U, unit);
                    combined.AnyActive = combined.AnyActive || block.AnyActive;
                    combined.FullNonZero = combined.FullNonZero || block.FullNonZero;
                    combined.FullZeroWrite = combined.FullZeroWrite || block.FullZeroWrite;
                    combined.DimNonZero = combined.DimNonZero || block.DimNonZero;
                }

                if (unit < LampCurrentTestActive_.size() &&
                    LampCurrentTestActive_[unit])
                {
                    if (healthyActiveLamp &&
                        !LampCurrentSensePulseIssued_[unit])
                    {
                        // A healthy cold filament causes a short pulse on the
                        // MUX5 current-sense return. Two sampled D0 replies
                        // cover the alternating address polls without turning
                        // the pulse into a permanent asserted level.
                        LampCurrentSensePulseIssued_[unit] = true;
                        LampCurrentSensePulseReplies_[unit] = 2U;
                    }

                    if (combined.FullNonZero)
                        LampCurrentTestSawFullDrive_[unit] = true;

                    // Test 3.1 preheats the selected bulb on the dim plane,
                    // samples it at full drive, then writes zero to the full
                    // plane. That final clear ends the temporary preheat; it
                    // must not remain latched into test 3.2.
                    if (LampCurrentTestSawFullDrive_[unit] &&
                        combined.FullZeroWrite && !combined.FullNonZero)
                    {
                        Lamps_.EndCurrentTest(unit);
                        LampCurrentTestActive_[unit] = false;
                        LampCurrentTestSawFullDrive_[unit] = false;
                    }
                }
            }
            break;

        case 0xE0:
            if (command == 0xE1U && available > 0U &&
                message[offset] == 0x7EU &&
                unit < LampCurrentTestActive_.size())
            {
                Lamps_.BeginCurrentTest(unit);
                LampCurrentTestActive_[unit] = true;
                LampCurrentTestSawFullDrive_[unit] = false;
                LampCurrentSensePulseIssued_[unit] = false;
                LampCurrentSensePulseReplies_[unit] = 0U;
            }
            break;

        default:
            break;
        }

        // The recovered MFME serial module supplies the standard class-zero
        // Barbus acknowledgement/status replies as well as delivering the
        // display payload to MPU5.  Keep that board-level behaviour here so
        // the serial class itself remains reusable.
        UINT32 replyLength = 5U;
        message[2] = 0x00U;

        if (command == 0x00U)
        {
            message[2] = 0xF1U;
            message[3] = 0xF0U;
            replyLength = 6U;
        }
        else if ((command & 0xF0U) == 0x70U && length > 3U)
        {
            message[2] = 0x02U;
            message[3] = 0x1EU;
            message[4] = subcommand == 0x0DU ? 0x1EU : 0x01U;
            replyLength = 7U;
        }
        else if ((command & 0xF0U) == 0xB0U)
        {
            const UINT8 group = static_cast<UINT8>((message[1] >> 1) & 7U);
            auto switchByte = [this](UINT16 first) -> UINT8
            {
                UINT8 value = 0;
                for (UINT8 bit = 0; bit < 8U; ++bit)
                {
                    if (Switches_[static_cast<UINT8>(first + bit)] != 0)
                    {
                        value |= static_cast<UINT8>(1U << bit);
                    }
                }
                return value;
            };
            // MFME returns matrix[4 + 2*flag] and matrix[5 + 2*flag].
            // With Amber's linear switch numbering those matrices begin at
            // switch 32; the previous zero-based mapping made every Barbus
            // button read from the wrong group.
            const UINT16 switchBase = static_cast<UINT16>(32U + static_cast<UINT16>(group) * 16U);
            message[2] = 0x02U;
            message[3] = switchByte(switchBase);
            message[4] = switchByte(static_cast<UINT16>(switchBase + 8U));
            replyLength = 7U;
        }
        else if ((command & 0xF0U) == 0xD0U)
        {
            message[2] = 0x01U;
            UINT8 currentSense = 0x80U;
            if (unit < LampCurrentSensePulseReplies_.size() &&
                LampCurrentSensePulseReplies_[unit] != 0U)
            {
                currentSense = static_cast<UINT8>(currentSense | 0x20U);
                --LampCurrentSensePulseReplies_[unit];
            }
            message[3] = currentSense;
            replyLength = 6U;
        }
        else if (command == 0xF1U && subcommand == 0x12U)
        {
            message[2] = 0x03U;
            message[3] = 0x01U;
            message[4] = 0x00U;
            message[5] = 0x03U;
            replyLength = 8U;
        }

        WriteBarbusChecksum(message.data(), replyLength);
        QueueBarbusReply(message.data(), replyLength);
    }
    else if (messageClass == 1 && unit < 2U)
    {
        const UINT32 replyLength = Reels_.ProcessMessage(unit, message.data(), length);
        if (replyLength != 0)
        {
            QueueBarbusReply(message.data(), replyLength);
        }
    }
}

void MPU5::InitialiseDUARTTraceFile()
{
    FILE* file = OpenDiagnosticFile("MPU5_DUART_TRACE.txt", "w");
    if (file)
    {
        std::fprintf(file, "MPU5 external MC68681 transmit trace\n");
        std::fprintf(file, "cycles,channel,byte,clock_select,mode1,mode2,command\n");
        std::fclose(file);
        DUARTTraceFileInitialised_ = true;
    }
}

void MPU5::TraceDUARTByte(const MPU5DUART::TxEvent& event)
{
    if (!DUARTTraceEnabled_) { return; }
    if (!DUARTTraceFileInitialised_) { InitialiseDUARTTraceFile(); }

    FILE* file = OpenDiagnosticFile("MPU5_DUART_TRACE.txt", "a");
    if (!file) { return; }
    std::fprintf(file, "%llu,%c,%02X,%02X,%02X,%02X,%02X\n",
        static_cast<unsigned long long>(TotalCycles_),
        event.Channel == 0 ? 'A' : 'B', event.Value, event.ClockSelect,
        event.Mode1, event.Mode2, event.Command);
    std::fclose(file);
}

void MPU5::ProcessEDCByte(const MPU5DUART::TxEvent& event)
{
    if (!EDCEnabled_) { return; }

    // The MPU5 Dataport is on MC68681 channel A.  Ignore channel B and the
    // DUART local-loopback power-on diagnostic, which also transmits on A.
    if (event.Channel != 0U || (event.Mode2 & 0x80U) != 0U) { return; }

    const UINT8 response = EDC_.Write(event.Value);
    if (response != 0U)
    {
        // The EDC/Datapak acknowledges a valid message on the same RS232
        // channel.  ReceiveA preserves the DUART receiver/interrupt rules.
        DUART_.ReceiveA(response);
    }
}

void MPU5::ProcessDUARTTransmit()
{
    MPU5DUART::TxEvent event{};
    while (DUART_.PopTransmittedByte(event))
    {
        TraceDUARTByte(event);
        ProcessEDCByte(event);
        if (event.Channel == 1U && (event.Mode2 & 0x80U) == 0U && Hoppers_.IsSerialBusEnabled())
            Hoppers_.WriteSerialByte(event.Value);
    }
}

void MPU5::InitialiseTimingTraceFile()
{
    FILE* file = OpenDiagnosticFile("MPU5_TIMING_TRACE.txt", "w");
    if (file)
    {
        std::fprintf(file, "MPU5 timing diagnostics - DLL 1.2976 - nominal clock %u Hz\n", CPUCyclesPerSecond);
        std::fprintf(file, "wall_seconds,run_calls,requested_cycles,executed_cycles,device_cycles,instructions,carry,max_run_ahead,cumulative_requested_mhz,cumulative_executed_mhz,interval_requested_mhz,interval_executed_mhz,external_bus_cycles,bus_penalty_cycles,interval_bus_penalty_mhz,cs0_accesses,cs1_accesses,cs2_accesses,cs3_accesses,cs0_penalty,cs1_penalty,cs2_penalty,cs3_penalty\n");
        std::fclose(file);
        TimingTraceFileInitialised_ = true;
    }
}

void MPU5::MaybeWriteTimingTrace()
{
    if (!TimingTraceEnabled_) { return; }

    const auto now = std::chrono::steady_clock::now();
    if (now - TimingTraceLastWrite_ < std::chrono::seconds(1)) { return; }
    if (!TimingTraceFileInitialised_) { InitialiseTimingTraceFile(); }

    const double wallSeconds = std::chrono::duration<double>(now - TimingTraceStart_).count();
    const double intervalSeconds = std::chrono::duration<double>(now - TimingTraceLastWrite_).count();
    if (wallSeconds <= 0.0 || intervalSeconds <= 0.0) { return; }
    const double requestedMHz = static_cast<double>(TimingStats_.RequestedCycles) / wallSeconds / 1000000.0;
    const double executedMHz = static_cast<double>(TimingStats_.ExecutedCycles) / wallSeconds / 1000000.0;
    const double intervalRequestedMHz = static_cast<double>(TimingStats_.RequestedCycles - TimingTracePreviousRequestedCycles_) / intervalSeconds / 1000000.0;
    const double intervalExecutedMHz = static_cast<double>(TimingStats_.ExecutedCycles - TimingTracePreviousExecutedCycles_) / intervalSeconds / 1000000.0;
    const double intervalPenaltyMHz = static_cast<double>(TimingStats_.BusPenaltyCycles - TimingTracePreviousBusPenaltyCycles_) / intervalSeconds / 1000000.0;

    FILE* file = OpenDiagnosticFile("MPU5_TIMING_TRACE.txt", "a");
    if (!file) { return; }
    std::fprintf(file, "%.6f,%llu,%llu,%llu,%llu,%llu,%lld,%d,%.6f,%.6f,%.6f,%.6f,%llu,%llu,%.6f,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",
        wallSeconds,
        static_cast<unsigned long long>(TimingStats_.RunCalls),
        static_cast<unsigned long long>(TimingStats_.RequestedCycles),
        static_cast<unsigned long long>(TimingStats_.ExecutedCycles),
        static_cast<unsigned long long>(TimingStats_.DeviceCycles),
        static_cast<unsigned long long>(TimingStats_.Instructions),
        static_cast<long long>(TimingStats_.RunCycleCarry),
        TimingStats_.MaximumRunAhead, requestedMHz, executedMHz,
        intervalRequestedMHz, intervalExecutedMHz,
        static_cast<unsigned long long>(TimingStats_.ExternalBusCycles),
        static_cast<unsigned long long>(TimingStats_.BusPenaltyCycles),
        intervalPenaltyMHz,
        static_cast<unsigned long long>(TimingStats_.ChipSelectAccesses[0]),
        static_cast<unsigned long long>(TimingStats_.ChipSelectAccesses[1]),
        static_cast<unsigned long long>(TimingStats_.ChipSelectAccesses[2]),
        static_cast<unsigned long long>(TimingStats_.ChipSelectAccesses[3]),
        static_cast<unsigned long long>(TimingStats_.ChipSelectPenaltyCycles[0]),
        static_cast<unsigned long long>(TimingStats_.ChipSelectPenaltyCycles[1]),
        static_cast<unsigned long long>(TimingStats_.ChipSelectPenaltyCycles[2]),
        static_cast<unsigned long long>(TimingStats_.ChipSelectPenaltyCycles[3]));
    std::fclose(file);

    TimingTracePreviousRequestedCycles_ = TimingStats_.RequestedCycles;
    TimingTracePreviousExecutedCycles_ = TimingStats_.ExecutedCycles;
    TimingTracePreviousBusPenaltyCycles_ = TimingStats_.BusPenaltyCycles;
    TimingTraceLastWrite_ = now;
}

void MPU5::ProcessInternalSerialMessages()
{
    std::array<UINT8, MC68340Serial::MaximumMessageLength> message{};
    UINT32 length = 0;
    if (Serial_.PopTransmittedMessage(0, message, length))
    {
        ProcessBarbusMessage(message, length);
    }

    // Channel B is present in the MC68340 but is not connected to an MPU5 Barbus device.
    if (Serial_.PopTransmittedMessage(1, message, length))
    {
        (void)length;
    }
}

UINT8 MPU5::HighestInterruptLevel() const
{
    UINT8 result = 0;
    for (UINT8 channel = 0; channel < 2U; ++channel)
    {
        if (Timer_.InterruptPending(channel))
        {
            result = std::max(result, Timer_.GetInterruptLevel(channel));
        }
    }
    if (SIM_.InterruptPending()) { result = std::max(result, SIM_.GetInterruptLevel()); }
    for (UINT8 channel = 0; channel < 2U; ++channel)
    {
        if (DMA_.InterruptPending(channel))
        {
            result = std::max(result, DMA_.GetInterruptLevel(channel));
        }
    }
    if (Serial_.InterruptPending()) { result = std::max(result, Serial_.GetInterruptLevel()); }
    if (DUART_.InterruptPending()) { result = std::max<UINT8>(result, 3U); }
    return result;
}

INT32 MPU5::InterruptVectorForLevel(UINT8 level)
{
    for (UINT8 channel = 0; channel < 2U; ++channel)
    {
        if (Timer_.InterruptPending(channel) && Timer_.GetInterruptLevel(channel) == level)
        {
            return Timer_.GetInterruptVector(channel);
        }
    }
    if (SIM_.InterruptPending() && SIM_.GetInterruptLevel() == level)
    {
        const UINT8 vector = SIM_.GetInterruptVector();
        SIM_.AcknowledgeInterrupt();
        return vector;
    }
    for (UINT8 channel = 0; channel < 2U; ++channel)
    {
        if (DMA_.InterruptPending(channel) && DMA_.GetInterruptLevel(channel) == level)
        {
            return DMA_.GetInterruptVector(channel);
        }
    }
    if (Serial_.InterruptPending() && Serial_.GetInterruptLevel() == level)
    {
        return Serial_.GetInterruptVector();
    }
    if (level == 3U && DUART_.InterruptPending())
    {
        return SIM_.UseAutoVector(level) ? M68K_INT_ACK_AUTOVECTOR : DUART_.GetInterruptVector();
    }
    return M68K_INT_ACK_AUTOVECTOR;
}

void MPU5::UpdateInterruptLine()
{
    const UINT8 level = HighestInterruptLevel();
    if (level == AssertedIRQLevel_) { return; }

    AssertedIRQLevel_ = level;
    m68k_set_irq(level == 0 ? M68K_IRQ_NONE : level);
}

void MPU5::ApplyCPUAccessTiming(UINT32 address, UINT8 accessBytes)
{
    if (!m68k_is_executing()) { return; }

    const MC68340SIM::ExternalAccessTiming timing =
        SIM_.GetExternalAccessTiming(address, CurrentFunctionCode_, accessBytes);
    if (timing.ChipSelect == 0U || timing.BusCycles == 0U) { return; }

    TimingStats_.ExternalBusCycles += timing.BusCycles;
    const UINT8 index = static_cast<UINT8>(timing.ChipSelect - 1U);
    if (index < 4U)
    {
        ++TimingStats_.ChipSelectAccesses[index];
        TimingStats_.ChipSelectPenaltyCycles[index] += timing.AdditionalCycles;
    }
    TimingStats_.BusPenaltyCycles += timing.AdditionalCycles;
    m68k_add_cycles(static_cast<int>(timing.AdditionalCycles));
}

void MPU5::TickDevices(UINT32 cycles)
{
    TotalCycles_ += cycles;
    TimingStats_.DeviceCycles += cycles;

    Timer_.Tick(cycles);
    TickTestInput(cycles);
    PIC_.Tick(cycles);
    SIM_.Tick(cycles);
    DMA_.Tick(cycles, Serial_.GetOutputPort());
    if (Serial_.Tick(cycles)) { ProcessInternalSerialMessages(); }

    if (DUART_.Tick(cycles)) { ProcessDUARTTransmit(); }
    Lamps_.Tick(cycles);
    Segments_.Tick(cycles);
    Meters_.Tick(cycles);
    Mars_.Tick(cycles);
    Reels_.Tick(cycles);
    Hoppers_.Tick(cycles);
    UINT8 serialHopperByte = 0U;
    if (Hoppers_.SerialReplyReady(serialHopperByte) && DUART_.TryReceiveB(serialHopperByte))
        Hoppers_.ConsumeSerialReplyByte();

    UpdateInterruptLine();
}

int __fastcall MPU5::cpu_irq_ack(int level)
{
    const INT32 vector = InterruptVectorForLevel(static_cast<UINT8>(level));
    AssertedIRQLevel_ = 0;
    m68k_set_irq(M68K_IRQ_NONE);
    return vector;
}

void __fastcall MPU5::cpu_set_fc(int functionCode)
{
    CurrentFunctionCode_ = static_cast<UINT8>(functionCode & 7);
}

void __fastcall MPU5::cpu_inst_hook(int cycles)
{
    ++TimingStats_.Instructions;
    // MFME performs this compatibility substitution after the personality
    // lookup returns. Once it has fired there is no reason to compare the PC
    // after every subsequent instruction.
    if (CharacteriserHookPending_ && m68ki_cpu.pc == CharacteriserHookAddress_)
    {
        m68ki_cpu.dar[0] = 0x4F4C4420U; // "OLD "
        CharacteriserHookPending_ = false;
    }

    if (cycles > 0) { TickDevices(static_cast<UINT32>(cycles)); }
}

void __fastcall MPU5::cpu_pulse_reset() {}

UINT8 __fastcall MPU5::cpu_read_byte(int address)
{
    const UINT32 mappedAddress = static_cast<UINT32>(address);
    ApplyCPUAccessTiming(mappedAddress, 1U);
    return ReadMemoryByte(mappedAddress);
}

UINT16 __fastcall MPU5::cpu_read_word(int address)
{
    const UINT32 mappedAddress = static_cast<UINT32>(address);
    ApplyCPUAccessTiming(mappedAddress, 2U);
    return ReadMemoryWord(mappedAddress);
}

UINT32 __fastcall MPU5::cpu_read_long(int address)
{
    const UINT32 mappedAddress = static_cast<UINT32>(address);
    ApplyCPUAccessTiming(mappedAddress, 4U);
    return ReadMemoryLong(mappedAddress);
}

void __fastcall MPU5::cpu_write_byte(int address, UINT8 value)
{
    const UINT32 mappedAddress = static_cast<UINT32>(address);
    ApplyCPUAccessTiming(mappedAddress, 1U);
    WriteMemoryByte(mappedAddress, value);
}

void __fastcall MPU5::cpu_write_word(int address, UINT16 value)
{
    const UINT32 mappedAddress = static_cast<UINT32>(address);
    ApplyCPUAccessTiming(mappedAddress, 2U);
    WriteMemoryWord(mappedAddress, value);
}

void __fastcall MPU5::cpu_write_long(int address, UINT32 value)
{
    const UINT32 mappedAddress = static_cast<UINT32>(address);
    ApplyCPUAccessTiming(mappedAddress, 4U);
    WriteMemoryLong(mappedAddress, value);
}

UINT32 MPU5::GetOutputSnapshot(PA2_OutputSnapshot& out) const
{
    std::memset(&out, 0, sizeof(out));
    out.SizeBytes = static_cast<UINT32>(sizeof(out));
    out.Version = PA2_OUTPUT_SNAPSHOT_VERSION;
    out.MatrixLampCount = std::min<UINT32>(MPU5Lamps::LampCount, PA2_MAX_MATRIX_LAMPS);
    out.ReelCount = PA2_NUM_REELS;
    out.AlphaSegmentedDisplayCount = PA2_NUM_ALPHA_DISPLAYS;
    out.LedDisplayCount = PA2_NUM_LED_DISPLAYS;
    out.ElectronicMechCount = 1;
    out.CoinEntryLampCount = 2;
    out.MeterCount = PA2_NUM_METERS;
    out.DipCount = PA2_NUM_DIPS;
    out.HopperCount = PA2_NUM_HOPPERS;

    for (UINT32 i = 0; i < out.MatrixLampCount; ++i)
    {
        PA2_LampState& lamp = out.MatrixLamps[i];
        lamp.OnOff = Lamps_.IsOn(static_cast<UINT16>(i));
        lamp.Brightness = Lamps_.GetBrightness(static_cast<UINT16>(i));
        const float3 colour = Lamps_.GetColour(static_cast<UINT16>(i));
        lamp.FilamentR = colour.x;
        lamp.FilamentG = colour.y;
        lamp.FilamentB = colour.z;
    }
    for (UINT8 i = 0; i < PA2_NUM_REELS; ++i) { out.Reels[i].Position = Reels_.GetPosition(i); }
    // Project Amber numbers visible alpha displays from zero.  MPU5 Alpha_[0]
    // is the optional on-board display, while Barbus external displays are
    // Alpha_[1] and Alpha_[2].  A normal cabinet uses external unit 0 as its
    // primary display, so expose the two external units in frontend slots 0/1.
    // Fall back to the on-board alpha when no external display has become active.
    const bool externalAlphaActive = Alpha_[1].IsEnabled() || Alpha_[2].IsEnabled();
    for (UINT8 display = 0; display < PA2_NUM_ALPHA_DISPLAYS; ++display)
    {
        const UINT8 source = externalAlphaActive
            ? static_cast<UINT8>(display + 1U)
            : display;
        PA2_AlphaSegmentedState& alpha = out.AlphaSegmented[display];
        alpha.SegmentCount = PA2_ALPHA_SEGMENTS_IMPACT;
        alpha.Brightness = static_cast<float>(Alpha_[source].GetBrightness()) / 31.0f;
        for (UINT8 character = 0; character < PA2_NUM_ALPHA_CHARS; ++character)
        {
            alpha.Segments[character] = Alpha_[source].GetSegments(character);
            alpha.DotComma[character] = Alpha_[source].GetDotComma(character);
        }
    }
    for (UINT16 i = 0; i < PA2_NUM_LED_DISPLAYS; ++i)
    {
        out.LedDisplays[i].OnOff = Segments_.GetSegments(i);
        out.LedDisplays[i].Brightness = static_cast<float>(Segments_.GetBrightness(i)) / 255.0f;
    }
    out.ElectronicMechs[0].CoinLamp[0] = Mars_.GetCoinLamp(0);
    out.ElectronicMechs[0].CoinLamp[1] = Mars_.GetCoinLamp(1);
    // Bit n is set when coin channel n must currently be rejected. For
    // parallel/BCD mechanisms this includes the physical inhibit outputs and
    // the inter-coin busy interval. For ccTalk it reflects the DMOD master
    // inhibit, per-denomination inhibit mask and unread event-buffer capacity.
    out.ElectronicMechs[0].LockoutState = GetCoinLockoutState();
    for (UINT8 i = 0; i < 2; ++i)
    {
        out.CoinEntryLamps[i].OnOff = Mars_.GetCoinLamp(i);
        out.CoinEntryLamps[i].Brightness = out.CoinEntryLamps[i].OnOff ? 1.0f : 0.0f;
        out.CoinEntryLamps[i].FilamentR = out.CoinEntryLamps[i].FilamentG = out.CoinEntryLamps[i].FilamentB = 1.0f;
    }
    for (UINT8 i = 0; i < PA2_NUM_METERS; ++i)
        out.Meters[i] = SECFitted_ ? SEC_.GetCounter(i) : Meters_.GetCounter(i);
    for (UINT8 i = 0; i < PA2_NUM_DIPS; ++i) { out.Dips[i] = Dips_[i]; }
    for (UINT8 i = 0; i < PA2_NUM_HOPPERS; ++i)
    {
        out.HopperLevel[i] = Hoppers_.GetLevel(i);
        out.HopperFullLevel[i] = Hoppers_.GetFullLevel(i);
        out.HopperLoLevel[i] = Hoppers_.GetLowLevel(i);
        out.HopperHiLevel[i] = Hoppers_.GetHighLevel(i);
        out.HopperCoinsIn[i] = Hoppers_.GetCoinsIn(i);
        out.HopperCoinsOut[i] = Hoppers_.GetCoinsOut(i);
        out.HopperCoinsRefilled[i] = Hoppers_.GetCoinsRefilled(i);
    }
    out.StatusLED = GetStatusLED();
    return static_cast<UINT32>(sizeof(out));
}

UINT32 MPU5::FillAudioFrames(INT16* outInterleavedStereo, UINT32 framesRequired)
{
    return Sound_.FillAudioFrames(outInterleavedStereo, framesRequired);
}

UINT32 MPU5::GetTimingStats(PA2_MPU5TimingStats& out) const
{
    out = TimingStats_;
    out.SizeBytes = static_cast<UINT32>(sizeof(out));
    out.Version = PA2_MPU5_TIMING_STATS_VERSION;
    return static_cast<UINT32>(sizeof(out));
}

void MPU5::ResetTimingStats()
{
    TimingStats_ = {};
    TimingStats_.SizeBytes = static_cast<UINT32>(sizeof(TimingStats_));
    TimingStats_.Version = PA2_MPU5_TIMING_STATS_VERSION;
    TimingStats_.RunCycleCarry = RunCycleCarry_;
    TimingTraceStart_ = std::chrono::steady_clock::now();
    TimingTraceLastWrite_ = TimingTraceStart_;
    TimingTracePreviousRequestedCycles_ = 0;
    TimingTracePreviousExecutedCycles_ = 0;
    TimingTracePreviousBusPenaltyCycles_ = 0;
    TimingTraceFileInitialised_ = false;
    if (TimingTraceEnabled_) { InitialiseTimingTraceFile(); }
}

void MPU5::SetTimingTrace(UINT8 enable)
{
    const bool enabled = enable != 0U;
    if (enabled == TimingTraceEnabled_) { return; }
    TimingTraceEnabled_ = enabled;
    TimingTraceFileInitialised_ = false;
    TimingTraceStart_ = std::chrono::steady_clock::now();
    TimingTraceLastWrite_ = TimingTraceStart_;
    TimingTracePreviousRequestedCycles_ = TimingStats_.RequestedCycles;
    TimingTracePreviousExecutedCycles_ = TimingStats_.ExecutedCycles;
    TimingTracePreviousBusPenaltyCycles_ = TimingStats_.BusPenaltyCycles;
    if (TimingTraceEnabled_) { InitialiseTimingTraceFile(); }
}

void MPU5::SetDUARTTrace(UINT8 enable)
{
    const bool enabled = enable != 0U;
    if (enabled == DUARTTraceEnabled_) { return; }
    DUARTTraceEnabled_ = enabled;
    DUARTTraceFileInitialised_ = false;
    if (DUARTTraceEnabled_) { InitialiseDUARTTraceFile(); }
}

UINT8 MPU5::GetLampOn(UINT16 lamp) const { return Lamps_.IsOn(lamp); }
float MPU5::GetLampBrightness(UINT16 lamp) const { return Lamps_.GetBrightness(lamp); }
float3 MPU5::GetLampColour(UINT16 lamp) const { return Lamps_.GetColour(lamp); }
float MPU5::GetLampTemperatureK(UINT16 lamp) const { return Lamps_.GetTemperatureK(lamp); }
float MPU5::GetLampResistanceOhms(UINT16 lamp) const { return Lamps_.GetResistanceOhms(lamp); }
float MPU5::GetLampElectricalPowerW(UINT16 lamp) const { return Lamps_.GetElectricalPowerW(lamp); }
float MPU5::GetLampDuty(UINT16 lamp) const { return Lamps_.GetDuty(lamp); }
float MPU5::GetLampVoltageRMS(UINT16 lamp) const { return Lamps_.GetVoltageRMS(lamp); }
UINT8 MPU5::GetStatusLED() const
{
    switch (ASIC_.GetStatusLED())
    {
    case 0x10U: return PA2_STATUS_LED_RED;
    case 0x20U: return PA2_STATUS_LED_GREEN;
    case 0x30U: return PA2_STATUS_LED_YELLOW;
    default: return PA2_STATUS_LED_OFF;
    }
}
INT16 MPU5::GetReelPosition(UINT8 reel) const { return static_cast<INT16>(Reels_.GetPosition(reel)); }
UINT8 MPU5::GetReelLamp(UINT8 reel) const
{
    const UINT8 hardwareMask = static_cast<UINT8>(Reels_.GetReelLamp(reel) & 0x07U);
    // REEL5 reports bottom/centre/top while Amber's generic reel-lighting ABI
    // expects top/centre/bottom. Keep the board model native and translate only
    // when the state crosses the DLL boundary.
    return static_cast<UINT8>((hardwareMask & 0x02U) |
        ((hardwareMask & 0x01U) << 2U) |
        ((hardwareMask & 0x04U) >> 2U));
}
UINT16 MPU5::GetAlphaSegments(UINT8 character) const
{
    const UINT8 source = (Alpha_[1].IsEnabled() || Alpha_[2].IsEnabled()) ? 1U : 0U;
    return Alpha_[source].GetSegments(character);
}
UINT8 MPU5::GetAlphaDotComma(UINT8 character) const
{
    const UINT8 source = (Alpha_[1].IsEnabled() || Alpha_[2].IsEnabled()) ? 1U : 0U;
    return Alpha_[source].GetDotComma(character);
}
UINT8 MPU5::GetAlphaBrightness() const
{
    const UINT8 source = (Alpha_[1].IsEnabled() || Alpha_[2].IsEnabled()) ? 1U : 0U;
    return Alpha_[source].GetBrightness();
}
UINT8 MPU5::GetAlphaCharacter(UINT8 character) const
{
    const UINT8 source = (Alpha_[1].IsEnabled() || Alpha_[2].IsEnabled()) ? 1U : 0U;
    return Alpha_[source].GetCharacter(character);
}
UINT8 MPU5::GetSegmentOn(UINT16 number) const
{
    const UINT16 display = static_cast<UINT16>(number / 16U);
    const UINT8 segment = static_cast<UINT8>(number % 16U);
    if (segment >= PA2_NUM_LED_SEGMENTS) { return 0U; }
    return (Segments_.GetSegments(display) & (1U << segment)) != 0U ? 1U : 0U;
}
UINT8 MPU5::GetSegmentBrightness(UINT16 display) const { return Segments_.GetBrightness(display); }
UINT32 MPU5::GetMeterCounter(UINT8 meter) const
{
    return SECFitted_ ? SEC_.GetCounter(meter) : Meters_.GetCounter(meter);
}

void MPU5::SetCFolder(const char* folder) { CFolder_ = folder ? folder : ""; }
void MPU5::SetCFileName(const char* fileName) { CFileName_ = fileName ? fileName : ""; }

bool MPU5::SaveRAM(const char* fileName) const
{
    if (!fileName || !fileName[0]) { return false; }
    std::ofstream output(fileName, std::ios::binary | std::ios::trunc);
    if (!output) { return false; }
    output.write(reinterpret_cast<const char*>(RAM_.data()), static_cast<std::streamsize>(RAM_.size()));

    const MPU5Hoppers::PersistentState hopperState = Hoppers_.GetPersistentState();
    std::vector<UINT8> payload;
    payload.reserve((MPU5Hoppers::Count * 16U) + 12U);
    for (UINT32 index = 0U; index < MPU5Hoppers::Count; ++index)
    {
        AppendLE32(payload, hopperState.Level[index]);
        AppendLE32(payload, hopperState.CoinsIn[index]);
        AppendLE32(payload, hopperState.CoinsOut[index]);
        AppendLE32(payload, hopperState.CoinsRefilled[index]);
    }
    AppendLE32(payload, hopperState.SerialDispenseCount);
    AppendLE32(payload, hopperState.SerialLifeDispenseCount);
    payload.push_back(hopperState.SerialLastPayoutPaid);
    payload.push_back(hopperState.SerialLastPayoutUnpaid);
    payload.push_back(0U);
    payload.push_back(0U);

    std::vector<UINT8> trailer;
    trailer.reserve(kHopperStateMagic.size() + 8U + payload.size() + 4U);
    trailer.insert(trailer.end(), kHopperStateMagic.begin(), kHopperStateMagic.end());
    AppendLE32(trailer, kHopperStateVersion);
    AppendLE32(trailer, static_cast<UINT32>(payload.size()));
    trailer.insert(trailer.end(), payload.begin(), payload.end());
    AppendLE32(trailer, HopperStateChecksum(trailer.data(), trailer.size()));
    output.write(reinterpret_cast<const char*>(trailer.data()),
        static_cast<std::streamsize>(trailer.size()));
    return static_cast<bool>(output);
}

bool MPU5::LoadRAM(const char* fileName)
{
    if (!fileName || !fileName[0]) { return false; }
    std::ifstream input(fileName, std::ios::binary);
    if (!input) { return false; }
    RAM_.fill(0);
    input.read(reinterpret_cast<char*>(RAM_.data()), static_cast<std::streamsize>(RAM_.size()));
    if (input.gcount() != static_cast<std::streamsize>(RAM_.size())) return false;

    std::vector<UINT8> trailer;
    char byte = 0;
    while (input.get(byte)) trailer.push_back(static_cast<UINT8>(byte));

    const size_t minimumTrailer = kHopperStateMagic.size() + 8U + 4U;
    if (trailer.size() >= minimumTrailer &&
        std::equal(kHopperStateMagic.begin(), kHopperStateMagic.end(), trailer.begin()))
    {
        size_t offset = kHopperStateMagic.size();
        UINT32 version = 0U;
        UINT32 payloadSize = 0U;
        if (ReadLE32(trailer, offset, version) && ReadLE32(trailer, offset, payloadSize) &&
            (version == 1U || version == kHopperStateVersion) &&
            payloadSize == (MPU5Hoppers::Count * 16U) + 12U &&
            offset + payloadSize + 4U == trailer.size())
        {
            const size_t checksumOffset = offset + payloadSize;
            size_t checksumReadOffset = checksumOffset;
            UINT32 storedChecksum = 0U;
            if (ReadLE32(trailer, checksumReadOffset, storedChecksum) &&
                storedChecksum == HopperStateChecksum(trailer.data(), checksumOffset))
            {
                MPU5Hoppers::PersistentState state{};
                bool valid = true;
                for (UINT32 index = 0U; index < MPU5Hoppers::Count && valid; ++index)
                {
                    valid = ReadLE32(trailer, offset, state.Level[index]) &&
                        ReadLE32(trailer, offset, state.CoinsIn[index]) &&
                        ReadLE32(trailer, offset, state.CoinsOut[index]) &&
                        ReadLE32(trailer, offset, state.CoinsRefilled[index]);
                }
                valid = valid && ReadLE32(trailer, offset, state.SerialDispenseCount) &&
                    ReadLE32(trailer, offset, state.SerialLifeDispenseCount);
                if (valid && offset + 4U == checksumOffset)
                {
                    state.SerialLastPayoutPaid = trailer[offset++];
                    state.SerialLastPayoutUnpaid = trailer[offset++];
                    offset += 2U; // Reserved.
                    Hoppers_.SetPersistentState(state, version == 1U);
                }
            }
        }
    }

    return true;
}

std::string MPU5::StateFilePath() const
{
    if (CFileName_.empty()) { return {}; }
    if (CFolder_.empty()) { return CFileName_ + ".ram"; }
    const char last = CFolder_.back();
    return CFolder_ + ((last == '/' || last == '\\') ? "" : "\\") + CFileName_ + ".ram";
}

void MPU5::SaveState() const { const std::string path = StateFilePath(); if (!path.empty()) { SaveRAM(path.c_str()); } }
void MPU5::LoadState() { const std::string path = StateFilePath(); if (!path.empty()) { LoadRAM(path.c_str()); } }
