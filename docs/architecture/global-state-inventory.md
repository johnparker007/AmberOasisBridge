# Global-state inventory

## Significant process-wide state

This is a risk-oriented inventory, not a claim that every class member is global. Most machine state is aggregated in the single board object; Musashi and caches add independent file/static state.

| Core | Symbol / file / type | Class | Init/reset/shutdown evidence | Supported-model and reload impact | Future bridge / monolithic concern |
|---|---|---|---|---|---|
| Epoch | `g_Core`, `Interface.cpp`, `unique_ptr<BoardEpoch>` | Mutable emulator state | lazy `EnsureCore`; reset calls `PowerOnReset`; shutdown resets pointer | serial one-machine operation; stale state if caller omits reset; destruction on unload expected but order Unknown | bridge must serialize and define lifecycle |
| Epoch | `g_FlashROMMode`, `Interface.cpp`, `UINT8` | Mutable host/config state | static zero; setter; **not cleared by Shutdown** | configuration can survive shutdown/reinitialisation while DLL remains loaded | hidden lifecycle state |
| Epoch | `A`, `B`, `C`, `version`, `Meters.cpp` | Mutable emulator/protocol state | static initialization; reset/shutdown behavior Unknown | test/reload isolation Unknown | generic external names collide in monolith |
| Epoch | `DbugFile`, `H8Peripherals.cpp`, `FILE*` | Mutable host integration | open/close behavior requires runtime path audit; reset behavior Unknown | possible file-handle/unload risk | host diagnostics should be controlled |
| Epoch | YMZ `diff_lookup`, `FruitSoundYMZ280.cpp` | Temporary shared cache | process static; initialization path in sound code | safe only if initialization repeatable; Unknown | duplicated implementation/collision risk |
| JPM System 6 | `sys6_board`, `Interface.cpp`, raw `JPMSystem6*` | Mutable emulator state | allocated by initialise path; reset/shutdown through exports | explicit shutdown is important; repeated initialise/leak behavior must be tested | raw singleton state |
| JPM System 6 | `IOFile`, `JPMSystem6.cpp`, `FILE*` | Mutable host integration | file lifecycle in board code; reset behavior Unknown | stale handle and unload risk if shutdown incomplete | collision/host coupling |
| JPM System 6 + MPU5 | Musashi globals (`m68ki_initial_cycles`, callback-data statics), `m68kcpu.cpp` | Mutable CPU state | library initialization/reset; shutdown behavior Unknown | compatible with one board when serialized; reset completeness needs tests | duplicate external symbols prevent straightforward monolithic linking |
| JPM System 6 + MPU5 | disassembler `g_initialized`, masks and 100-byte buffers, `m68kdasm.cpp` | Mutable cache | lazy/process static; no shutdown | non-reentrant diagnostic output | duplicate symbols and static buffers |
| JPM System 6 + MPU5 | filament lookup globals, `FilamentLamp.cpp` | Temporary shared cache | lazily keyed by params; no shutdown | shared mutable cache; serial access assumed | duplicate implementation names |
| MPU5 | `g_Core`, `Interface.cpp`, `unique_ptr<MPU5>` | Mutable emulator state | lazy; explicit member shutdown then reset | one machine supported; shutdown ordering relevant to reload | serialize access |

Large ROM/RAM, CPU and device arrays are predominantly members of `BoardEpoch`, `JPMSystem6`, and `MPU5` (see their headers), but because only one global board is reachable they remain process-wide in practice. Static lookup tables declared `const` are read-only shared data and are not lifecycle risks.

## Conclusions

**The currently supported model is one active emulator instance per process.** Lack of multi-instance support is not a defect for this stage. Relevant risks within that model are: reset may not cover wrapper configuration or independent library statics; repeated initialise/shutdown needs contract tests; raw/static file handles can complicate unload; callee-owned EDC strings can become stale; and no synchronization supports concurrent `Run`/getter calls. These are documentation and test concerns, not recommendations to remove globals now.
