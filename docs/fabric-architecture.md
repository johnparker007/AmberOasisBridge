# Fabric architecture foundation

## Direction and boundaries

Fabric is a standalone emulator runtime: it is not a renamed Amber bridge and has no dependency on
Oasis, Amber, JPM, or a particular machine. The target dependency flow is:

> Oasis → Fabric API → backend abstraction → Amber DLL supplied by Oasis

The frontend supplies a backend kind, machine identifier, an **absolute** backend library or executable
path, ROM paths, and opaque machine configuration. Fabric selects a provider using both neutral
identifiers and passes the launch request through without inventing a filename or relative location.
An Amber provider must therefore load the exact path in the request; it must not hard-code
`AmberBridge.dll`, `AmberOasis.JPMSystem6.dll`, or any platform.

`include/fabric/fabric.h` is the versioned, exception-free C ABI for managed interop. Every public data
structure starts with size and version fields. Runtime and session handles are opaque, ownership is
explicit, and destroy operations are deterministic. Backend exceptions are caught inside the runtime
boundary and never cross the C ABI.

## Runtime and providers

Each `FabricRuntime` owns a registry of `FabricBackendProvider` objects. Registration is explicit at the
composition root; there is no process-global registry or dynamic plugin discovery. A provider decides
whether it supports a backend-kind/machine-identifier pair and creates one `FabricBackendInstance`.
The session then owns that instance for initialise, reset, advance, input, snapshot/audio retrieval,
shutdown, and destruction. Calls on a session are serialised.

Providers describe *what* a backend can do, not its transport. An instance might adapt an in-process
DLL, supervise an executable, or use another local mechanism later. `advance(elapsed_nanoseconds)` is a
time-oriented scheduling request, not a CPU-cycle contract. Cycle conversion and the existing System 6
1 kHz pump/frame-based PCM timing belong inside the future Amber adapter.

## Neutral machine model

The first public model contains digital inputs, lamps, signed reel positions, character displays,
segment masks, and PCM audio. Lamps carry a stable identifier, optional numerical index, logical state,
and independent brightness. Caller-owned snapshot arrays prevent backend memory from escaping across
the ABI. Capability flags are extensible: consumers must ignore unknown bits, allowing newer providers
to work with older hosts.

No Amber snapshot or platform header is included by the public API. Transport handles, DLL entry
points, executable process details, cycle counts, and proprietary configuration layouts remain backend
implementation details.

## Deliberate omissions

This foundation contains no MAME implementation, networking, gRPC, JSON IPC, or discovery system. It
also does not replace the current Amber C ABI. Those omissions keep this change architectural and let
the established Oasis integration remain the behavioural oracle while adapters are developed behind
contract tests.

## Second migration stage

`FabricRuntime` is a shared library (`FabricRuntime.dll` on Windows) with an exported C ABI. Runtime errors cover failures before session creation. Snapshot display data is inline caller-owned storage. The built-in `amber-api-v2` backend loads only the exact absolute requested path: it uses provider API v2 when `AmberGetApi` exists and otherwise adapts the existing production System 6 exports. No Amber rebuild or ABI change is required, and Amber-specific adapter types remain internal. `src/Cores` and the old bridge remain. MAME is deferred.
