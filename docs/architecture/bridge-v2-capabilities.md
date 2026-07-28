# API v2 capabilities and observable data

`AmberCapabilitiesV1.feature_bits` is a 64-bit, versioned mask returned for an already-created instance. The System 6 adapter is designed to report all six currently defined bits: switch input, output snapshot, audio, reel configuration, coin configuration, and percentage switch. Unknown bits are ignored by clients. `max_switches` is 256 for System 6; other limits are ABI constants in the proposal header and actual output counts come from each snapshot.

A function is present even when its bit is clear and then returns `AMBER_NOT_SUPPORTED`. Capability absence is distinct from `AMBER_UNSUPPORTED_VERSION`, which applies only to `AmberGetApi` negotiation or an independently versioned structure.

## Snapshot representation

The snapshot is fixed-capacity to make bounds, ownership, and atomicity unambiguous. Indices are zero-based throughout.

| Field | Capacity / System 6 count | Maintained evidence and conversion |
|---|---:|---|
| matrix lamps | 512 / 512 | PA2 declares and fills 512. `UpdateLamps()` is required before reads. `OnOff` becomes 0/1; floating brightness becomes unsigned Q16.16, saturated to `UINT32_MAX`. |
| reels | 8 / 8 | `ReelDrive` owns eight signed `INT16` positions and snapshot widens `GetPosOut` to signed 32-bit. |
| alpha displays | 2 / **1** | PA2 reserves/reports two, but maintained accessors have no display argument and read only `Alpha1`; PA2 currently duplicates it twice. V2 reports one until a second independent display is implemented. Each has 16 native 16-bit segment masks, 16 dot/comma bytes, brightness `GetAlphaBright()/31` as Q16.16. |
| seven-segment displays | 40 / 40 | PA2 creates 40 eight-bit masks from the segment matrix. V2 uses the low eight bits of `segment_mask`; brightness `GetSegBright()/255` becomes Q16.16. |

A bridge implementation must execute the maintained `UpdateLamps()` and `UpdateSegs()` immediately before copying, just as `FillOutputSnapshot` does. It then converts all fields while the caller's instance-call serialization excludes `Run`, producing a coherent atomic observation. Values remain stable between serialized calls except that requesting a snapshot performs the documented lazy lamp/segment update. No dot-matrix data exists: `AlphaDotDisplayCount` is zero and System 6 supplies segmented alpha data, so v2 contains no dot-matrix payload.

Unused array tails and reserved fields are zero. A count of zero is the unavailable representation. The fixed 4592-byte object makes the former two-call `GetOutputSnapshotSize` pattern unnecessary.

## Brightness

The public ABI avoids compiler floating-point representation as persistent contract data. Q16.16 encodes 0 as dark and 65536 as nominal brightness 1.0. Lamp brightness may exceed 1.0 (the maintained filament model explicitly permits over-bright values); therefore it is not clamped to unity. Alpha and seven-segment brightness normally range 0..65536.
