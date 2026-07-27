# Amber Bridge ABI v1

Amber Bridge 0.1 is a C ABI facade, not a replacement for the existing direct JPM integration. The only DLL export is `AmberGetApi`; it accepts `AMBER_API_VERSION_1`, the caller's table size, and caller-owned table storage. A version must match exactly. The returned function table exposes bridge information, indexed core enumeration, create/destroy, initialise/reset/run/shutdown, and last-error copying.

All calls use `__cdecl`, fixed-width values, opaque `AmberHandle`, and C-compatible structures. Strings are UTF-8, NUL-terminated, borrowed for the duration of a call, and never freed by the bridge. Structure `struct_size` fields permit compatible appended fields in later revisions. Function-table members may only be appended in a new API version. V1 enumerates exactly `jpm-system6`; indexed enumeration allows Epoch and MPU5 to be added without changing existing layouts.

`AmberGetApi` and all returned functions return `AmberResult`. API and result numeric values are stable. The bridge owns a successful handle and its loaded core module until `Destroy`; callers own every supplied buffer and ROM-path string.
