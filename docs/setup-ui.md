# Gunblade NY preparation panel

The Android beginning panel now follows the current Teraburst/Virtua Cop VR4ALL preparation reference: a wide resizable Quest panel, dark deck and violet glow, VR4ALL/game/version header, cream/violet headline, file-status badges, lime Play button and expandable file help. The Quest default panel is 860 × 640 dp; content remains scrollable and the file badges stack in a narrower panel.

Gunblade keeps its existing media workflow. Setup creates and checks the app-owned `files/roms` folder without an all-files grant. Downloads is an explicit optional alternative, with app-owned `gunblade.zip` taking priority. A complete 18-payload `gunblade.zip` works alone; split sets may supply `segabill.zip` in the same selected folder. The existing validator still checks packed ROM size, CRC32 and SHA1 in place. Missing-payload errors now identify that selected folder rather than always pointing at Download.

Fresh library entry validates and automatically enters VR once. A normal game return, a suppressed entry or recreation after an attempted handoff remains on the panel. Play revalidates before handoff. Pausing cancels a pending manual request and invalidates an in-flight file check; returning from settings validates the current folder/access state again. Stale results, invalid media and duplicate clicks cannot create another handoff. A failed Android handoff offers an explicit Play retry.

`GameActivity`, its SDL audio backend selection, ROM arguments, application ID, task affinity and signing configuration are unchanged by this UI work. The controls help retains Gunblade's automatic fire and either-controller initial Start behavior.

Review identified a source-proven recreation/resize interleaving: the old activity's asset worker can continue after the new worker launches VR. Assets now copy into unique temporary files beside the destination and publish through an atomic replacement only after output closes. Failed copying or replacement preserves the prior complete asset and removes temporary files. There is no truncating fallback if atomic replacement is unavailable. This race was identified from source; it was not reproduced on a headset.

Software verification on 2026-10-29:

- New setup tests were observed failing before implementation: missing panel dimensions/policy, and missing-ROM help pointing at the wrong location.
- `D:/Python314/python.exe -B -m unittest discover -s ports/gunblade-ny-vr/android/tests -v`: 17 tests passed, exit 0. Existing tests use synthetic fixtures in this game's private test directory; no original ROMs were edited. Asset regressions use real files, injected read failure, overlapping worker gates, an already-open reader and a rejected replacement; they verify complete contents and temporary cleanup.
- All Android production Java sources compile with JDK 21 against `E:/AndroidSdk/platforms/android-36.1/android.jar`, exit 0. Compiler notes identify existing deprecated Android APIs.
- An initial attempt to relocate all existing delivery-test scratch files into the workspace reached the package output boundary by design and failed two package tests; the unchanged canonical scratch-path suite above passed after the focused filesystem escalation.

This is software/source evidence. The root task owns the fresh APK, installation and headset appearance/automatic-entry checks; those results are recorded separately. No publication was performed.
