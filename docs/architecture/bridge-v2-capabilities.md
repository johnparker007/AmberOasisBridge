# API v2 capabilities and observable data

`AmberCapabilitiesV1.feature_bits` is a 64-bit, versioned mask returned for an already-created instance. The System 6 adapter reports when all corresponding optional exports resolve all six currently defined bits: switch input, output snapshot, audio, reel configuration, coin configuration, and percentage switch. Unknown bits are ignored by clients. `max_switches` is 256 for System 6; other limits are ABI constants in the production header and actual output counts come from each snapshot.

A function is present even when its bit is clear and then returns `AMBER_NOT_SUPPORTED`. Capability absence is distinct from `AMBER_UNSUPPORTED_VERSION`, which applies only to `AmberGetApi` negotiation or an independently versioned structure.

## Snapshot representation

The snapshot is fixed-capacity to make bounds, ownership, and atomicity unambiguous. Indices are zero-based throughout.

| Field | Capacity / System 6 count | Maintained evidence and conversion |
|---|---:|---|
| matrix lamps | 512 / 512 | PA2 declares and fills 512. `UpdateLamps()` is required before reads. `is_on` is copied from maintained logical `OnOff` and normalised to 0/1; it is never inferred from brightness. Brightness is converted independently, preserving filament fade while logical power is off. |
| reels | 8 / 8 | `ReelDrive` owns eight signed `INT16` positions and snapshot widens `GetPosOut` to signed 32-bit. Disabled reels remain in this count and at their stable index. Their maintained native position is still read and reported; `enabled` controls configuration application only, not observation or membership. |
| alpha displays | 2 / **1** | PA2 reserves/reports two, but maintained accessors have no display argument and read only `Alpha1`; PA2 currently duplicates it twice. V2 reports one until a second independent display is implemented. Each has 16 native 16-bit segment masks, 16 dot/comma bytes, brightness `GetAlphaBright()/31` converted as specified below. |
| seven-segment displays | 40 / **16** | The maintained segment plane has 256 cells and its legacy mask construction uses a 16-cell stride, so 16 independently readable displays exist. For display `d`, its eight visible segment cells are source indices `display * 16` through `display * 16 + 7`; cells 8..15 in the stride are not part of that display mask. The low eight bits of `segment_mask` preserve the maintained construction order. |

A bridge implementation must execute the maintained `UpdateLamps()` and `UpdateSegs()` immediately before copying, just as `FillOutputSnapshot` does. It then converts all fields while the caller's instance-call serialization excludes `Run`, producing a coherent atomic observation. Values remain stable between serialized calls except that requesting a snapshot performs the documented lazy lamp/segment update. No dot-matrix data exists: `AlphaDotDisplayCount` is zero and System 6 supplies segmented alpha data, so v2 contains no dot-matrix payload.

For each available seven-segment display, brightness is the maximum of those eight source-cell brightness values, divided by 255 and converted to Q16.16. If all eight cells are dark the result is zero. An unavailable display is outside `seven_segment_display_count`; its fixed-capacity tail record is entirely zero. This deliberately replaces the legacy snapshot's unrelated `GetSegBright(display)` expression.

Alpha `dot_comma[ch]` describes the same character index as `segment_masks[ch]`. The adapter converts the maintained System 6 command byte 46 to `AMBER_ALPHA_DECIMAL_POINT` (`0x01`) and byte 44 to `AMBER_ALPHA_COMMA_TAIL` (`0x02`); the maintained default/sentinel byte 6 and every other value become zero. Bit 0 means a decimal point and bit 1 means a comma tail. Bits 2..7 are reserved and always zero. If punctuation information is unavailable, every byte is zero.

Unused array tails and reserved fields are zero. A count of zero is the unavailable representation. The fixed 4592-byte object makes the former two-call `GetOutputSnapshotSize` pattern unnecessary.

## Brightness

The public ABI avoids compiler floating-point representation as persistent contract data. One conversion is used for lamps, alpha, and seven-segment brightness. For source `value`: NaN or either infinity makes the snapshot fail with `AMBER_INTERNAL_ERROR` and the caller's snapshot remains zeroed; `value <= 0` becomes zero; a finite positive value is multiplied by 65536 in a range-checked wider floating calculation and rounded to nearest integer with exact halves rounded upward (equivalent to `floor(scaled + 0.5)`); a finite result greater than `UINT32_MAX` saturates to `UINT32_MAX`. Thus 65536 is nominal brightness 1.0. Lamp brightness may exceed 1.0 and is not clamped to unity. Alpha and seven-segment brightness normally range 0..65536.
