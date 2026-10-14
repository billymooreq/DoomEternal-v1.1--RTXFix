# doomfix — black weapon fix for DOOM Eternal 1.1 on NVIDIA

**English** | [Русский](README.ru.md)

On current NVIDIA drivers, DOOM Eternal **1.1** draws the shaded parts of the first-person weapon pitch black,
and the world loses its local lights, decals and volumetric light. doomfix is a small Vulkan layer that fixes this
**without rolling your driver back to 516.94**, which is not even possible on RTX 40 and RTX 50 cards.

| Before | After |
|---|---|
| ![Before: the weapon is black](docs/images/before.png) | ![After: the weapon is lit](docs/images/after.png) |

## Is this safe?

- It is a **Vulkan layer**: the same kind of add-on that ReShade, MSI Afterburner / RivaTuner and the Steam overlay use.
  Windows loads it into the game when the game starts.
- **No game files are changed.** Nothing in the game folder is touched.
- **Gameplay and timings are not affected.** The layer adds one synchronization flag to three GPU barriers in the
  game's lighting code, exactly as id Software did themselves in later patches. Everything else passes through as is.
- **Open source** (MIT license): you can read every line and build it yourself.
- **Easy to remove:** `uninstall.bat` takes it out completely.

**Speedrunners:** whether a tool like this is allowed in runs is up to the moderators of each leaderboard.
Please check your leaderboard's rules before submitting runs made with it.

## Compatibility

| | |
|---|---|
| **Tested** | GeForce RTX 4070, NVIDIA driver 617.42, DOOM Eternal 1.1 (Steam) |
| **Should work** | any NVIDIA GPU with a recent driver that shows the black-weapon bug |
| **AMD** | the bug does not happen on AMD; the layer does not break anything there |
| **OS** | Windows only (64-bit) |

Tried it on another GPU or driver? Please share the result, good or bad, in [Issues](../../issues):
GPU, driver version, game version and whether the weapon looks right.

## Installation

> **There are no ready-made downloads yet**, so for now doomfix has to be built from source:
> see [docs/BUILDING.md](docs/BUILDING.md). Pre-built versions will appear on the [Releases](../../releases) page,
> and then installation will be just this:

1. Download the latest `doomfix-<version>.zip` from [Releases](../../releases).
2. Unpack it anywhere you like, for example `Documents\doomfix`. Keep this folder: the launcher and the uninstaller live there.
3. Run `install.bat`. No administrator rights are needed.
4. Start the game with `run_doom_fixed.bat`.

## Usage

### Starting the game

**With the launcher (recommended):** run `run_doom_fixed.bat`. The first time, it finds DOOM Eternal in your Steam
libraries (or asks where the game is) and remembers the folder in `doomfix_game_path.txt` next to the launcher.
Keep Steam running. To pick another folder later, delete that file.

**From Steam:** the layer only switches on in programs that have the environment variable `DOOMFIX_ENABLE=1`.
Set it for your Windows user, then fully restart Steam so that it picks the variable up:

```bat
setx DOOMFIX_ENABLE 1
```

With the variable set, every Vulkan program you start gets the layer. That is harmless, but if you prefer, use the
launcher instead. To remove the variable:

```bat
reg delete HKCU\Environment /v DOOMFIX_ENABLE /f
```

### Checking that it works

Every game start writes a log file to `%LOCALAPPDATA%\doomfix\logs\DOOMEternalx64vk_<date-time>_<pid>.log`.
Once you are in a level, it should contain lines like these:

```
INFO  doomfix 0.2.0 (VK_LAYER_DOOMFIX) loaded into <game folder>\DOOMEternalx64vk.exe, pid 1234
INFO  indirect-barrier-fix [on]: PATCHED vkCmdPipelineBarrier in command buffer 000002CD804070D0
INFO  indirect-barrier-fix [on]: barriers with DRAW_INDIRECT dst 1800 (+1800), patched 1800 (+1800), ...
```

The last line repeats every few seconds while you play, with growing numbers.

### Modes

The fix has three modes, selected with the `DOOMFIX_INDIRECT_FIX` variable. Each mode has its own launcher:

| Mode | Launcher | What it does |
|---|---|---|
| `on` (default) | `run_doom_fixed.bat` | The fix. Use this one. |
| `off` | `run_fix_off.bat` | The layer is loaded but changes nothing, to compare before/after. |
| `aggressive` | `run_fix_aggressive.bat` | Uses heavier, "wait for everything" barriers. A fallback in case `on` is ever not enough; normally not needed. |

## Uninstall

1. Run `uninstall.bat`.
2. Delete the doomfix folder.
3. Optional: delete `%LOCALAPPDATA%\doomfix` (only log files are left there), and if you used `setx`,
   remove the variable as shown above.

## Troubleshooting

**The game starts, but the weapon is still black and no new log appears.**
DOOM Eternal can restart itself through Steam, and the restarted game no longer has the doomfix variables.
The launcher prevents this by setting `SteamAppId=782330`; make sure Steam is running before you use it.
If you start the game from Steam, set `DOOMFIX_ENABLE` with `setx` and restart Steam completely (exit it from the tray).

**`install.bat` warns about another loader settings file.**
That file comes from Vulkan Configurator (vkconfig), part of the Vulkan SDK. The Vulkan loader only uses one such file,
so doomfix does not overwrite it. Close vkconfig or turn off its "Vulkan Loader Management", then run `install.bat` again.

**My antivirus complains about `doomfix_layer.dll`.**
The DLL is not code-signed and loads into the game like an overlay does, which some antivirus programs dislike.
The source code is open: you can build the DLL yourself ([docs/BUILDING.md](docs/BUILDING.md)) and/or add an exception
for the doomfix folder and `%LOCALAPPDATA%\doomfix\layer`.

**There is no log at all.**
Check that `install.bat` finished without errors, and that the game was started with `DOOMFIX_ENABLE=1`
(through the launcher, or `setx` plus a Steam restart). `run_doom_fixed.bat -DryRun` checks the installation and
the game folder without starting the game.

## How it works

Every frame, DOOM Eternal sorts lights, decals and reflection probes into a 3D grid of small "clusters" on the GPU.
This happens in two steps: the first step writes down how much work the second step has to do, and the second step
reads those numbers. Before the read, the game has to tell the GPU to make the freshly written numbers visible,
and version 1.1 forgets one flag for that (`VK_ACCESS_INDIRECT_COMMAND_READ_BIT`) in three places.
AMD GPUs happen to see the right numbers anyway; current NVIDIA drivers read stale zeros, so every cluster ends up
with only the sun, and a weapon in shadow gets no light at all. doomfix adds the missing flag while the game records
its GPU commands: the same change id Software made in later updates.

More details: [docs/TECHNICAL.md](docs/TECHNICAL.md). The full investigation with RenderDoc captures and a comparison
with the current game version: [analysis/report.md](analysis/report.md) (in Russian).

## Credits

- **billymooreq**: investigation and fix.
- The investigation and the code were done with the help of [Claude Code](https://claude.com/claude-code) by Anthropic.
- Tools: [RenderDoc](https://renderdoc.org) by Baldur Karlsson (capture analysis),
  [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers) by Khronos,
  [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross).
- DOOM Eternal is © id Software / Bethesda. This project is not affiliated with or endorsed by them.

## License

[MIT](LICENSE)
