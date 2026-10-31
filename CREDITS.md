# Attribution

MAME 0.289, commit f34f02505e32c1993c6a782b6814232cbfc74e36, provides the original hardware simulation. The complete upstream source license is retained as LICENSE; per-file upstream licenses apply. Model 2 driver/video code credits R. Belmont, Olivier Galibert, ElSemi, Angelo Salese and Matthew Daniels, among the upstream contributors.

Model 2 scene capture, projection, bounded diagnostics and media verification are adapted from the licensed local ports/vcop2-vr foundation, with Gunblade-specific namespace, media metadata, cabinet mapping and tests. Platform build fixes come from the existing VR4ALL port projects. New native adapter code is GPL-2.0-or-later.

The common OpenXR host, VR4ALL splash, purple panels, gun effects, controller labels, GLB loader and settings components are adapted from the licensed local VR4ALL projects by mikermak and their credited contributors. Original per-file notices are preserved. Third-party software notices are in licenses/ and model credits in assets/models/CREDITS.txt. The compiled Arcade mesh is the attributed Namco Arcade Gun adaptation; it is not an extracted Gunblade game model.

The optional adaptive audio worker uses SoundTouch 2.4.1 by Olli Parviainen and its contributors, under LGPL-2.1-or-later. Unmodified library sources, notices and the exact upstream tag/commit are retained in vendor/soundtouch; its license is also supplied as licenses/SoundTouch.txt. Official source: https://codeberg.org/soundtouch/soundtouch. This processing is downstream of original hardware synthesis and does not change native game clocks or the native WAV recorder.

Gunblade NY original game data, graphics, firmware and trademarks belong to their respective owners and are not included. The project uses separately verified user-supplied media for private development. No commercial data is downloaded.
