# Validation record

Date: 2026-09-30. Target: Steam Skyrim Special Edition 1.7.104.0, official
SKSE 2.3.1, Address Library v13, Proton Experimental, Arch Linux/Hyprland,
NVIDIA RTX 3050 Ti. This is a playable development alpha, with the limits below.

## Automated and build checks

- Native C++ core: CMake/Ninja build and CTest pass (one test executable with
  multiple assertions). Checks include 30 versus 144 FPS integration, normalized
  diagonals, jump height and gravity, creative hover/ascent, bounded gliding,
  attack recharge/critical conditions, and recipe transaction preflight.
- Windows plugin: clang-cl Release cross-build with pinned CommonLibSSE-NG,
  ImGui, vcpkg dependencies and Microsoft SDK/CRT. The normal build explicitly
  sets `VOXEL_PLAYTEST=OFF`; the command-file driver is development-only.
- Python tools compile and launcher/build shell scripts pass syntax checks.
- Installer: isolated fixture checks installation, restoring a previous DLL,
  and refusing to overwrite a file changed outside the installer.
- Recipe ingredient/output identities checked against the local Skyrim.esm.
- Steve mesh generation and texture conversion completed with the pinned nifly
  revision. The official Minecraft client archive hash was verified before
  extracting its skin. The skin remains a separate local asset, not GPL source.

## Live evidence

The following were observed in the actual game, not inferred from compilation:

- SKSE loads the plugin and the D3D11 overlay displays in Skyrim.
- W movement changes the actual position at approximately 4.32 m/s and stops on
  release. The original broken implementation was being overwritten by Skyrim's
  character controller; the final adapter supplies velocity at its actual
  SetLinearVelocityImpl call. The hooks distinguish the player's controller.
- A jump rises roughly 79 Skyrim units (about 1.13 m) and returns to the floor.
  Ground support no longer cancels the ascent on the first frame.
- Creative ascent changes height from roughly -140 to 2168 units; release holds
  that height. Gliding changes horizontal position and altitude; a three-second
  boost spent roughly 43 magicka and reached about 41 m/s horizontally.
- F3 diagnostics and the first/rear/front F5 camera cycle render. The front view
  faces the player correctly and uses a practical distance.
- Classic Steve is attached to the third-person player root, with the wide arms,
  classic skin and animated limbs. An outdoor front-flight capture shows the
  full body. F10 visibly restores the original Skyrim body and controls.
- Workbench mouse input creates a healing potion and consumes one of each input.
  Firebolt matrix creation consumes its ingredients, teaches the spell, and
  disables repeated fabrication. Right-click launches its real projectile and
  spends 36.5 magicka in the test character's configuration.
- The native magic-menu request closes that menu and opens the workbench.
- Activating an alchemy station closes its original menu and opens the workbench
  after the first-use tutorial is dismissed. The player leaves the furniture and
  the movement controller resumes. The handoff restores only control flags the
  plugin owned before crafting saved its control state.
- Melee targets are picked through Skyrim's actual crosshair. The live bandit
  test records health 35 -> 27.43 -> 19.86 -> 12.30 -> 4.73 -> -2.84, followed by
  Skyrim's corpse search prompt. `Actor::DoDamage` applies the hit; the earlier
  HandleHealthDamage call alone did not change health on this runtime.

Local evidence is under ignored `local/testing/`: playtest logs, save hash audit,
and game-only screenshots. Development-driver captures run in the background at
about 15 FPS; these are not a foreground performance benchmark. Foreground
rendering was also observed, but no sustained performance benchmark is claimed.

## Runtime setup and preservation

The old Proton prefix contained a 2016 Microsoft C++ runtime that failed during
plugin initialization. The current official v14 x64 redistributable resolved the
diagnostic and game startup checks. Its DLLs were installed app-local in the game
folder; the prefix's existing system DLLs were not replaced. Runtime installation
hashes and file history are recorded in `local/runtime-install-manifest.json`.

Skyrim's viewport must be correct at startup. This Hyprland machine uses a
borderless 3440x1440 window on DP-2 and a temporary game-specific floating/fullscreen
rule in the launcher. Resizing an initially tiled Skyrim window after startup
produced incomplete world rendering, so the launcher applies the rule first.

The original profile was backed up before testing. All 39 original save files
matched their backup hashes after the live tests. Tests use an unsaved temporary
character; the installer does not modify save files. Crafting during ordinary
play does alter inventory and learned spells through normal game APIs.

## Remaining alpha limitations

- Only the listed runtime, keyboard and mouse, and this Proton setup have been
  exercised. VR, gamepads, other runtimes, and other mod combinations are untested.
- This is a six-recipe starter workbench, not the full Skyrim alchemy catalogue
  or a technology progression tree. F10 restores access to Skyrim's menus.
- Steve is a third-person visual replacement. First-person arms remain vanilla;
  armor layers and held items are not drawn on Steve. Equipment stats remain.
- Custom melee has charge and critical/knockback calculations, but does not yet
  integrate Skyrim's weapon animation, perk, enchantment, or skill-XP pipeline.
  Sprint knockback and falling criticals have core tests, but their full live
  combat behaviour is not exhaustively tested. The inert bandit damage test is
  evidence of targeting, health reduction and death, not AI combat balance.
- Movement keeps Skyrim's collision controller. Interiors, jumping and flight
  have been exercised; a full terrain, ceiling, staircase, swimming, mount,
  furniture, quest, and cutscene regression sweep has not been completed.
- Long campaign/save compatibility has not been established. Use a test
  character for experiments and retain the pre-install backup.
