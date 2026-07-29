#include "AmberLegacyAdapter.h"

#include "../../../Cores/JPMSystem6/PA2CoreInterface.h"
#include "AmberDynamicLibrary.h"
#include "fabric/fabric_amber.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <sstream>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fabric {
namespace {
static_assert(alignof(PA2_OutputSnapshot) == 4,
              "production Amber snapshot must retain pack(4)");
static_assert(sizeof(PA2_OutputSnapshot) == 24812,
              "production Amber snapshot ABI size changed");
static_assert(sizeof(PA2_AudioFormat) == 24,
              "production Amber audio ABI size changed");
bool trace_enabled() noexcept {
  const char *value = std::getenv("FABRIC_AMBER_TRACE");
  return value && value[0] && std::strcmp(value, "0") != 0;
}

void trace(const std::string &message) noexcept {
  if (!trace_enabled())
    return;
  const std::string line =
      "[Fabric production Amber adapter] " + message + "\n";
#ifdef _WIN32
  OutputDebugStringA(line.c_str());
#else
  std::fputs(line.c_str(), stderr);
#endif
}

template <typename T>
T resolve(AmberDynamicLibrary &library, const char *name) {
  void *symbol = library.symbol(name);
  T result = nullptr;
  static_assert(sizeof(result) == sizeof(symbol), "function pointer size");
  std::memcpy(&result, &symbol, sizeof(result));
  return result;
}

struct LegacyApi {
  uint8_t (*Initialise)() = nullptr;
  uint8_t (*Shutdown)() = nullptr;
  void (*Reset)() = nullptr;
  int32_t (*Run)(uint32_t) = nullptr;
  uint32_t (*LoadROM)(uint8_t *, uint8_t *, uint8_t *, uint8_t *) = nullptr;
  uint32_t (*GetOutputSnapshotSize)() = nullptr;
  uint32_t (*GetOutputSnapshot)(void *, uint32_t) = nullptr;
  void (*TurnSwitchOn)(uint8_t) = nullptr;
  void (*TurnSwitchOff)(uint8_t) = nullptr;
  uint32_t (*LoadSoundROM)(uint8_t *, uint8_t *, uint8_t *,
                           uint8_t *) = nullptr;
  uint32_t (*GetAudioFormat)(PA2_AudioFormat *, uint32_t) = nullptr;
  uint32_t (*FillAudioFrames)(int16_t *, uint32_t) = nullptr;
  void (*SetOptoInvert)(uint8_t, uint8_t) = nullptr;
  void (*SetOptoStart)(uint8_t, uint8_t) = nullptr;
  void (*SetOptoEnd)(uint8_t, uint8_t) = nullptr;
  void (*SetSteps)(uint8_t, uint8_t) = nullptr;
  void (*SetCoinValue)(uint8_t, uint8_t) = nullptr;
  void (*SetCoinEnable)(uint8_t, uint8_t) = nullptr;
  void (*SetLockoutVal)(uint8_t, uint8_t) = nullptr;
  void (*SetLockoutInvert)(uint8_t, uint8_t) = nullptr;
  void (*SetPercent)(uint8_t) = nullptr;
};

class LegacyInstance final : public FabricBackendInstance {
public:
  LegacyInstance(std::unique_ptr<AmberDynamicLibrary> library, LegacyApi api,
                 std::vector<std::string> program,
                 std::vector<std::string> sound,
                 const FabricAmberConfigurationV1 *configuration,
                 std::string path)
      : library_(std::move(library)), api_(api), program_(std::move(program)),
        sound_(std::move(sound)), path_(std::move(path)) {
    if (configuration)
      config_ = *configuration;
  }
  ~LegacyInstance() override {
    if (started_ && !stopped_)
      api_.Shutdown();
  }

  FabricResult initialise() noexcept override {
    const uint8_t native = api_.Initialise();
    trace("Initialise: Amber return=" + std::to_string(native));
    if (!native)
      return fail("Initialise", "Amber return=0; DLL='" + path_ + "'");
    started_ = true;
    if (!load_roms(api_.LoadROM, program_, "program"))
      return FABRIC_NOT_FOUND;
    if (!sound_.empty() && !load_roms(api_.LoadSoundROM, sound_, "sound"))
      return api_.LoadSoundROM ? FABRIC_NOT_FOUND : FABRIC_NOT_SUPPORTED;
    if (!configure()) {
      error_ = "production Amber adapter: Configure failed: " + error_ +
               "; DLL='" + path_ + "'";
      trace(error_);
      return FABRIC_NOT_SUPPORTED;
    }
    trace("initialisation complete: Fabric result=0");
    return ok();
  }
  FabricResult reset() noexcept override {
    api_.Reset();
    if (!reset_traced_) {
      trace("Reset: completed; Fabric result=0");
      reset_traced_ = true;
    }
    return ok();
  }
  FabricResult advance(uint64_t ns) noexcept override {
    const uint64_t total = remainder_ + ns;
    uint64_t cycles = total / 125u;
    remainder_ = total % 125u;
    while (cycles) {
      const uint32_t request =
          static_cast<uint32_t>(std::min<uint64_t>(cycles, INT32_MAX));
      const int32_t native = api_.Run(request);
      /* The flat ABI exposes the emulator/CPU return value for observation;
       * it does not define that value as a progress count or error status.
       * The requested argument is the consumed time budget. */
      cycles -= request;
      if (advance_trace_count_ < 8) {
        trace("Run: requested cycles=" + std::to_string(request) +
              "; returned value=" + std::to_string(native) +
              "; elapsed nanoseconds=" + std::to_string(ns) +
              "; Fabric result=0");
        ++advance_trace_count_;
      }
    }
    return ok();
  }
  FabricResult shutdown() noexcept override {
    release_inputs();
    if (stopped_)
      return shutdown_result_;
    stopped_ = true;
    if (started_ && !api_.Shutdown())
      return shutdown_result_ =
                 fail("Shutdown", "Amber return=0; DLL='" + path_ + "'");
    started_ = false;
    trace("Shutdown: Amber return=1; Fabric result=0");
    return shutdown_result_ = ok();
  }
  FabricResult submit_input(const FabricInput &input) noexcept override {
    if (input.numerical_index < 0 || input.numerical_index > 255)
      return invalid(
          "production Amber switch index must be in the range 0..255");
    const uint8_t index = static_cast<uint8_t>(input.numerical_index);
    if (input.active) {
      api_.TurnSwitchOn(index);
      asserted_.insert(index);
    } else {
      api_.TurnSwitchOff(index);
      asserted_.erase(index);
    }
    return ok();
  }
  FabricResult capabilities(FabricCapabilities &out) noexcept override {
    out.flags = FABRIC_CAPABILITY_DIGITAL_INPUT | FABRIC_CAPABILITY_LAMPS |
                FABRIC_CAPABILITY_REELS | FABRIC_CAPABILITY_CHARACTER_DISPLAYS |
                FABRIC_CAPABILITY_SEGMENT_DISPLAYS;
    if (api_.GetAudioFormat && api_.FillAudioFrames)
      out.flags |= FABRIC_CAPABILITY_AUDIO;
    return ok();
  }
  FabricResult snapshot(FabricMachineSnapshot &out) noexcept override {
    PA2_OutputSnapshot source{};
    const uint32_t expected = api_.GetOutputSnapshotSize();
    if (expected != sizeof(source))
      return fail("GetOutputSnapshotSize",
                  "returned size=" + std::to_string(expected) +
                      "; expected size=" + std::to_string(sizeof(source)) +
                      "; DLL='" + path_ + "'");
    const uint32_t returned = api_.GetOutputSnapshot(&source, sizeof(source));
    if (returned != sizeof(source) || source.SizeBytes != sizeof(source) ||
        source.Version != PA2_OUTPUT_SNAPSHOT_VERSION)
      return fail("GetOutputSnapshot",
                  "returned size=" + std::to_string(returned) +
                      "; embedded size=" + std::to_string(source.SizeBytes) +
                      "; version=" + std::to_string(source.Version) +
                      "; DLL='" + path_ + "'");
    if (source.MatrixLampCount < 512 ||
        source.MatrixLampCount > PA2_MAX_MATRIX_LAMPS || source.ReelCount < 8 ||
        source.ReelCount > PA2_NUM_REELS ||
        source.AlphaSegmentedDisplayCount < 1 ||
        source.AlphaSegmentedDisplayCount > PA2_NUM_ALPHA_DISPLAYS ||
        source.LedCount < 256 || source.LedCount > PA2_MAX_LEDS)
      return fail("GetOutputSnapshot",
                  "invalid production counts: matrix lamps=" +
                      std::to_string(source.MatrixLampCount) + "; reels=" +
                      std::to_string(source.ReelCount) + "; alpha displays=" +
                      std::to_string(source.AlphaSegmentedDisplayCount) +
                      "; LEDs=" + std::to_string(source.LedCount));
    out.lamp_count = 512;
    out.reel_count = 8;
    out.character_display_count = 1;
    out.segment_display_count = 16;
    if (out.lamp_capacity < out.lamp_count ||
        out.reel_capacity < out.reel_count ||
        out.character_display_capacity < out.character_display_count ||
        out.segment_display_capacity < out.segment_display_count)
      return buffer_too_small("GetOutputSnapshot", out);
    if ((out.lamp_count && !out.lamps) || (out.reel_count && !out.reels) ||
        (out.character_display_count && !out.character_displays) ||
        (out.segment_display_count && !out.segment_displays))
      return invalid("snapshot output buffer is null");
    if (!std::isfinite(source.AlphaSegmented[0].Brightness))
      return fail("GetOutputSnapshot",
                  "alpha-display brightness is non-finite; index=0");
    for (uint32_t i = 0; i < 256; ++i)
      if (!std::isfinite(source.Leds[i].Brightness))
        return fail("GetOutputSnapshot",
                    "LED brightness is non-finite; index=" + std::to_string(i));
    for (uint32_t i = 0; i < out.lamp_count; ++i) {
      auto &d = out.lamps[i];
      d = {};
      d.struct_size = sizeof(d);
      d.struct_version = FABRIC_ABI_VERSION_1;
      std::snprintf(d.identifier, sizeof(d.identifier), "amber.lamp.%u", i);
      d.numerical_index = static_cast<int32_t>(i);
      d.logical_state = source.MatrixLamps[i].OnOff ? 1 : 0;
      if (!std::isfinite(source.MatrixLamps[i].Brightness))
        return fail("GetOutputSnapshot",
                    "matrix lamp brightness is non-finite; index=" +
                        std::to_string(i));
      d.brightness = source.MatrixLamps[i].Brightness;
    }
    for (uint32_t i = 0; i < out.reel_count; ++i) {
      auto &d = out.reels[i];
      d = {};
      d.struct_size = sizeof(d);
      d.struct_version = FABRIC_ABI_VERSION_1;
      std::snprintf(d.identifier, sizeof(d.identifier), "amber.reel.%u", i);
      d.numerical_index = static_cast<int32_t>(i);
      d.position = source.Reels[i].Position;
    }
    for (uint32_t i = 0; i < out.character_display_count; ++i) {
      auto &d = out.character_displays[i];
      d = {};
      d.struct_size = sizeof(d);
      d.struct_version = FABRIC_ABI_VERSION_1;
      std::snprintf(d.identifier, sizeof(d.identifier), "amber.alpha.%u", i);
      d.character_count = PA2_NUM_ALPHA_CHARS;
      d.character_capacity = FABRIC_CHARACTER_CAPACITY;
      for (uint32_t j = 0; j < PA2_NUM_ALPHA_CHARS; ++j) {
        d.characters[j] = source.AlphaSegmented[i].Segments[j];
        const uint8_t native = source.AlphaSegmented[i].DotComma[j];
        d.attributes[j] = native == static_cast<uint8_t>('.')   ? 1
                          : native == static_cast<uint8_t>(',') ? 2
                                                                : 0;
      }
    }
    for (uint32_t i = 0; i < out.segment_display_count; ++i) {
      auto &d = out.segment_displays[i];
      d = {};
      d.struct_size = sizeof(d);
      d.struct_version = FABRIC_ABI_VERSION_1;
      std::snprintf(d.identifier, sizeof(d.identifier),
                    "amber.seven-segment.%u", i);
      d.digit_count = 1;
      d.digit_capacity = FABRIC_SEGMENT_DIGIT_CAPACITY;
      uint64_t mask = 0;
      for (uint32_t segment = 0; segment < 8; ++segment)
        mask = (mask << 1) |
               (source.Leds[i * 16 + segment].OnOff ? UINT64_C(1) : 0);
      d.segment_masks[0] = mask;
    }
    if (!snapshot_traced_) {
      trace("GetOutputSnapshot: returned size=" + std::to_string(returned) +
            "; lamps=512; reels=8; alpha=1; segment displays=16; Fabric "
            "result=0");
      snapshot_traced_ = true;
    }
    out.sequence = ++sequence_;
    return ok();
  }
  FabricResult audio_format(FabricAudioFormat &out) noexcept override {
    if (!api_.GetAudioFormat || !api_.FillAudioFrames)
      return unsupported("audio format",
                         "required audio exports are unavailable");
    PA2_AudioFormat f{};
    f.SizeBytes = sizeof(f);
    f.Version = PA2_AUDIO_FORMAT_VERSION;
    const uint32_t returned = api_.GetAudioFormat(&f, sizeof(f));
    if (returned != sizeof(f) || f.SizeBytes != sizeof(f) ||
        f.Version != PA2_AUDIO_FORMAT_VERSION)
      return fail("GetAudioFormat", "returned size or structure identity is "
                                    "incompatible; returned size=" +
                                        std::to_string(returned) + "; DLL='" +
                                        path_ + "'");
    if (f.Format != PA2_AUDIO_FORMAT_PCM_S16 || f.Channels != 2 ||
        f.BitsPerSample != 16)
      return unsupported(
          "GetAudioFormat",
          "format is not representable interleaved PCM16; sample rate=" +
              std::to_string(f.SampleRate) +
              "; channels=" + std::to_string(f.Channels) +
              "; bits=" + std::to_string(f.BitsPerSample) +
              "; encoding=" + std::to_string(f.Format));
    out.sample_rate = f.SampleRate;
    out.channel_count = static_cast<uint16_t>(f.Channels);
    out.bits_per_sample = 16;
    out.interleaved = out.signed_samples = out.little_endian = 1;
    trace("GetAudioFormat: Amber return=" + std::to_string(returned) +
          "; sample rate=" + std::to_string(f.SampleRate) + "; channels=" +
          std::to_string(f.Channels) + "; bits=16; Fabric result=0");
    return ok();
  }
  FabricResult read_audio(int16_t *samples, uint32_t capacity,
                          uint32_t &written) noexcept override {
    written = 0;
    if (!api_.FillAudioFrames || !api_.GetAudioFormat)
      return unsupported("FillAudioFrames",
                         "required audio exports are unavailable");
    if (capacity && !samples)
      return invalid("audio buffer is null");
    if (capacity > UINT32_MAX / 2u ||
        static_cast<uint64_t>(capacity) * 2u * sizeof(int16_t) > SIZE_MAX)
      return invalid("FillAudioFrames frame capacity overflows the stereo "
                     "sample extent; requested frames=" +
                     std::to_string(capacity));
    written = api_.FillAudioFrames(samples, capacity);
    if (audio_trace_count_ < 8) {
      trace("FillAudioFrames: requested frames=" + std::to_string(capacity) +
            "; returned frames=" + std::to_string(written) +
            (written <= capacity ? "; Fabric result=0" : "; Fabric result=7"));
      ++audio_trace_count_;
    }
    if (written > capacity) {
      const uint32_t invalid = written;
      written = 0;
      return fail("FillAudioFrames",
                  "returned frames=" + std::to_string(invalid) +
                      "; requested frames=" + std::to_string(capacity));
    }
    return ok();
  }
  std::string last_error() const noexcept override { return error_; }

private:
  using Load = uint32_t (*)(uint8_t *, uint8_t *, uint8_t *, uint8_t *);
  bool load_roms(Load load, const std::vector<std::string> &paths,
                 const char *role) {
    if (paths.empty())
      return true;
    if (!load) {
      error_ = std::string("production Amber adapter: Load ") + role +
               " ROMs failed: required export is unavailable; DLL='" + path_ +
               "'";
      return false;
    }
    uint8_t *p[4]{};
    for (size_t i = 0; i < paths.size(); ++i)
      p[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(paths[i].c_str()));
    const uint32_t native = load(p[0], p[1], p[2], p[3]);
    trace(std::string("Load ") + role +
          " ROMs: slots=" + std::to_string(paths.size()) +
          "; Amber return=" + std::to_string(native) +
          (native ? "; Fabric result=0" : "; Fabric result=3"));
    if (!native) {
      error_ = std::string("production Amber adapter: Load ") + role +
               " ROMs failed: Amber return=0; slots=" +
               std::to_string(paths.size()) + "; DLL='" + path_ + "'";
      return false;
    }
    return true;
  }
  bool configure() {
    if (config_.flags & FABRIC_AMBER_CONFIGURE_REELS) {
      if (!api_.SetSteps || !api_.SetOptoInvert || !api_.SetOptoStart ||
          !api_.SetOptoEnd) {
        error_ = "production Amber reel configuration exports are unavailable";
        return false;
      }
      for (uint32_t i = 0; i < AMBER_MAX_REELS; ++i)
        if (config_.reels.apply_mask & (1u << i)) {
          const auto &r = config_.reels.reels[i];
          if (r.steps > 255 || r.opto_start > 255 || r.opto_end > 255 ||
              r.opto_invert > 255) {
            error_ =
                "production Amber reel configuration value exceeds 8-bit ABI";
            return false;
          }
          const auto index = static_cast<uint8_t>(i);
          api_.SetSteps(index, static_cast<uint8_t>(r.steps));
          api_.SetOptoStart(index, static_cast<uint8_t>(r.opto_start));
          api_.SetOptoEnd(index, static_cast<uint8_t>(r.opto_end));
          api_.SetOptoInvert(index, static_cast<uint8_t>(r.opto_invert));
        }
    }
    if (config_.flags & FABRIC_AMBER_CONFIGURE_COINS) {
      if (!api_.SetCoinValue || !api_.SetCoinEnable) {
        error_ = "production Amber coin configuration exports are unavailable";
        return false;
      }
      for (uint32_t i = 0; i < AMBER_MAX_COIN_CHANNELS; ++i)
        if (config_.coins.channel_apply_mask & (1u << i)) {
          const auto &c = config_.coins.channels[i];
          if (c.value > 255 || c.enabled > 255 || c.lockout_invert > 255) {
            error_ =
                "production Amber coin configuration value exceeds 8-bit ABI";
            return false;
          }
          const auto index = static_cast<uint8_t>(i);
          api_.SetCoinValue(index, static_cast<uint8_t>(c.value));
          api_.SetCoinEnable(index, static_cast<uint8_t>(c.enabled));
          if (c.lockout_invert) {
            if (!api_.SetLockoutInvert) {
              error_ = "production Amber lockout configuration export is "
                       "unavailable";
              return false;
            }
            api_.SetLockoutInvert(index,
                                  static_cast<uint8_t>(c.lockout_invert));
          }
        }
      if (config_.coins.route_apply_mask || config_.coins.configuration_flags) {
        error_ = "production Amber cannot represent requested extended coin "
                 "configuration";
        return false;
      }
    }
    if (config_.flags & FABRIC_AMBER_CONFIGURE_PERCENTAGE) {
      if (!api_.SetPercent) {
        error_ =
            "production Amber percentage configuration export is unavailable";
        return false;
      }
      api_.SetPercent(static_cast<uint8_t>(config_.percentage_switch));
    }
    trace("Configure: flags=" + std::to_string(config_.flags) +
          "; Fabric result=0");
    return true;
  }
  void release_inputs() {
    if (started_ && !stopped_)
      for (uint8_t i : asserted_)
        api_.TurnSwitchOff(i);
    asserted_.clear();
  }
  FabricResult ok() {
    error_.clear();
    return FABRIC_OK;
  }
  FabricResult fail(const std::string &operation, const std::string &detail) {
    error_ = "production Amber adapter: " + operation + " failed: " + detail;
    trace(error_);
    return FABRIC_BACKEND_ERROR;
  }
  FabricResult invalid(const std::string &m) {
    error_ = "production Amber adapter: " + m;
    return FABRIC_INVALID_ARGUMENT;
  }
  FabricResult unsupported(const std::string &operation,
                           const std::string &detail) {
    error_ =
        "production Amber adapter: " + operation + " is unsupported: " + detail;
    return FABRIC_NOT_SUPPORTED;
  }
  FabricResult buffer_too_small(const char *operation,
                                const FabricMachineSnapshot &out) {
    error_ = "production Amber adapter: " + std::string(operation) +
             " failed: Fabric snapshot capacities are too small; lamps=" +
             std::to_string(out.lamp_capacity) +
             "/512; reels=" + std::to_string(out.reel_capacity) +
             "/8; alpha=" + std::to_string(out.character_display_capacity) +
             "/1; segment=" + std::to_string(out.segment_display_capacity) +
             "/16";
    return FABRIC_BUFFER_TOO_SMALL;
  }
  std::unique_ptr<AmberDynamicLibrary> library_;
  LegacyApi api_{};
  std::vector<std::string> program_, sound_;
  FabricAmberConfigurationV1 config_{};
  std::set<uint8_t> asserted_;
  std::string error_, path_;
  uint64_t remainder_ = 0, sequence_ = 0;
  uint32_t advance_trace_count_ = 0, audio_trace_count_ = 0;
  bool started_ = false, stopped_ = false, reset_traced_ = false,
       snapshot_traced_ = false;
  FabricResult shutdown_result_ = FABRIC_OK;
};

template <typename T>
bool required(AmberDynamicLibrary &lib, T &fn, const char *name,
              const std::string &path, std::string &error) {
  fn = resolve<T>(lib, name);
  if (fn)
    return true;
  error = "Failed to initialise production Amber adapter for '" + path +
          "': required export '" + name +
          "' was not found during export resolution.";
  return false;
}
} // namespace

FabricResult
CreateLegacyAmberInstance(const FabricLaunchRequest &request,
                          std::unique_ptr<AmberDynamicLibrary> library,
                          std::unique_ptr<FabricBackendInstance> &out,
                          std::string &error) noexcept {
  try {
    LegacyApi a{};
#define REQ(n)                                                                 \
  if (!required(*library, a.n, #n, request.backend_path, error))               \
  return FABRIC_NOT_SUPPORTED
    REQ(Initialise);
    REQ(Shutdown);
    REQ(Reset);
    REQ(Run);
    REQ(LoadROM);
    REQ(GetOutputSnapshotSize);
    REQ(GetOutputSnapshot);
    REQ(TurnSwitchOn);
    REQ(TurnSwitchOff);
#undef REQ
#define OPT(n) a.n = resolve<decltype(a.n)>(*library, #n)
    OPT(LoadSoundROM);
    OPT(GetAudioFormat);
    OPT(FillAudioFrames);
    OPT(SetOptoInvert);
    OPT(SetOptoStart);
    OPT(SetOptoEnd);
    OPT(SetSteps);
    OPT(SetCoinValue);
    OPT(SetCoinEnable);
    OPT(SetLockoutVal);
    OPT(SetLockoutInvert);
    OPT(SetPercent);
#undef OPT
    std::vector<std::pair<uint32_t, std::string>> p, s;
    if (request.rom_resource_count) {
      for (uint32_t i = 0; i < request.rom_resource_count; ++i) {
        const auto &r = request.rom_resources[i];
        if (r.role == FABRIC_ROM_ROLE_PROGRAM)
          p.emplace_back(r.slot, r.path);
        else if (r.role == FABRIC_ROM_ROLE_SOUND)
          s.emplace_back(r.slot, r.path);
      }
    } else
      for (uint32_t i = 0; i < request.rom_path_count; ++i)
        p.emplace_back(i, request.rom_paths[i]);
    auto sort = [](auto &v) {
      std::sort(v.begin(), v.end(),
                [](const auto &a, const auto &b) { return a.first < b.first; });
    };
    sort(p);
    sort(s);
    std::vector<std::string> program, sound;
    for (auto &v : p)
      program.push_back(std::move(v.second));
    for (auto &v : s)
      sound.push_back(std::move(v.second));
    const auto *config = request.machine_configuration_size
                             ? static_cast<const FabricAmberConfigurationV1 *>(
                                   request.machine_configuration)
                             : nullptr;
    trace("selected for DLL='" + std::string(request.backend_path) + "'");
    out = std::make_unique<LegacyInstance>(std::move(library), a,
                                           std::move(program), std::move(sound),
                                           config, request.backend_path);
    return FABRIC_OK;
  } catch (const std::exception &e) {
    error = e.what();
    return FABRIC_INTERNAL_ERROR;
  }
}
} // namespace fabric
