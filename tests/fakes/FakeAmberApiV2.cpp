#include "FakeAmberControl.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>

struct AmberInstance_t {
  bool initialised = false;
};
namespace {
FakeAmberState state{};
void event(const char *value) {
  if (state.lifecycle[0])
    std::strncat(state.lifecycle, ",",
                 sizeof(state.lifecycle) - std::strlen(state.lifecycle) - 1);
  std::strncat(state.lifecycle, value,
               sizeof(state.lifecycle) - std::strlen(state.lifecycle) - 1);
}
void copy(char *to, size_t size, const char *from) {
  if (from)
    std::snprintf(to, size, "%s", from);
}
AmberResult AMBER_CALL bridge(AmberBridgeInfo *info) {
  if (!info)
    return AMBER_INVALID_ARGUMENT;
  info->api_version = AMBER_API_VERSION_2;
  info->name = "Fake Amber";
  info->bridge_version = "test";
  return AMBER_OK;
}
AmberResult AMBER_CALL enumerate(uint32_t index, AmberCoreInfo *core) {
  event("enumerate");
  if (state.fail_operation == 3)
    return AMBER_INTERNAL_ERROR;
  if (!core)
    return AMBER_INVALID_ARGUMENT;
  if (index)
    return AMBER_NO_MORE_ITEMS;
  core->core_id = "jpm-system6";
  core->display_name = "Fake System 6";
  return AMBER_OK;
}
AmberResult AMBER_CALL create(const char *id, AmberHandle *handle) {
  event("create");
  copy(state.selected_core, sizeof(state.selected_core), id);
  if (state.fail_operation == 4)
    return AMBER_INTERNAL_ERROR;
  if (!id || !handle || std::strcmp(id, "jpm-system6"))
    return AMBER_INVALID_ARGUMENT;
  *handle = new (std::nothrow) AmberInstance_t;
  ++state.create_count;
  return *handle ? AMBER_OK : AMBER_INTERNAL_ERROR;
}
AmberResult AMBER_CALL destroy(AmberHandle handle) {
  event("destroy");
  delete handle;
  ++state.destroy_count;
  return AMBER_OK;
}
AmberResult AMBER_CALL initialise(AmberHandle handle,
                                  const AmberInitialiseParams *params) {
  event("initialise");
  if (state.fail_operation == 6)
    return AMBER_INITIALISE_FAILED;
  if (!handle || !params)
    return AMBER_INVALID_ARGUMENT;
  for (unsigned i = 0; i < 4; ++i) {
    copy(state.program_roms[i], sizeof(state.program_roms[i]),
         params->program_roms[i]);
    copy(state.sound_roms[i], sizeof(state.sound_roms[i]),
         params->sound_roms[i]);
  }
  handle->initialised = true;
  ++state.initialise_count;
  return AMBER_OK;
}
AmberResult AMBER_CALL reset(AmberHandle h) {
  event("reset");
  return h ? AMBER_OK : AMBER_INVALID_ARGUMENT;
}
AmberResult AMBER_CALL run(AmberHandle h, uint32_t requested, int32_t *ran) {
  event("run");
  if (!h || !ran)
    return AMBER_INVALID_ARGUMENT;
  if (state.run_call_count < 32)
    state.run_requests[state.run_call_count] = requested;
  ++state.run_call_count;
  *ran = state.run_mode ? state.run_result : static_cast<int32_t>(requested);
  return AMBER_OK;
}
AmberResult AMBER_CALL shutdown(AmberHandle h) {
  event("shutdown");
  ++state.shutdown_count;
  if (state.fail_operation == 10)
    return AMBER_INTERNAL_ERROR;
  return h ? AMBER_OK : AMBER_INVALID_ARGUMENT;
}
AmberResult AMBER_CALL last_error(AmberHandle, char *buffer, uint32_t capacity,
                                  uint32_t *required) {
  const char *message = "fake Amber failure";
  if (!required)
    return AMBER_INVALID_ARGUMENT;
  *required = static_cast<uint32_t>(std::strlen(message) + 1);
  if (!buffer || capacity < *required)
    return AMBER_BUFFER_TOO_SMALL;
  std::memcpy(buffer, message, *required);
  return AMBER_OK;
}
AmberResult AMBER_CALL capabilities(AmberHandle h, AmberCapabilitiesV1 *caps) {
  event("capabilities");
  if (state.fail_operation == 5)
    return AMBER_INTERNAL_ERROR;
  if (!h || !caps)
    return AMBER_INVALID_ARGUMENT;
  caps->feature_bits = state.capabilities;
  caps->max_switches = state.max_switches;
  return AMBER_OK;
}
AmberResult AMBER_CALL set_switch(AmberHandle h, uint32_t index,
                                  uint32_t level) {
  event(level ? "switch-on" : "switch-off");
  if (!h || index >= state.max_switches)
    return AMBER_INVALID_RANGE;
  if (state.switch_count < 32) {
    state.switch_indices[state.switch_count] = index;
    state.switch_levels[state.switch_count] = level;
  }
  ++state.switch_count;
  return AMBER_OK;
}
AmberResult AMBER_CALL snapshot(AmberHandle h, AmberOutputSnapshotV1 *s) {
  event("snapshot");
  if (!h || !s)
    return AMBER_INVALID_ARGUMENT;
  s->matrix_lamp_count = 1;
  s->matrix_lamps[0] = {1, 32768};
  s->reel_count = 1;
  s->reel_positions[0] = -3;
  s->alpha_display_count = 1;
  s->alpha_displays[0].segment_masks[0] = 0x1234;
  s->alpha_displays[0].dot_comma[0] =
      AMBER_ALPHA_DECIMAL_POINT | AMBER_ALPHA_COMMA_TAIL;
  s->seven_segment_display_count = 1;
  s->seven_segment_displays[0].segment_mask = 0xabcdef;
  return AMBER_OK;
}
AmberResult AMBER_CALL format(AmberHandle h, AmberAudioFormatV1 *f) {
  event("audio-format");
  if (!h || !f)
    return AMBER_INVALID_ARGUMENT;
  f->sample_rate = 44100;
  f->channels = 2;
  f->sample_format = AMBER_AUDIO_SAMPLE_PCM_S16;
  f->interleaving = AMBER_AUDIO_INTERLEAVED;
  return AMBER_OK;
}
AmberResult AMBER_CALL audio(AmberHandle h, int16_t *samples, uint32_t capacity,
                             uint32_t *written) {
  event("audio");
  if (!h || !written || (capacity && !samples))
    return AMBER_INVALID_ARGUMENT;
  state.audio_capacity = capacity;
  *written = std::min(capacity, 2u);
  state.audio_written = *written;
  for (uint32_t i = 0; i < *written * 2; i++)
    samples[i] = static_cast<int16_t>(i);
  return AMBER_OK;
}
AmberResult AMBER_CALL reels(AmberHandle h, const AmberReelConfigurationV1 *c) {
  event("reels");
  ++state.reel_calls;
  if (c)
    state.reel_apply_mask = c->apply_mask;
  if (state.fail_operation == 7)
    return AMBER_INTERNAL_ERROR;
  return h ? AMBER_OK : AMBER_INVALID_ARGUMENT;
}
AmberResult AMBER_CALL coins(AmberHandle h, const AmberCoinConfigurationV1 *c) {
  event("coins");
  ++state.coin_calls;
  if (c) {
    state.coin_channel_mask = c->channel_apply_mask;
    state.coin_route_mask = c->route_apply_mask;
  }
  if (state.fail_operation == 8)
    return AMBER_INTERNAL_ERROR;
  return h ? AMBER_OK : AMBER_INVALID_ARGUMENT;
}
AmberResult AMBER_CALL percent(AmberHandle h, uint32_t value) {
  event("percentage");
  ++state.percentage_calls;
  state.percentage_value = value;
  if (state.fail_operation == 9)
    return AMBER_INTERNAL_ERROR;
  return h ? AMBER_OK : AMBER_INVALID_ARGUMENT;
}
} // namespace
FAKE_EXPORT void FAKE_CALL FakeAmberReset() {
  std::memset(&state, 0, sizeof(state));
  state.capabilities = AMBER_CAP_SWITCH_INPUT | AMBER_CAP_OUTPUT_SNAPSHOT |
                       AMBER_CAP_AUDIO | AMBER_CAP_REEL_CONFIGURATION |
                       AMBER_CAP_COIN_CONFIGURATION | AMBER_CAP_PERCENT_SWITCH;
  state.max_switches = 256;
}
FAKE_EXPORT const FakeAmberState *FAKE_CALL FakeAmberGetState() {
  return &state;
}
FAKE_EXPORT void FAKE_CALL FakeAmberSetRun(uint32_t mode, int32_t result) {
  state.run_mode = mode;
  state.run_result = result;
}
FAKE_EXPORT void FAKE_CALL FakeAmberSetCapabilities(uint64_t bits) {
  state.capabilities = bits;
}
FAKE_EXPORT void FAKE_CALL FakeAmberSetFailure(uint32_t operation) {
  state.fail_operation = operation;
}
extern "C" AMBER_EXPORT AmberResult AMBER_CALL AmberGetApi(uint32_t version,
                                                           uint32_t size,
                                                           void *out) {
  state.requested_version = version;
  state.requested_size = size;
  event("get-api");
  if (version != AMBER_API_VERSION_2 || state.fail_operation == 1)
    return AMBER_UNSUPPORTED_VERSION;
  if (!out || size < sizeof(AmberApiV2))
    return AMBER_INVALID_ARGUMENT;
  AmberApiV2 api{};
  api.struct_size = sizeof(api);
  api.api_version = AMBER_API_VERSION_2;
  api.GetBridgeInfo = bridge;
  api.EnumerateCore = enumerate;
  api.Create = create;
  api.Destroy = destroy;
  api.Initialise = initialise;
  api.Reset = reset;
  api.Run = run;
  api.Shutdown = shutdown;
  api.GetLastError = last_error;
  api.GetCapabilities = capabilities;
  api.SetSwitchState = set_switch;
  api.GetOutputSnapshot = snapshot;
  api.GetAudioFormat = format;
  api.FillAudioFrames = audio;
  api.ConfigureReels = reels;
  api.ConfigureCoins = coins;
  api.SetPercentageSwitch = percent;
  if (state.fail_operation == 2)
    api.Run = nullptr;
  std::memcpy(out, &api, sizeof(api));
  return AMBER_OK;
}
struct Initializer {
  Initializer() { FakeAmberReset(); }
} initializer;
