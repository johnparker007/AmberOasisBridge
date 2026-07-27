# Bridge error model

Stable `AmberResult` values distinguish success, invalid arguments, unsupported versions, DLL-load failures, missing exports, invalid state/handle, instance limit, initialisation failure, internal failure, enumeration end, and insufficient buffers. Existing values 0–8 were not renumbered; v0.1.1 appends `AMBER_NO_MORE_ITEMS` (9) and `AMBER_BUFFER_TOO_SMALL` (10).

`GetLastError(handle, nullptr, 0, &required)` is a successful size query and returns `AMBER_OK`, with `required` including the NUL. A non-null undersized destination returns `AMBER_BUFFER_TOO_SMALL`, reports the required size when requested, performs no partial copy, and does not replace the stored diagnostic. Other null-buffer combinations are invalid. A valid handle selects its diagnostic; null selects thread-local/pre-instance diagnostics. A non-null invalid or stale handle returns `AMBER_INVALID_STATE` without dereferencing it; the resulting message can then be queried with a null handle.

Win32 loader failures preserve the immediately captured numeric `GetLastError`, add readable `FormatMessageW` text where available, convert text and attempted paths to UTF-8, and include the attempted absolute DLL path. C++ standard and unknown exceptions are caught at every C ABI function, recorded using bounded non-throwing storage, and return `AMBER_INTERNAL_ERROR`.
