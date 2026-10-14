# Building doomfix

[← README](../README.md) · [Technical notes](TECHNICAL.md)

This page is for building doomfix from source, running its test program, packaging a release and running the
RenderDoc analysis scripts. To just use the fix, see the [README](../README.md).

## Prerequisites

| What | Why | How to install |
|---|---|---|
| Windows 10 or 11, 64-bit | doomfix is a Windows DLL loaded into the 64-bit game | — |
| Visual Studio 2022 Build Tools with the **Desktop development with C++** workload | MSVC compiler, Windows SDK, and the CMake + Ninja that ship with it | `winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"` (about 5–7 GB, asks for administrator rights) |
| Git | to clone the repository; CMake also uses it to download Vulkan-Headers when no Vulkan SDK is installed | `winget install --id Git.Git -e` |
| Vulkan SDK *(optional)* | if present, its headers are used instead of downloading Vulkan-Headers; it also brings the validation layers | `winget install KhronosGroup.VulkanSDK` (see the note about vkconfig below) |
| RenderDoc 1.46 *(optional)* | only for the analysis scripts in `analysis/` | <https://renderdoc.org> |

`--includeRecommended` adds MSVC x64, the Windows SDK and "C++ CMake tools for Windows" (CMake and Ninja).
The layer needs only the Vulkan *headers*; it does not link against `vulkan-1.lib`.

**About vkconfig:** Vulkan Configurator from the Vulkan SDK registers its own loader settings file, and the loader
uses only one such file. While vkconfig manages the loader, `install.bat` cannot set the layer order and prints a
warning. Close vkconfig (or turn off "Vulkan Loader Management") before installing doomfix.

## Build

```bat
git clone https://github.com/billymooreq/DoomEternal-v1.1--RTXFix.git doomfix
cd doomfix
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
```

`build.ps1` finds Visual Studio through `vswhere`, opens an x64 developer environment and builds with CMake + Ninja
into `build\`. The output goes to `build\bin\`:

- `doomfix_layer.dll`, `VK_LAYER_DOOMFIX.json` (the layer manifest) and the `.pdb`;
- `doomfix_barrier_test.exe`, the test program (turn it off with `-DDOOMFIX_BUILD_TESTS=OFF`).

The default configuration is `RelWithDebInfo`; use `-Config Release` for another one.
If no Vulkan SDK is installed, the first configure downloads Vulkan-Headers `v1.4.341` with Git.

Manual build from an "x64 Native Tools Command Prompt for VS 2022":

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

## Install and run from source

```bat
install.bat
run_doom_fixed.bat
```

`install.bat` (`scripts\install.ps1`) picks up the DLL from `build\bin\` automatically. After a rebuild, close the game
and run `install.bat` again: the installed copy lives in `%LOCALAPPDATA%\doomfix\layer` and is not updated by itself.
Installation options (`-SourceDir`, `-GameExe`, `-NoOrdering`) are described in the header of `scripts\install.ps1`
and in [TECHNICAL.md](TECHNICAL.md#layer-order).

## Test program

`doomfix_barrier_test.exe` checks the indirect-barrier-fix through the real Vulkan loader, without the game.
It tests the **installed** layer, so run `install.bat` first. For each mode (`on`, `strict`, `aggressive`, `off`)
it starts itself in a child process with `DOOMFIX_ENABLE=1`, debug logging and a separate log folder. The child
creates a Vulkan 1.3 device and records barriers with the masks taken from a game capture, plus edge cases
(an execution-only barrier, a buffer barrier, an already correct barrier, a barrier that only `strict` changes,
`vkCmdWaitEvents`, `vkCmdPipelineBarrier2`, `vkCmdWaitEvents2`). Nothing is submitted to the GPU. The parent then
reads the layer log and compares what was passed down to the driver with the expected masks for that mode.

```bat
install.bat
build\bin\doomfix_barrier_test.exe
```

It prints `ok` or `FAIL` for every case and ends with `ALL PASSED` (exit code 0). On failure the layer logs are
kept and their folder is printed. With `--soak`, an extra run in mode `on` has four threads re-record command
buffers for about 12 seconds and checks that the layer's counters match the number of recorded barriers.

The program also prints whether `VK_AMD_memory_overallocation_behavior` is visible to it. With the default settings it
must be visible whenever the driver exposes it; with `DOOMFIX_HIDE_OVERALLOCATION=1` it must not be. Drivers that do not
expose the extension (NVIDIA 617.42, for example) print `no` in both cases; the startup log line
`hide-device-extensions: ... hiding ON/OFF` still shows that the switch works.

## Making a release zip

The launchers and the installer work both in the repository (DLL in `build\bin\`) and in a release folder
(DLL in `bin\`). A release zip has this layout:

```
doomfix-<version>\
  bin\doomfix_layer.dll
  bin\VK_LAYER_DOOMFIX.json
  scripts\install.ps1  scripts\uninstall.ps1  scripts\launch.ps1  scripts\steam_mode.ps1
  install.bat  uninstall.bat  run_doom_fixed.bat  run_fix_strict.bat  run_fix_off.bat  run_fix_aggressive.bat
  enable_steam_mode.bat  disable_steam_mode.bat
  README.md  README.ru.md  LICENSE
```

It can be assembled from a Release build like this:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Config Release -BuildDir build-release
$v = 'doomfix-0.2.0'; $d = "dist\$v"
New-Item -ItemType Directory -Force "$d\bin", "$d\scripts" | Out-Null
Copy-Item build-release\bin\doomfix_layer.dll, build-release\bin\VK_LAYER_DOOMFIX.json "$d\bin"
Copy-Item scripts\install.ps1, scripts\uninstall.ps1, scripts\launch.ps1, scripts\steam_mode.ps1 "$d\scripts"
Copy-Item install.bat, uninstall.bat, run_doom_fixed.bat, run_fix_strict.bat, run_fix_off.bat, run_fix_aggressive.bat, enable_steam_mode.bat, disable_steam_mode.bat, README.md, README.ru.md, LICENSE $d
Compress-Archive -Path $d -DestinationPath "dist\$v.zip" -Force
```

## Analysis scripts

`analysis/` contains the scripts used to find the bug in RenderDoc captures and the investigation report
([analysis/report.md](../analysis/report.md), in Russian). They need RenderDoc 1.46: its Python module is built into
`qrenderdoc.exe`, so the scripts run inside it through a wrapper:

```bat
powershell -ExecutionPolicy Bypass -File analysis\run_analysis.ps1 -Script s20_overview -Capture <path to capture>.rdc -Out mycapture
```

- `-Capture`: the `.rdc` file. It is opened read-only.
- `-Out`: optional subfolder of `analysis\` for the results.
- `-RenderDoc`: the RenderDoc folder, if it is not `%ProgramFiles%\RenderDoc` (or set `DOOMFIX_RENDERDOC`).
- `-Set @{ NAME = 'value' }`: extra parameters for the scripts that take them (`s24`–`s29` read
  `DOOMFIX_DRAW`, `DOOMFIX_TARGET`, `DOOMFIX_PIXEL`, ... — see the header of each script).

The wrapper checks the paths, passes them to the script through environment variables and prints the script's log.
`analysis\scripts\shader_diff.py` is plain Python 3:
`python analysis\scripts\shader_diff.py <old.spv> <new.spv> <tag> --renderdoc <RenderDoc folder>`.

Notes:

- To take your own captures with doomfix loaded, see
  [Analysing the game in RenderDoc](TECHNICAL.md#analysing-the-game-in-renderdoc) (it explains `DOOMFIX_HIDE_OVERALLOCATION`).
- Event IDs (EIDs) and resource IDs in the early scripts (`s1`–`s17`) are those of the author's captures; adjust them
  for your own capture. The later scripts (`s20`+) take them as parameters.
- Game data produced by the scripts (shaders and their decompilation, shader-debugger traces, buffer contents,
  frame images) is id Software's material and is excluded from the repository by `.gitignore`. Do not commit it.
