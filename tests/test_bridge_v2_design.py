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
ABI_ASSERT(AMBER_BUFFER_TOO_SMALL == 10, "v1 result changed");
ABI_ASSERT(AMBER_NOT_SUPPORTED == 11, "results must append");
ABI_ASSERT(AMBER_MAX_MATRIX_LAMPS == 512, "lamp capacity");
ABI_ASSERT(AMBER_MAX_REELS == 8, "reel capacity");
ABI_ASSERT(sizeof(AmberApiV1) == 80, "v1 x64 size");
ABI_ASSERT(sizeof(AmberApiV2) == 144, "v2 x64 size");
ABI_ASSERT(offsetof(AmberApiV2, GetLastError) == offsetof(AmberApiV1, GetLastError), "v1 prefix");
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


if __name__ == "__main__":
    unittest.main()
