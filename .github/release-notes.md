Fixes DOOM Eternal **1.1** on current NVIDIA drivers: the first-person weapon is no longer pitch black in shadow,
and local lights, decals and volumetric light are back. No driver rollback, no changes to game files.
doomfix adds the one synchronization flag that version 1.1 forgets in three GPU barriers, the same change
id Software made in later updates. [Русская версия описания](https://github.com/{{REPO}}/blob/{{TAG}}/README.ru.md)

## Install

1. Download **{{ZIP}}** below and unpack it anywhere, for example `Documents\doomfix`. Keep this folder.
2. Run `install.bat` (no administrator rights needed).
3. Start the game with `run_doom_fixed.bat`. The first time, confirm the game folder it finds in your Steam library.

Prefer pressing Play in Steam? Run `enable_steam_mode.bat` once; `disable_steam_mode.bat` turns it off.

## Uninstall

Run `uninstall.bat`, then delete the doomfix folder.

## More

- [README](https://github.com/{{REPO}}/blob/{{TAG}}/README.md): usage, modes, troubleshooting
- [Technical notes](https://github.com/{{REPO}}/blob/{{TAG}}/docs/TECHNICAL.md): what exactly the layer changes and why

## Checksum

SHA256 of `{{ZIP}}` (also in `{{ZIP}}.sha256`):

```
{{SHA256}}
```

In PowerShell, `Get-FileHash .\{{ZIP}}` must print the same value (in capital letters).

{{VERIFY}}
