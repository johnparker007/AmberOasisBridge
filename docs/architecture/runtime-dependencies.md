# Runtime dependencies

## Dependency matrix

| Dependency | Core | Build/runtime; linkage | Reference and use | Classification / boundary / portability |
|---|---|---|---|---|
| MSVC v143, Windows SDK 10 | all | Build-time | `AmberOasis.*.vcxproj` | Build infrastructure; Windows-only build definitions |
| MSVC C/C++ runtime | Epoch, JPM: DLL runtime default/explicit `/MD`; MPU5 explicit static `/MT` | Runtime for `/MD`; static for `/MT` | project `RuntimeLibrary` settings | Build/runtime; deployment differs by core; exact imported CRT DLLs Unknown without PE inspection |
| Win32 API | Epoch, JPM | compile and runtime system DLL imports | `stdafx.h`; Epoch sound headers | Legacy/host integration and basic platform types; exact imports Unknown |
| DirectSound (`dsound.h`) and wave formats (`MMreg.h`) | Epoch | build plus runtime system/legacy API | `FruitSound.h`, `SoundMain.h/.cpp` | Frontend/presentation-related playback coupling; emulated sound generation is essential, platform playback is a future separation candidate |
| `dsound.lib` / legacy DirectX SDK | Epoch | **Unknown**: headers are present but project has no explicit additional library directory/input | sound implementation | Legacy; June 2010 SDK requirement not evidenced by current project settings; developer/Windows link verification required |
| C/C++ standard library | all | static/dynamic follows runtime choice | containers, smart pointers, algorithms, file streams | Runtime/build infrastructure; compiler ABI internal except allocation/lifetime behavior |
| C stdio/filesystem and host paths | all | runtime | ROM/sound loaders, `SaveRAM`/`LoadRAM`, state and debug files | Runtime only; emulation data essential, file selection/workflow frontend responsibility; encoding and error contract Unknown |
| Musashi 68K implementation | JPM, MPU5 | compiled statically into each DLL | `m68k*.cpp/.h` | Emulation-essential; duplicated source/symbols hinder monolith |
| H8 implementation | Epoch | compiled statically | `H83002*`, `H8Peripherals.cpp` | Emulation-essential |
| device/audio implementations | all | compiled statically | core project source lists | Emulation-essential except direct host playback/logging portions |

No `AdditionalDependencies`, `AdditionalLibraryDirectories`, or delay-load setting was found in maintained projects. No external codec include was found. This does **not** prove absence of default Windows libraries or runtime imports; binary inspection is required.

## Environment and data assumptions

The ABI accepts up to four mutable byte-string paths for program ROM and, separately, sound ROM. Save/load takes byte-string paths; folder and filename setters imply core-composed paths. Null handling varies by wrapper, and relative paths consequently depend on the process working directory. Required ROM count, permitted absent names, path encoding, maximum sizes, state-file format compatibility, permissions, and failure diagnostics are **Unknown** and require known game configurations or developer input. Debug `FILE*` globals may create additional working-directory files depending on code paths.

## Desired direction (not implemented)

ROM decoding, CPUs, devices, and audio **generation** are emulation-essential. DirectSound playback, Win32 input/windowing (none evidenced as exported), debug-file policy, and file-selection workflows belong at the host boundary. This task removes nothing.
