# Future bridge design constraints

This records constraints; it intentionally does **not** define or implement an ABI.

## Capability and instance policy

The future bridge must preserve paths to all meaningful capabilities in the complete matrix, not merely a current Unity subset: lifecycle, ROM/sound loading, stepping, switches/configuration, coins/EDC, tubes/hoppers/meters, persistence, snapshots, audio, displays, platform diagnostics, and legacy aliases where compatibility demands them. Optional or rarely used functions require capability discovery or platform extensions rather than deletion.

**Initial bridge target: one active emulator instance per process.** A later ABI may use an opaque context for cleanliness, but multi-instance safety is not required and must not drive core refactoring now. If multiple machines are needed, separate processes are acceptable.

## Presentation and dependency direction

Expose audio samples rather than playback, lamp state rather than rendering, reel positions rather than graphics, logical input rather than capture, and characters/segments rather than UI.

```text
Frontend
    ↓
Future Amber bridge API
    ↓
Core adapter
    ↓
Maintained emulator core
```

The emulator must not depend on Unity. Host platform playback, diagnostics destinations, file dialogs, and windows should remain above the data boundary.

## Compatibility constraints

* Define an ABI/version query independently of emulator marketing/version floats.
* Give every extensible structure a byte size and version; document packing and reserved-zero fields.
* Prefer fixed-width integers and byte flags. Avoid `bool`, `long`, `size_t`, STL/C++ classes, and compiler-owned memory.
* State one calling convention and C linkage on every platform; test undecorated/decorated Win32 and x64 exports.
* Make caller/callee allocation, buffer capacity, string encoding/termination, validity duration, and partial-write behavior explicit.
* Define stable status/error reporting; existing `void`, zero/success conventions, and file errors are inconsistent or undocumented.
* Provide capability discovery and optional function/version rules so MPU5 diagnostics and Epoch/JPM features survive without pretending uniformity.
* Keep platform-specific extensions namespaced/versioned rather than forcing false common semantics.
* Treat DLL names and loader search policy as versioned deployment inputs.
* Serialize access until evidence supports concurrency; snapshots are preferable to many potentially torn live getters.
* Define reset, repeat-initialise, shutdown, and unload contracts around existing global state before wrapping them.

## Separate versus monolithic deployment

Today the solution builds three separate DLLs: `AmberOasis.Epoch.dll`, `AmberOasis.JPMSystem6.dll`, and `AmberOasis.MPU5.dll`. One possible, undecided future arrangement is:

```text
AmberBridge.dll
AmberOasis.Epoch.dll
AmberOasis.JPMSystem6.dll
AmberOasis.MPU5.dll
```

A monolith is difficult because JPM and MPU5 compile duplicate Musashi external symbols, several cores use identical generic exported names, duplicated filament/disassembler implementations exist, and board/audio state is singleton-like. Epoch also has generic namespace symbols. Symbol visibility/renaming and singleton interactions would require deliberate work; no such work belongs in this inventory.

## Smoke-host feasibility and next task

A loader-only host can safely `LoadLibrary` each DLL, enumerate/resolve the source-derived export list, query `GetDLLVersion`, and query packet size functions without ROMs. Calling `Initialise`, `Reset`, audio, or shutdown may allocate devices, open platform audio, or establish hidden state; safe ordering and no-ROM behavior are not documented. Therefore the recommended next task is **Create a native export and lifecycle smoke-test host**, initially limiting lifecycle calls behind explicit tests/known fixtures and first verifying PE exports on Win32/x64. Its evidence should precede final versioned ABI design.

## Unknowns requiring developer evidence

* actual PE export names/decorations and runtime imports on Win32/x64;
* supported ROM combinations, path encoding, working directory, and error meanings;
* whether repeated initialise/reset/shutdown and unload-without-shutdown are supported;
* thread affinity/concurrency requirements, especially DirectSound;
* ownership/lifetime of `GetEDCString` and persistence format compatibility;
* intended semantics of placeholder/no-op persistence/update functions;
* which legacy aliases must be binary-compatible versus capability-compatible.
