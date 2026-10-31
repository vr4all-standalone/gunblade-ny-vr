# Gunblade NY VR implementation plan

> For agentic workers: use superpowers:executing-plans for inline execution. The user authorized autonomous implementation, with no unrelated changes, staging or publishing.

**Goal:** Original Gunblade NY on Quest 3/3S and Windows OpenXR with full VR4ALL feature parity.

**Architecture:** A private pinned MAME simulation feeds immutable native Model 2 scenes into the common OpenXR host. A separate Gunblade adapter owns Model 2B inputs, measured game state, held automatic fire and accepted-shot events.

**Tech stack:** C++20, MAME 0.289, OpenXR, OpenGL/GLES, Android NDK 29, Python 3.

**Spec:** docs/design.md.

## Global constraints

Only ports/gunblade-ny-vr and its private D: compiler/research directories may be changed. Do not mutate source media, installer, VC2 or shared engine. No commercial-data download or publishing. App identity com.vrlightgunhub.gunbladeny. Keep large files on D:.

## Review focus

Unsafe/corrupt ZIPs; diagnostic state overlapping original media; early or held Start/trigger across boot/menu/focus changes; wrong ADC channel order or axis ranges; feedback asserted from physical pulls rather than measured native acceptance.

## Task 1: Verified native baseline

Interfaces: tools/game_files.py validates/stages originals; tools/run_diagnostic.py runs a bounded gunblade child in fresh output directories; tools/baseline.lua supplies native inputs and captures evidence.

- [x] Add and run failing media/diagnostic tests. Adapt isolated media tools and run green.
- [x] Generate game-set.json from the pinned driver and device ROM definitions. Audit both supplied Gunblade sets and billboard firmware; stage a complete private ZIP.
- [x] Build an independent Windows Model 2 executable in D:/VR4ALL-Gunblade-Build/engine using exported platform patches.
- [x] Run native boot, P1/P2 start/join, held fire, service and calibration diagnostics. Record visible outcomes and native speed.

## Task 2: Independent game adapter

Interfaces: native/cabinet_io.h provides cabinet bit masks and ADC mapping; native/input_policy.h provides tracked-hand ownership and release gates. Native state observation provides evidence, never guessed offsets.

- [x] Add failing C++ regressions for Model 2B ADC order/ranges, inherited trigger bits and invalid aim.
- [x] Implement cabinet mapping and calibratable native aim; verify against native input test captures.
- [x] Add failing regressions for held-trigger/menu/focus/layout changes and Start sequencing. Implement and run green.
- [x] Observe native readiness, active players, weapon and shot acceptance; document validated offsets or unresolved evidence.

## Task 3: Common host and platform delivery

- [x] Integrate stable common Model 2 stereo renderer and OpenXR host with neutral-view fidelity checks.
- [x] Connect startup/help/player/service/settings/feedback features and licensed gun choices. Verify software transitions with held controls; original game gun extraction remains unverified.
- [x] Build Windows and Android ARM64 with isolated identities and state. Package software only with licenses/checksums.
- [ ] Run physical headset verification. Record untested hardware features accurately and obtain independent review before completion.

## Execution ledger

2026-10-27: Direct autonomous execution authorized by delegation. Existing project scope takes precedence over creating an unrelated worktree or editing shared root documentation. No unrelated files will be staged or committed. Native baseline and adapter are useful independent work while the common host is being developed in the original chat.

2026-10-28: Native baseline, service/calibration observations, independent game-state and accepted-shot adapters, common host/UI and both platform compilations are implemented. Model 2B texture-bank sizing and source-polygon interpolation were identified from real captures. Final GPU fidelity and review fixes are being integrated before rebuilding matched software-only packages. Headset verification remains open; see validation.md.

2026-10-28 final software gates: ABI03 integrates native shot ownership and calibration recovery release gates. All eight capture replays pass the unchanged fidelity threshold with actual geometry counts. The final 75-second native/production-host run passes with both players and measured reticle inverse. Windows AMD64 and Android ARM64 software-only deliveries are rebuilt; physical headset validation remains open because installation was outside the delegated scope.

2026-10-28 optimization follow-up: One ordered world draw per eye preserves all 40 baseline replay images byte for byte and reduces measured desktop draw cost. Rejected-scene fallback agrees with aiming. Patch 0007's CPU raster experiment remains opt-in and default off, with native service/reference/direct-framebuffer fallbacks. The original verified deliveries remain in artifacts/final; optimized builds use artifacts/optimized. The user subsequently authorized headset installation, to follow the software checks; physical playability remains a separate verification gate.

2026-10-28 installation: After the PC restart, completed the app-owned storage selector and its executable Java regression (12 delivery tests pass). Rebuilt only the Java/APK layer; all five native libraries match the verified optimized APK. The scoped-storage APK is installed on Quest 3S and its installed hash verified. The owned complete game archive is transferred and hash-verified in the app's own folder without an all-files access grant. Headset lock prevents startup verification until the user unlocks it. Packages and installation evidence are under artifacts/scoped-storage and quest/headset-installation.json; no publishing occurred.

2026-10-28 audio follow-up: The user unlocked and played the initial stereo build. Wider View defaults on with a one-time settings migration. Live SDL counters establish sample shortages, and matched ARM64 paced/unthrottled runs establish a CPU-limited native workload. Cached raster preflight and controller rays preserve renderer outputs. Revised UART batching preserves clock phase, the original CPU scheduling grid and paired native WAV/RAM/images on Windows and Quest, including independent Windows save/load checks. Android strong-function local binding retains weak/object/import identities. The user rejected the timing build's actual Gunblade audio; earlier positive feedback was for TeraBurst. Live Gunblade stage speed is about 90–91%, still short of full speed.

2026-10-28 adaptive playback: Added the separately loaded, pitch-preserving SoundTouch worker and SDL bridge without changing native clocks or reference synthesis. Fourteen CTest targets, eleven standalone native tests, 36 general and twelve delivery regressions pass. Captured music replay at 80%/90% production and the Quest CPU worker tests have zero steady shortages. The signed candidate is installed under existing authorization and preserved in artifacts/adaptive-audio. Live startup, pause/resume and listening validation await headset wake. Sudden rate-change robustness and full native speed remain open. See performance.md and the private installation record for evidence and limits.
