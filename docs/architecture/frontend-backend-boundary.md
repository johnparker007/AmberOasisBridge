# Frontend/backend responsibility boundary

## Target split

The backend loads/receives configuration and ROM data, executes emulation, accepts machine inputs, exposes hardware outputs/diagnostics, generates audio data, resets, persists state where supported, and reports failures. The frontend captures keyboard/mouse/controller/touch, selects files, plays audio, renders reels/lamps/displays/UI, owns windows/frame presentation, and may use separate processes for multiple machines.

| Current surface/subsystem | Placement | Reason / evidence |
|---|---|---|
| `Run`, reset, ROM decoding, CPU/devices | Correctly placed | emulation lifecycle |
| switch/door/DIP/stake/prize/percent setters and coin events | Correctly placed | accepts logical machine state; no exported host input capture |
| snapshots and scalar lamp/reel/display/meter/hopper getters | Correctly placed | exposes state, not graphics |
| caller-pull PCM (`GetAudioFormat`, `FillAudioFrames`) | Correctly placed | data rather than playback |
| Epoch DirectSound objects/playback code | Candidate for future separation | platform audio playback is frontend presentation (`FruitSound.h`, `SoundMain.*`) |
| ROM/RAM path opening inside cores | Acceptable legacy coupling | frontend chooses path, core performs I/O; future data-buffer option may improve portability |
| `SetCFolder`/`SetCFileName` and parameterless state calls | Candidate for future separation | cached application path/workflow is blurred; semantics Unknown |
| EDC mutable string pointer | Acceptable legacy coupling | diagnostic data belongs in backend, but ownership/lifetime boundary is weak |
| MPU5 `PopEDCMessage` caller buffer | Correctly placed | explicit copied diagnostic event |
| trace flags and debug `FILE*` output | Candidate for future separation | diagnostics are backend data; destination/presentation should be host-controlled |
| lamp electrical telemetry/broken-lamp injection | Correctly placed | hardware simulation and diagnostic state, although only MPU5 exposes it |
| legacy `UpdateLamps`/`UpdateSegs` | Unclear | wrappers are no-ops in some cores; retained for compatibility, external usage unknown |

No API should be removed because current Unity usage is not assessed. Rendering, playback, direct input capture, file dialogs, and window management are not required backend capabilities; any internal occurrence is legacy coupling to document and later place behind tests.
