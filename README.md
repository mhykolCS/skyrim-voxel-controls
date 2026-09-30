# VoxelControls

A native Skyrim mod exploring Minecraft-inspired movement, flight, melee combat,
and a tech-mod-style alchemy and spell workbench. It uses Skyrim's world,
collision system, actors, spells, and inventory. Minecraft does not need to run.
There is no block placement or terrain destruction.

**Development alpha.** Compilation, automated tests, and live-game validation are
separate checks. See [validation](docs/VALIDATION.md) for the current evidence.

## Target

- Steam Skyrim Special Edition **1.7.104.0**, 64-bit.
- Official [SKSE 2.3.1](https://www.nexusmods.com/skyrimspecialedition/mods/30379).
- [Address Library v13](https://www.nexusmods.com/skyrimspecialedition/mods/32444),
  including the database for 1.7.104.0.
- Keyboard and mouse. The plugin refuses other game runtimes.
- Microsoft Visual C++ v14 x64 runtime. An old 2016 CRT in a Proton prefix is
  insufficient for the current C++ dependencies; see `docs/VALIDATION.md`.

## Controls

| Input | Action |
| --- | --- |
| W A S D | Move; diagonals are normalized |
| Space | Jump; ascend in creative flight |
| Shift | Move slowly; descend in creative flight |
| Ctrl | Sprint; faster creative flight; glide boost uses magicka |
| F3 | Debug overlay: position, velocity, frame rate, controller state |
| F5 | First person, rear third person, front third person |
| F6 | Enter or leave creative flight |
| Space twice | Toggle hovering while creative mode is armed |
| F7 while airborne | Deploy or fold the glider |
| F8 | Open or close the workbench |
| F10 | Toggle the mod and restore vanilla movement/input |
| Left mouse | Cooldown-based melee, when holding a melee weapon or unarmed |
| 1 / 2 / 3 | Select Firebolt / Ice Spike / Lightning matrix |
| Right mouse | Cast the selected learned matrix while in melee mode |

F5 through F8 and the three matrix number keys are reserved in the in-memory gameplay
mapping while the mod is enabled. The user's control configuration is not edited.
F10 restores those bindings. Ranged weapons and equipped native spells retain
their native attack inputs. Alchemy stations and the magic menu open the new
workbench while enabled; F10 gives access to the original menus.

## Mechanics

The movement simulation runs at 20 Hz while rendering at the game's frame rate.
Walking is 4.317 m/s, sprinting 5.612 m/s, and the jump impulse is 8.4 m/s.
Skyrim's Havok controller resolves collisions. Creative mode provides directional
flight and hover. The independent glider model trades altitude for speed, permits
pitch-driven climbing, and caps speed; Ctrl boost consumes 15 magicka/second.

Melee uses a 1.6 attacks/second recharge with reduced damage for early attacks,
falling criticals, sprint knockback, a three-metre reach check, and the game's
crosshair target for line of sight. Armor mitigates damage. These are new mechanics,
not a copy of Minecraft's implementation. Steve's right arm swings with each
custom attack. Vanilla weapon animations, perk-driven damage, enchantment
triggers, and Skyrim skill progression are not integrated with this melee path yet.

The workbench has six starter recipes. Materials are checked together and consumed
on the game thread. Potions are real inventory items; spell matrices teach real
Skyrim spells and casting spends magicka. A learned matrix cannot consume materials
again. The recipe identities are verified from the user's local Skyrim.esm using
`tools/inspect_game.py`. No game assets are included in this source tree.

| Recipe | Inputs | Output |
| --- | --- | --- |
| Healing mixture | Blue mountain flower + wheat | Minor healing potion |
| Magicka mixture | Red mountain flower + mora tapinella | Minor magicka potion |
| Stamina mixture | Purple mountain flower + thistle | Minor stamina potion |
| Firebolt matrix | Fire salts + filled petty soul gem | Learn Firebolt |
| Ice spike matrix | Frost salts + filled petty soul gem | Learn Ice Spike |
| Lightning matrix | Void salts + filled petty soul gem | Learn Lightning Bolt |

## Classic Steve

The optional character model uses the original wide-arm Steve proportions, six
separate box parts, and the classic Minecraft 1.8.9 skin. Walking, attack swings,
head pitch, and a horizontal gliding pose are animated by the plugin. F5 exposes
the rear and front views. First-person arms still use Skyrim's original model.

The model replaces the third-person appearance only. Your race, equipment,
armor statistics, and character data are retained. F10 restores the normal body.
It is a visual prototype: there are no visible armor layers or animated held items.

Generate the local assets separately:

```sh
python3 tools/prepare-steve.py
```

This needs CMake, Ninja, a native C++ compiler, Git, and ImageMagick. It downloads
the pinned nifly source and obtains the skin from Mojang's official client archive.
Mojang's texture is kept outside the open-source files in ignored `local/steve/`.

## Build and install

The pure C++ core builds on Linux without Skyrim or third-party dependencies:

```sh
cmake -S . -B build-native -G Ninja -DVOXEL_BUILD_PLUGIN=OFF
cmake --build build-native
ctest --test-dir build-native --output-on-failure
```

The Windows DLL is built with clang-cl, the Microsoft SDK/CRT obtained by xwin,
and vcpkg. The setup used here follows CommonLibSSE-NG's
[Linux cross-compilation guide](https://github.com/alandtse/CommonLibSSE-NG/blob/ng/examples/linux-cross-compile/README.md).
Dependency revisions are recorded in `DEPENDENCIES.json`. The shader compiler
uses a separate Wine prefix at `tools/shader-wine`.

```sh
python3 tools/fetch-sources.py
bash tools/build-windows.sh
python tools/install.py '/path/to/Skyrim Special Edition'
```

Install the official SKSE and Address Library separately. Start the game through
`skse64_loader.exe`. On this Linux installation, use the **Skyrim - VoxelControls**
application launcher or run `skyrim-voxel-controls` in a terminal. The ordinary
Steam Play action does not select SKSE automatically. The plugin log is in the game's Documents directory under
`My Games/Skyrim Special Edition/SKSE/VoxelControls.log`.

To remove this plugin and restore any previous DLL backed up by the installer:

```sh
python tools/install.py '/path/to/Skyrim Special Edition' --uninstall
```

The installer tracks all four optional plugin/model files, checks their hashes
before replacement, and preserves previous versions for uninstall. It never edits saves. Crafting during play changes the character's
normal inventory and learned spells, as normal gameplay does; use a new test
character when experimenting with this alpha.

This is a six-recipe playable prototype, not a replacement for the full Skyrim
alchemy catalogue or a Minecraft tech-mod progression tree. F10 restores Skyrim's
original menus for features outside the starter workbench.

## Source and licensing

VoxelControls is GPL-3.0-or-later with the modding and linking permissions in
`EXCEPTIONS.md`. Dependencies keep their own notices; see `THIRD_PARTY_NOTICES.md`.
This is an independent fan project, not affiliated with Mojang, Microsoft,
Bethesda, or the creator of the crossover video that prompted the experiment.
