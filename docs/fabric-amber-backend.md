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

Amber cycle counts are adapter-local. The production System 6 adapter reproduces the direct pump:
elapsed time is accumulated into 1 ms ticks, each complete tick makes one `Run(8000)` call, and a
single advance executes at most three ticks. Excess complete delayed ticks are discarded while the
sub-millisecond remainder is retained. The flat `Run(UINT32)` return value is native observational
information, not a progress count or status; zero is valid. Provider-style API v2 progress semantics
are unchanged.

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

The append-only launch-request diagnostic callback is the primary production-adapter diagnostic
path. It receives bounded `amber.production` events for adapter/library selection, initialise, ROM
loads, resets, effective configuration, the first 32 native runs, the first 16 snapshots and audio
reads, audio format, and shutdown. Callback failures are ignored. If no callback is supplied, set
`FABRIC_AMBER_TRACE=1` before starting Oasis to emit the same lifecycle evidence to
an append-only UTF-8 file and, on Windows, the debugger. By default the authoritative file is
`%TEMP%\fabric-amber-<pid>.log`; set `FABRIC_AMBER_TRACE_FILE` to an absolute path to override it.
Every line is serialized and flushed immediately. Tracing covers runtime/module identity, session
requests, adapter selection, initialise, ROM slot filenames, configuration, the first resets, the
first 16 `Run` calls, the first eight activity summaries, audio-format queries, the first 16 audio
summaries, shutdown, and destruction. Normal execution creates no file, and traces never contain ROM
contents or raw audio samples. Native `OutputDebugString` messages may not appear with managed-only
debugging, so use the file as the authoritative output.

Startup performs initialise, program and sound ROM loads, then uses the normal reset path. The
production reset order is native `Reset`, retained reel configuration, retained coin channels,
lockout and selected routes, then retained percentage. The same application method is used after ROM
loading and after every reset; only selected masks are replayed, and disabled reels are not given
invented settings.

For PowerShell, launch Oasis from the same environment:

```powershell
$env:FABRIC_AMBER_TRACE = "1"
$env:FABRIC_AMBER_TRACE_FILE = "$env:TEMP\fabric-amber-oasis.log"
```

For a Visual Studio project debug profile, add environment entries
`FABRIC_AMBER_TRACE=1` and `FABRIC_AMBER_TRACE_FILE=%TEMP%\fabric-amber-oasis.log`
(the native Windows environment expansion syntax used by the Debugging property page). Before a
production test, remove stale `FabricRuntime.dll` copies and confirm the trace's
`Fabric runtime module:` line names the newly copied DLL.

The snapshot ABI is the existing pack-4 `PA2_OutputSnapshot` (24,812 bytes). Production System 6
supplies at least 512 matrix lamps, eight reels, one segmented alpha display, and 256 LED-plane
entries. Fabric maps these to the same stable 512/8/1/16 output shape used by the established Amber
bridge; it does not expose the unrelated 40-entry `LedDisplays` array as 40 Fabric displays.

`GetAudioFormat` returns the size of the 24-byte `PA2_AudioFormat`. `FillAudioFrames` receives a
frame capacity, writes interleaved mono or stereo samples (`frames * channels` samples), and returns frames,
not samples. Fabric validates the stereo extent and the returned frame count before reporting it.

### Production audio timing and units

The production core has no PCM queue. `Run(8000)` executes one millisecond of the machine and may
issue sound commands, but PCM is generated on demand later when `FillAudioFrames` is called. The
direct backend calls that export for the 48 frames corresponding to each 48 kHz tick. Previously,
Fabric passed the frontend's entire read capacity to this demand-driven export. A 96-frame capacity
therefore advanced sample playback by 2 ms even when only one 1 ms native tick had executed. The
returned count was correctly a frame count—the regression was an unbounded time entitlement, not a
stereo sample/count conversion.

Fabric now accrues a session-owned frame entitlement from executed ticks. At 48 kHz a tick earns 48
frames; at 44.1 kHz successive ticks use a fractional accumulator and earn 44 or 45 frames, totaling
441 frames per 10 ms. Catch-up earns frames once for each native tick actually executed. Reads pass
only `min(requested_frames, available_frames)` to Amber and subtract only the frames Amber actually
returns, so partial fills preserve the unused entitlement. The entitlement is capped at three ticks
to match the execution catch-up limit; excess delayed audio time is explicitly discarded rather than
allowing stale latency to grow without bound. Reset and shutdown discard the entitlement.

The complete pipeline is:

```text
FabricSessionAdvance(elapsed nanoseconds)
  -> fixed-tick accumulator
  -> zero to three Run(8000) calls
  -> sound commands/native playback state
  -> frame entitlement (sample_rate / 1000 per executed tick)
FabricSessionReadAudio(destination int16 elements, requested frame capacity)
  -> clamp to frame entitlement
  -> production FillAudioFrames(destination, permitted frames)
  -> production writes returned_frames * channels int16 elements
  -> Fabric returns frames_written (frames) to Oasis
  -> Oasis submits those interleaved PCM frames to its audio sink
```

| Quantity | Meaning | Unit |
|---|---|---|
| `elapsed_nanoseconds` | Host time offered to Fabric | nanoseconds |
| `executed_ticks` | Fixed native calls performed after clamping | 1 ms ticks |
| `request` | Argument to production `Run` | CPU cycles (8,000/tick) |
| `audio_frame_fraction_` | Fractional sample-rate numerator retained between ticks | frames × 1/1000 |
| `audio_frames_available_` | Bounded permission to demand-generate PCM | frames |
| `capacity` / `permitted` | Public/request-clamped audio buffer extent | frames |
| `written` | Value returned by production Amber and Fabric | frames |
| `audio_channel_count_` | Interleaved elements in one frame | channels |
| `written * audio_channel_count_` | Initialized destination extent | `int16_t` elements/samples |
| `... * sizeof(int16_t)` | Initialized destination byte extent | bytes |

Diagnostics distinguish timing entitlement from actual PCM generation: `AudioBudget` reports earned
frames/samples and bounded depth after each of the first 32 runs; `AudioGenerate` reports frames and
interleaved samples actually produced by Amber; `AmberReadAudio` reports requested, permitted,
returned, and remaining frames; and `AudioQueueShutdown` reports discarded entitlement. No sample
contents are logged.
