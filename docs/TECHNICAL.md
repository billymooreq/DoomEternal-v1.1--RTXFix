# doomfix: technical notes

[← README](../README.md) · [Building](BUILDING.md)

## Overview

doomfix is an implicit Vulkan layer, `VK_LAYER_DOOMFIX`, registered for the current Windows user. The loader loads
implicit layers into every Vulkan application, but this one only switches on when the process has
`DOOMFIX_ENABLE=1` in its environment (`enable_environment` in the manifest). `DOOMFIX_DISABLE=1` turns it off even
then. In any other process the loader does not load it at all.

The layer has two features:

| Feature | Default | What it does |
|---|---|---|
| `indirect-barrier-fix` | **on** | adds `VK_ACCESS_INDIRECT_COMMAND_READ_BIT` to the barriers in front of the game's indirect dispatches: the actual fix for the black weapon / missing local lights on NVIDIA |
| `hide-device-extensions` | **off** | hides `VK_AMD_memory_overallocation_behavior` from the game; only needed to make RenderDoc captures (see [Analysing the game in RenderDoc](#analysing-the-game-in-renderdoc)) |

Game files are never touched: everything happens in API calls inside the game process.

## The bug and the fix

DOOM Eternal 1.1 builds per-frame cluster lists of lights, decals and reflection probes ("clustered binning") with
compute shaders. A first compute pass writes indirect arguments, a barrier follows, and `vkCmdDispatchIndirect` then
consumes them; this happens in two phases with the argument buffers rewritten in between. The three barriers in front of
the indirect dispatches look like this in 1.1:

```
srcStageMask  = VERTEX_SHADER | FRAGMENT_SHADER | COMPUTE_SHADER
dstStageMask  = TOP_OF_PIPE | DRAW_INDIRECT | COMPUTE_SHADER
dstAccessMask = UNIFORM_READ | SHADER_READ | SHADER_WRITE          <- no INDIRECT_COMMAND_READ
```

Without `VK_ACCESS_INDIRECT_COMMAND_READ_BIT` the shader writes are not guaranteed to be visible to the indirect
argument fetch. AMD reads indirect arguments through the same coherent cache, so the game works there. On current
NVIDIA drivers the indirect dispatches read stale zero arguments and do no work: every cluster keeps only the sun,
local lights, decals and volumetric lighting disappear, and the first-person weapon, whose ambient light data at the
camera position is zero in this level, turns black in shadow. The current game version fixed exactly these three
barriers by adding the access bit (and dropping `TOP_OF_PIPE`).

The evidence (RenderDoc captures of 1.1 without and with the layer, and of the current version) is in
[analysis/report.md](../analysis/report.md): section 6 for the root cause, section 10 for the confirmation.

### indirect-barrier-fix in detail

The fix is applied when the game **records** its command buffers (DOOM Eternal re-records them every frame).
It intercepts `vkCmdPipelineBarrier`, `vkCmdPipelineBarrier2`, `vkCmdPipelineBarrier2KHR`, `vkCmdWaitEvents`,
`vkCmdWaitEvents2` and `vkCmdWaitEvents2KHR`; the game itself only uses `vkCmdPipelineBarrier` (about 79 calls per
frame, 3 of them with `DRAW_INDIRECT`). Calls without `DRAW_INDIRECT` in `dstStageMask` pass through untouched and
without copies; only an array that actually changes is copied, into a per-thread buffer.

| `DOOMFIX_INDIRECT_FIX` | Effect on barriers with `DRAW_INDIRECT` in `dstStageMask` |
|---|---|
| `on` (default) | the same change id Software made in later updates: adds `INDIRECT_COMMAND_READ` to `dstAccessMask` of every memory and buffer barrier; if there are none, adds a global `VkMemoryBarrier` (writes of the source stages → `INDIRECT_COMMAND_READ`). `srcStageMask` is not changed. |
| `strict` | `on` plus a write-after-read guard: if `srcStageMask` has `COMPUTE_SHADER`, `DRAW_INDIRECT` is added to it, so that the argument buffers are not rewritten while earlier indirect calls still read them. The current game version does without this; it was the `on` behavior in v0.2 and is kept in case `on` ever shows flicker or missing lights. |
| `aggressive` | turns such a barrier into a full one: `ALL_COMMANDS → ALL_COMMANDS`, `MEMORY_WRITE → MEMORY_READ \| MEMORY_WRITE` (image barriers keep their layouts). Fallback only. |
| `off` | changes nothing, only counts and logs what would be patched; for before/after comparisons. |

Rules for events: in `vkCmdWaitEvents` the `srcStageMask` must equal the stage mask of the matching `vkCmdSetEvent`,
so only the destination half is changed. `vkCmdWaitEvents2` is only counted, because its dependency must match the
one given to `vkCmdSetEvent2`; a `WARN` is logged if such a call ever has `DRAW_INDIRECT`. Functions that the device
does not provide (for example `vkCmdPipelineBarrier2KHR` without the extension) are not returned from
`vkGetDeviceProcAddr`.

Logging: the first patch of each function is logged in detail (masks before and after), then a counter every
5 seconds and a final summary when the device is destroyed. With `DOOMFIX_LOG_LEVEL=debug` every patched barrier is
logged.

## Analysing the game in RenderDoc

doomfix is designed to sit above RenderDoc (see [Layer order](#layer-order)), so a capture taken with the layer
enabled already contains the patched barriers; with `DOOMFIX_INDIRECT_FIX=off` it shows the original ones.

**hide-device-extensions.** When the driver exposes `VK_AMD_memory_overallocation_behavior`, DOOM Eternal enables it and
chains `VkDeviceMemoryOverallocationCreateInfoAMD` into `vkCreateDevice`. RenderDoc records that, and such captures fail
to replay. The feature removes the extension from `vkEnumerateDeviceExtensionProperties` and from the extensions the
game enables, and strips the structure from the `pNext` chain, so RenderDoc never sees it. The game does not need the
extension and the fix does not depend on it, so the feature is off by default. Turn it on only for capture sessions:

```bat
set DOOMFIX_ENABLE=1
set DOOMFIX_HIDE_OVERALLOCATION=1
```

In RenderDoc: *Launch Application* → executable `<game folder>\DOOMEternalx64vk.exe`, and under *Environment Variables*
add `DOOMFIX_ENABLE=1`, `DOOMFIX_HIDE_OVERALLOCATION=1` and `SteamAppId=782330` (otherwise the game may restart itself
through Steam and RenderDoc loses the process). The startup log tells which state is active:

```
INFO    hide-device-extensions: VK_AMD_memory_overallocation_behavior hiding ON (DOOMFIX_HIDE_OVERALLOCATION=1)
INFO    hide-device-extensions: VK_AMD_memory_overallocation_behavior hiding OFF (set DOOMFIX_HIDE_OVERALLOCATION=1 for RenderDoc captures)
```

Not every driver exposes the extension (NVIDIA 617.42, for example, does not); then hiding has nothing to do, and the
`vkEnumerateDeviceExtensionProperties` log line shows `hidden: (none)`. `DOOMFIX_HIDE_DEVICE_EXTENSIONS=A,B` hides further
extensions by name (without any `pNext` handling) regardless of `DOOMFIX_HIDE_OVERALLOCATION`.

The scripts used to analyse captures are described in [BUILDING.md](BUILDING.md#analysis-scripts).

## Layer order

Layers closer to the application see its calls first. doomfix must sit **above** RenderDoc, so that captures already
contain the patched barriers and RenderDoc never sees a hidden extension.

The loader reads implicit layers from `HKLM` before `HKCU`, and the first one found ends up closest to the
application. RenderDoc registers itself in `HKLM`, so a plain `HKCU` registration would put doomfix *below* it.
The order is therefore set with the loader's own mechanism (loader 1.3.2xx and newer), a **loader settings** file.
`install.ps1` writes `%LOCALAPPDATA%\doomfix\loader\vk_loader_settings.json` and registers it in
`HKCU\SOFTWARE\Khronos\Vulkan\LoaderSettings`:

```json
{ "file_format_version": "1.0.1",
  "settings_array": [ { "layers": [
      { "control": "auto", "name": "VK_LAYER_DOOMFIX", "path": "...\\VK_LAYER_DOOMFIX.json",
        "treat_as_implicit_manifest": true },
      { "control": "unordered_layer_location" } ] } ] }
```

- `control: auto`: the layer follows its normal rules, i.e. it is active only with `DOOMFIX_ENABLE=1`.
- `unordered_layer_location`: all other layers (RenderDoc, Steam overlay, OBS, ...) keep their usual order after
  doomfix. Without this entry the loader would disable every other layer.
- The rule is global, but changes nothing for processes without `DOOMFIX_ENABLE=1`. It can be limited to one exe with
  `install.ps1 -GameExe "<game folder>\DOOMEternalx64vk.exe"`; the loader compares that path case-sensitively, so the
  global default is more robust.
- If a loader settings file is already registered by Vulkan Configurator (vkconfig), `install.ps1` and the installer
  leave it alone and warn: the loader uses only one such file.

The log line `chain below doomfix: ...` lists the DLLs of all layers below doomfix; if RenderDoc is loaded but missing
from that line, a `WARN` suggests re-running the installer.

## Environment variables

| Variable | Meaning |
|---|---|
| `DOOMFIX_ENABLE=1` | turns the layer on for this process (checked by the loader). The doomfix shortcut and the `run_*.bat` launchers set it for the game process only, never for the user or the system |
| `DOOMFIX_DISABLE=1` | turns it off even if `DOOMFIX_ENABLE=1` |
| `DOOMFIX_INDIRECT_FIX` | `on` (default) / `strict` / `aggressive` / `off` |
| `DOOMFIX_LOG_LEVEL` | `debug` / `info` (default) / `warn` / `error` |
| `DOOMFIX_DATA_DIR` | data root, default `%LOCALAPPDATA%\doomfix` |
| `DOOMFIX_LOG_DIR` | log folder, default `<DATA_DIR>\logs` |
| `DOOMFIX_HIDE_OVERALLOCATION=1` | hide `VK_AMD_memory_overallocation_behavior` (off by default; for RenderDoc captures) |
| `DOOMFIX_HIDE_DEVICE_EXTENSIONS` | extra device extensions to hide, comma-separated (no pNext stripping for them) |
| `SteamAppId=782330` | not read by doomfix: set by the launcher so that the game does not restart itself through Steam (which would drop the variables above) |

## Log

`%LOCALAPPDATA%\doomfix\logs\<exe>_<date-time>_<pid>.log`, one file per process. A typical game start:

```
INFO  doomfix 0.4.0 (VK_LAYER_DOOMFIX) loaded into <game folder>\DOOMEternalx64vk.exe, pid 1234
INFO    hide-device-extensions: VK_AMD_memory_overallocation_behavior hiding OFF (set DOOMFIX_HIDE_OVERALLOCATION=1 for RenderDoc captures)
INFO    indirect-barrier-fix: mode=on (DOOMFIX_INDIRECT_FIX="on")
INFO  feature: hide-device-extensions
INFO  feature: indirect-barrier-fix
INFO  vkCreateInstance: app="DOOMEternal" ..., apiVersion=1.1.0
INFO  vkCreateInstance: chain below doomfix: ... -> vulkan-1.dll
INFO  vkEnumerateDeviceExtensionProperties [NVIDIA GeForce RTX 4070]: N reported below, N visible to app, hidden: (none)
INFO  vkCreateDevice [NVIDIA GeForce RTX 4070]
INFO  indirect-barrier-fix [on]: intercepted at record time; available on device: vkCmdPipelineBarrier=yes, ...
INFO  indirect-barrier-fix: first call of vkCmdPipelineBarrier
INFO  indirect-barrier-fix [on]: PATCHED vkCmdPipelineBarrier in command buffer ...
    before: srcStage=VERTEX_SHADER|FRAGMENT_SHADER|COMPUTE_SHADER dstStage=TOP_OF_PIPE|DRAW_INDIRECT|COMPUTE_SHADER
      memory[0]: UNIFORM_READ|SHADER_READ|SHADER_WRITE -> UNIFORM_READ|SHADER_READ|SHADER_WRITE
    after:  srcStage=DRAW_INDIRECT|VERTEX_SHADER|FRAGMENT_SHADER|COMPUTE_SHADER dstStage=TOP_OF_PIPE|DRAW_INDIRECT|COMPUTE_SHADER
      memory[0]: UNIFORM_READ|SHADER_READ|SHADER_WRITE -> INDIRECT_COMMAND_READ|UNIFORM_READ|SHADER_READ|SHADER_WRITE
INFO  indirect-barrier-fix [on]: barriers with DRAW_INDIRECT dst 1800 (+1800), patched 1800 (+1800), already correct 0; calls: vkCmdPipelineBarrier=47400
```

## Project layout

```
CMakeLists.txt                       build (Vulkan-Headers are fetched if there is no SDK)
layer/VK_LAYER_DOOMFIX.json.in       layer manifest template
src/doomfix_layer.def                exported entry points
src/common/                          config (environment variables), log (file), util (WinAPI)
src/layer/layer.cpp                  core: loader negotiation, vkGet*ProcAddr, instance/device creation,
                                     interceptors, hook tables
src/layer/dispatch.h                 next-layer function tables (X-macro), handle tracking
src/layer/device_create_edit.*       safe editing of VkDeviceCreateInfo (extensions, pNext)
src/layer/chain_diag.*               layer chain diagnostics
src/layer/vk_util.*                  formatting helpers for the log
src/features/feature.h               feature interface (events from the core)
src/features/features.cpp            list of enabled features
src/features/hide_device_extensions.cpp
src/features/indirect_barrier_fix.cpp
src/launcher/                        doomfix_launcher.exe (the installer's shortcuts start the game through it)
src/version.rc.in                    version resource of the DLL and the launcher
tests/barrier_test.cpp               doomfix_barrier_test.exe
installer/doomfix.iss                the installer (Inno Setup)
scripts/                             build, package, installer, install, uninstall, launch, tests (PowerShell)
*.bat                                user entry points: install, uninstall, launchers
analysis/                            RenderDoc analysis scripts and the investigation report
docs/                                this documentation
```

The core only intercepts calls and hands events to the features; all behaviour lives in `src/features/`.
Besides device creation, features get events for barrier recording (`OnCmdPipelineBarrier`, `OnCmdWaitEvents`,
`OnCmdPipelineBarrier2`, `OnCmdWaitEvents2`) and may replace the arguments before they go down the chain.

### Adding a feature that needs a new Vulkan call

1. Add the function to `DOOMFIX_DEVICE_FUNCS` in `src/layer/dispatch.h` (for example `X(CreateShaderModule)`).
2. Add an event to `src/features/feature.h`, e.g.
   `virtual void OnCreateShaderModule(DeviceData&, VkShaderModuleCreateInfo& info, std::vector<uint32_t>& replacement)`.
3. Write the interceptor in `src/layer/layer.cpp`: copy the create info, run it through the features, call
   `device->vk.CreateShaderModule`, and add it to `kDeviceHooks` (`DOOMFIX_OPTIONAL_HOOK` if the device may not
   provide the function).
4. Put the feature into `src/features/<name>.cpp`, register it in `src/features/features.cpp` and add the file to
   `CMakeLists.txt`.
