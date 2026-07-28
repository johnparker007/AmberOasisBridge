# Amber Bridge API v2 design contract

> **Status: implemented.** API v2 is active in `AmberGetApi`; `AMBER_API_VERSION_CURRENT` is v2, while v1 remains supported unchanged.

## Public declaration and negotiation

The complete production C declaration is `include/amber/amber_api.h`. Semantic API 2 is encoded as `AMBER_API_VERSION_2 == 0x00020000u`, following v1's major-in-bits-16..31 convention. A client requests exactly that value and passes a zero-filled `AmberApiV2` whose `struct_size` is `sizeof(AmberApiV2)`.

V2 repeats (does not embed) the complete 80-byte `AmberApiV1` prefix, followed by eight functions. This lets a v2 client use lifecycle calls from one 144-byte table while leaving the type and bytes of `AmberApiV1` untouched. `AmberGetApi` now:

1. reject unknown versions with `AMBER_UNSUPPORTED_VERSION`;
2. require `api != NULL` and `api_size >= 144` for v2 (`AMBER_BUFFER_TOO_SMALL` otherwise);
3. construct a zero-filled table, set `struct_size=144` and `api_version=0x00020000`, then copy exactly 144 bytes;
4. leave bytes beyond 144 in a larger caller buffer untouched.

A v1 request continues to require/copy exactly the v1 table and never receives v2. Function pointers in a successfully negotiated v2 table are non-null. Per-core absence is reported by capabilities and `AMBER_NOT_SUPPORTED`, never by null pointers. No separate exported v2 functions are proposed.

## Common structure rules

Every aggregate exchanged by a v2-only function begins with `struct_size` and an independently versioned `version`. Callers zero the whole structure, set both fields, and use the exact v1 size. Version 1 functions require at least the documented size, read/write only that size, ignore a larger tail, and reject a short structure with `AMBER_BUFFER_TOO_SMALL` or an unknown semantic version with `AMBER_INVALID_ARGUMENT`. Reserved fields must be zero on input and are written as zero. Element records are fixed-layout members of their owning versioned aggregate.

All output storage is caller-owned and remains valid only according to caller storage lifetime. Calls copy data; the bridge retains no pointer. Input configuration is copied during the call. There are no callbacks or bridge-owned output buffers.

## Operations

* `GetCapabilities` reports the instance core's feature mask and limits; it does not require initialisation.
* `SetSwitchState` accepts switch indices 0..255 and `is_on` exactly 0 or 1. It is persistent-level, idempotent, and includes coin switches when a project models a coin as a matrix switch. The distinct Mars coin accepter pulse operation is not exposed in v2 (see deferred work).
* `GetOutputSnapshot` returns one fixed-capacity, coherent copy. No size-query call is needed. Counts bound meaningful zero-based prefixes; all unavailable entries/counts are zero.
* `GetAudioFormat` reports signed 16-bit PCM; `FillAudioFrames` receives capacity and returns production in **frames**, never bytes or samples. `frames_written` is mandatory and is set to zero before handle, state, capacity, or destination validation. At zero capacity the sample pointer may be null and the call succeeds with zero; at nonzero capacity it must be non-null and aligned for `int16_t`. Before writing, the bridge verifies that `frame_capacity * channels` and its byte size are representable in `uint32_t` and `size_t`; overflow is `AMBER_INVALID_RANGE`. Partial fills are successful and unwritten capacity is left unchanged.
* `ConfigureReels` and `ConfigureCoins` validate the complete aggregate before changing anything. Apply masks select entries; unselected entries retain current/default configuration.
* `SetPercentageSwitch` accepts the raw 4-bit switch value 0..15, not a human percentage.

## Errors

V1 result declarations remain unchanged. Proposed values are append-only: `AMBER_NOT_SUPPORTED=11` for a feature absent from the selected core, `AMBER_INVALID_RANGE=12` for a validly shaped scalar/index outside its domain, and `AMBER_MALFORMED_CONFIGURATION=13` for inconsistent aggregate content or nonzero reserved input. Existing `AMBER_INVALID_ARGUMENT`, `AMBER_INVALID_STATE`, and `AMBER_BUFFER_TOO_SMALL` retain their meanings. Audio silence/idle is successful, not an error. Every failure sets text retrievable with the v1-prefix `GetLastError`; failed aggregate configuration makes no changes.

## x64 natural layouts

No packing directive is used. Fixed integers retain normal C alignment and function/data pointers are 8 bytes.

| Type | Size | Important offsets |
|---|---:|---|
| `AmberCapabilitiesV1` | 32 | version 4, feature_bits 8, max_switches 16 |
| `AmberLampStateV1` | 8 | brightness 4 |
| `AmberAlphaDisplayStateV1` | 52 | dot_comma 32, brightness 48 |
| `AmberSevenSegmentStateV1` | 8 | brightness 4 |
| `AmberOutputSnapshotV1` | 4592 | arrays: lamps 40, reels 4136, alpha 4168, seven-segment 4272 |
| `AmberAudioFormatV1` | 32 | sample_rate 8, reserved 24 |
| `AmberReelConfigV1` / aggregate | 24 / 208 | aggregate entries 16 |
| `AmberCoinChannelConfigV1` | 20 | lockout_invert 12 |
| `AmberCoinRouteConfigV1` | 32 | counter_in 8, level 24 |
| `AmberCoinConfigurationV1` | 408 | channels 16, routes 136, lockout base 392, value 396, flags 400, reserved 404 |
| `AmberApiV1` / `AmberApiV2` | 80 / 144 | v1 first pointer 8, v2 GetCapabilities 80, SetPercentageSwitch 136 |

The contract test compiles these assertions as C11 and C++17 and checks the prefix offsets. C++ exceptions must never cross any table function boundary.

## Deferred functionality

Mars `CoinIn`/`MarsCoinIn`, direct diagnostic switch aliases, meters/tubes/hoppers, coin-entry lamps, filament RGB, dot displays, save states, and other PA2-wide snapshot categories are deliberately absent. They are either not required by AB2, not populated for System 6, or need further frontend semantics. V2 does not promise runtime support until a later implementation change explicitly negotiates it.
