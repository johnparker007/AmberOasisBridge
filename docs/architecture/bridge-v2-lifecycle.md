# API v2 lifecycle, threading, and validity

API v2 preserves the v1 single-instance lifecycle and caller-serialization rule. All calls using an `AmberHandle`, including output and audio reads, must be made by one caller at a time and must not overlap `Run` or each other. The bridge does not add internal instance serialization or worker threads. Consequently a snapshot is atomic relative to execution by contract, not by lock-free access. `GetCapabilities` is also serialized even though it is read-only.

“Running” means a `Run` call is currently on the stack; every other v2 instance operation is invalid during it. Between completed `Run` calls the state remains Initialised.

| Operation | Created | Initialised (before/after Reset or Run) | Running | Shutdown | InitialiseFailed | Destroyed |
|---|---|---|---|---|---|---|
| `GetCapabilities` | yes | yes | no | yes | yes | invalid handle |
| `SetSwitchState` | invalid state | yes | invalid state | invalid state | invalid state | invalid handle |
| `GetOutputSnapshot` | invalid state | yes | invalid state | invalid state | invalid state | invalid handle |
| `GetAudioFormat` | invalid state | yes | invalid state | invalid state | invalid state | invalid handle |
| `FillAudioFrames` | invalid state | yes | invalid state | invalid state | invalid state | invalid handle |
| `ConfigureReels` | invalid state | yes | invalid state | invalid state | invalid state | invalid handle |
| `ConfigureCoins` | invalid state | yes | invalid state | invalid state | invalid state | invalid handle |
| `SetPercentageSwitch` | invalid state | yes | invalid state | invalid state | invalid state | invalid handle |

An initialised call is legal immediately after `Initialise`, after `Reset`, and after any completed `Run`. Configuration is specifically supported in Oasis's existing order: `Create; Initialise; Reset; ConfigureReels; ConfigureCoins; SetPercentageSwitch; Run`. It is not part of `AmberInitialiseParams`, preserving v1.

Reset clears native matrix and runtime state. The v2 adapter shall retain the last successful reel, coin and percentage configurations and reapply them during every successful Reset; switch input levels themselves clear to off and callers reassert desired controls. Snapshot then reflects reset outputs. Audio playback resets to silence. Shutdown discards retained configuration and switch levels. A subsequent Initialise on a lifecycle that permits it starts from defaults. Failed Initialise makes all operational calls invalid but still permits capability inspection and error retrieval. Destroy invalidates the handle immediately; no call, including `GetLastError`, may dereference it.

All non-OK results update the per-instance last-error text where a live handle exists; negotiation/global failures use the existing global error mechanism. No configuration operation changes anything unless its entire structure validates. Output structures are zeroed before population so unsupported tails cannot expose stale caller or core data.
