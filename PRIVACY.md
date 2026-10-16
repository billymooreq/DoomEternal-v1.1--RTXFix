# Privacy

**doomfix does not collect or send any data.** None of its parts (the Vulkan layer `doomfix_layer.dll`, the launcher
`doomfix_launcher.exe`, the installer and the scripts) connects to the network: there is no telemetry, no update
check, no crash reporting and no analytics. You can verify this in the source code.

This program will not transfer any information to other networked systems unless specifically requested by the user
or the person installing or operating it.

## What is stored on your computer

Everything below stays on your computer. doomfix never uploads it anywhere.

### Log files

While the layer is active in a program, it writes one log file per start to `%LOCALAPPDATA%\doomfix\logs\` (another folder can be set with the
`DOOMFIX_DATA_DIR` or `DOOMFIX_LOG_DIR` environment variables). The layer is active only in a program started with
`DOOMFIX_ENABLE=1`, and only the doomfix shortcut and the `run_*.bat` launchers set that variable, for the game they
start; it is never stored for your Windows user or the system. A log contains:

- the date and time, the process and thread IDs;
- the full path of the program's exe file, for example `C:\Users\<your name>\...\DOOMEternalx64vk.exe`. **Local paths
  like this can include your Windows user name**;
- the application and engine name the program reports to Vulkan, your GPU's name, lists of Vulkan extensions, the
  file names of other Vulkan layers loaded into the program;
- what the fix changed (counts of patched GPU barriers).

The doomfix launcher (`doomfix_launcher.exe`, which the installer's shortcuts start) writes its own log file to the
same folder each time it runs, named `doomfix_launcher_<date-time>_<pid>.log`. It contains:

- the date and time, the process and thread IDs, the doomfix and Windows versions;
- the full paths of the launcher, of the game's exe file and of the shortcut it was started from (these can include
  your Windows user name);
- the file name of the program that started the launcher (usually `explorer.exe`);
- whether the launcher runs as administrator and why: the integrity level of the process, the User Account Control
  settings of Windows (`EnableLUA`, `ConsentPromptBehaviorAdmin`, `FilterAdministratorToken`), whether the account is
  the built-in Administrator, and the compatibility settings of the launcher and the game, such as *Run this program
  as an administrator*;
- where the doomfix layer is registered, whether Steam is running, whether the game started and with which
  integrity level (if the launcher runs as administrator, it starts the game without administrator rights).

Log files are not deleted automatically, and the uninstaller keeps them; delete the `logs` folder yourself if you do
not need it. If you attach a log to a GitHub issue, look through it first: it is published together with the
issue, and you may want to replace your user name in paths.

### Game folder

The folder of DOOM Eternal is saved in a small text file `doomfix_game_path.txt`:

- by the installer and `doomfix_launcher.exe`: in `%LOCALAPPDATA%\doomfix\`;
- by `run_doom_fixed.bat` and the other launchers of the zip: next to the `.bat` files.

To find the game, the installer and `run_doom_fixed.bat` read where Steam is installed from the registry and the
list of Steam libraries from `steamapps\libraryfolders.vdf`. They only read these; nothing is changed in Steam.

### Installed files

The layer, the launcher and the loader settings file go to `%LOCALAPPDATA%\doomfix\` (subfolders `layer` and
`loader`). The installer adds a *DOOM Eternal (doomfix)* shortcut to the Start menu and, if you choose so, to the
desktop. While it runs, the installer unpacks its own files to a temporary folder in `%TEMP%` and removes them when
it finishes.

## What is changed in Windows

doomfix needs no administrator rights and changes only settings of your own Windows user (`HKEY_CURRENT_USER`):

| Registry | What | Set by |
|---|---|---|
| `HKCU\SOFTWARE\Khronos\Vulkan\ImplicitLayers` | registers the doomfix layer with the Vulkan loader | installer, `install.bat` |
| `HKCU\SOFTWARE\Khronos\Vulkan\LoaderSettings` | points the Vulkan loader to doomfix's settings file, which puts the doomfix layer first in the layer order; the file applies to all Vulkan programs, but the layer is inactive without `DOOMFIX_ENABLE=1` | installer, `install.bat` |
| `HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\{B3F23E39-9C8E-4D02-B62B-4A2E262905A6}_is1` | the entry in *Settings → Apps → Installed apps* | installer |

Version 0.3.1 had an optional "Steam mode" that stored `DOOMFIX_ENABLE=1` in `HKCU\Environment`, turning the layer on
for every Vulkan program of the user. It is removed: if that value is still there, the installer, `install.bat` and
uninstalling delete it.

## Removing everything

Uninstalling (*Settings → Apps → Installed apps → doomfix*, or `uninstall.bat` for the zip) removes all registry values
from the table above (and the old Steam mode value, if present), the installed files and the shortcuts. Only the log files in `%LOCALAPPDATA%\doomfix\logs\`
stay; delete that folder by hand if you want. For the zip, also delete its folder (with `doomfix_game_path.txt`).

## Downloads

doomfix is downloaded from GitHub Releases. GitHub's own [privacy statement](https://docs.github.com/site-policy/privacy-policies/github-general-privacy-statement)
applies to that download and to the GitHub issues; doomfix itself does not contact GitHub or anyone else.

## Questions

Open an issue in the [repository](https://github.com/billymooreq/DoomEternal-v1.1--RTXFix/issues).
