# Settings menu

The menu opens on Gun, followed by World, System and In-game. Hold the existing
Menu control to open or close it, or choose Close. Aim and press the trigger
to select; the active controller's stick adjusts the focused setting.

Only one controller shows the settings laser. Press the other controller's
trigger to take control; that handover press does not activate a setting.
The next fresh press selects. Both players retain their normal gameplay lasers.
A white contact point with a purple glow marks the active ray's intersection
on top of the menu, including when gameplay Laser is Off. Clicking an adjustable
row body advances its value; the painted minus/plus and option pills still
select their exact displayed action. The active controller's stick also adjusts.

## Gun

Gun model, Gun angle, Laser, Crosshair and Haptics are the main controls.
Laser is one main-page selector: Off, Normal, 5x and 10x. Normal uses the former 2x width. Advanced gun settings
contains Gun size, Crosshair min/max size and Crosshair shrink distance. The gameplay beam fades over at most three metres. The menu pointer uses its own width and is available with gameplay Laser Off.
Existing Off/width choices persist through this update. Laser and Crosshair
remain separate choices, and hidden/inactive crosshair tuning keeps its values.

## World

The main World page contains Wider view and Advanced world settings.
Wider view offers Off, On and Fade. Fade keeps the wider scene and smoothly
darkens extra scenery outside the original arcade screen, reaching black a
quarter-raster beyond its edges. It leaves the original image clear. Native
panels, crosshairs, service pages and raster fallback retain their existing output.
Older On/Off choices retain their behavior; Fade is an explicit saved choice.
Advanced exposes exact World size, HUD size, Panel distance and View width.
Named size presets are removed; existing numerical sizes remain unchanged.

## System

Two players selects P1-left/P2-right or the reverse. Native joining remains
unchanged. Open Service Menu retains original operator and AIM SET access.
How to play shows controller help. Reset to defaults confirms before restoring
VR gun/aim/world preferences; game files, progress and arcade settings stay intact.
Quit game also requires confirmation. Resume/Recenter/Fire mode rows are removed;
the existing controller recenter shortcut and automatic trigger fire remain.

## Game

These are the actual Revision A service options, verified in the original game:

|Setting|Values|
|---|---|
|Difficulty|1/8 through8/8|
|Shifting difficulty|1/8 through8/8|
|Player life|1through7 starting lives|
|Attract sound|Off/On|

Adjust these validated values at any time. Edits remain staged across closing
and reopening the menu; select Apply settings and restart to save and restart
the original arcade game. This ends the current round. The operation resets to
the original boot, applies through original Test/Service saving, then restarts
and verifies the values before returning to the game. Keep the headset focused.
Untouched fields follow the latest native snapshot rather than overwriting
manual Service changes. Saving uses the
original Test/Service menu and original EEPROM save. It can take a short time;
progress is shown, and the menu waits for saving to finish. Keep the headset
focused. A focus interruption reports the interrupted save and preserves
original Service access for recovery. Source/revision mismatches disable these
controls. Changes are read back before a successful save is reported.
If native RAM changed before an interrupted EEPROM save, complete or exit the
original Service menu as instructed; unchanged RAM is not proof of a saved
setting. Pending UI edits clear only after successful acknowledgement.
The full operation locks controls until completion; manual Service and unsafe
state requests are rejected. Opening the menu during original boot allows initialization to finish behind
the panel; focus loss and loaded play/attract/service retain pause behavior.
Bounded menu-open and trigger diagnostics are appended to the existing external
Gunblade XR log's `.menu` companion for input/readiness troubleshooting.

Country, Cabinet type, Gun reaction and pricing are recorded in the native
audit rather than added as unrelated VR controls. Original hardware/diagnostic,
calibration and bookkeeping access stays in Open Service Menu. The earlier
four-word difficulty catalogue was inaccurate for this measured Revision A;
the actual game has eight numeric levels and a real Player Life row.

See native-game-options.md for the measured native contract and software limits.
New headset play and Windows OpenXR runtime acceptance remain separate checks.
