# Fix environment lighting for DOOM Eternal 1.1 on NVIDIA

**English** | [Русский](README.ru.md)

On current NVIDIA drivers, DOOM Eternal **1.1** loses its lighting: local lights, decals and volumetric light disappear.
doomfix is a small Vulkan layer that fixes this **without rolling your driver back to 516.94**, which is not even
possible on RTX 40 and RTX 50 cards.

| Before | After |
|---|---|
| ![Before: lighting is missing](docs/images/before.png) | ![After: lighting is back](docs/images/after.png) |

## Installation

Download the newest version from [Releases](../../releases/latest). No administrator rights needed.

1. Run `doomfix-<version>-setup.exe`.
2. Confirm the game folder (it is found in your Steam library automatically).
3. Play with the **DOOM Eternal (doomfix)** shortcut while Steam is running.

> [!NOTE]
> Windows may show *Windows protected your PC*: click **More info → Run anyway**.
> The installer is not code-signed yet.

> [!IMPORTANT]
> Pressing **Play** in Steam starts the game **without** the fix.

<details>
<summary>Zip version, without the installer</summary>

1. Unpack `doomfix-<version>.zip` into a folder you will keep, for example `Documents\doomfix`.
2. Run `install.bat`.
3. Play with `run_doom_fixed.bat` while Steam is running. Do not run it as administrator.

To switch to the installer later, just run it: it replaces the zip installation.
</details>

## Uninstall

- **Installer:** *Settings → Apps → Installed apps → doomfix → Uninstall*.
- **Zip:** run `uninstall.bat`, then delete the doomfix folder.

Log files stay in `%LOCALAPPDATA%\doomfix`; delete it if you do not need them.

## Is this safe?

- It is a **Vulkan layer**: the same kind of add-on that ReShade, MSI Afterburner / RivaTuner and the Steam overlay use.
  Windows loads it into the game when the game starts.
- **No game files are changed.** Nothing in the game folder is touched.
- **Gameplay and timings are not affected.** The layer adds one synchronization flag to three GPU barriers in the
  game's lighting code, exactly as id Software did themselves in later patches. Everything else passes through as is.
- **Open source** (MIT license): you can read every line and build it yourself.
- **Built in the open:** every release is built from this source by GitHub Actions, with SHA256 checksums and build
  attestation.
- **No data collection:** doomfix never connects to the network. Details: [PRIVACY.md](PRIVACY.md).
- **Easy to remove:** *Settings → Apps* (or `uninstall.bat` for the zip) takes it out completely.

## Compatibility

- **Game:** DOOM Eternal 1.1 (Steam), Windows 64-bit.
- **GPU:** NVIDIA with a recent driver. AMD is not affected.

### Tested by

| Tester | GPU | Driver | Result |
|---|---|---|---|
| Callmesync_ | GeForce GTX 1070 | 582.78 | ✅ works ¹ |
| archerthe | GeForce RTX 3060 | 617.42 | ✅ works |
| Bodynuke | GeForce RTX 3070 | 616.64 | ✅ works ² |
| GhostDMG_ | GeForce RTX 3070 Ti | unknown | ✅ works |
| billymoore (author) | GeForce RTX 4070 | 617.42 | ✅ works |
| lavvy_lav | GeForce RTX 4070 Ti Super | unknown | ✅ works |
| Fisukki | GeForce RTX 5070 Ti | 617.14 | ✅ works |

¹ **Callmesync_**: "ate the fps apparently". Note: the lower FPS is most likely the cost of the lighting that is back;
without the fix the game skips this work.

² **Bodynuke**: "Profile settings reset on their own". Note: doomfix does not touch the game's settings; most likely
version 1.1 and the current version of the game share settings files with different contents.

Tried it on another GPU or driver? Share the result, good or bad, in [Issues](../../issues): GPU, driver version and
whether the lighting is back.

## Troubleshooting

| Problem | What to do |
|---|---|
| Lighting is still missing | Start the game with the **doomfix shortcut**, not Play in Steam. Steam must be running. |
| Windows or antivirus warns about doomfix | The files are not code-signed yet. Click *More info → Run anyway*, or add an exception for `%LOCALAPPDATA%\doomfix`. |
| Installer warns about "another loader settings file" | Close Vulkan Configurator (vkconfig) and run the installer again. |
| Still not working | [Open an issue](../../issues) and attach the files from `%LOCALAPPDATA%\doomfix\logs`. |

## How it works

Every frame, DOOM Eternal sorts lights, decals and reflection probes into a 3D grid of small "clusters" on the GPU.
This happens in two steps: the first step writes down how much work the second step has to do, and the second step
reads those numbers. Before the read, the game has to tell the GPU to make the freshly written numbers visible,
and version 1.1 forgets one flag for that (`VK_ACCESS_INDIRECT_COMMAND_READ_BIT`) in three places.
AMD GPUs happen to see the right numbers anyway; current NVIDIA drivers read stale zeros, so every cluster keeps only
the sun, and local lights, decals and volumetric light disappear. doomfix adds the missing flag while the game records
its GPU commands: the same change id Software made in later updates.

More details: [docs/TECHNICAL.md](docs/TECHNICAL.md). The full investigation with RenderDoc captures and a comparison
with the current game version: [analysis/report.md](analysis/report.md) (in Russian).

## Technical details

For curious users and bug reports; you do not need any of this to use doomfix.

### Checking that it works

Every game start writes a log file to `%LOCALAPPDATA%\doomfix\logs\DOOMEternalx64vk_<date-time>_<pid>.log`.
Once you are in a level, it should contain lines like these:

```
INFO  doomfix 0.4.2 (VK_LAYER_DOOMFIX) loaded into <game folder>\DOOMEternalx64vk.exe, pid 1234
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

Everything else (what exactly the layer changes, the log, the launcher, administrator rights):
[docs/TECHNICAL.md](docs/TECHNICAL.md). To build doomfix yourself: [docs/BUILDING.md](docs/BUILDING.md).

## Credits

- **billymooreq**: investigation and fix.
- Thanks to the testers listed in [Tested by](#tested-by) for checking doomfix on their GPUs.
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
