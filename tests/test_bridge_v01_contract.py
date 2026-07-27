import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class BridgeV01ContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.types = (ROOT / "include/amber/amber_types.h").read_text()
        cls.api = (ROOT / "include/amber/amber_api.h").read_text()
        cls.bridge = (ROOT / "src/Bridge/AmberBridge.cpp").read_text()
        cls.jpm_header = (ROOT / "src/Cores/JPMSystem6/Interface.h").read_text()

    def test_result_values_are_stable_and_appended(self):
        expected = {
            "AMBER_OK": 0, "AMBER_INVALID_ARGUMENT": 1,
            "AMBER_UNSUPPORTED_VERSION": 2, "AMBER_DLL_LOAD_FAILED": 3,
            "AMBER_EXPORT_MISSING": 4, "AMBER_INVALID_STATE": 5,
            "AMBER_INSTANCE_LIMIT": 6, "AMBER_INITIALISE_FAILED": 7,
            "AMBER_INTERNAL_ERROR": 8, "AMBER_NO_MORE_ITEMS": 9,
            "AMBER_BUFFER_TOO_SMALL": 10,
        }
        for name, value in expected.items():
            self.assertRegex(self.types, rf"\b{name}\s*=\s*{value}\b")

    def test_api_table_field_order_is_unchanged(self):
        table = re.search(r"typedef struct AmberApiV1 \{(.*?)\} AmberApiV1;", self.api, re.S).group(1)
        fields = ["struct_size", "api_version", "GetBridgeInfo", "EnumerateCore", "Create",
                  "Destroy", "Initialise", "Reset", "Run", "Shutdown", "GetLastError"]
        positions = [table.index(field) for field in fields]
        self.assertEqual(positions, sorted(positions))

    def test_amber_get_api_negotiates_version_and_size(self):
        self.assertIn("if (!api) return AMBER_INVALID_ARGUMENT", self.bridge)
        self.assertIn("version!=AMBER_API_VERSION_1", self.bridge)
        self.assertIn("size<sizeof(AmberApiV1)", self.bridge)
        self.assertIn("std::memcpy(api,&value,sizeof(value))", self.bridge)

    def test_only_amber_get_api_is_declared_as_a_public_export(self):
        declarations = re.findall(r"AMBER_EXPORT\s+AmberResult\s+AMBER_CALL\s+(\w+)", self.api)
        self.assertEqual(declarations, ["AmberGetApi"])

    def test_required_exports_are_resolved_by_expected_names(self):
        names = re.findall(r'Resolve\(library,"([^"]+)"', self.bridge)
        self.assertEqual(names, ["GetDLLVersion", "Initialise", "LoadROM", "LoadSoundROM",
                                 "Reset", "Run", "Shutdown"])

    def test_typedefs_match_maintained_source_declarations(self):
        declarations = [
            "float GetDLLVersion(void)", "UINT8 Initialise(void)", "UINT8 Shutdown(void)",
            "void Reset(void)", "INT32 Run(UINT32 Cycles)",
            "UINT32 LoadROM(UINT8*, UINT8*, UINT8*, UINT8*)",
        ]
        for declaration in declarations:
            self.assertIn(declaration, self.jpm_header)
        self.assertRegex(self.jpm_header, r"UINT32 LoadSoundROM\(UINT8 \*name1, UINT8 \*name2, UINT8 \*name3, UINT8 \*name4\)")
        for typedef in ["JpmInt32 (__cdecl *)(JpmUint32)",
                        "JpmUint32 (__cdecl *)(JpmUint8*, JpmUint8*, JpmUint8*, JpmUint8*)"]:
            self.assertIn(typedef, self.bridge)

    def test_loader_is_bridge_relative_and_absolute(self):
        self.assertIn("GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS", self.bridge)
        self.assertIn("GetModuleFileNameW", self.bridge)
        self.assertIn('L"AmberOasis.JPMSystem6.dll"', self.bridge)
        self.assertIn("LoadLibraryExW(path.c_str()", self.bridge)
        self.assertNotIn('LoadLibraryExW(L"AmberOasis.JPMSystem6.dll"', self.bridge)

    def test_invalid_handle_is_checked_before_state_access(self):
        for function in ["DestroyImpl", "InitialiseImpl", "ResetImpl", "RunImpl", "ShutdownImpl"]:
            body = re.search(rf"AmberResult {function}\([^{{]+\) \{{(.*?)\n\}}", self.bridge, re.S).group(1)
            self.assertLess(body.index("IsLive("), body.find("->") if "->" in body else len(body))
        self.assertIn("if (h && !IsLive(h)) return InvalidHandle();", self.bridge)

    def test_lifecycle_guards_repeat_shutdown_and_stale_destroy(self):
        self.assertIn("if (!IsLive(handle)) return InvalidHandle();", self.bridge)
        self.assertIn("!h->core_initialised || h->core_shutdown", self.bridge)
        self.assertIn("h->core_shutdown=true; h->core_initialised=false", self.bridge)
        self.assertIn("g_instance=nullptr", self.bridge)

    def test_all_returned_boundaries_and_get_api_contain_catches(self):
        for name in ["BridgeInfo", "Enumerate", "Create", "Destroy", "Initialise", "Reset", "Run", "Shutdown", "LastError"]:
            start = self.bridge.index(f"AMBER_CALL {name}(")
            following = self.bridge[start:self.bridge.find("\n}\n", start) + 3]
            self.assertIn("catch", following, name)
        exported = self.bridge[self.bridge.index('extern "C" AMBER_EXPORT'):]
        self.assertIn("catch (const std::exception&", exported)
        self.assertIn("catch (...)", exported)

    def test_no_v02_capabilities_in_public_api(self):
        for forbidden in ["Lamp", "Reel", "Display", "Audio", "Snapshot", "Input", "Persistence"]:
            self.assertNotIn(forbidden, self.api)


if __name__ == "__main__":
    unittest.main()
