# JPM System 6 adapter v1

The adapter loads `AmberOasis.JPMSystem6.dll` at Create and resolves every required export once: `GetDLLVersion`, `Initialise`, `Shutdown`, `Reset`, `Run`, `LoadROM`, and `LoadSoundROM`. A missing symbol causes immediate unload and `AMBER_EXPORT_MISSING`; no partially usable adapter is returned. Destroy unloads the module after lifecycle cleanup.

The loader requests the literal DLL name using `LoadLibraryEx` with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS`. Consequently Windows searches the loading module's directory and the process safe default DLL directories; deployments should colocate the bridge and maintained core in the established `build/bin/x64/<Configuration>` output. It does not search the working directory implicitly or accept an arbitrary core path.

Typed `__cdecl` pointers preserve the existing JPM signatures and global singleton. The bridge neither statically links nor modifies the emulator and does not claim multi-instance safety. V1 deliberately excludes snapshots, lamps, reels, displays, audio, inputs, and persistence. Later table versions can append common capabilities and core-specific extension discovery while preserving this table.
