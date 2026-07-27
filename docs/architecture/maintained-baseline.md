# Maintained emulator baseline

The maintained copies under `src/Cores/` provide independently editable integration baselines while preserving the imported cores' behaviour and public DLL interfaces. `Upstream/` remains immutable reference material; future deliveries are compared there and selected changes are ported into the maintained copies.

This initial copy intentionally retains each core's duplicate devices, CPU implementation, interfaces, and global state. That duplication establishes traceable platform baselines and is not architectural approval of the legacy design. No common library, common ABI, adapter, or bridge has been introduced.

`AmberOasis.Epoch`, `AmberOasis.JPMSystem6`, and `AmberOasis.MPU5` are independent Visual Studio 2022 C++ projects. Every configuration uses the `v143` toolset. The umbrella solution supports `Debug|x64` and `Release|x64`; each project also retains its upstream Win32 configurations for direct project use. They emit separate `AmberOasis.Epoch.dll`, `AmberOasis.JPMSystem6.dll`, and `AmberOasis.MPU5.dll` files under `build/bin/$(Platform)/$(Configuration)/`, with intermediates under project-specific `build/obj/` directories.

The source copy contains no intentional behavioural or ABI changes. Subsequent work should begin with an inventory of exports, ABI details, and global state before adapters, tests, or architectural refactoring are designed.

The maintained JPM System 6 project has had inherited, hard-coded June 2010
DirectX SDK `IncludePath` and `LibraryPath` entries removed from Debug and
Release for Win32 and x64. Its conditional `AmberRuntimeDeps.props` import was
also removed because the file is absent and has no documented or active role.
No behaviour-relevant source or exported interface changed in that metadata
cleanup. The maintained projects do not link an explicit DirectX library; a
VS2022 rebuild and DLL import inspection are still required because this result
was established in a non-Windows environment. The detailed findings, including
excluded historical Epoch frontend files, are in
[`frontend-dependency-audit.md`](frontend-dependency-audit.md).
