# Fabric migration plan

## Invariants

Migration must preserve exact Amber API v2 negotiation and discovery; create/initialise/reset/run/
shutdown; switch input; coherent snapshots; separate lamp logic and brightness; signed reels; alpha
and seven-segment masks; interleaved PCM16; reel, coin, and percentage configuration; serialised native
calls; deterministic cleanup; and the 1 kHz System 6 pump with frame-based audio timing.

## Stages

1. **Foundation (this change).** Add the neutral C ABI, provider/instance boundary, multi-provider
   registry, deterministic runtime/session ownership, fake backend tests, and architecture documents.
   Keep the existing Amber bridge and all maintained cores unchanged and buildable.
2. **Amber adapter and contract harness.** Implement a provider that negotiates API v2 against the exact
   absolute Amber DLL path supplied in `FabricLaunchRequest`. Use a fake test DLL for failure, ABI, and
   lifecycle coverage; compare all output/configuration/audio semantics with the current Oasis path.
3. **Frontend integration.** Add Fabric consumption to Oasis in a separate repository change, retain a
   controlled compatibility path, and run end-to-end tests against representative Amber machines.
4. **Process backend.** Specify a transport-neutral execution contract and add MAME out-of-process;
   do not force process behaviour into the public machine model.
5. **Consolidation.** Only after the external-DLL path reaches behavioural parity and integration tests
   pass may the maintained Amber source directories and legacy bridge be considered for removal.

## Foundation implementation checklist

- [x] Versioned neutral public C ABI with sized structures and opaque handles.
- [x] Transport-neutral provider and instance interfaces.
- [x] Explicit registry supporting multiple providers and machine-aware selection.
- [x] Serialised, deterministic session lifecycle and safe backend errors.
- [x] Generic fake provider tests, including lamps and unknown capabilities.
- [x] Preserve the current Amber API, bridge, projects, and core sources.
- [ ] Implement the Amber DLL provider (next stage).
- [ ] Modify Oasis, add MAME, or add an IPC/discovery protocol (later stages).

## Current stage

Stage 2 implements the external `amber-api-v2` provider, typed ordered ROMs, translated lifecycle, capabilities, snapshots, audio, and versioned backend configuration. Direct historical Amber core DLL support is not implemented. Oasis integration is the following PR. `src/Cores` and the old bridge remain until parity and migration are proven; MAME remains deferred.
