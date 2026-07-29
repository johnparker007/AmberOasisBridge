# Amber backend compatibility contract

The Amber backend is an adapter, not part of Fabric's public machine model. It receives a launch
request selected by backend and machine identifiers and loads the **exact absolute DLL path supplied by
the frontend**. It must not search relative directories, substitute `AmberBridge.dll`, choose
`AmberOasis.JPMSystem6.dll`, or infer a specific Amber platform.

Fabric first detects the provider-style API by the single `AmberGetApi` export. If it is absent,
Fabric deterministically detects the unchanged production/System 6 ABI using `Initialise`,
`Shutdown`, `Reset`, `Run`, `LoadROM`, `GetOutputSnapshotSize`, `GetOutputSnapshot`,
`TurnSwitchOn`, and `TurnSwitchOff`. A partially matching DLL is rejected with the exact missing
symbol, DLL path, adapter, and resolution phase. Amber does **not** need to be rebuilt and does not
need to export `AmberGetApi`; the compatibility adapter is private to `FabricRuntime`.

The production ABI is the existing `extern "C"`, native C calling-convention interface declared by
the maintained System 6 bridge. It is a DLL-owned singleton: `Initialise` acquires it and `Shutdown`
destroys it. ROM paths are borrowed only for the duration of `LoadROM`/`LoadSoundROM`; snapshots and
audio are copied into caller-owned buffers. Its `PA2_OutputSnapshot` structures use 4-byte packing.
Optional audio is detected only when both `GetAudioFormat` and `FillAudioFrames` exist. The adapter maps discovered capabilities to extensible Fabric
flags. It translates Fabric digital input and generic snapshot objects without exposing Amber
structures. In particular it must retain logical lamp state separately from brightness, preserve
signed reel positions, translate alpha attributes and seven-segment masks without reinterpretation,
and expose PCM16 interleaved frames using the negotiated format.

All native calls for an instance will be serialised. Create, initialise, reset, execution, shutdown,
and unload must have deterministic ordering, including partial-failure cleanup. Adapter errors are
copied into Fabric-owned boundary text. Exceptions, borrowed Amber pointers, and loader handles never
cross the C ABI.

Amber cycle counts are adapter-local. System 6 receives 8 MHz CPU-cycle budgets (125 ns per cycle),
with sub-cycle nanoseconds retained between calls. The flat `Run(UINT32)` return value is the native
CPU/emulator return value, not a documented progress count or status. Fabric therefore consumes the
requested budget after one call, accepts zero, and uses the return only for diagnostics. Requests
larger than `INT32_MAX` are split into bounded calls; provider-style API v2 progress semantics are
unchanged.

The maintained Amber sources under `src/Cores` and the current bridge remain compatibility assets.
They will be removed only after the external-DLL implementation reaches behavioural parity and its
integration tests pass. MAME will be a separate, out-of-process provider and is not implemented here.

## ABI and migration status

Typed ROM resources carry role, slot, and path and are independently ordered for program and sound inputs. The original flat list remains as an append-only ABI v1 compatibility field. Character and segment payloads are fixed-capacity inline arrays in caller-owned snapshot entries; no Amber-owned pointer crosses the ABI.

Negotiation uses the encoded `AMBER_API_VERSION_2` value (`0x00020000`), never the ordinal
integer `2`. Program and sound slots are independently required to be contiguous from slot zero;
sparse slots are rejected rather than silently compressed. Paths are copied into provider-owned
strings before the session is returned.

The original PR #8 launch prefix is the minimum accepted ABI v1 request size. Fabric copies only the
bytes covered by `struct_size`, treats missing append-only typed-ROM fields as absent, and never reads
beyond the caller-declared prefix. New callers should use `sizeof(FabricLaunchRequest)`.

Amber API v2 has a single coherent output-snapshot capability. The adapter intentionally maps that
bit to Fabric lamps, reels, character displays, and segment displays because those are the four fixed
families present in `AmberOutputSnapshotV1`.

`FakeAmberApiV2` is only a provider-style contract test implementation; it is not the required
production ABI. `FakeAmberLegacy` deliberately omits `AmberGetApi` and exercises the production
adapter, including ROM/configuration translation, lifecycle, output, 44.1 kHz stereo PCM16 audio,
timing remainders, repeated loading, and unloading.

## Using an existing Amber DLL

Keep the backend identifier `amber-api-v2` for Oasis compatibility and set `backend_path` to the
absolute path of the existing Amber/System 6 DLL. Supply the two program ROMs as typed
`FABRIC_ROM_ROLE_PROGRAM` slots 0 and 1, and sound ROMs (when used) as contiguous
`FABRIC_ROM_ROLE_SOUND` slots. Fabric loads that exact path and resolves dependencies from the DLL
directory and standard controlled Windows loader directories.

Proprietary DLLs and ROMs are never committed. For an opt-in local smoke test, set
`FABRIC_REAL_AMBER_DLL` to the absolute production DLL path and provide ROM paths through the local
test harness (program slots 0/1, plus optional sound slots), then run `AmberLegacyBackendTests` or
the equivalent application lifecycle: create, initialise, reset, advance, snapshot, audio query,
shutdown, and destroy. CI uses `FakeAmberLegacy` and requires no proprietary files. An absent
`FABRIC_REAL_AMBER_DLL` is intentionally treated as a skipped local integration test.

Both provider-style API v2 DLLs and direct production core DLLs are supported. `src/Cores` and the
old bridge remain compatibility assets; MAME remains deferred.

## Production diagnostics and pump contracts

Set `FABRIC_AMBER_TRACE=1` before starting Oasis to emit bounded production-adapter diagnostics to
the Windows debugger. Tracing covers adapter selection, initialise, ROM loading, configuration,
the first reset, the first eight `Run` calls, the first snapshot, audio-format queries, the first
eight audio reads, and shutdown. Normal execution emits no trace, and paths are logged without ROM
contents or audio samples.

The snapshot ABI is the existing pack-4 `PA2_OutputSnapshot` (24,812 bytes). Production System 6
supplies at least 512 matrix lamps, eight reels, one segmented alpha display, and 256 LED-plane
entries. Fabric maps these to the same stable 512/8/1/16 output shape used by the established Amber
bridge; it does not expose the unrelated 40-entry `LedDisplays` array as 40 Fabric displays.

`GetAudioFormat` returns the size of the 24-byte `PA2_AudioFormat`. `FillAudioFrames` receives a
frame capacity, writes interleaved stereo samples (`frames * channels` samples), and returns frames,
not samples. Fabric validates the stereo extent and the returned frame count before reporting it.
