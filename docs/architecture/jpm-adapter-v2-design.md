# JPM System 6 adapter mapping for API v2

## Exact maintained surface

`Interface.h`/`Interface.cpp` export C `__declspec(dllexport)` functions with the default C calling convention: `void TurnSwitchOn(UINT8)`, `void TurnSwitchOff(UINT8)`, `UINT32 GetOutputSnapshotSize(void)`, `UINT32 GetOutputSnapshot(void*, UINT32)`, `UINT32 GetAudioFormat(PA2_AudioFormat*, UINT32)`, `UINT32 FillAudioFrames(INT16*, UINT32)`, reel setters `void SetSteps/SetOptoStart/SetOptoEnd/SetOptoInvert(UINT8 ReelNum, UINT8 value)`, coin functions `SetCoinEnable(UINT8,UINT8)`, `SetCoinValue(UINT8,UINT8)`, `SetLockoutVal(UINT8,UINT8)`, `SetLockoutInvert(UINT8,UINT8)`, routing functions `SetEnable(UINT8,UINT8)`, `SetCounterIn/Out(UINT8,UINT32)`, `SetPortIndex/SetCoin/SetLevel/SetFullLevel(UINT8,UINT8)`, and optional-in-practice `void SetPercent(UINT8)`.

These are genuine maintained direct exports but are **not** exports of Amber Bridge. The eventual adapter will resolve them only after v2 negotiation is implemented.

## Inputs

`SwitchMatrix` contains `UINT8 Matrix[256]`; its setters directly assign 1 or 0 with no edge bookkeeping. A `UINT8` index therefore covers exactly 0..255, repeated calls are safe/idempotent, and reset calls `Switches.Init()` so levels clear. The CPU reads those levels during `Run`; mutation and execution have no locking, so they must not overlap. System 6 door/test helpers use the same matrix (including index 255). Some frontend coin buttons can use matrix switches, while Mars serial coin insertion uses `CoinIn` and is not equivalent or included.

V2 maps this to one `SetSwitchState(uint32_t,uint32_t)`, range-validates before narrowing, and permits it only while initialised. Configure after reset/before a run, or between completed `Run` calls.

## Reel configuration

The native arrays have eight entries and setters guard `ReelNum < 8`. All parameters are `UINT8`; `Steps` is stored as `INT16`, opto fields as `INT16`. `SetSteps` ignores zero, so v2 accepts steps 1..255; opto positions accept 0..255. There is no native disabled flag. `enabled=0` is an explicit adapter state: the implementation shall exclude that reel from drive/output configuration; it does not map to a legacy setter. `enabled` and inversion must be 0/1.

The native reset path reinitialises runtime and defaults, so v2 configuration is adapter-owned and must be re-applied after every successful `Reset`. Disabled state is retained and reapplied too, but disabled reels keep their stable snapshot index and observable native position; disabling suppresses property application only. This supports the established `Initialise; Reset; Configure; Run` ordering. A single validated aggregate is atomic from the v2 caller's perspective; apply-mask omissions preserve defaults/current adapter settings.

## Coin configuration

There are two real index spaces, deliberately represented separately:

* Mars has six channels (`NUMCOINS=6`). `SetCoinEnable` and `SetCoinValue` silently ignore indices >=6. Values are bytes; enable is normalized by v2 to 0/1. `SetLockoutInvert` is per channel. Native `SetLockoutVal(index,data)` is not per coin: it applies an eight-bit port value to channels whose separately-held lockout drive falls in `index..index+7`. Because the requested direct surface omits `SetLockoutDrive`, v2 preserves the observable call as aggregate `lockout_port_base/value` bytes rather than pretending it is a channel value.
* Tube/solenoid routing has eight rows (`NUMSOLENOIDS=8`). `SetEnable`, counters, port index, coin code, level and full level configure a row. Port index is 0..7 (legacy invalid values collapse to zero; v2 rejects them). Native setters take byte levels even though storage/getters are 32-bit, so v2 restricts `level/full_level` and `coin_code` to 0..255; counters retain full uint32 range.

Masks make partial configuration meaningful without ambiguity. Counts are fixed by the arrays. Every selected entry's embedded index must equal its array index; booleans are 0/1; masks may use only six/eight low bits. `configuration_flags` may contain only `AMBER_COIN_CONFIG_APPLY_LOCKOUT_PORT`. With that flag clear, `lockout_port_base/value` are ignored and the bridge must not call `SetLockoutVal`; with it set, both must be 0..255 and the bridge applies `SetLockoutVal(base,value)`. Unknown flags, nonzero `reserved`, or invalid selected fields produce `AMBER_MALFORMED_CONFIGURATION`. Channel and route masks are independent of this flag, and every unselected channel, route, or lockout-port state remains unchanged. Validate the complete request before invoking any native setter, ensuring no partial failure. Configuration is valid after initialisation, conventionally after reset and before first run, persists between runs, and is re-applied by the adapter after reset.

## Percentage switch

`SetPercent(UINT8)` tests only bits 0..3 and writes matrix switches 8..11. It is a raw switch nibble, not a literal percentage; high bits were silently ignored by the old function. V2 accepts only 0..15, applies after reset/before run, and adapter-retains/reapplies it after subsequent resets. If the symbol is unavailable the capability bit is clear and the call returns `AMBER_NOT_SUPPORTED`.

## Audio

The maintained format export is callable without a board and returns version 1, 48000 Hz, two channels, 16-bit signed PCM. `FillAudioFrames(INT16*,UINT32 FramesRequired)` requires a board and non-null buffer. It always fills exactly the requested number of stereo frames, duplicates the mono sample L/R, and emits zeros while idle; it advances/consumes sample playback state as it generates. It holds `AudioLock`, although the broader bridge contract intentionally does not promise concurrent instance calls. There is no queue, underrun, drain, or drop concept: generation is demand-driven and sound commands originate during `Run`. Reset resets sound playback state; clients need not drain.

V2 format is 48000/2/PCM_S16/interleaved. `frames_written` is always non-null and is zeroed before any other processing. The destination has `alignof(int16_t)` alignment and space for `frame_capacity * 2` samples. Zero capacity permits a null destination and succeeds; nonzero capacity requires it. Sample-count and byte-count multiplication are checked for overflow before any write. Partial fills are successful, and unused destination capacity is not cleared. Current System 6 normally returns the full capacity. Format is available after initialisation through the instance API; frame copying is valid after initialisation and after reset/run, and consumes generated frames.

## Former snapshot discrepancies

The PA2 snapshot is packed to 4 bytes and contains broad cross-core categories. System 6 populates matrix lamps, a 512-element LED/segment plane, reels, alpha, a legacy count of 40 LED displays, coin mech, meters, tubes, DIPs and hoppers, while leaving many counts zero. V2 does not copy it blindly: it uses natural ABI alignment, fixed integers/Q16.16, only AB2 visual categories, corrects the duplicated two-alpha-display claim, and reports the 16 displays addressable through the maintained 256-cell plane at its 16-cell stride. Its deterministic brightness is the maximum of each display's eight visible cells, not the legacy `GetSegBright(display)` expression.
