# Fullscreen exit: content briefly stands still while the border moves

Status: the end-of-animation hold is found and fixed (committed 2026-10-30,
margin widened to 80 ms 2026-10-31). A second failure on the first exit
after a launch (the picture frozen at the fullscreen layout while the frame
shrinks) was reproduced but is NOT explained and NOT fixed: neither an exit
snapshot window, nor moving the style mask change to the end of the
animation with a title bar cover window (2026-10-31, see "Fix: style mask
change at the end, title bar cover"), changes its rate, about 1 exit in 30,
nearly always one of the first two exits after a launch. Master therefore
went back to the 9e61b42ae behaviour (style mask change at the start of the
animation, real title bar throughout, no cover) with the temporary
diagnostics removed; the deferred mask, the cover and the diagnostics are
kept on the branch `fullscreen-exit-titlebar-cover` (commits 56e1ed2eb and
724a15106). Anyone picking the freeze up again starts from that branch.

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

The diagnostics (removed from master 2026-10-31, still on the branch
`fullscreen-exit-titlebar-cover`) log to a file when the user default
`FullscreenExitLog` names one:

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

To get the diagnostics back, cherry-pick or diff them from that branch
(everything marked `// TEMPORARY` in `GlkController+FullScreen.m`: the
`#pragma mark - Temporary diagnostics` section, the `fsExitLog*` category
methods, the hooks in the exit method and in
`windowDidEnterFullScreen:`/`windowDidExitFullScreen:`, and
`#include <unistd.h>`). After using them,
`defaults delete net.ccxvii.spatterlight FullscreenExitLog`.

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

## Tried: a snapshot window over the exit (2026-10-31, not kept)

The enter animation covers the content with a snapshot window (a borderless
window showing a `CGWindowListCreateImage` of the game window). The exit had
no such cover, so the same was tried for the exit, as a mitigation for the
frozen-picture failure above: in `customWindowsToExitFullScreenForWindow:`
take a picture of the fullscreen window, show it in the snapshot window over
the content area (centred, aligned to the top so the status grid lines up,
clipped by the shrinking frame), move it with every step, and hide it 50 ms
after the system's `windowDidExitFullScreen:`. The patch, with its per-step
diagnostics, is saved as `exit_snapshot.patch` in the harness folder.

Three versions were run, 15 exits each (3 launches x 3 exits, then 2 x 3
with the home kept), captured and traced with `trace.sh`:

1. Snapshot window returned in the custom-windows list next to the game
   window, picture aligned to the bottom. The log's server bounds showed it
   left behind in the fullscreen Space: its x slid 22 -> 552 -> 1435 over the
   animation while the game window's stayed at 0..20, and it was below the
   game window in the on-screen list. So it never covered the shrinking
   window; it landed on the desktop at the end and sat over the final window
   for 50-140 ms with the wrong (bottom-aligned) picture on every exit.
2. Same, aligned to the top: no end artefact, still not covering.
3. Snapshot window made a child window of the game window (`addChildWindow:`
   `ordered:NSWindowAbove`, only the game window in the list): server bounds
   follow the content area exactly, it is above the game window, and the
   pictures match frame by frame, so the hand-over at the end is invisible.
   This is the version in the patch.

Result on the failure it was meant to hide: none. The freeze happened in
every version, with the snapshot frozen along with everything else (s3_1,
s3_2, tk2_2, u3_1, u1_3: picture stuck at the fullscreen layout, no title
bar drawn, then the whole Space slides out and a blank window stands at the
final frame for ~45 ms before the content appears). The log for those exits
is again indistinguishable from the good ones. It also got more frequent
and stopped being a first-exit thing: 5 of 45 exits with the snapshot (first,
second and third exits of a launch) against 2 of 45 without (first exits
only). Small numbers, but no reason to keep an extra window in the
transition, a `CGWindowListCreateImage` per exit (the first step came 20 ms
later on first exits), and a picture that would not match a scrolled buffer
window at the hand-over. Reverted (`GlkController+FullScreen.m` as committed
in 9e61b42ae; the next section changes it again).

What the server-side failure looks like in detail, from these captures: the
game window and the snapshot both keep their last fullscreen-layout picture
while their frames shrink (the frames are right: the clipped region follows
the frame), the title bar is not drawn at all, the departing Space slides
out as one picture with that content in it, and at the Space change the
window on the desktop is blank until the next commit after the system's
`windowDidExitFullScreen:`. That is the window server dropping the app's
commits for the duration of the transition, not anything the app does
differently. Possibly worth a Feedback Assistant report with the `firstexits`
captures; nothing further to try from the app side short of not using a
custom exit animation.

## Fix: style mask change at the end, title bar cover (2026-10-31)

The guess from "The first couple of exits" was tried next: clear the
fullscreen bit at the end of the animation instead of the start. With the
bit kept, the window has no title bar during the animation and its frame is
its content rect, so the steps animate to the content rect of the target
frame (`frameRectForContentRect:` with the fullscreen mask), and the
completion clears the bit and sets the target frame, which adds the title
bar on top and leaves the content where it is. The mask change in the
completion takes 17-60 ms and everything is committed before the system's
Space switch at ~510-530 ms (last step 420-440 ms as before).

Result: no frozen picture in 73 exits (15 + 28 + 30 over three builds, fresh
and kept homes, first exits included), against 2 of 45 with the mask change
at the start and 5 of 45 with the exit snapshot. The log cannot show why;
the working theory is that the mask change at the start, which rebuilds the
window's frame view while the transition is being set up, is what left the
window server with a stale picture on cold exits. Not proven, but the
numbers are good enough to keep.

The cost: the title bar does not exist during the animation, and when it is
added at the end it is drawn black, with a thin white line, for one or two
frames (the band tracker shows it as the status band's row dipping from 96
to 84 for 17-33 ms around 950-1000 ms of the capture). Laying the window out
before the first draw did not help; what the user saw was "a flashing /
flickering window title bar".

So a cover for the title bar (the user's idea): a borderless child window
showing a picture of the title bar, placed along the top edge of the game
window at every step, removed one second after the completion. Its picture
is the top strip of the enter snapshot when the snapshot has the final width
(`newTitleBarImageForWidth:height:`), which gives the real title bar with the
key-window look; while the window is wider than that, a layer behind the
picture fills the rest with the plain title bar colour sampled from the
picture, with the top corners rounded. Failing the snapshot (a game restored
in fullscreen as the app starts, a width change in fullscreen) the title bar
of a never-shown window of the final size is drawn with `cacheDisplayInRect:`
(forced for testing with `-FullscreenExitTitleBarOffscreen YES`). That one
has the inactive look, grey buttons and grey title, until the real title bar
takes over; see the comment on the method for why that is left alone.

Things learned getting the cover on screen, in the order they bit:

1. `setFrame:` on a borderless window is constrained to the screen's visible
   area, so the cover sat at y=838 under the menu bar until the window's top
   edge got there. `TitleBarCoverWindow` overrides
   `constrainFrameRect:toScreen:` to return the rect unchanged.
2. A window first ordered in from inside
   `window:startCustomAnimationToExitFullScreenWithDuration:` is never seen:
   `visible` is YES, it has server bounds, it is a child of the game window,
   but it is not in the on-screen window list at any point of the animation
   (it was checked with `CGWindowListCopyWindowInfo(OnScreenOnly)`, which
   lists windows front to back; the cover was simply absent) and the
   captures show the desktop behind where it should be. Ordering it with
   `orderWindow:relativeTo:`, giving it
   `NSWindowCollectionBehaviorFullScreenAuxiliary`, made no difference. The
   system has already started its transition to the desktop Space by then
   and new windows do not join the departing fullscreen Space. Made and
   ordered in from `customWindowsToExitFullScreenForWindow:` instead, as the
   rejected exit snapshot was, it is in the list right above the game window
   (indices 3 and 4) from the first step and visible in every frame.
3. Child windows move with their parent's origin, so the cover is placed
   after each step's `setFrame:`, not before.

The hand-over at the end is invisible: the title bar region differs by at
most 11/255 per channel between the last frame with the cover and the first
without. The band tracker no longer shows the dip (row 92-96 throughout),
and `trace.sh` shows the usual fall from ~430 to 20 with no freeze.

Batches with the final build (6 launches x 3 exits fresh, the same with the
home kept, 36 exits, 2 harness misses): no title bar flash in any of the 34
exits, but 2 FREEZES, both first exits after a launch (fresh home, launches 2
and 3). They look like the old first-exit freeze with a title bar: the cover
moves with the frame while the picture under it stays at the fullscreen
layout, so the content shows white until the Space switch is over and the
final picture appears at ~1050 ms. (In both, the frozen fullscreen picture has
no status band and a title bar strip along the top: it is the window as it
looked just before the green button was clicked, in fullscreen with the title
bar revealed by the mouse at the top of the screen, which covers the status
line. The server shows that stale picture, clipped to the moving frame, until
the Space switch is over.)
Without the cover the deferred style mask had 0 freezes in 73; the 13 cover
development exits had none either. 2 of 34 is the rate the freeze had before
(2 of 45), and the exit snapshot window, also an extra window, made it worse
(5 of 45), so the cover may well bring the freeze back. A control run with
the cover disabled (`-FullscreenExitNoCover YES`, TEMPORARY default) against
another run with it is the next thing; results below when done.

Control (same build, fresh home, 12 launches x 2 exits each, so 12 first
exits per run; the first no-cover run was discarded because the harness lost
the game window on several launches, and a warm-up launch was added):

- cover disabled (`-FullscreenExitNoCover YES`): 2 freezes in 22 exits (2
  harness misses), launches 4 and 6, both first exits.
- cover enabled: 1 freeze in 24, launch 2, the SECOND exit.

So the cover is not what brings the freeze back, and the deferred style mask
does not keep it away either: with the mask change at the end, the day's
total is 5 freezes in 166 exits (0/73 before noon, 2/34 + 1/24 + 2/22 after),
against 2 in 45 with the mask change at the start. Same rate, same pattern
(5 of 5 within the first two exits after a launch), and the app log is
identical for frozen and good exits. The freeze is server-side and nothing
done from the app so far has moved it; the exit snapshot window may just
have had a bad run too. What the deferred mask and the cover buy, then, is
nothing measurable; what they cost is ~230 lines, a child window during the
exit, and the inactive-look title bar for a game restored in fullscreen. The
simpler state before them (9e61b42ae, mask change at the start, real title
bar throughout, no flash) is the candidate to go back to; the user decides.

Decided 2026-10-31: master went back to 9e61b42ae's fullscreen code, minus
the diagnostics; the deferred mask + cover + diagnostics live on the branch
`fullscreen-exit-titlebar-cover` (tip 724a15106).

Captures: `za*`/`zak*` (final batch), `nd*` (no cover), `cv*` (cover),
logs `fsexit_final36.log`, `fsexit_nd.log`, `fsexit_cv.log`. `geom2.py` is
`geom.py` with black runs starting at x >= 640 ignored (a dark Safari page
sat to the right of the game window during the control runs).

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
- `geom.py DIR LO HI` prints, per frame, the longest black run (the Curses
  status band) and the longest grey run (the quote box); `trace.sh DIR...`
  reduces that to one line per capture of the band's left edge over time. A
  good exit's edge falls from ~430 to 20; the frozen-picture failure keeps it
  still (or creeping right by a pixel a frame) and then sends it right with
  the departing Space. `chk2.py`'s hold metric does not see that failure.
- `fsexit_fixed_nk.log`, `fsexit_oldtiming.log`, `fsexit_margin80.log`,
  `fsexit_snap1.log`/`snap2.log`/`snap3.log` (the three snapshot versions),
  `fsexit_maskend1.log`/`maskend2.log` (mask change at the end, without the
  cover), `fsexit_cover1.log`/`cover2.log`/`cover_dev.log` (cover
  development) and `fsexit.log` (the final batches), `runs_*.txt` and
  `runs_trace_*.txt` are the logs and capture results behind the numbers
  above; capture folders `n*`, `k*`, `o*`, `m*`, `mk*`, `s*`, `sk*`, `t*`,
  `tk*`, `u*`, `uk*` (snapshot), `v*`..`x*` (mask at the end), `y*`, `z*`
  (cover; `z21_1`..`z91_1` captured 600 rows to see the title bar), `za*`,
  `zak*` (final).
- `firstexits.sh` passes `$EXTRA` to the app as extra arguments (zsh:
  `${=EXTRA}`, since an unquoted `$EXTRA` is not word-split) and captures
  `$CAPH` rows (default 300; 600 shows the title bar region). A capture with
  the trace all at 826 and no exit in the log is a harness miss (the click
  did not land, the window is still fullscreen), not a freeze.

Harness notes kept from before: test only on an isolated copy with its own
bundle ID and inline `tell (first application process whose bundle identifier
is ...)`, checking the frontmost app before input; in fullscreen the green
button is at about (62, 46) on this 1440x900 display once the title bar is
revealed; the window can't be repositioned while in fullscreen.
