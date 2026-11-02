# Gunblade NY Revision A native game options

## Apply settings and restart (current VR action)

The VR action accepts staged settings during a round, clears native inputs,
schedules an original soft reset, waits for a real boot/eligible attract state,
then runs the guarded Test/Service save queue below. After the original EEPROM
acknowledgements it soft-resets again, verifies native readback, and reports
completion. Shared runtime ABI is 0x47424e05. Manual Service, focus-loss, invalid
values/revision and timeout guards remain; this is not a direct memory write.
See the 2026-11-01 apply-restart native proof in the overseer change record.


Verified2026-11-01 against the owned Revision A original program and the existing
private engine. The original Game Assignments page differs materially from the
older manual-only audit. Use the measured native choices below.

| Original row | Original choices | Default | Readback byte |
| --- | --- | --- | --- |
| ADVERTISE SOUND | OFF / ON | ON | `0x54c0e4` raw0/1 |
| COUNTRY | JAPAN / USA / EXPORT | JAPAN | `0x54c0e5` raw0/1/2 |
| GAME DIFFICULTY |1/8 through8/8 |4/8 | `0x54c0e9` raw0..7 |
| SHIFTING DIFFICULTY |1/8 through8/8 |4/8 | `0x54c0e7` raw0..7 |
| PLAYER LIFE |1 through7 |3 | `0x54c0ea` raw1..7 |
| GUN REACTION | OFF / ON | ON | `0x54c0ed` raw0/1 |
| CABINET TYPE | DX / SD | DX | `0x54c0ee` raw0/1 |

All listed choice cycles were driven through the actual native Test and Service
switches with fresh isolated native storage. The default visible page has no
Language row. Strings/code for a conditional Language row exist; that row was
not independently exposed and is outside the player-facing adapter. No Easy /
Normal / Hard / Hardest translation is applied to the eight native levels.

The VR Game page exposes Difficulty, Shifting difficulty, Player life and
Attract sound. Region, language, cabinet type, gun motor reaction, pricing,
bookkeeping and backup clear remain in the original Service Menu. PLAYER LIFE
is an original setting, not a live-health override. Native stage/continue
initialization reads its configured count; no current player's life is edited.

## Production adapter contract

`read_game_options(Read32)` accepts aligned physical i960 program-space reads.
It applies the existing six-word game-state revision guard plus six instruction /
operand guards at the assignment reader, life writer, save dispatch and save
routine. It also checks the original SEGA/GBNY EEPROM-buffer signature, native
size marker and every exposed option's range. Invalid input disables the page.

`GameOptionsTransaction` belongs to the emulation thread and is ticked once per
native vblank. A request contains an ID, a `GameOption` and its native raw value.
It starts only in original attract/ready modes0/1/8/9, with no joined players.
It refuses boot, active game/selection, existing Service and invalid values.
Repeated IDs cannot replay; a new ID while saving does not replace the active
request. An unchanged requested value is acknowledged without entering Service.

The transaction returns only discrete original Test(`0x04`) and Service(`0x08`)
edge requests. It uses the existing12-frame `OperatorLatch`, leaving24 ticks
between edge emissions. It enters native Test Menu, navigates to Game
Assignments(index5), cycles the requested row to the requested value, exits the
assignment page, then selects the parent native Exit(index10). There are no
native-memory, EEPROM, CPU-register or savefile writes in this adapter.

Original row handlers `0xefc10..0xf01d4` update the live EEPROM buffer and mark
dirty at `0x54bf20`. Parent Exit calls `0xde30`, which increments the native
write count at`0x54c0dc`, updates the original checksum at`0x54c0d8`, and calls
the original serial EEPROM writer via`0x75230` /`0x751e0`. Its44 halfwords come
from`0x54c0d0..0x54c127`. MAME retains its normal NVRAM file-save lifecycle;
cold-boot verification here includes normal engine exit. Crash-durable disk
flushing is not newly implemented or claimed.

Completion requires return from native Service to eligible attract, requested
value readback, exactly one additional native write, and unchanged unrelated
EEPROM bytes. Only the selected setting byte, checksum and native write counter
may differ. Therefore staged UI changes are not native changes until Apply.
Applying several settings serially performs one original transaction per changed
field, preserving the previous fields at every transaction.

Settings take effect for the next game: apply between games, then Start normally.
Native Service return reruns the game's normal initialization; the application
need not restart. No live difficulty/health behavior is promised during a game.
Player life's configured initial-life word is specifically verified to refresh
on final native Exit: `0xf1b7c` calls`0x6b930`, whose`0x6b9ec` /`0x6baa0`
read the native setting and store it at`0x5329f0`. The existing after-Exit
snapshot has setting5/configured5, its cold snapshot has5/5, and the Player life
and four-option cold snapshots both have7/7. Subsequent native stage/continue
initialization copies that configured count; no live player-health write occurs.

Focus loss immediately fails with Interrupted and stops emitting edges.
Integration must also clear the existing operator latch/pending/latched fields,
since a previously emitted pulse otherwise remains held for up to12 ticks.
Failure/timeout relinquishes service ownership so original Service controls can
recover. A partially changed native option is not silently rolled back; finish
or inspect the original menu after an interruption. Transactions time out after
1800 native ticks. The presentation thread only stages/queues requests and reads
acknowledgements; it never edits native storage.

## Evidence and limitations

Private evidence root:
`D:/VR4ALL-Gunblade-Build/research/settings-menu-20261101`.
Commercial program data, screenshots and memory/EEPROM dumps remain there.
The public batch has metadata only in
`overseer/changes/2026-11-01-gunblade-menu-implementation/native-options-evidence.json`.

Four independent live engine runs use the actual production transaction and
actual production OperatorLatch through a read-only observation / real cabinet
input bridge. Targets are Difficulty8/8, Shifting difficulty1/8, Player life7
and Attract soundOff. All four acknowledge an original EEPROM commit, preserve
all unselected EEPROM bytes/calibration and isolated zero-credit storage, and
retain the target values on independent cold restarts. An early-boot probe was
correctly refused as Unavailable. The public C++ test also reads all four private
cold snapshots optionally through `--native-evidence <program> <research-root>`.

This establishes software/native save behavior. Audible attract playback,
physical headset interaction, nonzero-credit fixtures, full campaign balance
and simultaneous real-user operator intervention were not physically tested.
No headset install, engine rebuild or publication was part of this research.

## Integrated queue and pause verification

The final isolated proof calls the actual `SharedRuntime::begin_game_options`,
`SharedRuntime::tick_game_options` and`SharedRuntime::engine_pause_requested`
methods used by production `host_bridge.h`. One Apply queues all four changed
settings, targeting raw values7/0/7/0. It drives the original engine through
four native assignments/save/Exit sequences. This is the production queue and
latch implementation, not an independently reimplemented host policy.

Across2822 observed native ticks, every pre-final tick maintains busy and the
menu-pause override, including the three queue transitions. The four
native writes advance1→2→3→4→5. After the fourth final native Exit, status is
Complete, busy clears and the menu pause decision resumes. A separate cold
process retains all four targets together; unrelated EEPROM/calibration and
zero-credit storage remain unchanged. Metadata is in the batch's
`native-options-runtime-evidence.json`.

`tests/game_options_queue_test.cpp` verifies a queued four-setting submission,
focus loss after the first real Test pulse enters the native latch, one-time
clearing of latch/pending/gameplay state, and the original manual Service pulse
remaining usable for its full12-frame latch after failure. This complements
the parent's `game_options_runtime_test.cpp` revision-failure/recovery coverage.
