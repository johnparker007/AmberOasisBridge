# Emulator project build assessment

## Upstream/reference metadata

The files below remain imported, immutable history; their settings have not been rewritten.

| Platform | Imported project metadata | Toolset | Historical output layout |
| --- | --- | --- | --- |
| Epoch | VS 2012-labelled solution; C++ project `ToolsVersion="15.0"`; Windows 10 SDK | `v145` | Solution-local `Debug32/64` and `Release32/64` |
| JPM System 6 | VS 18-labelled solution; C++ project `ToolsVersion="15.0"`; Windows 10 SDK | `v143` | Solution-local `Debug32/64` and `Release32/64` |
| MPU5 | VS 18-labelled solution; Windows 10 SDK | `v145` | Solution-local `Debug32/64` and `Release32/64` |

The maintained JPM System 6 project no longer imports the optional
`BuildSupport/AmberRuntimeDeps.props`. The property sheet does not exist in this
repository, has no documentation or active consumer, and was not available to a
repository build. The immutable imported project is unchanged.

## Maintained Oasis metadata

| Project | IDE/toolset | Solution configurations | Output |
| --- | --- | --- | --- |
| `AmberOasis.Epoch` | Visual Studio 2022 / `v143` | `Debug|x64`, `Release|x64` | `build/bin/$(Platform)/$(Configuration)/AmberOasis.Epoch.dll` |
| `AmberOasis.JPMSystem6` | Visual Studio 2022 / `v143` | `Debug|x64`, `Release|x64` | `build/bin/$(Platform)/$(Configuration)/AmberOasis.JPMSystem6.dll` |
| `AmberOasis.MPU5` | Visual Studio 2022 / `v143` | `Debug|x64`, `Release|x64` | `build/bin/$(Platform)/$(Configuration)/AmberOasis.MPU5.dll` |

The maintained projects also retain clean Win32 project configurations, but the umbrella solution intentionally advertises only the required x64 configurations. Intermediate files go to `build/obj/<ProjectName>/$(Platform)/$(Configuration)/`.

JPM System 6 formerly prepended the June 2010 DirectX SDK `Include` directory
and its architecture-specific `Lib\x86` or `Lib\x64` directory to `IncludePath`
and `LibraryPath` in all four project configurations. Those machine-specific
values were stale: the compiled JPM sources contain no DirectX headers, calls,
or explicit DirectX libraries, so the properties were deleted rather than
redirected to another SDK location. Epoch and MPU5 had no DirectX project paths.

Windows clean builds and DLL import-table inspection were not available in the
Linux maintenance environment used for this cleanup. Consequently Debug x64
and Release x64 are not recorded as successfully rebuilt here, and binary
DirectX/runtime dependencies remain to be verified on a VS2022 host. See the
[frontend dependency audit](architecture/frontend-dependency-audit.md) for the
source and project-metadata results.
