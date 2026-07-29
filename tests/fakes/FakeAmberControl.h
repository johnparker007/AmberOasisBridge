#ifndef TESTS_FAKE_AMBER_CONTROL_H
#define TESTS_FAKE_AMBER_CONTROL_H
#include "amber/amber_api.h"
#include <stdint.h>
#ifdef _WIN32
#define FAKE_CALL __cdecl
#define FAKE_EXPORT extern "C" __declspec(dllexport)
#else
#define FAKE_CALL
#define FAKE_EXPORT extern "C" __attribute__((visibility("default")))
#endif
struct FakeAmberState {
  uint32_t requested_version, requested_size;
  uint64_t capabilities;
  uint32_t max_switches;
  int32_t run_result;
  uint32_t run_mode; /* 0 complete, 1 fixed run_result */
  uint32_t run_call_count;
  uint32_t run_requests[32];
  char selected_core[64];
  char program_roms[4][256];
  char sound_roms[4][256];
  uint32_t switch_count;
  uint32_t switch_indices[32], switch_levels[32];
  uint32_t audio_capacity, audio_written;
  uint32_t reel_calls, coin_calls, percentage_calls, percentage_value;
  uint32_t reel_apply_mask, coin_channel_mask, coin_route_mask;
  uint32_t
      fail_operation; /* 1 negotiate, 2 table, 3 enumerate, 4 create, 5 caps, 6
                         init, 7 reels, 8 coins, 9 percent, 10 shutdown */
  uint32_t create_count, initialise_count, shutdown_count, destroy_count;
  char lifecycle[256];
};
FAKE_EXPORT void FAKE_CALL FakeAmberReset(void);
FAKE_EXPORT const FakeAmberState *FAKE_CALL FakeAmberGetState(void);
FAKE_EXPORT void FAKE_CALL FakeAmberSetRun(uint32_t mode, int32_t result);
FAKE_EXPORT void FAKE_CALL FakeAmberSetCapabilities(uint64_t bits);
FAKE_EXPORT void FAKE_CALL FakeAmberSetFailure(uint32_t operation);
#endif
