# Fullscreen exit: content briefly stands still while the border moves

Status: the end-of-animation hold is found and fixed (committed 2026-10-30,
margin widened to 80 ms 2026-10-31). A second failure on the first exit
after a launch is reproduced but not explained (see "The first couple of
exits" section); awaiting the user's confirmation on their own runs. The
temporary diagnostics are still in `GlkController+FullScreen.m`, marked
`TEMPORARY`, and should be removed once the fix is confirmed. Then delete
this file.

## The problem

Sometimes, during the custom exit-fullscreen animation, the window contents
look stationary for a fraction of a second while the window border moves left.
Intermittent, seen in plain Curses, possibly only with the green button, never
in earlier harness runs (which captured at 22 fps and looked only for a
mismatch between the status bar and the window's right edge).

## Cause

The system runs its own clock for the exit transition. It gives
`window:startCustomAnimationToExitFullScreenWithDuration:` a duration
(0.5 s), and when that duration is up it moves the window from the fullscreen
Space to the desktop whatever the custom animation has done. During that Space
switch, about 100 ms, the window's picture is frozen: no Core Animation commit
from the app reaches the screen, while the window's frame on the server still
moves. Frozen picture plus moving frame is exactly the reported look.

Our animation overran that deadline on every exit:

- The step timer's clock started after the style mask change, which takes
  40–70 ms (`setStyleMask:` runs an `NSPerformVisuallyAtomicChange` that
  resizes the window). So the whole animation ran that much behind the system.
- The last step therefore landed 55–95 ms after the system's deadline, and the
  `NSWorkspaceActiveSpaceDidChangeNotification` arrived at ~517 ms, before our
  final step (553–593 ms). The commit for the second-to-last step took ~90 ms
  instead of ~15 ms, spanning the Space switch.

On this M1 the lag was one or two steps (the window 6–22 pt wider than final),
so the visible effect was a 34–86 ms hold of a slightly-too-wide picture that
then snapped to the final frame. Any extra stall on the main thread during the
exit (the user's machine, a busy game, a long layout) makes the lag larger and
the freeze obvious. That explains the intermittency.

Evidence, from 8 baseline runs with the diagnostics on (log and 60 fps band
capture; details under "Harness"):

| | before the fix | after the fix (16 runs) |
|---|---|---|
| last step (eased 1.0) | 553–593 ms | 449–467 ms |
| Space change notification | 516–522 ms | 513–520 ms |
| pre-final picture hold | 34–86 ms, window 6–22 pt too wide | 0 ms in every run |

## The fix

In `window:startCustomAnimationToExitFullScreenWithDuration:`:

- The animation's progress is measured from the moment the system calls us,
  before the style mask change, through a new
  `stepFullScreenAnimationWithDuration:from:step:completion:` (the old
  three-argument method calls it with "now", so the enter animation is
  unchanged).
- The steps run for `duration - kExitFullScreenMargin` (margin 0.08 s), so the
  final frame is set and committed about 80 ms before the system takes over.
  The margin was 0.05 s at first; see "Results with margin 80 ms" below.

The first step now arrives 43–79 ms after the call with eased 0.02–0.07; the
window simply stands still for the style mask change, which is a start delay,
not a content/border split.

`display:YES` in the step: the log shows each step takes about 2 ms including
the draw, and the content moves in step with the frame on every tick, so there
is no reason to revert to the `display:NO` variant. Keep it.

Ruled out by the log: no foreign frame changes during the animation (the only
"outside a step" resize is our own `setStyleMask:` call), no late ticks beyond
the first, the snapshot window from the enter is on screen but with its
content view hidden and plays no part, the system's `windowDidExitFullScreen:`
arrives after our completion and is ignored as intended.

## Confirming it on the user's machine

The diagnostics log to a file when the user default `FullscreenExitLog` names
one:

```
defaults write net.ccxvii.spatterlight FullscreenExitLog /tmp/fsexit.log
```

or `-FullscreenExitLog /tmp/fsexit.log` as a scheme argument. Each exit writes
a block: start (window/server/target frames, on-screen windows, snapshot
window), the style mask change, every step (gap since the previous one, LATE
if over 25 ms, eased value, frame set, server bounds, game view frame, time
taken, mouse), the CA commit completion per step, every window/game view frame
change outside a step with a call stack, and the notifications (will/did exit
fullscreen, Space change, screen, occlusion, key). A glitching exit would show
the step with `eased 1.000` after the `NSWorkspaceActiveSpaceDidChangeNotification`
line, or a large gap in the steps around 450–520 ms. With the fix, the last
step should always precede the Space change by ~50 ms.

To remove the diagnostics: delete everything marked `// TEMPORARY` in
`GlkController+FullScreen.m` (the `#pragma mark - Temporary diagnostics`
section, the `fsExitLog*` category methods, the hooks in the exit method and
in `windowDidEnterFullScreen:`/`windowDidExitFullScreen:`, and
`#include <unistd.h>`), then `defaults delete net.ccxvii.spatterlight FullscreenExitLog`.

## The "first couple of exits after opening a game" pattern

The user sees the glitch on the first exits after opening a game, then not
again until the game is closed. Checked on 2026-10-31 with fresh launches of
the isolated app (`firstexits.sh`: launch, open Curses, three green-button
exits starting as soon as the window is up; with and without a kept home so
the game also autorestores). 13 first exits and 32 later exits in all.

What the captures show is a second, different failure, seen only on first
exits after a launch (2 of 13; never on a later exit):

- `mk1_1` (margin 80 ms, autorestored game): the border shrinks normally
  until about step 14 (~300 ms), then the picture stops updating while the
  frame keeps moving: the status band's left edge stays at x=173 from 806 ms
  to 1014 ms of the capture while the right edge goes on from 713 to 567, and
  the final picture appears at 1033 ms, right after the Space switch. That is
  exactly "content stationary while the border moves left".
- `m1_1` (margin 80 ms, fresh home): the window is never seen shrinking at
  all. The whole fullscreen Space slides out to the right as one picture with
  the window still at its fullscreen layout, then a blank white window stands
  at the final frame from 913 ms, and the content appears at 1034 ms.

The app's log is the same in these two exits as in the good ones: every step
sets its frame on time (last step 424–435 ms, Space change 530–536 ms), the
server bounds follow, and every step's Core Animation commit completes in
20–55 ms. The pictures left the app; the window server or the Space
transition did not show them. So this one is not the timing bug fixed above,
and nothing in the log distinguishes it. It needs system-side tracing (or a
guess at a mitigation, such as doing the style mask change at the end of the
animation instead of the start, since on these cold exits the transition may
be capturing the window into the departing Space's picture) before anything
is changed.

What the fix above does for first exits: the end-of-animation hold, which
happened on every exit before the fix, is gone on first exits too. The first
exit after a launch is the slowest one (first step 76–83 ms after the call,
later exits 38–60 ms; the mid-animation commit stall at step 13–14 is 38–55
ms instead of 23–32), which is why the old timing hurt most there. With the
old timing restored for comparison, the hold came back in 3 of 9 exits
(39–75 ms), not only on first ones; the Space change itself varies between
506 and 575 ms, and the old timing glitched whenever it came early.

Results with margin 80 ms (15 exits, 5 launches): last step 423–438 ms,
Space change 508–536 ms, so at least 70 ms of slack; hold 0 ms in every
capture except the two first-exit cases above, where the tracker's "hold"
numbers (16 ms at R 1000, 68 ms at R 568) are misreadings of the frozen
picture, not an end hold. With margin 50 ms the slack was 40–55 ms and one
single-frame (17 ms) hold was seen on a first exit, hence the change.

## Harness

In `~/spatterlight-harness/fullscreen-exit/` (moved out of the session
scratchpad, which is wiped daily):

- `mkiso.sh` copies the Debug build to `iso/SpatFS.app` with bundle ID
  `net.tmp.spatfs` and signs it ad hoc.
- `firstexits.sh LAUNCHES EXITS tag [keephome]` launches that copy afresh
  (own HOME, `SPATTERLIGHT_TEST_STORE=spatfs`, the log on), opens `curses.z5`,
  moves the window to (20, 60) and does EXITS green-button exits, capturing
  each; `keephome` keeps the home folder so the game autorestores.
- `reccap2 DIR SECONDS Y0 HEIGHT` captures a full-width band of the display at
  60 fps with ScreenCaptureKit into memory and writes PNGs named by ms.
  `mouse move|click|path` posts CGEvents. `act.applescript` drives the window
  through System Events with the inline bundle-ID tell.
- `chk2.py DIR` tracks the content (the longest grey run: the status bar, or
  the title quote box) against the window's right edge, flags mismatches, and
  prints the "pre-final hold": how long the last non-final picture stayed on
  screen before the final one. That hold is the glitch's measurable signature.
- `summ.py fsexit.log` prints one line per exit from the diagnostics log:
  first step, last step, completion, Space change, longest gap and step.
- `fsexit_fixed_nk.log`, `fsexit_oldtiming.log`, `runs_*.txt` are the logs and
  capture results behind the numbers above.

Harness notes kept from before: test only on an isolated copy with its own
bundle ID and inline `tell (first application process whose bundle identifier
is ...)`, checking the frontmost app before input; in fullscreen the green
button is at about (62, 46) on this 1440x900 display once the title bar is
revealed; the window can't be repositioned while in fullscreen.
