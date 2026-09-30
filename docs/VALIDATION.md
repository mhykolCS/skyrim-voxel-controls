# Validation record

Date: 2026-09-30. Target: Steam Skyrim Special Edition 1.7.104.0, official
SKSE 2.3.1, Address Library v13, Proton Experimental, Arch Linux/Hyprland,
NVIDIA RTX 3050 Ti. This is a playable development alpha, with the limits below.

## Automated and build checks

- Native C++ core: CMake/Ninja build and CTest pass (three test executables with
  multiple assertions). Checks include 30 versus 144 FPS integration, normalized
  diagonals, jump height and gravity, creative hover/ascent, bounded gliding,
  attack recharge/critical conditions, and recipe transaction preflight.
  Control-policy regressions reject every custom gameplay action during script,
  AI, character-creation, furniture, and camera restrictions; distinguish pause
  from a scene; and exercise a quest taking and releasing control mid-gameplay.
  Version 0.1.2 also checks 900 independent Java movement reference ticks and
  15/30/60/144 FPS jump displacement; see [physics target](PHYSICS.md).
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
  the movement controller resumes. Version 0.1.1 also passes this handoff and the
  magic-menu check, without writing Skyrim's current or stored control flags.
- Melee targets are picked through Skyrim's actual crosshair. The live bandit
  test records health 35 -> 27.43 -> 19.86 -> 12.30 -> 4.73 -> -2.84, followed by
  Skyrim's corpse search prompt. `Actor::DoDamage` applies the hit; the earlier
  HandleHealthDamage call alone did not change health on this runtime.

Local evidence is under ignored `local/testing/`: playtest logs, save hash audit,
and game-only screenshots. Development-driver captures run in the background at
about 15 FPS; these are not a foreground performance benchmark. Foreground
rendering was also observed, but no sustained performance benchmark is claimed.

## 0.1.1 scripted-control regression

The earlier alpha suppressed native input by changing global ControlMap flags.
That could obscure a quest disabling the same controls. Camera and combat actions
also ran before the movement eligibility check. Version 0.1.1 filters native input
handlers only during ordinary custom gameplay and gates all custom actions on
Skyrim's live control state. Scripted scenes restore normal gravity, stop velocity
overrides, drop queued movement, and release the custom camera/model state.

Live checks on the same runtime confirmed:

- Keyboard button events through the actual input dispatcher moved the player,
  jumped, and reached the native-handler filters during ordinary gameplay.
- Disabling player controls while moving in creative flight immediately stopped
  custom physics updates and restored controller gravity to 1. Camera, flight,
  workbench, attack, and cast attempts remained blocked. F10 off/on did not change
  Skyrim's `FFFFFBBE` control mask. Re-enabling controls resumed ordinary movement.
- AI-driven player movement suspended the plugin even with all control flags
  enabled. Ending AI control resumed the plugin.
- Alchemy's first-use tutorial and native magic-menu replacement still returned
  to the workbench with Skyrim's control flags intact.
- Loaded both existing Helgen saves: the prisoner lineup before character
  creation and the named-character autosave before the execution. Repeated W,
  Space, Shift, F5/F6/F7/F8, attack, and cast attempts did not run custom physics
  or change the scripted camera. The named-character save naturally progressed
  through the walk to the block, execution animation, Alduin's arrival, and
  `Make your way to the Keep`, without quest-stage or control-enabling commands.
  Native walking remained available once the game released movement while
  other introduction restrictions were still in place.

## 0.1.2 Java physics and creative flight

The normal-ground physics model now agrees with 900 independently generated
Java reference ticks. The target parameters, exact ordering, source provenance,
and engine differences are recorded in [PHYSICS.md](PHYSICS.md). The original
render adapter applied the last tick's velocity for a whole frame; it now
integrates partial ticks before supplying Havok's velocity.

Live checks with the game visible on DP-2 confirmed:

- A single Space jump in QASmoke rose from Z 6976.3501 to 7063.7358: 87.3857
  Skyrim units, or **1.2485 blocks** at the engine's 69.99125 units/block scale.
  The Java model target is 1.2522 blocks; the observed difference is about 0.3%.
- Double Space directly entered creative flight without using F6. Another
  double tap exited to survival movement. F6 still entered flight independently.
  Holding Shift to touch down also ended flight and restored the baseline FOV.
- Outdoor ascent settled at 7.50 blocks/second. Horizontal flight approached
  10.89 normally and **21.7769 with Ctrl**.
- With the test character's stamina regeneration set to zero, stamina remained
  exactly **50.0000** throughout the flight/boost trace; magicka stayed at 100.
  Skyrim's native sprint state stayed false.
- The existing world FOV was 80 degrees. Creative flight eased to 88 degrees,
  Ctrl flight to 101.2, Ctrl release back to 88, and flight exit back to 80.
  Game captures show the outdoor sprint-flight view and diagnostic readings.
- A control lock during boosted flight restored gravity to 1 and world FOV to
  80, rejected double Space and F6, and left the script's flags intact. Releasing
  that lock resumed survival movement. Pausing/resuming preserved the flight
  mode and altitude; F10 restored vanilla gravity, input, and FOV.

These tests used an unsaved temporary character. Test-only actor values and
teleports were not applied to the user's saved character. The final installed
build excludes the development command-file driver. All 43 existing save-directory
files matched their pre-test hashes, and the tests created no additional saves.

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

Before the 0.1.1 fix, a second backup preserved all 43 current save-directory
files, including the user's new Helgen character. The cutscene regression loaded
those saves without overwriting them. All 43 file hashes still matched afterward.

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
  furniture and quest regression sweep has not been completed. The Helgen
  execution sequence and simulated control/AI handoffs are now covered; other
  campaign scenes remain untested.
- Long campaign/save compatibility has not been established. Use a test
  character for experiments and retain the pre-install backup.
