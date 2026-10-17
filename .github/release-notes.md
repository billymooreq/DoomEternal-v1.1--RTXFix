Fixes missing lighting in DOOM Eternal **1.1** on current NVIDIA drivers: local lights, decals and volumetric light
are back. No driver rollback, no game files changed. [Русский](https://github.com/{{REPO}}/blob/{{TAG}}/README.ru.md)

## Install

1. Download and run **{{SETUP}}** below.
2. Confirm the game folder.
3. Play with the **DOOM Eternal (doomfix)** shortcut while Steam is running.

> [!NOTE]
> Windows may show *Windows protected your PC*: click **More info → Run anyway**.

> [!IMPORTANT]
> Pressing **Play** in Steam starts the game **without** the fix.

Don't want an installer? Use **{{ZIP}}**, see the [README](https://github.com/{{REPO}}/blob/{{TAG}}/README.md#installation).

## What's new in 0.4.3

- New look for the installer: redesigned panels and buttons.
- Shorter welcome page.

<details>
<summary>Checksums and verification</summary>

SHA256 (also in the `.sha256` files next to the downloads):

```
{{SETUP_SHA256}}  {{SETUP}}
{{SHA256}}  {{ZIP}}
```

In PowerShell, `Get-FileHash .\{{SETUP}}` (or `.\{{ZIP}}`) must print the same value (in capital letters).

{{VERIFY}}

</details>

[README](https://github.com/{{REPO}}/blob/{{TAG}}/README.md) ·
[Troubleshooting](https://github.com/{{REPO}}/blob/{{TAG}}/README.md#troubleshooting) ·
[Privacy](https://github.com/{{REPO}}/blob/{{TAG}}/PRIVACY.md) ·
[Code signing policy](https://github.com/{{REPO}}/blob/{{TAG}}/docs/CODE_SIGNING_POLICY.md)
