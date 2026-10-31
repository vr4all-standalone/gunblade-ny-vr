# Gunblade NY native state evidence

This adapter targets **Gunblade NY Revision A**, running the pinned MAME source
`f34f02505e32c1993c6a782b6814232cbfc74e36`. All addresses below are physical
i960 **program-space** addresses for this game. They were obtained from the
verified private game set and read-only disassembly/RAM observations. No game
data, research dumps, or original ROM changes are distributed here.

## Interface and revision guard

`read_game_state(Read32)` accepts an aligned physical-address-to-u32 reader.
It compares six small instruction/operand words at the main dispatcher, join
handler, life gate, AIM SET initialization, and service dispatch table. A
mismatch returns a completely disabled state. This identifies the audited
revision; it is not a substitute for the private media audit.

`GameState.session` is assigned by the host, **after** reading native state and
**before** passing it to `ShotTracker::update(state, read)`. Session generations
must change on native reset/new play. Call the tracker on the emulation thread,
once per native vblank; call `suspend()` if its surrounding lifetime is reset.
The stateless reader leaves shot feedback disabled. A tracker can enable both
`counting_shots` and `shot_counters_valid` for trustworthy observations.

## Modes, Start eligibility, and players

| Native value | Evidence and adapter meaning |
| --- | --- |
| `u32 0x532d54` | Main mode: dispatcher at `0x75d64..0x75da8`, table `0x75d00`. Observed 11=boot, 0=title/attract, 2=play including selection/intro, 10=operator service. |
| `u32 0x532d48` | Submode. Observed main 2/sub 1=selection, sub 6=intro, sub 2=active stage. `playing` means main 2; the shot tracker additionally requires sub 2. |
| `u32 0x5401d0` | Native Start lockout in main mode 2. Exact Start dispatcher `0xed830..0xed878` permits modes 0, 1, 8, 9 and mode 2 only when this value is zero. `ready` follows that table; it does not claim that a player has sufficient credit. |
| `byte 0x532d58` | Joined bits 0/1. Native Start sets them at `0xed8f8..0xed8fc` and `0xed9e8..0xed9f0`. Higher bits have other meanings and are ignored. `PlayerState.state` is a normalized 0/1 joined value, not an invented native player-state enum. |
| `u32 0x540114`, `0x540154` | P1/P2 remaining lives, separated by 0x40 bytes. Stage initialization `0x6bc50..0x6bcfc` and continue initialization `0x6d7e0..0x6d814` copy the configured initial life count from `0x5329f0`. |

The native shot-creation routine checks the signed life value at
`0x6d414..0x6d418` and returns when it is nonpositive. The native death handler
`0x72f94..0x73030` tests that same per-player `+0x14` word and clears the player's
joined bit when it is nonpositive. HUD calls at `0x72cec` and `0x72d04` pass the
same words to separate player life displays. The reader exposes health as
`remaining / configured_initial`, preserving zero for missing, negative,
inconsistent, or unjoined values. It exposes joined/life data only in main 2.

The preserved 10 Hz RAM run shows no joined players before Start, P1 joined at
17 seconds, both players joined by 31 seconds, and P1 lives 3→2→1 during combat.
The additional per-frame run records the losses at 45.008816 and 49.041904
native seconds. A zero-life death is covered by the native branch and synthetic
fixtures; it was not reached in these bounded runs.

## Accepted shots

The native routine `0x6d400..0x6d648` creates an actual projectile after checking
remaining lives. It writes the player owner at `0x6d440` into an eight-entry ring:

| Field | Address/layout |
| --- | --- |
| Next slot | `u32 0x5329ac`, values 0..7 |
| Ring base/stride | `0x532a40`, 0x48 bytes per entry |
| Owner | `u32 slot + 0x2c`, 0=P1, 1=P2 |
| Advance | `0x6d5b4..0x6d5d0`: add one, mask with seven, store cursor |
| Native stage tick | `u32 0x540088`; increment at `0x6bf34..0x6bf44` |
| Ring initialization | `0x6cf10..0x6cf50` clears ring lifetimes and resets cursor |

The tracker accepts only the audited native shooting handlers selected through
`u32 0x532964`: `0x71110` (P1), `0x71440` (P2), and `0x71770` (both).
Their calls at `0x71260`, `0x71590`, `0x71900`, and `0x71bc0` create at most one
projectile per player in a native stage tick. The separate demo shooter
`0x71d70` is excluded. Held triggers may create many successive projectiles;
the reader does not turn trigger edges or held durations into shots.

For contiguous stage ticks, the tracker visits the new slots in cursor order,
including wrap 7→0, and increments the corresponding observed counters. It
rejects skipped/reset ticks, corrupt cursors, more than two events, duplicate
owners in one tick, and owners without a joined bit. It establishes a fresh
baseline on join, session, or stage/handler changes. Repeated sampling of one
unchanged tick cannot replay its events. Counters represent observed accepted
projectiles; events during a sampling gap are deliberately omitted.

The 51-second original-engine trace contains 2,934 frame observations. In the
active stage, 1,023 adjacent sample pairs comprise 917 zero-shot ticks, 89
one-shot ticks, 16 two-shot ticks, and one stage-timer reset without a shot.
Every event tick advanced by one; every owner was 0/1; simultaneous ticks had
different owners. The adapter replay reports **66 P1 / 55 P2** shots, matching
the independent slot traversal. Both sustained independent fire and simultaneous
fire were exercised. The bytes at `0x532ced`/`0x532cf5` are held/edge input
mirrors, and `0x540134`/`0x540174` are firing-duration clocks reset by shot creation;
none is a cumulative shot counter. Persistent completed-playtime averages at
`0x1d00234`/`0x1d00250` are also not shots.

## Original AIM SET and committed calibration

Service dispatch `0xee4a0..0xee54c` uses main mode 10, parent submode 1 for the
menu and 3 for a selected page. `u32 0x54bdf8` is the service selection: 1 is
INPUT TEST, 7 is AIM SET. Selection 7 dispatches through `0xf1bcc` to `0xf0ba0`.
`gun_calibration` requires service mode, selected-page submode 3, and selection 7.

The two independent native AIM SET phases are `u32 0x54bdc4` (P1) and
`u32 0x54bdc8` (P2): 0=idle, 1=initialize extrema, 2=scan extremes until the
next native trigger edge commits them. `service_submode` packs P1 in bits 0..7
and P2 in bits 8..15. `service_busy` is a player mask for phases 1/2. This is
derived from those actual phases, not from the unrelated pending-settings-write
flag `0x54bf20`, which remains set after calibration completes.

The 33-second original-engine AIM SET run records P1 0→1→2→0 at
23.035440/23.052824/26.042872 seconds and P2 at
28.042032/28.059416/31.049464 seconds. Native screenshots show the original
instructions and scan page. All changes were made by original cabinet inputs.

`read_native_calibration(read, player)` returns committed native values:

| Value | P1 | P2 |
| --- | --- | --- |
| Left/right | bytes `0x54c0f1` / `0x54c0f2` | bytes `0x54c102` / `0x54c103` |
| Top/bottom | bytes `0x54c0f0` / `0x54c0ef` | bytes `0x54c101` / `0x54c100` |
| Center X/Y | bytes `0x54c0f3` / `0x54c0f4` | bytes `0x54c104` / `0x54c105` |
| Origin X/Y | floats `0x54c0f8` / `0x54c0fc` | floats `0x54c108` / `0x54c10c` |

The default initializer `0xded4..0xdf78` uses X endpoints 64/192, Y endpoints
192/64, centers 128/128, and origins (-3.2,-1.5) / (+3.2,-1.5).
`factory` means that the values match this initializer, not a claim about their
history. The helper preserves the negative factory Y span. Initialization
`0x70f70..0x7106c` establishes angular gains `(37.5 / X_span) * 183` and
`(27 / Y_span) * 183`. Gameplay subtracts the stored center before applying
these gains (`0x71114..0x71148`, `0x7118c..0x711bc`, and P2 equivalents), so
substituting an endpoint midpoint would change native behavior.

The verified calibration run commits P1 X105..255, P2 X0..150, both Y17..174.
The committed native centers are **P1 (145,65), P2 (109,65)**. The helper replay
verifies these values and factory defaults. These stored native calibration
values are distinct from cabinet input limits and from the user's VR aim map.

The native conversion sends the truncated integer angular quantity through
SHARC/TGP command 29, with amplitude 13. The original program then adds the
stored gun origin. A separate 43-second read-only run sweeps an 81-point ADC grid
for each player. After stage initialization and input settling, all **162 distinct
positions** match:

```text
angle = signed16(truncate_float((ADC - stored_center) * native_gain))
native_axis = stored_origin + 13 * sin(angle * pi / 32767)
native_z = 10
```

The largest native-coordinate error is **5.72136e-7**. Using pi/32768 or a tangent
instead of sine does not match the observations. This identifies command 29's
behavior for the audited aim range empirically; it does not claim a complete
disassembly of the original SHARC firmware. The i960 call/argument/center/gain
sequence is independently visible at `0x71114..0x711ec` and the P2 equivalents.

The native **reticle** uses focus **600/600**, distinct from world window 1's
400/400. The program explicitly sets float 600 into `0x52a2e0`/`0x52a2e4` at
`0x71270..0x71280` before reticle submission, and restores the world focus at
`0x71400..0x71418`. Preserved scene capture sequence 2015 (35.046144 seconds)
contains 30 reticle polygons per player in window 4, each with focus 600/600
and matrix translation exactly matching its native gun point. The capture's
native center (248,320), CRTC offsets (0,128), and MAME `model2_3d_project` give:

```text
screen_x = 248 + 600 * native_x / 10
screen_y = 192 - 600 * native_y / 10
visible_raster = 496 x 384
normalized_u = screen_x / 495
normalized_v = screen_y / 383
```

`native_aim_point(calibration,x,y)` exposes that measured native forward
conversion. `native_screen_to_adc(player,u,v,calibration)` finds the closest
discrete ADC to the requested normalized reticle coordinate using that
projection. It searches only the separately established physical cabinet ranges
from `cabinet_io.h`, preserving the original game's integer angle quantization.
Finite offscreen coordinates clamp to the raster edge; trigger offscreen policy
remains the host's responsibility. Invalid player/calibration/nonfinite inputs
cannot enable the result. `screen_inverse_valid` is true for calibration the
reader has validated for this audited gameplay reticle mapping.

The normalized coordinates follow `NativeLens`'s inclusive pixel-endpoint
convention, so the inverse targets `u*495` and `v*383`, not raster dimensions.
The native evidence replay verifies **368 exact ADC round trips** for observed
onscreen points. Factory normalized (.5,.5) maps to P1 (176,97) and P2 (79,97);
factory top-left P1 maps to (114,28), and bottom-right P2 to (142,163). Output remains
quantized to the original eight-bit ADC. This helper aligns the original native
reticle; it does not replace native collision, projectile, or enemy logic, and
it is not asserted for the service menu's different presentation.

## Reproduction and limits

`tests/game_state_test.cpp` runs without commercial files using synthetic word
fixtures. It was verified failing first for revision gating, native events, and
calibration activity/helper support, then compiled with C++20, `-Wall -Wextra`.
The optional private verification accepts:

```text
game_state_test.exe --native-evidence <private-interleaved-program.bin> <private-research-root>
```

This checks nine preserved RAM snapshots, the entire 2,934-frame gameplay trace,
the independent AIM SET phase/committed-calibration trace, and the 162-position
aim sweep with 368 ADC round trips. Private evidence
lives under `D:/VR4ALL-Gunblade-Build/research/state-adapter/`; it was generated
with `native-baseline.exe`, fresh isolated writable state, and the read-only
`observe.lua`. Runs ended with exit 0, no Lua errors, and bounded wall deadlines.
The existing Gunblade project test command also passed (24 Python tests and its
native regressions). No engine rebuild, game-memory write, ROM patch, network
download, or publication was part of this state-adapter work.

`ammo`, `capacity`, and `weapon` remain zero because the unlimited automatic
machinegun does not justify a copied ammo/weapon decoder. The host now connects
the tracker, attributes native acknowledgements to delivered hand ownership,
and consumes the measured reticle inverse in the ABI03 integration. Long-run
stage transitions/continues, enemy-hit alignment,
headset gameplay, and physical cabinet comparison remain integration/hardware
verification work.
