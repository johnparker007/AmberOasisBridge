# Lifecycle and dataflow

## Supported model

**One active emulator instance per process.** The exported interfaces have no context parameter. Epoch and MPU5 lazily create a namespace-scope `unique_ptr` in `EnsureCore`; JPM System 6 uses the file-scope `sys6_board` pointer (`src/Cores/*/Interface.cpp`). No synchronization is visible: serialize all lifecycle, stepping, input, and retrieval calls.

| Phase | Epoch | JPM System 6 | MPU5 |
|---|---|---|---|
| DLL load | C++ statics only; no `DllMain` found | same | same |
| initialise | Explicit `Initialise`; lazy object then `Init` | Explicit; allocates/initialises global board | Explicit; lazy object then member `Initialise` |
| ROM/game | four path pointers; separate sound ROM; Epoch flash mode | four program paths and four sound paths | four program and four sound paths |
| configuration | switches, DIP, stake/prize/percent, reels, mechs, meters/hoppers | same plus coin tubes | same plus PIC/SEC, characteriser, reel profile and diagnostics |
| reset | explicit `UINT8 Reset` | explicit `void Reset` | explicit `UINT8 Reset` |
| step | explicit `Run(Cycles)` | same | same, with timing statistics |
| retrieve | packet snapshot plus transitional getters | packet snapshot, narrower legacy getter surface | packet snapshot plus richest transitional/diagnostic surface |
| audio | frontend-pull PCM format/frames | same | same |
| persistence | path-based RAM and parameterless state; `ClearRAM` | RAM/state calls | RAM/state calls described as placeholders in header |
| shutdown | explicit; destroys object | explicit global shutdown | explicit member shutdown then destroys object |
| unload | implicit C++ teardown; safety without `Shutdown` Unknown | raw pointer cleanup behavior Unknown | implicit `unique_ptr` teardown; platform resources should first receive `Shutdown` |

Initialise/reset repeatability, call ordering, partial-load recovery, and unload without shutdown are **Unknown** because headers specify no contract.

## Dataflow classification

| Domain | Submission / production | Classification | Evidence |
|---|---|---|---|
| switches/buttons/doors | setters mutate matrix/cabinet state; `ReadSwitch` returns state | Emulated hardware input | `Interface.h`, switch methods in each `Interface.cpp` |
| lamps/segments/alpha/reels/status | snapshot and scalar getters | Emulated hardware state presented by frontend | `PA2_OutputSnapshot`; getter definitions |
| meters, hoppers, coin tubes/cashbox | configuration, coin events, counters and snapshot | Emulated electromechanical state | Interface hopper/mech APIs; `CashBox`, `Meters`, `Hoppers` |
| sound | ROM load; core produces interleaved PCM frames | Generated data; playback is frontend work | `GetAudioFormat`, `FillAudioFrames` |
| serial/EDC | mech configuration; `GetEDCString`; MPU5 queued pop | Hardware/diagnostic state with host buffer boundary | `EDC.*`, interface functions |
| RTC | device-owned Epoch RTC; other presence not exposed directly | Emulated state; public control Unknown | `Epoch/DeviceEpochRTC.*` |
| save data | filename/folder and RAM/state operations | Host file integration around emulator state | persistence exports and core save/load implementations |
| diagnostics | version/status; MPU5 timing and trace controls | Diagnostic data/host logging coupling | MPU5 timing APIs |

No rendering API is exported. File selection occurs outside the DLL, but the DLL opens caller-selected paths. Snapshot arrays are copied into a caller buffer; legacy getters read shared live state and can be inconsistent if interleaved with `Run`.
