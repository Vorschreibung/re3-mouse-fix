# RE2MouseFix

RE2MouseFix removes Resident Evil 2 Remake's game-side mouse damping,
nonlinear response, pitch-dependent horizontal sensitivity, and erroneous
controller-magnitude scaling. It does not change menus, mouse buttons,
Windows pointer settings, difficulty, animation quality, or field of view.

This release targets the current Steam DX12/public build 11636119 and uses
REFramework's RE2/TDB70 managed plugin API. The development executable used
for validation has SHA-256:

`6caaa815bf9e95f8a841bc81fdf29fec55e836c87bab8a76d55c69bef9ada941`

## Installation

Exit the game, then extract the binary package into the directory containing
`re2.exe`. The resulting files are:

```text
dinput8.dll
reframework/plugins/RE2MouseFix.dll
reframework/data/RE2MouseFix.ini
```

Only `dinput8.dll` from the official non-VR REFramework package is installed;
the VR DLLs and bundled VR scripts are deliberately omitted.

For Steam/Proton, set this launch option in the game's Properties dialog:

```text
WINEDLLOVERRIDES="dinput8.dll=n,b" %command%
```

The first launch creates `re2_framework_log.txt`. Search that file for
`[RE2MouseFix] Initialization complete` to confirm the plugin loaded.

## Configuration

Edit `reframework/data/RE2MouseFix.ini` while the game is closed. Each fix can
be independently disabled with `0`. Changes apply on the next launch.

## Uninstallation

Remove:

```text
reframework/plugins/RE2MouseFix.dll
reframework/data/RE2MouseFix.ini
```

Remove `dinput8.dll` only if no other REFramework mods use it. Remove the
Proton DLL override if REFramework is no longer installed.

## Building on Linux with lmsvc

The source archive vendors only REFramework's two plugin API headers. With
`lmsvc` and its Visual Studio 2022 toolchain installed:

```sh
./build-lmsvc.sh
```

The output is `build/RE2MouseFix.dll`. No package manager or additional
library is required.

## Building in a Visual Studio developer prompt

Run:

```bat
build-msvc.cmd
```

The code is C++20 and builds as a 64-bit DLL using the dynamic release CRT.

## Credits and licensing

RE2MouseFix is MIT licensed. See `THIRD_PARTY_NOTICES.md` and the license files
under `third_party/licenses` for REFramework and REFix attribution.
