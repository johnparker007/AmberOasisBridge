# Tests

`AmberLegacyBackendTests` uses `FakeAmberLegacy`, which implements the unchanged production
System 6 ABI and intentionally has no `AmberGetApi` export. It covers adapter detection, actionable
missing-export diagnostics, typed ROM ordering, configuration, lifecycle, nanosecond remainder
conversion, inputs, snapshots, 44100 Hz stereo PCM16 audio, repeated creation, and unload/reload.

To exercise proprietary files locally, set `FABRIC_REAL_AMBER_DLL` to an absolute existing Amber
DLL path and supply its required ROM paths in a local test harness. The opt-in check should skip when
the variable or ROMs are unavailable; proprietary binaries and ROMs must not be added here.

Regression and contract tests for maintained code belong here. The initial repository safeguard tests are in `tools/tests/`.
