import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


ASSERTIONS = r'''
#include <stddef.h>
#include "amber/amber_api_v2_proposal.h"
#if defined(__cplusplus)
#define ABI_ASSERT(x, m) static_assert((x), m)
#else
#define ABI_ASSERT(x, m) _Static_assert((x), m)
#endif
ABI_ASSERT(AMBER_API_VERSION_1 == 0x00010000u, "v1 version changed");
ABI_ASSERT(AMBER_API_VERSION_2 == 0x00020000u, "v2 encoding");
ABI_ASSERT(AMBER_OK == 0, "v1 result changed");
ABI_ASSERT(AMBER_INVALID_ARGUMENT == 1, "v1 result changed");
ABI_ASSERT(AMBER_UNSUPPORTED_VERSION == 2, "v1 result changed");
ABI_ASSERT(AMBER_DLL_LOAD_FAILED == 3, "v1 result changed");
ABI_ASSERT(AMBER_EXPORT_MISSING == 4, "v1 result changed");
ABI_ASSERT(AMBER_INVALID_STATE == 5, "v1 result changed");
ABI_ASSERT(AMBER_INSTANCE_LIMIT == 6, "v1 result changed");
ABI_ASSERT(AMBER_INITIALISE_FAILED == 7, "v1 result changed");
ABI_ASSERT(AMBER_INTERNAL_ERROR == 8, "v1 result changed");
ABI_ASSERT(AMBER_NO_MORE_ITEMS == 9, "v1 result changed");
ABI_ASSERT(AMBER_BUFFER_TOO_SMALL == 10, "v1 result changed");
ABI_ASSERT(AMBER_NOT_SUPPORTED == 11, "results must append");
ABI_ASSERT(AMBER_INVALID_RANGE == 12, "results must append");
ABI_ASSERT(AMBER_MALFORMED_CONFIGURATION == 13, "results must append");
ABI_ASSERT(AMBER_ALPHA_DECIMAL_POINT == UINT8_C(0x01), "decimal point bit");
ABI_ASSERT(AMBER_ALPHA_COMMA_TAIL == UINT8_C(0x02), "comma tail bit");
ABI_ASSERT(AMBER_COIN_CONFIG_APPLY_LOCKOUT_PORT == UINT32_C(0x00000001), "lockout flag");
ABI_ASSERT(AMBER_MAX_MATRIX_LAMPS == 512, "lamp capacity");
ABI_ASSERT(AMBER_MAX_REELS == 8, "reel capacity");
ABI_ASSERT(sizeof(AmberApiV1) == 80, "v1 x64 size");
ABI_ASSERT(sizeof(AmberApiV2) == 144, "v2 x64 size");
#define ASSERT_V1_PREFIX(field) ABI_ASSERT(offsetof(AmberApiV2, field) == offsetof(AmberApiV1, field), "v1 prefix: " #field)
ASSERT_V1_PREFIX(struct_size);
ASSERT_V1_PREFIX(api_version);
ASSERT_V1_PREFIX(GetBridgeInfo);
ASSERT_V1_PREFIX(EnumerateCore);
ASSERT_V1_PREFIX(Create);
ASSERT_V1_PREFIX(Destroy);
ASSERT_V1_PREFIX(Initialise);
ASSERT_V1_PREFIX(Reset);
ASSERT_V1_PREFIX(Run);
ASSERT_V1_PREFIX(Shutdown);
ASSERT_V1_PREFIX(GetLastError);
ABI_ASSERT(offsetof(AmberApiV2, GetCapabilities) == sizeof(AmberApiV1), "v2 suffix");
ABI_ASSERT(sizeof(AmberCapabilitiesV1) == 32, "capabilities size");
ABI_ASSERT(sizeof(AmberAudioFormatV1) == 32, "audio size");
ABI_ASSERT(sizeof(AmberOutputSnapshotV1) == 4592, "snapshot size");
ABI_ASSERT(offsetof(AmberOutputSnapshotV1, matrix_lamps) == 40, "lamp offset");
ABI_ASSERT(offsetof(AmberOutputSnapshotV1, reel_positions) == 4136, "reel offset");
ABI_ASSERT(offsetof(AmberOutputSnapshotV1, alpha_displays) == 4168, "alpha offset");
ABI_ASSERT(offsetof(AmberOutputSnapshotV1, seven_segment_displays) == 4272, "seven offset");
ABI_ASSERT(sizeof(AmberReelConfigurationV1) == 208, "reel config size");
ABI_ASSERT(sizeof(AmberCoinConfigurationV1) == 408, "coin config size");
ABI_ASSERT(offsetof(AmberCoinConfigurationV1, lockout_port_base) == 392, "lockout base offset");
ABI_ASSERT(offsetof(AmberCoinConfigurationV1, lockout_port_value) == 396, "lockout value offset");
ABI_ASSERT(offsetof(AmberCoinConfigurationV1, configuration_flags) == 400, "coin flags offset");
int main(void) { return 0; }
'''


class BridgeV2DesignTests(unittest.TestCase):
    def _compile(self, compiler, suffix, standard):
        if not shutil.which(compiler):
            self.skipTest(f"{compiler} is unavailable")
        with tempfile.TemporaryDirectory() as directory:
            source = pathlib.Path(directory) / f"abi{suffix}"
            output = pathlib.Path(directory) / "abi-check"
            source.write_text(ASSERTIONS)
            subprocess.run(
                [compiler, standard, "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "include"),
                 str(source), "-o", str(output)],
                check=True,
            )

    def test_proposed_header_compiles_as_c11_and_matches_x64_layout(self):
        self._compile("cc", ".c", "-std=c11")

    def test_proposed_header_compiles_as_cpp17_and_matches_x64_layout(self):
        self._compile("c++", ".cpp", "-std=c++17")

    def test_proposal_is_not_advertised_by_production_headers_or_bridge(self):
        version = (ROOT / "include/amber/amber_version.h").read_text()
        bridge = (ROOT / "src/Bridge/AmberBridge.cpp").read_text()
        self.assertIn("AMBER_API_VERSION_CURRENT AMBER_API_VERSION_1", version)
        self.assertNotIn("AMBER_API_VERSION_2", bridge)
        self.assertNotIn("AmberApiV2", bridge)

    def test_seven_segment_brightness_rule_is_fully_specified(self):
        contract = (ROOT / "docs/architecture/bridge-v2-capabilities.md").read_text()
        self.assertIn("display * 16` through `display * 16 + 7", contract)
        self.assertIn("maximum of those eight", contract)

    def test_lockout_flag_semantics_are_fully_specified(self):
        contract = (ROOT / "docs/architecture/jpm-adapter-v2-design.md").read_text()
        self.assertIn("must not call `SetLockoutVal`", contract)
        self.assertIn("Unknown flags, nonzero `reserved`", contract)


if __name__ == "__main__":
    unittest.main()
