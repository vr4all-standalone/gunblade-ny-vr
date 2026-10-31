# Gunblade NY validation ledger

This is a local development build with partial Quest 3S play testing. The user has played the game and confirmed working stereo 3D. Audio crackles and native game speed remains below real time in the measured stage; hardware acceptance is still open. Evidence and commercial game captures remain private under `D:/VR4ALL-Gunblade-Build/research`. The source media and sibling ports are read-only references.

## Native game and input

The engine is MAME 0.289 at `f34f02505e32c1993c6a782b6814232cbfc74e36`, built in this port's private compiler checkout. Both supplied Revision A archives and billboard firmware were audited by size, CRC32 and SHA1. All 18 required payloads were staged into a separate canonical archive; source archive SHA256 values were unchanged after staging.

The untouched native baseline booted, reached mission selection and gameplay with both player HUDs, held automatic fire, TEST MENU, INPUT TEST and AIM SET. The 60-second headless gameplay baseline averaged 219.74% native speed on this development PC. This measurement does not predict headset performance.

The adapter uses IN1 bits 0x01/0x02 for triggers and the native eight-bit ADC order X1, X2, Y1, Y2. It does not emulate an offscreen reload. Native player joining, readiness, lives, service page activity and accepted projectile allocation were independently observed; details and original instruction locations are in `native-state-evidence.md`.

The shot tracker matched 121 accepted allocations across 2,934 observed native frames: P1 66, P2 55. The aiming helper matched 162 distinct native player/ADC positions with maximum world-coordinate error 5.72136e-7 and 368 exact ADC round trips. These checks establish the observed reticle transform; enemy-hit alignment and physical-controller accuracy still require play testing.

A final ABI03 production engine/host run with simulated controllers, native corner aiming and the corrected 3D renderer completed 75 native seconds with exit code zero, no script errors, both players active and accepted-shot feedback through P1 counter 129/P2 137. It averaged 193.31% headless native speed on this development PC. The final run is `final-live-gameplay/`, with host log `final-live.log`.

Native accepted events are attributed to the hand whose trigger was actually delivered to the game. Review regressions cover delayed old-hand acknowledgements across ownership changes, mixed old/new events, foreign sessions, invalid mappings and release after calibration recovery. Native state, calibration and ownership are consumed in one locked snapshot.

## Scene rendering

Eight complete native scene captures preserve geometry, texture banks, palette, luma, gamma, background, foreground and native rasters. The instrumented native screenshots match the untouched baseline byte for byte. Captured foreground composition has zero mismatches.

Gunblade Model 2B uses two one-MiB texture banks. The renderer's validity check now accepts those actual banks, and replay rejects geometry with incomplete material resources. A flat native fallback is not counted as stereo evidence.

The neutral-view replay gate requires at least 98% exact pixels, RGB mean absolute error no greater than one, and zero foreground mismatches. All eight captures pass through the final production GPU renderer. The two gameplay captures are 98.961% and 98.636% exact, with RGB mean absolute errors 0.04614 and 0.04858. All foreground mismatches are zero. The original polygon's native scanline UV/depth/alpha/checker samples are retained while geometry is projected independently for each eye.

Replay explicitly identifies actual visible world and panel triangles. Two startup frames are native-raster evidence; six contain real world geometry. The two gameplay frames render 2,897 and 2,334 world triangles, with 144,331 and 137,753 pixels differing between eyes. Wider-view changes are also nonzero. Final results are `final-renderer-replay/metrics.json`. Earlier fallback-only replay outputs are not 3D evidence.

On the development RTX 3090, the 3,016-polygon gameplay capture uses approximately 0.94 MiB of compact scanline data, with 1.368 ms source setup, 5.459 ms preparation/upload and 0.422 ms static native-resolution one-eye draw plus completion. The atlas has a guarded 64 MiB maximum. These measurements do not establish Quest frame pacing.

## Rendering optimization follow-up

The renderer now submits one ordered world draw per eye using flat integer material attributes. Gunblade's scanline atlas, native pixel-center material sampling and polygon indices are retained. All 40 PPM outputs (five passes across eight scenes) are byte-identical to the initial verified renderer, including the two native-raster startup scenes. Fidelity metrics, foreground composition, actual world-triangle evidence and stereo/wider-view differences are unchanged.

A real-GL regression verifies mixed solid/checker/transparent materials, window ordering, distinct panel/world depths and exactly one world draw. Rejected scenes use the same original native bitmap fallback for rendering and aiming. Both the draw-count and rejected-scene tests were verified failing before their fixes and passing afterward.

On the development RTX 3090, three-run median native-resolution one-eye draw plus completion improved from 0.39738 ms to 0.072658 ms, approximately 5.47 times faster. Preparation/upload remains approximately 5.3 ms and the scanline atlas remains 983,040 bytes. Evidence is in `graphics-review/ordered-world-draw/`; these are desktop measurements, not Quest performance results.

Patch 0007 also provides a conservative **experimental CPU raster shortcut**, opt-in in the engine. `GUNBLADE_FAST_RASTER=1` requests it only for verified active gameplay with a ready host, complete material resources, a full frame and visible world geometry. Eligibility retains the original window-1 path and now also accepts the observed full-display focus407 Area 2 profile with its known center, viewport and perspective depth variation. Reference/capture, service, direct framebuffer, rejected, missing and unusable scenes retain native rendering. `GUNBLADE_CPU_RASTER` or `GUNBLADE_SCENE_DIR` being present forces native reference rasterization. The current Android audio-performance launcher enables the shortcut after the paired native proofs below; the Windows launcher leaves it unset.

Actual direct framebuffer mode clears only the producer's unpublished owned polygons, creating a fresh owned frame when the DSP has no new geometry. The original native bitmap is then published, so the renderer and aiming use the native panel rather than retaining stale world geometry. This does not alter the independent trace collector or the shared ABI03 layout.

The paired 45-second gameplay proof compares 46 timed checkpoints for each of work RAM (`0x500000..0x5fffff`), extra RAM (`0x200000..0x23ffff`) and SRAM (`0x1d00000..0x1d03fff`): all 138 files, totaling 61,046,784 bytes, are identical. Native per-frame time, mode, joining and render-register traces are identical; the experimental run records 679 active-stage raster skips. Actual direct framebuffer mode occurs for 23 frames with 1,259 retained captured polygons cleared to zero; its native frame-032 screenshot is byte-identical to the reference. A separate service pair matches all 16 per-region checkpoints and all 16 native screenshots, with zero raster skips. These read-only native proofs are under `research/raster-proof`; they do not establish headset pacing.

## Software checks and delivery

The current suites pass: 36 Python media/diagnostic/patch-stack/replay-evidence regressions, twelve standalone native tests, fifteen CTest targets including real desktop GL rendering, presentation, launch policy and adaptive audio, and twelve Android/Windows delivery tests. The original-state evidence replay also passes after ABI03. All 40 frozen renderer images remain byte-identical. The UART refinement adds exhaustive clock-phase/grid regressions and actual paired native audio/RAM/image/save-load evidence; see performance.md.

The ABI03 Android ARM64 development APK has compiled and passed ELF ABI, app identity, v2 signature, native export, font, notices, archive integrity and software-only checks. The ARM64 DRC factory is linked. The initial verified packages remain under `D:/VR4ALL-Gunblade-Build/artifacts/final`. The optimized deliveries are built separately under `artifacts/optimized`, preserving that baseline.

Windows portable packaging checks AMD64 PE headers, required exports, the full DLL import closure and software-only contents. No publication has been performed. The user authorized installation on the connected Quest 3S after the software checks on 2026-10-28. The current Android setup prefers its own external app folder and works without an all-files storage grant. Installation and hardware startup results are recorded separately; full physical playability remains pending.

The app-folder build is saved under `artifacts/scoped-storage`. Its signed APK (SHA256 `9d58f12b2f9d2bb5259af48c26185efc1d15434e91dc2f387cba77484e40632b`) was installed successfully on the connected Quest 3S; the installed base APK has the same hash. The complete owned 18-payload archive was copied into this app's external `files/roms/gunblade.zip`, with its SHA256 verified after transfer. The app recreated the ROM directory with its own UID after a shell-owned directory initially blocked traversal. All-files access remains ungranted. The user subsequently unlocked the headset and played an active stereo session. Details are in `quest/headset-installation.json`; audio follow-up is recorded in `performance.md`.

Wider View now defaults on. Version-1 settings migrate once to the new default; version-2 settings preserve an explicit later Off selection. Native rendering and aim share the same cached, clipped scene geometry. The optional immersive-camera prototype remains disabled in the production host.

The native-timing APK SHA256 `68f7b39e5e6fd7561744a48e9cde8e442c56beed88ed193892322a7bba8093c0` is retained under `artifacts/audio-native-timing`. Its five native libraries match the tested build. The user rejected its actual Gunblade audio; earlier positive feedback came from TeraBurst and is excluded.

The adaptive-audio guard candidate was installed on Quest 3S: APK SHA256 `8b2e61d1fa1b69058e5b901f70b3a30c86b15a4a84c40baf93dce3d525b8bc59`, retained under `artifacts/adaptive-audio-guard`. All six packaged native libraries and the installed base APK match the verified build. The worker and final transition/jitter guard pass on desktop and the Quest CPU. Actual captured music replay at 80%/90% production has no steady shortages, drops or clipping.

The user confirms considerable improvement and a very playable first episode, with occasional remaining music distortion/short cuts and more slowdown in episode two. The focused live session starts with 100.107 steady seconds without a shortage; 207.449 seconds including pauses and heavier scenes has 1,152 nonwarmup missing frames (24 ms total), no input overflow, and consistent callback accounting. Pause-generation cleanup is counted separately. These results do not establish flawless audio or full native speed. Android backend underrun history and a separate first-launch video-initialization abort remain under investigation; see performance.md.

Gunblade's splash now yields to native content with a one-second focused-presentation cap, and native boot/credits presentation no longer waits for Start readiness. Settings version 3 migrates P1 to the left hand/P2 to the right once, preserving subsequent explicit reversals. Tests cover default two-player controls, migration, content dismissal and a rendered native-credits overlay. The user confirms the two-player assignment is substantially right and clarifies that either controller must also be able to start the first player; that small initial-Start correction is being added.

The combined AAudio comparison APK `5168d2742c985dbb528f14fb4fe40208714987ae1d6960cd959ea068a758453a` passed software checks but failed live audio discovery and produced silence. It is rejected. The immediate Java-only rollback APK `9de5def5a27965dd42d419cdaae16551892fbec4d52a067ba56afe08f8dfb70a` restores accepted SDL/OpenSL ES selection and remains preserved under `artifacts/restored-audio-launch-hands`.

The final corrected candidate is installed with base APK SHA256 `cb2a5c32a2c766b13feb7af696a3ec86743ec2ee35be9279f5699a76a25a113c` (`artifacts/restored-audio-either-start`). Either controller now sends P1 credit followed by delayed P1 Start when nobody has joined. Once P2 joins, the default assignment is P1 left/P2 right; explicit reversed settings remain supported. Join acknowledgments clear stale credit/Start pulses and block held buttons from carrying into another player. The four combinations of saved layout and first controller pass credit/Start, held-trigger, join-handoff and reverse-setting regressions. All fifteen CTest targets pass after this correction.

All six packaged native payloads match the stripped build. The audio worker, SDL and C++ runtime libraries are byte-identical to the accepted `8b2e61d1` audio build, and the rejected backend preference is absent from packaged Java bytecode. AAudio is not enabled; its discovery-fix experiment is parked privately outside the patch stack.

The user unlocked, tested the final `cb2a5c32` build and confirms sound is back and right-hand initial P1 Start works. Fresh process 16111 selects `openslES` and opens adaptive 48 kHz stereo S16 output. Over 197.344 seconds, the worker records 128 nonwarmup missing frames (2.67 ms), 2,560 startup-warmup frames, no input/output drops and valid callback accounting. Both native players produce accepted shots. Android track 2369's 192 underrun frames do not increase across a 27.2-second continuous-play comparison, so the older aggregate underrun history does not establish a steady backend fault. User feedback now points toward occasional sound-effect distortion rather than music; transient fidelity remains under investigation. Episode-two native slowdown remains unresolved.

The user then hears a very small rough quality in both original native and stretched excerpts. A bounded untouched-desktop Sound Test, with native SCSP and speaker taps, finds all seven selected effects nonzero and unclipped; speaker float-to-S16 conversion exactly matches the WAV. The effects use dry 8-bit sample paths, mostly at 22.05 kHz source pitch. This does not prove a specific emulator defect or original-hardware authenticity. The accepted headset build remains unchanged; the rejected time-stretch/backend experiments remain private. See performance.md for the source, transient-test and Sound Test evidence.

## Remaining verification

- Physical Quest 3, Quest 3S and Windows OpenXR sessions: per-eye stereo, tracking/recenter, frame pacing, pause/focus loss, held controls through menus and service, haptics, calibration, player joining/continue and long-run stage transitions.
- Original Gunblade held-gun/pickup geometry: not separately identified or extracted. Existing licensed/controller meshes are available.
- Enemy-hit alignment and laser/reticle agreement with physical tracked controllers.

Unverified hardware behavior remains marked untested in package metadata.
