# Future Amber backend contract

The Amber backend is an adapter, not part of Fabric's public machine model. It will receive a launch
request selected by backend and machine identifiers and load the **exact absolute DLL path supplied by
the frontend**. It must not search relative directories, substitute `AmberBridge.dll`, choose
`AmberOasis.JPMSystem6.dll`, or infer a specific Amber platform.

The adapter will negotiate exactly Amber API v2 and map discovered capabilities to extensible Fabric
flags. It will translate Fabric digital input and generic snapshot objects without exposing Amber
structures. In particular it must retain logical lamp state separately from brightness, preserve
signed reel positions, translate alpha attributes and seven-segment masks without reinterpretation,
and expose PCM16 interleaved frames using the negotiated format.

All native calls for an instance will be serialised. Create, initialise, reset, execution, shutdown,
and unload must have deterministic ordering, including partial-failure cleanup. Adapter errors are
copied into Fabric-owned boundary text. Exceptions, borrowed Amber pointers, and loader handles never
cross the C ABI.

Amber cycle counts are adapter-local. For System 6 the adapter must preserve the existing 1 kHz pump
and derive audio from elapsed time in complete PCM frames, including fractional-frame accumulation;
Fabric's generic time advance must not redefine that behaviour. Reel, coin, and percentage
configuration mappings require parity tests before frontend migration.

The maintained Amber sources under `src/Cores` and the current bridge remain compatibility assets.
They will be removed only after the external-DLL implementation reaches behavioural parity and its
integration tests pass. MAME will be a separate, out-of-process provider and is not implemented here.
