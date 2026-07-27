# Maintained source provenance

Copy date: **2026-07-27**. Baseline: branch `work`, repository commit `6faa64a4ee581f2d4c2b362cd8371a24b8349a83`.

Source and header files were copied without intentional behavioural changes. Project metadata was recreated for Visual Studio 2022: maintained names and GUIDs, `v143`, repository-level output/intermediate paths, and maintained DLL target names are deliberate differences. The umbrella solution exposes only x64 Debug and Release, while the projects retain the source projects' Win32 configurations.

| Core | Upstream source directory | Maintained destination | Starting project | Maintained project |
| --- | --- | --- | --- | --- |
| Epoch | `Upstream/Epoch/EpochCore/EpochCore/` | `src/Cores/Epoch/` | `Upstream/Epoch/EpochCore/EpochCore/EpochCore.vcxproj` | `src/Cores/Epoch/AmberOasis.Epoch.vcxproj` |
| JPM System 6 | `Upstream/JPMSystem6/` | `src/Cores/JPMSystem6/` | `Upstream/JPMSystem6/JPMSystem6Core.vcxproj` | `src/Cores/JPMSystem6/AmberOasis.JPMSystem6.vcxproj` |
| MPU5 | `Upstream/MPU5/` | `src/Cores/MPU5/` | `Upstream/MPU5/MPU5Core.vcxproj` | `src/Cores/MPU5/AmberOasis.MPU5.vcxproj` |

## Exclusions

Upstream `.sln`, `.vcxproj`, and `.vcxproj.filters` files were not copied as maintained source: new build metadata replaces them. Previously removed `Debug64`/`Release64` build products are generated outputs and were deliberately excluded. No generated output present in the source directories was copied.
