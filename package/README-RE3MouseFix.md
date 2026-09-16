# RE3MouseFix

RE3MouseFix removes Resident Evil 3 Remake's game-side mouse damping,
nonlinear response, pitch-dependent horizontal sensitivity, and erroneous
controller-magnitude scaling. It patches both the normal and sight aiming
cameras. It does not change menus, mouse buttons, Windows pointer settings,
difficulty, animation quality, or field of view.

This version targets the Steam DX12/public build 11960962 with REFramework's
RE3/TDB70 managed plugin API. The local `re3.exe` used to identify the target
build has SHA-256:

`54582f13e6e70cd312029e8dab6cab888708ab2b14b9d2fd6937fe39e153d1f9`

## Installation

Exit the game, then extract this package into the directory containing
`re3.exe`. The resulting files are:

```text
dinput8.dll
reframework/plugins/RE3MouseFix.dll
reframework/data/RE3MouseFix.ini
```

Only `dinput8.dll` from the official non-VR REFramework `RE3.zip` release is
included; its VR DLLs and scripts are omitted.

For Steam/Proton, set this launch option in the game's Properties dialog:

```text
WINEDLLOVERRIDES="dinput8.dll=n,b" %command%
```

On the first launch, search `re2_framework_log.txt` in the game directory for
`[RE3MouseFix] Initialization complete` to confirm that the plugin loaded.

## Configuration

Edit `reframework/data/RE3MouseFix.ini` while the game is closed. Each fix can
be independently disabled with `0`. Changes apply on the next launch.

## Uninstallation

Remove:

```text
reframework/plugins/RE3MouseFix.dll
reframework/data/RE3MouseFix.ini
```

Remove `dinput8.dll` only if no other REFramework mods use it. Remove the
Proton DLL override if REFramework is no longer installed.

## Credits and licensing

RE3MouseFix is MIT licensed. See `THIRD_PARTY_NOTICES.md` and the license files
under `licenses` for REFramework and REFix attribution.
