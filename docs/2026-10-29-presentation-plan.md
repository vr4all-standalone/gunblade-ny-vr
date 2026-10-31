# Gunblade NY VR presentation update

User request: stereo reticles like Teraburst, recheck FPS/audio, use an original held gun if usable, otherwise Arcade/Zapper only, correct controller help, apply the overseer's beginning UI and red/blue Arcade changes. Build and install on the Quest under the standing authorization.

Scope is Gunblade NY only: this applies the existing overseer behavior to its pending adapter. Reference mods remain read-only. Excluded and paused mods remain untouched. No GitHub publication.

## Constraints and decisions

- Preserve native CPU clocks, aim calibration, original media and accepted OpenSLES/SoundTouch audio path. Any performance change needs evidence and equivalent native output.
- Preserve either-hand initial P1 Start, solo hand transfer and default P1-left/P2-right after P2 joins.
- Use the overseer's standard P1 red/P2 blue for Arcade plastic body/grip, preserving screws, metal and lens; Zapper/controllers retain materials. Native reticle artwork keeps its original yellow/cyan colors.
- Apply the overseer angle baseline: displayed zero means the former physical -10 degrees, reset legacy angles once, then retain user changes. Use one pose helper for model/muzzle/aim/menu.
- Positively identify native reticle geometry and move it to the stereo aim surface without removing unrelated HUD or service graphics. Preserve reference rendering when unavailable.
- Original gun inclusion requires identifiable, usable player-held geometry. Do not label an absent model as supported.
- The existing untracked port and dedicated D: build root are this task's isolated ownership boundary; do not copy/reset another fork or stage the root's unrelated changes.

## Tasks

- [x] Inspect current source, reticle captures and reference behavior; record existing limitations.
- [x] Implement and test stereo reticle extraction/placement, including unrelated HUD and fallback cases.
- [x] Implement Arcade colors, angle migration and the supported gun selector; test player/hand ownership and persistence.
- [x] Correct controller help for initial/solo/dual/service states and actual buttons; test labels and GPU visibility.
- [x] Adapt the styled validated launch panel with automatic entry and safe returned/resume lifecycle; test real launch policy and file validation.
- [x] Review FPS/audio evidence and any bounded performance hypothesis; retain current audio unless a defect is reproduced.
- [x] Run target production suites and GPU checks, build Windows host and fresh signed Quest package, verify package payload/signing/hashes.
- [x] Review integrated changes, install with data preserved, verify installed bytes, collect live FPS/audio counters when headset is available. Physical appearance/audio acceptance remains a separate result.
- [ ] Apply the overseer's concurrent app-name-only update after the active headset test ends; preserve all tested code/assets.

## Review focus

Check held controls across menu/focus/join transitions; reverse hand layout and solo transfer; settings version 1/2/3 migration without loss of unrelated choices; reticle fallback and service overlays; launcher return/recreation without loops or broad storage permission requirements.

## Progress

- Baseline accepted installed build: `cb2a5c32a2c766b13feb7af696a3ec86743ec2ee35be9279f5699a76a25a113c` (sound restored and right-hand Start physically confirmed).
- Prior live capture: 197 seconds, 128 non-warmup missing audio frames, no input/output drops; native heavy scenes below full speed. Tiny grit also heard in untouched native clips; no proven clipping defect in seven isolated sound effects.
- Independent ownership: reticle/renderer adapter; Android launcher; read-only FPS/audio investigation. Root owns settings, presentation, controller help, integration and delivery.
- Final software results: 17 native/GPU CTests, 36 Python + 14 standalone checks, 17 Android checks; independent review no remaining blockers. Forty retained reference replays are unchanged. Details and exact hashes: overseer/changes/2026-10-29-gunblade-presentation/report.md.
- Installed build 6444d259... has fresh Quest play sessions. New 2c92bdf7... differs only in AndroidManifest.xml to use Gunblade NY - VR4ALL and is prepared for installation after the current test. Full source/audio/native runtime payload equality is verified.

## User follow-up after actual play

The first act's stereo reticles, gun angle, performance, haptics and recoil are accepted. Reticles become hard to see far away; user prefers larger artwork. Level1 act2 returns to flat reticles and performs worse. User explicitly removes grip-button shortcuts; keep size controls in the menu. The prepared naming-only candidate is superseded by this follow-up and was not installed.

- [x] Remove grip action/binding/sampling, resizing shortcuts and help label; preserve left/right stick-click controls.
- [x] Enlarge native vector marker by1.35 and retain a readable angular floor with distance, preserving actual depth and stereo disparity.
- [x] Add opt-in bounded asynchronous immutable-scene diagnostics and18th production regression; separate per-session capture folders.
- [ ] Correct measured duplicate rounding mismatch using new actual captures; retain positive source/material/transform identity.
- [ ] Investigate heavy-scene GPU/native timing separately; do not infer that fixing reticle recognition fixes FPS.
- [ ] Rebuild/test/package/install follow-up and collect physical acceptance. Preserve accepted haptics/recoil and restored sound path.
