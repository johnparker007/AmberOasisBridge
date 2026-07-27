# Frontend dependency audit

The maintained source, solution, and project metadata under `src/`,
`solutions/`, and `include/` were inspected for DirectX and legacy multimedia
headers, APIs, libraries, paths, delay-load settings, and linker pragmas.

| Core | DirectX project paths | DirectX headers | DirectX libraries | DirectX runtime DLL imports | Frontend API calls | Result |
| --- | --- | --- | --- | --- | --- | --- |
| Epoch | None | `dsound.h` occurs only in `FruitSound.h`, which is not a project item | None | Not inspected; Windows binaries/tools unavailable | `FruitSound.cpp` contains DirectSound buffer/playback calls, but neither it nor `FruitSoundYMZ280.cpp` is compiled by the maintained project | The built core has no source- or linker-declared DirectX dependency; retained sound code generates PCM frames for its caller |
| JPM System 6 | Removed June 2010 SDK `Include` and `Lib\x86`/`Lib\x64` paths from all four configurations | None | None | Not inspected; Windows binaries/tools unavailable | None | Stale metadata removed; sound emulation generates PCM frames for its caller |
| MPU5 | None | None | None | Not inspected; Windows binaries/tools unavailable | None | No DirectX dependency found; sound emulation generates PCM frames for its caller |

The Epoch `FruitSound` files are historical frontend implementation files still
present in the maintained source tree. Their DirectSound declarations and calls
are genuine, not mere name matches, but the maintained Epoch project neither
lists nor compiles those files. The compiled `SoundMain` implementation instead
mixes emulated YMZ sound into caller-provided PCM buffers. JPM System 6 and MPU5
likewise expose generated PCM data and do not directly play it.

No maintained project specifies DirectX libraries, delay-loaded DLLs, multimedia
COM setup, window creation, keyboard polling, rendering, `winmm`, `PlaySound`,
`waveOut`, or `timeGetTime`. Standard `windows.h` use remains and was not treated
as DirectX. The only absolute developer-machine paths found were the removed JPM
DirectX SDK paths.

`$(SolutionDir)BuildSupport\AmberRuntimeDeps.props` does not exist, is mentioned
nowhere except the former JPM import and build documentation, and could not have
contributed properties to a repository-only build. The unused conditional
import was therefore removed; no replacement property sheet was created.

This audit made no source or API changes. Debug x64 and Release x64 clean builds,
and import-table checks for all three DLLs, remain unverified because VS2022
MSBuild and Windows PE inspection tools were unavailable in the audit
environment. In particular, no claim about the generated DLL import tables is
made until those checks run on Windows. Project and compiled-source evidence no
longer requires the June 2010 DirectX SDK.
