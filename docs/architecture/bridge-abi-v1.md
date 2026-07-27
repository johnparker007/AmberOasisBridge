# Amber Bridge ABI v1

Amber Bridge 0.1.1 is a C ABI facade and does not replace the existing direct JPM integration. Its sole DLL export is `AmberGetApi`. All calls use `__cdecl`, fixed-width values, opaque `AmberHandle`, and C-compatible structures; no C++ exception is allowed to cross `AmberGetApi` or a returned function pointer.

`AmberGetApi(AMBER_API_VERSION_1, size, table)` requires a non-null table and at least `sizeof(AmberApiV1)` bytes. Exact-size and larger buffers are accepted, but the bridge writes exactly the known v1 table bytes and leaves any larger tail untouched. A one-byte-too-small buffer returns `AMBER_BUFFER_TOO_SMALL`; an unknown version returns `AMBER_UNSUPPORTED_VERSION`. Returned `struct_size` and `api_version` describe the v1 table deterministically.

The v1 table order remains bridge information, indexed core enumeration, create, destroy, initialise, reset, run, shutdown, and last-error copy. Strings are UTF-8 and NUL-terminated. Caller strings and buffers remain caller-owned; bridge-owned information strings remain valid while the bridge is loaded. `struct_size` fields reject inputs smaller than the fields v1 reads and accept larger future structures. Enumeration index zero returns `jpm-system6`; later indices return appended result `AMBER_NO_MORE_ITEMS`, allowing Epoch and MPU5 to be added without changing the ABI.

The public result numbers 0 through 8 are unchanged. V0.1.1 appends `AMBER_NO_MORE_ITEMS = 9` and `AMBER_BUFFER_TOO_SMALL = 10`. The Windows verification entry point is `tools/verify_bridge_v01.ps1`, which builds and checks Debug and Release x64.
