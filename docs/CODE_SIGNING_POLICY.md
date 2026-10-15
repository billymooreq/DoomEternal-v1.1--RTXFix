# Code signing policy

Free code signing provided by [SignPath.io](https://signpath.io), certificate by
[SignPath Foundation](https://signpath.org).

> **Status:** the application to SignPath Foundation is pending. Until it is approved and signing is set up, release
> files are **not** code-signed; their integrity can be checked with the SHA256 checksums and the GitHub build
> provenance attestations published with every release.

## What is signed

Only files built from this repository ([billymooreq/DoomEternal-v1.1--RTXFix](https://github.com/billymooreq/DoomEternal-v1.1--RTXFix))
for a published release:

| File | What it is |
|---|---|
| `doomfix_layer.dll` | the Vulkan layer that contains the fix |
| `doomfix_launcher.exe` | the launcher behind the *DOOM Eternal (doomfix)* shortcut |
| `doomfix-<version>-setup.exe` | the installer (built with [Inno Setup](https://jrsoftware.org/isinfo.php)), including its uninstaller |

No third-party binaries are signed. The release files do not bundle third-party libraries: the layer and the launcher
are built from this repository's source code with the Vulkan headers by Khronos (header-only), and the installer
contains Inno Setup's own setup and uninstall program.

## How releases are built

- Release files are built **only by GitHub Actions** ([`.github/workflows/release.yml`](../.github/workflows/release.yml))
  on GitHub-hosted runners, from a version tag (`v<major>.<minor>.<patch>`) in this repository. The workflow builds
  the files from the tagged commit, tests them, and only then publishes the release.
- Files built locally on a developer machine are never signed.
- The build steps are separate: first the layer and the launcher are built, then they are signed, then the installer
  is built from the signed files, then the installer is signed.
- Every release also has a SHA256 checksum file for each download and a
  [build provenance attestation](https://github.com/billymooreq/DoomEternal-v1.1--RTXFix/attestations) that ties the
  files to the workflow run and the commit.

## Team roles

The project currently has one maintainer.

| Role | Who |
|---|---|
| Author, committer and reviewer | [billymooreq](https://github.com/billymooreq) |
| Approver (approves each signing request and release) | [billymooreq](https://github.com/billymooreq) |

Changes from other contributors are accepted only through pull requests that the maintainer has reviewed. Every
signing request is approved manually; nothing is signed automatically.

The maintainer's GitHub account (billymooreq) uses two-factor authentication. The SignPath account will use
two-factor authentication as well.

## Privacy policy

This program will not transfer any information to other networked systems unless specifically requested by the user
or the person installing or operating it.

See [PRIVACY.md](../PRIVACY.md) for the details: doomfix collects and sends no data, and lists what it stores on your
computer and in the registry.

## Reporting problems

If you find a signed file that was not built from this repository, or any other misuse of the signature, please open
an [issue](https://github.com/billymooreq/DoomEternal-v1.1--RTXFix/issues) or contact SignPath Foundation.
