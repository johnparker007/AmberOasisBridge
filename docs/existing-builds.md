# Emulator project build assessment

## Upstream/reference metadata

The files below remain imported, immutable history; their settings have not been rewritten.

| Platform | Imported project metadata | Toolset | Historical output layout |
| --- | --- | --- | --- |
| Epoch | VS 2012-labelled solution; C++ project `ToolsVersion="15.0"`; Windows 10 SDK | `v145` | Solution-local `Debug32/64` and `Release32/64` |
| JPM System 6 | VS 18-labelled solution; C++ project `ToolsVersion="15.0"`; Windows 10 SDK | `v143` | Solution-local `Debug32/64` and `Release32/64` |
| MPU5 | VS 18-labelled solution; Windows 10 SDK | `v145` | Solution-local `Debug32/64` and `Release32/64` |

JPM System 6's optional `BuildSupport/AmberRuntimeDeps.props` import remains conditional; that file was absent from the imported delivery.

## Maintained Oasis metadata

| Project | IDE/toolset | Solution configurations | Output |
| --- | --- | --- | --- |
| `AmberOasis.Epoch` | Visual Studio 2022 / `v143` | `Debug|x64`, `Release|x64` | `build/bin/$(Platform)/$(Configuration)/AmberOasis.Epoch.dll` |
| `AmberOasis.JPMSystem6` | Visual Studio 2022 / `v143` | `Debug|x64`, `Release|x64` | `build/bin/$(Platform)/$(Configuration)/AmberOasis.JPMSystem6.dll` |
| `AmberOasis.MPU5` | Visual Studio 2022 / `v143` | `Debug|x64`, `Release|x64` | `build/bin/$(Platform)/$(Configuration)/AmberOasis.MPU5.dll` |

The maintained projects also retain clean Win32 project configurations, but the umbrella solution intentionally advertises only the required x64 configurations. Intermediate files go to `build/obj/<ProjectName>/$(Platform)/$(Configuration)/`.
