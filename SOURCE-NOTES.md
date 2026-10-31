# Gunblade NY VR

Independent Gunblade NY (Revision A, Model 2B) development port for Quest 3/3S and Windows OpenXR. The user has played the earlier build on Quest 3S and confirmed stereo 3D, restored audio and either-hand initial Start. The latest presentation changes, Quest 3 and Windows OpenXR hardware still require physical acceptance.

Application identity: `com.vrlightgunhub.gunbladeny`. Original media is read-only. Compiler output and private diagnostics live in `D:/VR4ALL-Gunblade-Build`, independently from Virtua Cop 2 and all other ports.

The complete requested feature set and implementation gates are recorded in [the design](docs/design.md) and [the implementation plan](docs/implementation-plan.md). Native emulation, captured geometry, software tests and headset playability are separate verification gates.

Hold the trigger for the original machine gun's automatic fire. Either X or A starts P1 from a fresh game, and either hand can control solo P1. After P2 joins, the default assignment is P1 left / P2 right: X is P1 Start and A is P2 Start. The port sends a credit followed by delayed native Start. Settings reverse the hands and provide laser, haptics, gun/world size, recenter, view width, help and native operator controls. Old hand settings migrate once; later explicit reversals stay saved. The original service and AIM SET pages remain in the game.

Arcade plastic is red for P1 and blue for P2, following logical identity across hand swaps; grey screws and the original lens retain their materials. Displayed Gun angle 0 is the former physical -10 degree pose. Legacy angles reset once on upgrade, and later choices persist. Left stick click recenters; right stick click toggles the laser. World and gun size are adjusted in the menu.

The original animated yellow/cyan targeting rings now sit at the aimed scene depth in stereo, including when the laser beam is Off. Only verified native reticle submissions are replaced; unrelated HUD and service/fallback graphics stay intact. See [reticle evidence](docs/native-reticles.md).

`Reticle` selects In-game, Custom or None. Custom is an outlined X with an empty center, red for P1 and blue for P2. `Reticle min size` adjusts its apparent-size floor in 1% steps. `Reticle shrink distance` controls how quickly it shrinks; larger values shrink more slowly. These choices are saved. The starting 25% floor is provisional for user tuning; existing installations retain their original perspective shrinking rate until that floor is reached.

The launch brand lasts at most one focused second and clears immediately when native content arrives. The game's original boot warning and opening credits are shown before Start becomes available.

The adapter uses verified Model 2B 8-bit ADC channels and native calibration. Feedback comes from accepted native projectile events. The world and aiming transforms are measured separately: world focus 400/400, reticle focus 600/600. Gun choices are Arcade and Zapper. A usable original player-held Gunblade mesh has not been identified.

Build and delivery details are in [platform notes](docs/platform-notes.md), measured game behavior in [native-state evidence](docs/native-state-evidence.md), and the current limits in [validation](docs/validation.md). Local software packages exclude game media, captures, saved state and signing keys. Supply original owned Revision A game data and billboard firmware separately; no CHD is used.
