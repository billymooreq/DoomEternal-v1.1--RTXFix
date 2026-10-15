Fixes DOOM Eternal **1.1** on current NVIDIA drivers: the first-person weapon is no longer pitch black in shadow,
and local lights, decals and volumetric light are back. No driver rollback, no changes to game files.
doomfix adds the one synchronization flag that version 1.1 forgets in three GPU barriers, the same change
id Software made in later updates. [Русская версия описания](https://github.com/{{REPO}}/blob/{{TAG}}/README.ru.md)

## Install

There are two ways; pick one.

### Installer (recommended)

1. Download **{{SETUP}}** below and run it. No administrator rights are needed.
2. Confirm the game folder it finds in your Steam library, and choose whether you want a desktop shortcut.
3. Start the game with the **DOOM Eternal (doomfix)** shortcut.

The installer is not code-signed yet, so Windows may show *Windows protected your PC*: click *More info*, then
*Run anyway*. Uninstall from *Settings → Apps → Installed apps → doomfix*.

### Zip, without an exe installer

1. Download **{{ZIP}}** below and unpack it anywhere, for example `Documents\doomfix`. Keep this folder.
2. Run `install.bat` (no administrator rights needed).
3. Start the game with `run_doom_fixed.bat`. The first time, confirm the game folder it finds in your Steam library.

To uninstall, run `uninstall.bat`, then delete the doomfix folder.

Both ways install the same layer to the same place. Already installed doomfix with `install.bat`? The installer
replaces that installation; you can then delete the old doomfix folder.

Pressing Play in Steam starts the game without the fix: use the doomfix shortcut or `run_doom_fixed.bat`
(with Steam running).

### Upgrading from 0.3.1

The optional Steam mode (`enable_steam_mode.bat`) is removed: it turned doomfix on for every Vulkan game started
from Steam, including online games with anti-cheat. If you had turned it on, the installer and `install.bat` turn it
off and tell you so; restart Steam afterwards.

## More

- [README](https://github.com/{{REPO}}/blob/{{TAG}}/README.md): usage, modes, troubleshooting
- [Technical notes](https://github.com/{{REPO}}/blob/{{TAG}}/docs/TECHNICAL.md): what exactly the layer changes and why
- [Privacy](https://github.com/{{REPO}}/blob/{{TAG}}/PRIVACY.md): doomfix collects and sends nothing
- [Code signing policy](https://github.com/{{REPO}}/blob/{{TAG}}/docs/CODE_SIGNING_POLICY.md): which files are signed and how releases are built

## Checksums

SHA256 (also in the `.sha256` files next to the downloads):

```
{{SETUP_SHA256}}  {{SETUP}}
{{SHA256}}  {{ZIP}}
```

In PowerShell, `Get-FileHash .\{{SETUP}}` (or `.\{{ZIP}}`) must print the same value (in capital letters).

{{VERIFY}}
