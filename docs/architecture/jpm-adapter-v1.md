# JPM System 6 adapter v1

## Deterministic loading

The bridge obtains its own module handle with `GetModuleHandleExW(FROM_ADDRESS | UNCHANGED_REFCOUNT)`, dynamically grows a `GetModuleFileNameW` buffer, removes the `AmberBridge.dll` filename, and constructs the absolute path `<bridge directory>\AmberOasis.JPMSystem6.dll`. It loads only that path using `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32`; target-local dependencies and Windows system dependencies are therefore available, while the working directory, PATH, host executable directory, user-added DLL directories, and caller-provided locations are excluded. The diagnostic independently constructs the absolute bridge path from its executable directory and applies the same dependency-search restriction.

Load failures report the attempted path, numeric Windows code, and `FormatMessageW` text. Error 126 can mean either that the target is absent or that a transitive dependency is unavailable. Debug JPM builds require the matching Debug Visual C++ runtime; Release deployment must contain compatible Release-built bridge/core binaries and their supported runtime. Run `tools/verify_bridge_v01.ps1` in a VS2022/v143 developer environment to build and inspect both x64 configurations.

## Exact maintained declarations

The declarations below are from `src/Cores/JPMSystem6/Interface.h`; `UINT8`, `UINT32`, and `INT32` are respectively `uint8_t`, `uint32_t`, and `int32_t` in `PA2CoreInterface.h`. The project sets `CallingConvention` to `Cdecl`, and the declarations are inside `extern "C"`.

| Export | Exact source declaration | Observed semantics |
|---|---|---|
| `GetDLLVersion` | `float GetDLLVersion(void)` | version by value |
| `Initialise` | `UINT8 Initialise(void)` | nonzero success; creates/resets the singleton |
| `Shutdown` | `UINT8 Shutdown(void)` | nonzero when it deletes an existing singleton |
| `Reset` | `void Reset(void)` | no result |
| `Run` | `INT32 Run(UINT32 Cycles)` | signed 32-bit result, unsigned 32-bit argument |
| `LoadROM` | `UINT32 LoadROM(UINT8*, UINT8*, UINT8*, UINT8*)` | nonzero success |
| `LoadSoundROM` | `UINT32 LoadSoundROM(UINT8*, UINT8*, UINT8*, UINT8*)` | nonzero success |

All seven are resolved once by their verified undecorated names, never ordinals. No Oasis Editor/Unity import wrapper is present in this repository; the source inventory likewise records external frontend usage as unavailable.

Although the ROM API declares mutable `UINT8*`, inspection of `Interface.cpp` shows paths are passed to `fopen_s`/read helpers and are not modified. The Amber ABI therefore correctly retains `const char*`. The adapter nevertheless creates private mutable, NUL-terminated byte copies for all non-null paths before calling JPM, so caller storage can never be modified even if implementation behavior changes.

V1 deliberately excludes snapshots, lamps, reels, displays, audio, inputs, and persistence.
