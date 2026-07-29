#include "FakeAmberControl.h"
#include "fabric/fabric.h"
#include "fabric/fabric_amber.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
namespace {
int failures = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      std::cerr << __FILE__ << ':' << __LINE__ << ": " #x " failed\n";         \
      ++failures;                                                              \
    }                                                                          \
  } while (0)
FabricLaunchRequest request() {
  FabricLaunchRequest r{};
  r.struct_size = sizeof(r);
  r.struct_version = FABRIC_ABI_VERSION_1;
  std::strcpy(r.backend_kind, "amber-api-v2");
  std::strcpy(r.machine_identifier, "jpm-system6");
  std::strcpy(r.backend_path, FAKE_AMBER_PATH);
  return r;
}
FabricMachineSession *create(FabricRuntime *r,
                             FabricLaunchRequest *q = nullptr) {
  auto local = q ? *q : request();
  FabricMachineSession *s = nullptr;
  CHECK(FabricCreateSession(r, &local, &s) == FABRIC_OK);
  return s;
}
std::string runtime_error(FabricRuntime *r) {
  char b[512]{};
  uint32_t n = 0;
  CHECK(FabricRuntimeGetLastError(r, b, sizeof(b), &n) == FABRIC_OK);
  return b;
}
std::string session_error(FabricMachineSession *s) {
  char b[512]{};
  uint32_t n = 0;
  CHECK(FabricSessionGetLastError(s, b, sizeof(b), &n) == FABRIC_OK);
  return b;
}
} // namespace
int main() {
  FabricRuntime *r = nullptr;
  CHECK(FabricCreateRuntime(FABRIC_ABI_VERSION_1, &r) == FABRIC_OK);
  auto q = request();
  FabricMachineSession *s = nullptr;
  std::strcpy(q.backend_path, "/definitely/missing/amber.so");
  CHECK(FabricCreateSession(r, &q, &s) == FABRIC_NOT_FOUND);
  CHECK(runtime_error(r).find("failed to load exact") != std::string::npos);
  q = request();
  std::strcpy(q.backend_path, FAKE_AMBER_NO_API_PATH);
  CHECK(FabricCreateSession(r, &q, &s) == FABRIC_NOT_SUPPORTED);
  CHECK(runtime_error(r).find("required export 'Initialise'") !=
        std::string::npos);
  q = request();
  std::strcpy(q.machine_identifier, "unknown-machine");
  CHECK(FabricCreateSession(r, &q, &s) == FABRIC_NOT_FOUND);
  q = request();
  FakeAmberReset();
  s = create(r, &q);
  CHECK(FakeAmberGetState()->requested_version == UINT32_C(0x00020000));
  CHECK(FakeAmberGetState()->requested_version == AMBER_API_VERSION_2);
  CHECK(FakeAmberGetState()->requested_size == sizeof(AmberApiV2));
  CHECK(std::strcmp(FakeAmberGetState()->selected_core, "jpm-system6") == 0);
  FabricDestroySession(s);
  struct OldLaunch {
    uint32_t size, version;
    char kind[64], machine[64], path[1024];
    const char *const *roms;
    uint32_t count;
    const void *config;
    uint32_t config_size, reserved;
  } old{};
  old.size = sizeof(old);
  old.version = FABRIC_ABI_VERSION_1;
  std::strcpy(old.kind, "amber-api-v2");
  std::strcpy(old.machine, "jpm-system6");
  std::strcpy(old.path, FAKE_AMBER_PATH);
  s = nullptr;
  CHECK(FabricCreateSession(r, reinterpret_cast<FabricLaunchRequest *>(&old),
                            &s) == FABRIC_OK);
  FabricDestroySession(s);
  FabricRomResource roms[] = {{sizeof(FabricRomResource),
                               FABRIC_ABI_VERSION_1,
                               FABRIC_ROM_ROLE_SOUND,
                               1,
                               "s1",
                               {0, 0}},
                              {sizeof(FabricRomResource),
                               FABRIC_ABI_VERSION_1,
                               FABRIC_ROM_ROLE_PROGRAM,
                               1,
                               "p1",
                               {0, 0}},
                              {sizeof(FabricRomResource),
                               FABRIC_ABI_VERSION_1,
                               FABRIC_ROM_ROLE_SOUND,
                               0,
                               "s0",
                               {0, 0}},
                              {sizeof(FabricRomResource),
                               FABRIC_ABI_VERSION_1,
                               FABRIC_ROM_ROLE_PROGRAM,
                               0,
                               "p0",
                               {0, 0}}};
  q = request();
  q.rom_resources = roms;
  q.rom_resource_count = 4;
  FakeAmberReset();
  s = create(r, &q);
  CHECK(FabricSessionInitialise(s) == FABRIC_OK);
  CHECK(!std::strcmp(FakeAmberGetState()->program_roms[0], "p0") &&
        !std::strcmp(FakeAmberGetState()->program_roms[1], "p1"));
  CHECK(!std::strcmp(FakeAmberGetState()->sound_roms[0], "s0") &&
        !std::strcmp(FakeAmberGetState()->sound_roms[1], "s1"));
  CHECK(FabricSessionShutdown(s) == FABRIC_OK);
  FabricDestroySession(s);
  const char *legacy[] = {"legacy"};
  q.rom_paths = legacy;
  q.rom_path_count = 1;
  s = nullptr;
  CHECK(FabricCreateSession(r, &q, &s) == FABRIC_INVALID_ARGUMENT);
  CHECK(runtime_error(r).find("cannot be supplied together") !=
        std::string::npos);
  q.rom_paths = nullptr;
  q.rom_path_count = 0;
  roms[0].slot = 3;
  CHECK(FabricCreateSession(r, &q, &s) == FABRIC_INVALID_ARGUMENT);
  roms[0].slot = 1;
  roms[0].path = "";
  CHECK(FabricCreateSession(r, &q, &s) == FABRIC_INVALID_ARGUMENT);
  roms[0].path = "s1";
  FakeAmberReset();
  s = create(r);
  CHECK(FabricSessionInitialise(s) == FABRIC_OK);
  CHECK(FabricSessionAdvance(s, 125) == FABRIC_OK &&
        FakeAmberGetState()->run_requests[0] == 1);
  for (int i = 0; i < 4; i++)
    CHECK(FabricSessionAdvance(s, 25) == FABRIC_OK);
  CHECK(FakeAmberGetState()->run_call_count == 1);
  CHECK(FabricSessionAdvance(s, 25) == FABRIC_OK &&
        FakeAmberGetState()->run_requests[1] == 1);
  CHECK(FabricSessionAdvance(s, 1000000000) == FABRIC_OK &&
        FakeAmberGetState()->run_requests[2] == 8000000);
  uint32_t irregular_start = FakeAmberGetState()->run_call_count;
  CHECK(FabricSessionAdvance(s, 100000001) == FABRIC_OK);
  CHECK(FabricSessionAdvance(s, 333333333) == FABRIC_OK);
  CHECK(FabricSessionAdvance(s, 566666666) == FABRIC_OK);
  uint64_t irregular_cycles = 0;
  for (uint32_t i = irregular_start; i < FakeAmberGetState()->run_call_count;
       ++i)
    irregular_cycles += FakeAmberGetState()->run_requests[i];
  CHECK(irregular_cycles == 8000000);
  uint32_t partial_index = FakeAmberGetState()->run_call_count;
  FakeAmberSetRun(1, 50);
  CHECK(FabricSessionAdvance(s, 12500) == FABRIC_OK &&
        FakeAmberGetState()->run_requests[partial_index] == 100);
  FakeAmberSetRun(0, 0);
  CHECK(FabricSessionAdvance(s, 0) == FABRIC_OK &&
        FakeAmberGetState()->run_requests[partial_index + 1] == 50);
  FakeAmberSetRun(1, 0);
  CHECK(FabricSessionAdvance(s, 125) == FABRIC_BACKEND_ERROR);
  CHECK(session_error(s).find("zero progress") != std::string::npos);
  FakeAmberSetRun(1, 2);
  CHECK(FabricSessionAdvance(s, 0) == FABRIC_BACKEND_ERROR);
  FakeAmberSetRun(1, -1);
  CHECK(FabricSessionAdvance(s, 0) == FABRIC_BACKEND_ERROR);
  CHECK(session_error(s).find("negative") != std::string::npos);
  FakeAmberSetRun(0, 0);
  CHECK(FabricSessionAdvance(s, 0) == FABRIC_OK);
  const uint32_t large_start = FakeAmberGetState()->run_call_count;
  CHECK(FabricSessionAdvance(s, UINT64_C(600000000000)) == FABRIC_OK);
  CHECK(FakeAmberGetState()->run_call_count == large_start + 3);
  CHECK(FakeAmberGetState()->run_requests[large_start] == INT32_MAX);
  CHECK(FabricSessionAdvance(s, 0) == FABRIC_OK);
  CHECK(FabricSessionAdvance(s, 0) == FABRIC_OK);
  FakeAmberSetRun(1, 0);
  CHECK(FabricSessionAdvance(s, UINT64_MAX) == FABRIC_BACKEND_ERROR);
  CHECK(FabricSessionShutdown(s) == FABRIC_OK);
  FabricDestroySession(s);
  FakeAmberReset();
  s = create(r);
  FabricCapabilities caps{sizeof(caps), FABRIC_ABI_VERSION_1, 0, {0}};
  CHECK(FabricSessionGetCapabilities(s, &caps) == FABRIC_OK &&
        (caps.flags & FABRIC_CAPABILITY_AUDIO));
  CHECK(FabricSessionInitialise(s) == FABRIC_OK);
  FabricMachineSnapshot snap{};
  snap.struct_size = sizeof(snap);
  snap.struct_version = FABRIC_ABI_VERSION_1;
  CHECK(FabricSessionGetSnapshot(s, &snap) == FABRIC_BUFFER_TOO_SMALL &&
        snap.lamp_count == 1);
  FabricLamp lamp{};
  FabricReel reel{};
  FabricCharacterDisplay alpha{};
  FabricSegmentDisplay segment{};
  snap.lamps = &lamp;
  snap.lamp_capacity = 1;
  snap.reels = &reel;
  snap.reel_capacity = 1;
  snap.character_displays = &alpha;
  snap.character_display_capacity = 1;
  snap.segment_displays = &segment;
  snap.segment_display_capacity = 1;
  CHECK(FabricSessionGetSnapshot(s, &snap) == FABRIC_OK && snap.sequence == 1);
  CHECK(lamp.logical_state == 1 && std::fabs(lamp.brightness - .5f) < .001f);
  CHECK(reel.position == -3);
  CHECK(alpha.attributes[0] == 3);
  CHECK(segment.segment_masks[0] == 0xabcdef);
  CHECK(FabricSessionGetSnapshot(s, &snap) == FABRIC_OK && snap.sequence == 2);
  FabricInput input{sizeof(input), FABRIC_ABI_VERSION_1, "", 7, 1, {0}};
  CHECK(FabricSessionSubmitInput(s, &input) == FABRIC_OK);
  input.active = 0;
  CHECK(FabricSessionSubmitInput(s, &input) == FABRIC_OK &&
        FakeAmberGetState()->switch_levels[1] == 0);
  input.active = 1;
  input.numerical_index = 256;
  CHECK(FabricSessionSubmitInput(s, &input) == FABRIC_INVALID_ARGUMENT);
  input.numerical_index = 8;
  CHECK(FabricSessionSubmitInput(s, &input) == FABRIC_OK);
  int16_t samples[8]{};
  uint32_t written = 99;
  CHECK(FabricSessionReadAudio(s, samples, 4, &written) == FABRIC_OK &&
        written == 2 && FakeAmberGetState()->audio_capacity == 4);
  CHECK(FabricSessionReadAudio(s, nullptr, 0, &written) == FABRIC_OK &&
        written == 0);
  CHECK(FabricSessionShutdown(s) == FABRIC_OK);
  CHECK(FakeAmberGetState()
            ->switch_levels[FakeAmberGetState()->switch_count - 1] == 0);
  FabricDestroySession(s);
  CHECK(FakeAmberGetState()->shutdown_count == 1 &&
        FakeAmberGetState()->destroy_count == 1);
  FakeAmberReset();
  FakeAmberSetCapabilities(0);
  s = create(r);
  CHECK(FabricSessionInitialise(s) == FABRIC_OK);
  CHECK(FabricSessionSubmitInput(s, &input) == FABRIC_NOT_SUPPORTED);
  CHECK(FabricSessionGetSnapshot(s, &snap) == FABRIC_NOT_SUPPORTED);
  CHECK(FabricSessionReadAudio(s, samples, 1, &written) ==
            FABRIC_NOT_SUPPORTED &&
        written == 0);
  CHECK(FabricSessionShutdown(s) == FABRIC_OK);
  FabricDestroySession(s);
  FabricAmberConfigurationV1 config{};
  config.magic = FABRIC_AMBER_CONFIGURATION_MAGIC;
  config.struct_size = sizeof(config);
  config.version = 1;
  config.flags = 7;
  config.reels.struct_size = sizeof(config.reels);
  config.reels.version = 1;
  config.reels.apply_mask = 3;
  config.coins.struct_size = sizeof(config.coins);
  config.coins.version = 1;
  config.coins.channel_apply_mask = 1;
  config.coins.route_apply_mask = 2;
  config.percentage_switch = 9;
  q = request();
  q.machine_configuration = &config;
  q.machine_configuration_size = sizeof(config);
  FakeAmberReset();
  s = create(r, &q);
  CHECK(FabricSessionInitialise(s) == FABRIC_OK);
  CHECK(FakeAmberGetState()->reel_calls == 1 &&
        FakeAmberGetState()->coin_calls == 1 &&
        FakeAmberGetState()->percentage_value == 9 &&
        FakeAmberGetState()->reel_apply_mask == 3 &&
        FakeAmberGetState()->coin_channel_mask == 1 &&
        FakeAmberGetState()->coin_route_mask == 2);
  CHECK(std::strstr(FakeAmberGetState()->lifecycle,
                    "initialise,reels,coins,percentage"));
  CHECK(FabricSessionShutdown(s) == FABRIC_OK);
  FabricDestroySession(s);
  for (uint32_t failure : {1u, 2u, 3u, 4u, 5u}) {
    FakeAmberReset();
    FakeAmberSetFailure(failure);
    s = nullptr;
    CHECK(FabricCreateSession(r, &q, &s) != FABRIC_OK);
    CHECK(!runtime_error(r).empty());
    CHECK(FakeAmberGetState()->destroy_count == (failure == 5 ? 1u : 0u));
  }
  FakeAmberReset();
  FakeAmberSetFailure(6);
  s = create(r);
  CHECK(FabricSessionInitialise(s) == FABRIC_BACKEND_ERROR);
  CHECK(session_error(s).find("Initialise") != std::string::npos);
  FabricDestroySession(s);
  CHECK(FakeAmberGetState()->destroy_count == 1 &&
        FakeAmberGetState()->shutdown_count == 0);
  for (uint32_t failure : {7u, 8u, 9u}) {
    FakeAmberReset();
    FakeAmberSetFailure(failure);
    s = create(r, &q);
    CHECK(FabricSessionInitialise(s) == FABRIC_BACKEND_ERROR);
    const char *operation = failure == 7   ? "ConfigureReels"
                            : failure == 8 ? "ConfigureCoins"
                                           : "SetPercentageSwitch";
    CHECK(session_error(s).find(operation) != std::string::npos);
    FabricDestroySession(s);
    CHECK(FakeAmberGetState()->shutdown_count == 1 &&
          FakeAmberGetState()->destroy_count == 1);
  }
  FakeAmberReset();
  FakeAmberSetCapabilities(AMBER_CAP_REEL_CONFIGURATION |
                           AMBER_CAP_PERCENT_SWITCH);
  s = create(r, &q);
  CHECK(FabricSessionInitialise(s) == FABRIC_NOT_SUPPORTED);
  CHECK(FakeAmberGetState()->reel_calls == 0);
  FabricDestroySession(s);
  FakeAmberReset();
  FakeAmberSetFailure(10);
  s = create(r);
  CHECK(FabricSessionInitialise(s) == FABRIC_OK);
  CHECK(FabricSessionShutdown(s) == FABRIC_BACKEND_ERROR);
  FabricDestroySession(s);
  CHECK(FakeAmberGetState()->shutdown_count == 1 &&
        FakeAmberGetState()->destroy_count == 1); /* no double shutdown */
  FabricDestroyRuntime(r);
  return failures ? 1 : 0;
}
