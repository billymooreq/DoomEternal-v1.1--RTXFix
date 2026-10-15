# DoomEternal-v1.1--RTXFix — fix environment lighting for DOOM Eternal 1.1 on NVIDIA

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
- **No data collection:** doomfix never connects to the network. Details: [PRIVACY.md](PRIVACY.md).
- **Easy to remove:** *Settings → Apps* (or `uninstall.bat` for the zip) takes it out completely.

## Compatibility
| | |
|---|---|
| **Tested** | GeForce RTX 4070, NVIDIA driver 617.42, DOOM Eternal 1.1 (Steam) |
|            | RTX 3070, Driver (616.64)                                       |
|            | RTX 3070Ti, Driver (Unknown) |
|            | RTX 5070 ti, Driver (617.14) |
| **Should work** | any NVIDIA GPU with a recent driver that shows the black-weapon bug |
| **AMD** | the bug does not happen on AMD; the layer does not break anything there |
| **OS** | Windows only (64-bit) |

Tried it on another GPU or driver? Please share the result, good or bad, in [Issues](../../issues):
GPU, driver version, game version and whether the weapon looks right.

## Installation

Open [Releases](../../releases) and pick one of the two downloads in the *Assets* of the newest release.
Both install the same layer for your Windows user; no administrator rights are needed.

**Installer (recommended): `doomfix-<version>-setup.exe`**

1. Run it. It finds DOOM Eternal in your Steam library (or click *Browse* to choose the folder with
   `DOOMEternalx64vk.exe`).
2. Choose whether you want a desktop shortcut (a Start menu shortcut is always created).
3. Start the game with the **DOOM Eternal (doomfix)** shortcut (desktop or Start menu).

The installer is not code-signed yet (see the [code signing policy](docs/CODE_SIGNING_POLICY.md)), so Windows may show
*Windows protected your PC*: click *More info*, then *Run anyway*.

**Zip, without an exe installer: `doomfix-<version>.zip`**

1. Unpack it anywhere you like, for example `Documents\doomfix`. Keep this folder: the launcher and the uninstaller live there.
2. Run `install.bat`.
3. Start the game with `run_doom_fixed.bat` (see [Usage](#usage)).

Already installed doomfix with `install.bat` and now want the installer? Just run it: it replaces the old installation
in place, and you can delete the old doomfix folder afterwards.

**Upgrading from 0.3.1:** the optional *Steam mode* (`enable_steam_mode.bat`) is gone, see
[Pressing Play in Steam](#pressing-play-in-steam). If you had turned it on, the installer and `install.bat` turn it off
and tell you so; restart Steam afterwards.

Every release is built from this source code by GitHub Actions and lists the SHA256 of both downloads in its description.
Releases marked *Pre-release* are test builds. To build doomfix yourself instead, see [docs/BUILDING.md](docs/BUILDING.md).

## Usage

### Starting the game

**Installer:** double-click the **DOOM Eternal (doomfix)** shortcut. That's it. Keep Steam running.
If the game folder changes, the shortcut asks for the new one.

**Zip:** double-click **`run_doom_fixed.bat`**. The first time, it finds DOOM Eternal in your Steam library and asks
you to confirm the folder (or asks you to type it in if the game is somewhere else). The folder is remembered, so from
then on the game just starts. Keep Steam running.

### Pressing Play in Steam

Pressing Play in Steam starts the game **without** the fix. doomfix is turned on only for the game started by the
doomfix shortcut or `run_doom_fixed.bat`, and never for other programs. Steam itself still has to run.

Version 0.3.1 had an optional *Steam mode* that turned the layer on for every Vulkan game started from Steam. It is
removed: that included online games with anti-cheat, which is an unnecessary risk.

### Checking that it works

Every game start writes a log file to `%LOCALAPPDATA%\doomfix\logs\DOOMEternalx64vk_<date-time>_<pid>.log`.
Once you are in a level, it should contain lines like these:

```
INFO  doomfix 0.4.0 (VK_LAYER_DOOMFIX) loaded into <game folder>\DOOMEternalx64vk.exe, pid 1234
INFO  indirect-barrier-fix [on]: PATCHED vkCmdPipelineBarrier in command buffer 000002CD804070D0
INFO  indirect-barrier-fix [on]: barriers with DRAW_INDIRECT dst 1800 (+1800), patched 1800 (+1800), ...
```

The last line repeats every few seconds while you play, with growing numbers.

### Modes

The fix has four modes, selected with the `DOOMFIX_INDIRECT_FIX` variable. The zip has a launcher for each mode;
with the installer, start `%LOCALAPPDATA%\doomfix\doomfix_launcher.exe --mode <mode>` (for example from a copy of the
shortcut with `--mode strict` added to its target).

| Mode | Launcher | What it does |
|---|---|---|
| `on` (default) | `run_doom_fixed.bat` | The fix, exactly as id Software made it in later updates. Use this one. |
| `strict` | `run_fix_strict.bat` | `on` plus one extra safety wait (this was `on` in v0.2). Try it if `on` ever shows flickering or missing lights. |
| `off` | `run_fix_off.bat` | The layer is loaded but changes nothing, to compare before/after. |
| `aggressive` | `run_fix_aggressive.bat` | Uses heavier, "wait for everything" barriers. A fallback in case `on` is ever not enough; normally not needed. |

## Uninstall

- **Installer:** *Settings → Apps → Installed apps → doomfix → Uninstall*.
- **Zip:** run `uninstall.bat`, then delete the doomfix folder.

Both remove the layer and its registry entries. Log files are kept: delete `%LOCALAPPDATA%\doomfix`
afterwards if you do not need them.

## Troubleshooting

**The fix does not work: the weapon is still black.**
Make sure the game was started with the doomfix shortcut or `run_doom_fixed.bat`, not with Play in Steam, and that
nothing involved runs **as administrator**: not the game, not the shortcut, not `run_doom_fixed.bat`.
Windows does not load doomfix into programs started as administrator, so the game then runs without the fix.
Also check the *Compatibility* tab in the properties of `DOOMEternalx64vk.exe`: the box
*Run this program as an administrator* must be unticked. The doomfix shortcut and the installer warn you about this.

**The game starts, but the weapon is still black and no new log appears.**
DOOM Eternal can restart itself through Steam, and the restarted game no longer has the doomfix variables.
The doomfix shortcut and `run_doom_fixed.bat` prevent this by setting `SteamAppId=782330`; make sure Steam is running
before you use them.

**The installer or `install.bat` warns about another loader settings file.**
That file comes from Vulkan Configurator (vkconfig), part of the Vulkan SDK. The Vulkan loader only uses one such file,
so doomfix does not overwrite it. Close vkconfig or turn off its "Vulkan Loader Management", then run the installer
(or `install.bat`) again.

**My antivirus or SmartScreen complains about doomfix.**
The files are not code-signed yet, and the DLL loads into the game like an overlay does, which some antivirus programs
dislike. Every release has SHA256 checksums and a build provenance attestation to check that the files come from
this repository.
The source code is open: you can build the DLL yourself ([docs/BUILDING.md](docs/BUILDING.md)) and/or add an exception
for the doomfix folder and `%LOCALAPPDATA%\doomfix\layer`.

**There is no log at all.**
Check that the installer or `install.bat` finished without errors, and that the game was started with
`DOOMFIX_ENABLE=1` (through the doomfix shortcut or `run_doom_fixed.bat`). `run_doom_fixed.bat -DryRun` checks the installation and
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

## Privacy and code signing

- [PRIVACY.md](PRIVACY.md): doomfix collects and sends no data; what it stores on your computer and in the registry.
- [Code signing policy](docs/CODE_SIGNING_POLICY.md): which files are signed and how releases are built.

## License

[MIT](LICENSE)
