# Imported project build assessment

The project and solution files remain byte-for-byte imported; none were rewritten for their new repository locations.

| Platform | Solution/project format | Toolset | Relative-path assessment | Verification |
| --- | --- | --- | --- | --- |
| Epoch | Solution format 12.00 (labelled Visual Studio 2012); MSBuild C++ project `ToolsVersion="15.0"`; Windows 10 SDK | `v145` | The solution-to-project path `EpochCore\\EpochCore.vcxproj` remains valid after moving the whole tree. No external relative source paths were found. | Not built: this environment has no Visual Studio C++/MSBuild toolchain or Windows SDK. |
| JPM System 6 | Solution format 12.00 (labelled Visual Studio 18); MSBuild C++ project `ToolsVersion="15.0"`; Windows 10 SDK | `v143` | The solution-to-project path remains valid. The optional `$(SolutionDir)BuildSupport\\AmberRuntimeDeps.props` file is absent, but its import is conditional; this was already absent before the move. | Not built: this environment has no Visual Studio C++/MSBuild toolchain or Windows SDK. |
| MPU5 | Solution format 12.00 (labelled Visual Studio 18); MSBuild C++ project (no explicit `ToolsVersion`); Windows 10 SDK | `v145` | The solution-to-project path and project-local include paths remain valid. No external relative source paths were found. | Not built: this environment has no Visual Studio C++/MSBuild toolchain or Windows SDK. |

## Conclusions

- Moving each complete project tree preserves its solution-relative paths; no path-only project adjustment is currently proposed.
- Toolsets `v145` and Visual Studio 18 identifiers may require pre-release or locally specific tooling and could not be verified here. They have intentionally not been upgraded or normalised.
- Removed `Debug64` and `Release64` file lists and link outputs were generated build products, not build inputs.
