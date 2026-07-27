# Export inventory

## Evidence and production mechanism

All 364 source declarations are inventoried in [the ABI matrix](current-abi-matrix.md). Epoch, JPM System 6, and MPU5 define `Interface_API` as `__declspec(dllexport)` while respectively building with `EPOCHCORE_EXPORTS`, `JPMSYSTEM6CORE_EXPORTS`, and `MPU5CORE_EXPORTS`; consumers receive `dllimport` (Epoch/MPU5 make the macro empty off Windows). Evidence: `src/Cores/*/Interface.h` and each `AmberOasis.*.vcxproj`. Each header places declarations in an `extern "C"` block. No explicit `__cdecl`, `__stdcall`, or other convention and no `.def`, `/EXPORT`, or `DelayLoadDLLs` entry was found. Thus linkage is C but convention is the MSVC default; x86 may decorate default-C names while x64 uses the unified Windows x64 convention. Actual spelling is **Unknown until binary inspection**.

| Core | Target | Source count | Mechanism |
|---|---|---:|---|
| Epoch | `AmberOasis.Epoch.dll` | 116 | `Interface_API`, `EPOCHCORE_EXPORTS` |
| JPM System 6 | `AmberOasis.JPMSystem6.dll` | 114 | `Interface_API`, `JPMSYSTEM6CORE_EXPORTS` |
| MPU5 | `AmberOasis.MPU5.dll` | 134 | `Interface_API`, `MPU5CORE_EXPORTS` |

These are complete declaration counts, not verified PE export-table counts. Every matching definition is recorded in the matrix.

## Binary-sensitive surface

`PA2CoreInterface.h` uses `<stdint.h>` aliases, `#pragma pack(push, 4)`, fixed arrays, `float`, and size/version fields. This avoids STL/classes and C++ `bool` in public packets, but callers must reproduce 4-byte packing and constants exactly. `void *` snapshots are caller allocated and guarded by byte size. Audio and `PopEDCMessage` are caller buffers with explicit sizes. ROM/path arguments are mutable `UINT8*` despite being used as strings; encoding, termination, retention, and maximum length are not specified. `GetEDCString` returns a mutable callee-owned pointer whose lifetime and capacity are unspecified. Floating return values require a compatible compiler ABI. There are no exported `long`, `size_t`, STL types, callbacks, or raw C++ classes.

Public constants describe packet maxima, status LEDs and PCM S16 audio. MPU5 additionally publishes `PA2_MPU5TimingStats`. Structure evolution is only partially protected: packets have `SizeBytes`/`Version`, but callers compiling a different header must honor the queried size and packing.

## Windows binary inspection

No built DLLs, `dumpbin`, `link.exe`, or usable PE inspection tool were available, so no generated binary was committed. Run:

```powershell
./tools/inspect_exports.ps1 build/bin/x64/Release/AmberOasis.Epoch.dll `
  build/bin/x64/Release/AmberOasis.JPMSystem6.dll `
  build/bin/x64/Release/AmberOasis.MPU5.dll
```

Compare the report to the matrix, on both Win32 and x64 when compatibility matters. A mismatch may indicate conditional compilation, linker elimination, or decoration and must be investigated rather than inferred.
