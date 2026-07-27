# Bridge error model

`AmberResult` distinguishes success, invalid arguments, unsupported API versions, DLL-load failures, missing exports, invalid lifecycle state, the process instance limit, core initialisation/ROM failures, and internal failures. No exception crosses the ABI.

`GetLastError` copies a diagnostic NUL-terminated UTF-8 string into caller-owned storage and optionally returns the required byte count (including NUL). A null or undersized buffer returns `AMBER_INVALID_ARGUMENT` while still reporting that size. Handle-specific messages live until the next error on that handle; pre-create and invalid-handle errors are thread-local. Result codes, rather than text, are the programmatic contract.
