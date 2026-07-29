#include "AmberDynamicLibrary.h"
#include "AmberLegacyAdapter.h"
#include "AmberTrace.h"
#include "FabricBackend.h"
#include "fabric/fabric_amber.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <set>
#include <utility>
#include <vector>

namespace fabric {
namespace {
using GetApi = AmberResult(AMBER_CALL *)(uint32_t, uint32_t, void *);

class AmberInstance final : public FabricBackendInstance {
public:
  using Rom = std::pair<uint32_t, std::string>;
  AmberInstance(std::unique_ptr<AmberDynamicLibrary> module,
                const AmberApiV2 &api, AmberHandle handle,
                std::vector<Rom> program, std::vector<Rom> sound,
                std::vector<Rom> other,
                const FabricAmberConfigurationV1 *configuration, bool system6)
      : module_(std::move(module)), api_(api), handle_(handle),
        program_(std::move(program)), sound_(std::move(sound)),
        other_(std::move(other)), system6_(system6) {
    if (configuration)
      config_ = *configuration;
  }
  ~AmberInstance() override {
    release_switches();
    if (handle_ && initialised_ && !shutdown_attempted_)
      api_.Shutdown(handle_);
    if (handle_)
      api_.Destroy(handle_);
  }

  bool query_capabilities() noexcept {
    caps_.struct_size = sizeof(caps_);
    caps_.version = AMBER_CAPABILITIES_VERSION_1;
    const AmberResult result = api_.GetCapabilities(handle_, &caps_);
    if (result != AMBER_OK) {
      amber_error("Amber GetCapabilities", result);
      return false;
    }
    return true;
  }

  FabricResult initialise() noexcept override {
    AmberInitialiseParams params{};
    params.struct_size = sizeof(params);
    for (size_t i = 0; i < program_.size(); ++i)
      params.program_roms[i] = program_[i].second.c_str();
    for (size_t i = 0; i < sound_.size(); ++i)
      params.sound_roms[i] = sound_[i].second.c_str();
    AmberResult result = api_.Initialise(handle_, &params);
    if (result != AMBER_OK)
      return amber_error("Amber Initialise", result);
    initialised_ = true;
    /* Validate every requested optional capability before applying any
     * configuration. */
    if ((config_.flags & FABRIC_AMBER_CONFIGURE_REELS) &&
        !(caps_.feature_bits & AMBER_CAP_REEL_CONFIGURATION))
      return unsupported("Amber reel configuration capability is unavailable");
    if ((config_.flags & FABRIC_AMBER_CONFIGURE_COINS) &&
        !(caps_.feature_bits & AMBER_CAP_COIN_CONFIGURATION))
      return unsupported("Amber coin configuration capability is unavailable");
    if ((config_.flags & FABRIC_AMBER_CONFIGURE_PERCENTAGE) &&
        !(caps_.feature_bits & AMBER_CAP_PERCENT_SWITCH))
      return unsupported("Amber percentage-switch capability is unavailable");
    if (config_.flags & FABRIC_AMBER_CONFIGURE_REELS) {
      result = api_.ConfigureReels(handle_, &config_.reels);
      if (result != AMBER_OK)
        return amber_error("Amber ConfigureReels", result);
    }
    if (config_.flags & FABRIC_AMBER_CONFIGURE_COINS) {
      result = api_.ConfigureCoins(handle_, &config_.coins);
      if (result != AMBER_OK)
        return amber_error("Amber ConfigureCoins", result);
    }
    if (config_.flags & FABRIC_AMBER_CONFIGURE_PERCENTAGE) {
      result = api_.SetPercentageSwitch(handle_, config_.percentage_switch);
      if (result != AMBER_OK)
        return amber_error("Amber SetPercentageSwitch", result);
    }
    error_.clear();
    return FABRIC_OK;
  }

  FabricResult reset() noexcept override {
    const AmberResult result = api_.Reset(handle_);
    return result == AMBER_OK ? success() : amber_error("Amber Reset", result);
  }

  FabricResult advance(uint64_t elapsed_nanoseconds) noexcept override {
    if (!system6_)
      return unsupported(
          "elapsed-time conversion is not defined for this Amber machine");
    const uint64_t whole = elapsed_nanoseconds / 125u;
    const uint64_t tail = elapsed_nanoseconds % 125u;
    const uint64_t combined = remainder_nanoseconds_ + tail;
    remainder_nanoseconds_ = combined % 125u;
    enqueue_cycles(whole + combined / 125u);
    while (!pending_cycles_.empty()) {
      uint64_t &pending = pending_cycles_.front();
      /* cycles_run is signed, so request only a representable completion. */
      const uint32_t requested =
          static_cast<uint32_t>(std::min<uint64_t>(pending, INT32_MAX));
      int32_t cycles_run = 0;
      const AmberResult result = api_.Run(handle_, requested, &cycles_run);
      if (result != AMBER_OK)
        return amber_error("Amber Run", result);
      if (cycles_run < 0)
        return fabric_error("Amber Run returned negative cycles_run " +
                            std::to_string(cycles_run));
      if (static_cast<uint32_t>(cycles_run) > requested)
        return fabric_error("Amber Run returned cycles_run greater than the "
                            "requested cycle count");
      if (cycles_run == 0)
        return fabric_error(
            "Amber Run made zero progress; execution work was retained");
      pending -= static_cast<uint32_t>(cycles_run);
      if (static_cast<uint32_t>(cycles_run) < requested)
        return success(); /* Retain partial progress for the next boundary. */
      if (!pending)
        pending_cycles_.pop_front();
    }
    return success();
  }

  FabricResult shutdown() noexcept override {
    if (shutdown_attempted_)
      return shutdown_result_;
    release_switches();
    shutdown_attempted_ = true;
    const AmberResult result = api_.Shutdown(handle_);
    if (result != AMBER_OK) {
      shutdown_result_ = amber_error("Amber Shutdown", result);
      return shutdown_result_;
    }
    shutdown_ = true;
    initialised_ = false;
    shutdown_result_ = success();
    return shutdown_result_;
  }

  FabricResult submit_input(const FabricInput &input) noexcept override {
    if (!(caps_.feature_bits & AMBER_CAP_SWITCH_INPUT))
      return unsupported("Amber switch-input capability is unavailable");
    if (input.numerical_index < 0 ||
        static_cast<uint32_t>(input.numerical_index) >= caps_.max_switches)
      return invalid("switch index exceeds the Amber maximum");
    const AmberResult result = api_.SetSwitchState(
        handle_, static_cast<uint32_t>(input.numerical_index),
        input.active ? 1u : 0u);
    if (result != AMBER_OK)
      return amber_error("Amber SetSwitchState", result);
    if (input.active)
      asserted_.insert(input.numerical_index);
    else
      asserted_.erase(input.numerical_index);
    return success();
  }

  FabricResult capabilities(FabricCapabilities &out) noexcept override {
    out.flags = 0;
    if (caps_.feature_bits & AMBER_CAP_SWITCH_INPUT)
      out.flags |= FABRIC_CAPABILITY_DIGITAL_INPUT;
    /* Amber v2 exposes all four known output families through its single
     * coherent snapshot. */
    if (caps_.feature_bits & AMBER_CAP_OUTPUT_SNAPSHOT)
      out.flags |= FABRIC_CAPABILITY_LAMPS | FABRIC_CAPABILITY_REELS |
                   FABRIC_CAPABILITY_CHARACTER_DISPLAYS |
                   FABRIC_CAPABILITY_SEGMENT_DISPLAYS;
    if (caps_.feature_bits & AMBER_CAP_AUDIO)
      out.flags |= FABRIC_CAPABILITY_AUDIO;
    return success();
  }

  FabricResult snapshot(FabricMachineSnapshot &out) noexcept override {
    if (!(caps_.feature_bits & AMBER_CAP_OUTPUT_SNAPSHOT))
      return unsupported("Amber output-snapshot capability is unavailable");
    AmberOutputSnapshotV1 source{};
    source.struct_size = sizeof(source);
    source.version = AMBER_OUTPUT_SNAPSHOT_VERSION_1;
    const AmberResult result = api_.GetOutputSnapshot(handle_, &source);
    if (result != AMBER_OK)
      return amber_error("Amber GetOutputSnapshot", result);
    if (source.matrix_lamp_count > AMBER_MAX_MATRIX_LAMPS ||
        source.reel_count > AMBER_MAX_REELS ||
        source.alpha_display_count > AMBER_MAX_ALPHA_DISPLAYS ||
        source.seven_segment_display_count > AMBER_MAX_SEVEN_SEGMENT_DISPLAYS)
      return fabric_error(
          "Amber snapshot reported counts beyond the API v2 limits");
    out.lamp_count = source.matrix_lamp_count;
    out.reel_count = source.reel_count;
    out.character_display_count = source.alpha_display_count;
    out.segment_display_count = source.seven_segment_display_count;
    if (out.lamp_capacity < out.lamp_count ||
        out.reel_capacity < out.reel_count ||
        out.character_display_capacity < out.character_display_count ||
        out.segment_display_capacity < out.segment_display_count)
      return FABRIC_BUFFER_TOO_SMALL;
    if ((out.lamp_count && !out.lamps) || (out.reel_count && !out.reels) ||
        (out.character_display_count && !out.character_displays) ||
        (out.segment_display_count && !out.segment_displays))
      return invalid("snapshot output buffer is null");
    for (uint32_t i = 0; i < out.lamp_count; ++i) {
      auto &dest = out.lamps[i];
      dest = {};
      dest.struct_size = sizeof(dest);
      dest.struct_version = FABRIC_ABI_VERSION_1;
      std::snprintf(dest.identifier, sizeof(dest.identifier), "amber.lamp.%u",
                    i);
      dest.numerical_index = static_cast<int32_t>(i);
      dest.logical_state = source.matrix_lamps[i].is_on ? 1 : 0;
      dest.brightness =
          static_cast<float>(source.matrix_lamps[i].brightness_q16_16) /
          65536.0f;
    }
    for (uint32_t i = 0; i < out.reel_count; ++i) {
      auto &dest = out.reels[i];
      dest = {};
      dest.struct_size = sizeof(dest);
      dest.struct_version = FABRIC_ABI_VERSION_1;
      std::snprintf(dest.identifier, sizeof(dest.identifier), "amber.reel.%u",
                    i);
      dest.numerical_index = static_cast<int32_t>(i);
      dest.position = source.reel_positions[i];
    }
    for (uint32_t i = 0; i < out.character_display_count; ++i) {
      auto &dest = out.character_displays[i];
      dest = {};
      dest.struct_size = sizeof(dest);
      dest.struct_version = FABRIC_ABI_VERSION_1;
      std::snprintf(dest.identifier, sizeof(dest.identifier), "amber.alpha.%u",
                    i);
      dest.character_count = AMBER_ALPHA_CHARACTERS;
      dest.character_capacity = FABRIC_CHARACTER_CAPACITY;
      for (uint32_t j = 0; j < AMBER_ALPHA_CHARACTERS; ++j) {
        dest.characters[j] = source.alpha_displays[i].segment_masks[j];
        dest.attributes[j] = source.alpha_displays[i].dot_comma[j];
      }
    }
    for (uint32_t i = 0; i < out.segment_display_count; ++i) {
      auto &dest = out.segment_displays[i];
      dest = {};
      dest.struct_size = sizeof(dest);
      dest.struct_version = FABRIC_ABI_VERSION_1;
      std::snprintf(dest.identifier, sizeof(dest.identifier),
                    "amber.seven-segment.%u", i);
      dest.digit_count = 1;
      dest.digit_capacity = FABRIC_SEGMENT_DIGIT_CAPACITY;
      dest.segment_masks[0] = source.seven_segment_displays[i].segment_mask;
    }
    out.sequence = ++sequence_;
    return success();
  }

  FabricResult audio_format(FabricAudioFormat &out) noexcept override {
    if (!(caps_.feature_bits & AMBER_CAP_AUDIO))
      return unsupported("Amber audio capability is unavailable");
    AmberAudioFormatV1 source{};
    source.struct_size = sizeof(source);
    source.version = AMBER_AUDIO_FORMAT_VERSION_1;
    const AmberResult result = api_.GetAudioFormat(handle_, &source);
    if (result != AMBER_OK)
      return amber_error("Amber GetAudioFormat", result);
    if (source.sample_format != AMBER_AUDIO_SAMPLE_PCM_S16 ||
        source.interleaving != AMBER_AUDIO_INTERLEAVED)
      return unsupported("Amber audio is not interleaved PCM16");
    if (source.channels > UINT16_MAX)
      return fabric_error("Amber audio channel count exceeds Fabric ABI range");
    out.sample_rate = source.sample_rate;
    out.channel_count = static_cast<uint16_t>(source.channels);
    out.bits_per_sample = 16;
    out.interleaved = 1;
    out.signed_samples = 1;
    out.little_endian = 1;
    return success();
  }

  FabricResult read_audio(int16_t *samples, uint32_t capacity,
                          uint32_t &written) noexcept override {
    written = 0;
    if (!(caps_.feature_bits & AMBER_CAP_AUDIO))
      return unsupported("Amber audio capability is unavailable");
    if (capacity && !samples)
      return invalid("audio buffer is null");
    const AmberResult result =
        api_.FillAudioFrames(handle_, samples, capacity, &written);
    if (result != AMBER_OK) {
      written = 0;
      return amber_error("Amber FillAudioFrames", result);
    }
    if (written > capacity) {
      written = 0;
      return fabric_error("Amber FillAudioFrames over-reported frames_written");
    }
    return success();
  }

  std::string last_error() const noexcept override { return error_; }

private:
  void enqueue_cycles(uint64_t cycles) {
    if (cycles)
      pending_cycles_.push_back(cycles);
  }
  FabricResult success() {
    error_.clear();
    return FABRIC_OK;
  }
  FabricResult invalid(const std::string &message) {
    error_ = message;
    return FABRIC_INVALID_ARGUMENT;
  }
  FabricResult unsupported(const std::string &message) {
    error_ = message;
    return FABRIC_NOT_SUPPORTED;
  }
  FabricResult fabric_error(const std::string &message) {
    error_ = message;
    return FABRIC_BACKEND_ERROR;
  }
  FabricResult amber_error(const char *operation, AmberResult result) {
    char buffer[FABRIC_ERROR_CAPACITY]{};
    uint32_t required = 0;
    const AmberResult error_result =
        api_.GetLastError(handle_, buffer, sizeof(buffer), &required);
    error_ = std::string(operation) + " failed (AmberResult " +
             std::to_string(static_cast<int>(result)) + ")";
    if (error_result == AMBER_OK && buffer[0])
      error_ += ": " + std::string(buffer);
    else if (error_result != AMBER_OK)
      error_ += "; Amber GetLastError also failed (AmberResult " +
                std::to_string(static_cast<int>(error_result)) + ")";
    return FABRIC_BACKEND_ERROR;
  }
  void release_switches() noexcept {
    if (handle_ && !shutdown_)
      for (int32_t index : asserted_)
        api_.SetSwitchState(handle_, static_cast<uint32_t>(index), 0);
    asserted_.clear();
  }

  std::unique_ptr<AmberDynamicLibrary> module_;
  AmberApiV2 api_{};
  AmberHandle handle_ = nullptr;
  AmberCapabilitiesV1 caps_{};
  std::vector<Rom> program_, sound_, other_;
  std::set<int32_t> asserted_;
  FabricAmberConfigurationV1 config_{};
  std::string error_;
  std::deque<uint64_t> pending_cycles_;
  uint64_t remainder_nanoseconds_ = 0;
  uint64_t sequence_ = 0;
  bool system6_ = false;
  bool initialised_ = false;
  bool shutdown_ = false;
  bool shutdown_attempted_ = false;
  FabricResult shutdown_result_ = FABRIC_OK;
};

bool complete_table(const AmberApiV2 &a) noexcept {
  return a.GetBridgeInfo && a.EnumerateCore && a.Create && a.Destroy &&
         a.Initialise && a.Reset && a.Run && a.Shutdown && a.GetLastError &&
         a.GetCapabilities && a.SetSwitchState && a.GetOutputSnapshot &&
         a.GetAudioFormat && a.FillAudioFrames && a.ConfigureReels &&
         a.ConfigureCoins && a.SetPercentageSwitch;
}

std::string table_error(const AmberApiV2 &api, AmberHandle handle,
                        const char *operation, AmberResult result) {
  std::string message = std::string(operation) + " failed (AmberResult " +
                        std::to_string(static_cast<int>(result)) + ")";
  char buffer[FABRIC_ERROR_CAPACITY]{};
  uint32_t required = 0;
  const AmberResult error_result =
      api.GetLastError(handle, buffer, sizeof(buffer), &required);
  if (error_result == AMBER_OK && buffer[0])
    message += ": " + std::string(buffer);
  else if (error_result != AMBER_OK)
    message += "; Amber GetLastError also failed (AmberResult " +
               std::to_string(static_cast<int>(error_result)) + ")";
  return message;
}

class Provider final : public FabricBackendProvider {
public:
  bool supports(const std::string &kind,
                const std::string &) const noexcept override {
    return kind == "amber-api-v2";
  }
  FabricResult create(const FabricLaunchRequest &request,
                      std::unique_ptr<FabricBackendInstance> &out,
                      std::string &error) noexcept override {
    try {
      if (!std::filesystem::path(request.backend_path).is_absolute()) {
        error = "Amber backend path must be absolute";
        return FABRIC_INVALID_ARGUMENT;
      }
      std::vector<AmberInstance::Rom> program, sound, other;
      FabricResult validation =
          validate_roms(request, program, sound, other, error);
      if (validation != FABRIC_OK)
        return validation;
      const FabricAmberConfigurationV1 *configuration = nullptr;
      if (request.machine_configuration_size) {
        if (request.machine_configuration_size !=
            sizeof(FabricAmberConfigurationV1)) {
          error = "malformed Amber backend configuration size";
          return FABRIC_INVALID_ARGUMENT;
        }
        configuration = static_cast<const FabricAmberConfigurationV1 *>(
            request.machine_configuration);
        if (configuration->magic != FABRIC_AMBER_CONFIGURATION_MAGIC ||
            configuration->struct_size != sizeof(*configuration) ||
            configuration->version != FABRIC_AMBER_CONFIGURATION_VERSION_1 ||
            (configuration->flags & ~UINT32_C(7))) {
          error = "malformed Amber backend configuration";
          return FABRIC_INVALID_ARGUMENT;
        }
        if ((configuration->flags & FABRIC_AMBER_CONFIGURE_REELS) &&
            (configuration->reels.struct_size != sizeof(configuration->reels) ||
             configuration->reels.version !=
                 AMBER_REEL_CONFIGURATION_VERSION_1 ||
             configuration->reels.reel_count > AMBER_MAX_REELS ||
             (configuration->reels.apply_mask &
              ~((UINT32_C(1) << AMBER_MAX_REELS) - 1)))) {
          error = "malformed Amber reel configuration";
          return FABRIC_INVALID_ARGUMENT;
        }
        if ((configuration->flags & FABRIC_AMBER_CONFIGURE_COINS) &&
            (configuration->coins.struct_size != sizeof(configuration->coins) ||
             configuration->coins.version !=
                 AMBER_COIN_CONFIGURATION_VERSION_1 ||
             (configuration->coins.channel_apply_mask &
              ~((UINT32_C(1) << AMBER_MAX_COIN_CHANNELS) - 1)) ||
             (configuration->coins.route_apply_mask &
              ~((UINT32_C(1) << AMBER_MAX_COIN_ROUTES) - 1)))) {
          error = "malformed Amber coin configuration";
          return FABRIC_INVALID_ARGUMENT;
        }
        if ((configuration->flags & FABRIC_AMBER_CONFIGURE_PERCENTAGE) &&
            configuration->percentage_switch > 15) {
          error = "Amber percentage switch must be in the range 0..15";
          return FABRIC_INVALID_ARGUMENT;
        }
      }
      auto library = std::make_unique<AmberDynamicLibrary>();
      if (!library->open(request.backend_path, error))
        return FABRIC_NOT_FOUND;
      void *get_api_symbol = library->symbol("AmberGetApi");
      amber_trace::Write(std::string("AmberGetApi present: ") +
                         (get_api_symbol ? "yes" : "no"));
      if (!get_api_symbol) {
        amber_trace::Write("selected adapter: production-legacy");
        return CreateLegacyAmberInstance(request, std::move(library), out,
                                         error);
      }
      amber_trace::Write("selected adapter: provider-v2");
      static_assert(sizeof(GetApi) == sizeof(get_api_symbol),
                    "dynamic function pointer representation");
      GetApi get_api = nullptr;
      std::memcpy(&get_api, &get_api_symbol, sizeof(get_api));
      AmberApiV2 api{};
      api.struct_size = sizeof(api);
      api.api_version = AMBER_API_VERSION_2;
      const AmberResult negotiated =
          get_api(AMBER_API_VERSION_2, sizeof(api), &api);
      if (negotiated != AMBER_OK || api.struct_size < sizeof(api) ||
          api.api_version != AMBER_API_VERSION_2) {
        error = "Amber API v2 negotiation failed (AmberResult " +
                std::to_string(static_cast<int>(negotiated)) + ")";
        return FABRIC_UNSUPPORTED_VERSION;
      }
      if (!complete_table(api)) {
        error = "Amber API v2 function table is incomplete";
        return FABRIC_NOT_SUPPORTED;
      }
      bool found = false;
      for (uint32_t index = 0;; ++index) {
        AmberCoreInfo core{};
        core.struct_size = sizeof(core);
        const AmberResult result = api.EnumerateCore(index, &core);
        if (result == AMBER_NO_MORE_ITEMS)
          break;
        if (result != AMBER_OK) {
          error = table_error(api, nullptr, "Amber EnumerateCore", result);
          return FABRIC_BACKEND_ERROR;
        }
        if (core.core_id &&
            std::string(request.machine_identifier) == core.core_id)
          found = true;
      }
      if (!found) {
        error = "requested machine is not enumerated by Amber DLL";
        return FABRIC_NOT_FOUND;
      }
      AmberHandle handle = nullptr;
      const AmberResult created =
          api.Create(request.machine_identifier, &handle);
      if (created != AMBER_OK || !handle) {
        error = table_error(api, handle, "Amber Create", created);
        return FABRIC_BACKEND_ERROR;
      }
      auto instance = std::make_unique<AmberInstance>(
          std::move(library), api, handle, std::move(program), std::move(sound),
          std::move(other), configuration,
          std::string(request.machine_identifier) == "jpm-system6");
      if (!instance->query_capabilities()) {
        error = instance->last_error();
        return FABRIC_BACKEND_ERROR;
      }
      out = std::move(instance);
      return FABRIC_OK;
    } catch (const std::exception &exception) {
      error = exception.what();
      return FABRIC_INTERNAL_ERROR;
    }
  }

private:
  static FabricResult validate_roms(const FabricLaunchRequest &request,
                                    std::vector<AmberInstance::Rom> &program,
                                    std::vector<AmberInstance::Rom> &sound,
                                    std::vector<AmberInstance::Rom> &other,
                                    std::string &error) {
    if (request.rom_path_count && request.rom_resource_count) {
      error = "legacy and typed ROM lists cannot be supplied together";
      return FABRIC_INVALID_ARGUMENT;
    }
    if (request.rom_resource_count) {
      std::set<std::pair<uint32_t, uint32_t>> seen;
      for (uint32_t i = 0; i < request.rom_resource_count; ++i) {
        const FabricRomResource &resource = request.rom_resources[i];
        if (resource.struct_size < sizeof(resource) ||
            resource.struct_version != FABRIC_ABI_VERSION_1) {
          error = "malformed typed ROM resource";
          return FABRIC_INVALID_ARGUMENT;
        }
        if (resource.role != FABRIC_ROM_ROLE_PROGRAM &&
            resource.role != FABRIC_ROM_ROLE_SOUND &&
            resource.role != FABRIC_ROM_ROLE_OTHER) {
          error = "unknown typed ROM role";
          return FABRIC_INVALID_ARGUMENT;
        }
        if (!resource.path || !resource.path[0]) {
          error = "typed ROM path is empty";
          return FABRIC_INVALID_ARGUMENT;
        }
        if (!seen.insert({resource.role, resource.slot}).second) {
          error = "duplicate typed ROM role and slot";
          return FABRIC_INVALID_ARGUMENT;
        }
        if ((resource.role == FABRIC_ROM_ROLE_PROGRAM ||
             resource.role == FABRIC_ROM_ROLE_SOUND) &&
            resource.slot >= 4) {
          error = "Amber program and sound ROM slots must be in the range 0..3";
          return FABRIC_INVALID_ARGUMENT;
        }
        if (resource.role == FABRIC_ROM_ROLE_PROGRAM)
          program.emplace_back(resource.slot, resource.path);
        else if (resource.role == FABRIC_ROM_ROLE_SOUND)
          sound.emplace_back(resource.slot, resource.path);
        else
          other.emplace_back(resource.slot, resource.path);
      }
      auto contiguous = [](std::vector<AmberInstance::Rom> &roms) {
        std::sort(roms.begin(), roms.end(), [](const auto &a, const auto &b) {
          return a.first < b.first;
        });
        for (size_t i = 0; i < roms.size(); ++i)
          if (roms[i].first != i)
            return false;
        return true;
      };
      if (!contiguous(program) || !contiguous(sound)) {
        error = "Amber program and sound ROM slots must be contiguous from "
                "slot zero";
        return FABRIC_INVALID_ARGUMENT;
      }
    } else {
      if (request.rom_path_count > 4) {
        error = "Amber supports at most four legacy program ROM paths";
        return FABRIC_INVALID_ARGUMENT;
      }
      for (uint32_t i = 0; i < request.rom_path_count; ++i) {
        if (!request.rom_paths[i] || !request.rom_paths[i][0]) {
          error = "legacy ROM path is empty";
          return FABRIC_INVALID_ARGUMENT;
        }
        program.emplace_back(i, request.rom_paths[i]);
      }
    }
    return FABRIC_OK;
  }
};
} // namespace
std::unique_ptr<FabricBackendProvider> MakeAmberBackendProvider() {
  return std::make_unique<Provider>();
}
} // namespace fabric
