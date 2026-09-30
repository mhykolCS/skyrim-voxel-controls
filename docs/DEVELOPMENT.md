# Development and live checks

Build the standalone simulation with the commands in the README. Game adapter
changes also need a real SKSE session on the exact supported runtime; core tests
cannot establish that a Havok virtual call, menu handoff, or inventory operation
works in Skyrim.

For local integration checks, explicitly opt into the development driver:

```sh
VOXEL_PLAYTEST=ON bash tools/build-windows.sh
python3 tools/install.py '/path/to/Skyrim Special Edition'
```

The `VOXEL_PLAYTEST` build polls a local command file every 200 ms. It does not
open a network port and does not send keyboard input to the desktop. Never
distribute or leave this development build installed as the normal release.

After starting SKSE, a new unsaved test character can be opened without loading
an existing save:

```sh
python3 tools/playtest.py '/path/to/Skyrim Special Edition' 'console coc qasmoke'
python3 tools/playtest.py '/path/to/Skyrim Special Edition' 'action debug'
python3 tools/playtest.py '/path/to/Skyrim Special Edition' 'hold 11 2'
python3 tools/playtest.py '/path/to/Skyrim Special Edition' capture --capture /tmp/game.png
```

Commands available only in the development DLL:

- `console COMMAND`: execute a native console command on the game thread.
- `action NAME [VALUE]`: debug, camera, creative, glide, workbench, craft,
  attack, cast, or enable. Craft values are recipe indices 0 through 5.
- `hold HEX_SCANCODE SECONDS`: hold one movement key for at most ten seconds.
- `spawn`, `aim`, `inspect`: make an inert test bandit, aim at its torso, and
  log its health plus recipe output inventory counts. These use real world
  references and crosshair picking; they do not replace the combat target.
- `alchemy`: spawn and activate a vanilla alchemy station.
- `magic`: request the native magic menu.
- `dismiss`: close the initial alchemy tutorial overlay during this test.
- `capture`: save only Skyrim's back buffer as a DDS. `playtest.py --capture`
  interprets its actual RGBA masks when making a PNG.

`hold` exercises the movement adapter but is not evidence of OS key delivery.
WASD, Space, mouse, and F-key delivery were checked separately in the foreground.
Keep the game active while testing; background rendering depends on the window
manager and Skyrim's `bAlwaysActive` preference. Restore ordinary focus/pause
behaviour after testing. Do not run these commands in a valued character session.

Finish with a normal build, which defaults back to driver off even when the
previous CMake cache had it on:

```sh
bash tools/build-windows.sh
python3 tools/install.py '/path/to/Skyrim Special Edition'
```

Check `VOXEL_PLAYTEST:BOOL=OFF` in the build cache, verify the installed DLL's
hash, and start a fresh SKSE process. Remove the development command and capture
files after preserving any evidence you need. Screenshots and local playtest
logs are excluded from the GPL source tree under `local/testing/`.
