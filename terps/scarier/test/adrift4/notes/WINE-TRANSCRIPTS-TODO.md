# TODO: Runner-transcript verification of the v4 walkthroughs

The *Professor Von Witt* exercise, generalised. That game's author walkthrough
was replayed command-for-command in the real Windows Runner under Wine, the
Runner's own transcript was diffed against Scarier's, and three engine bugs
fell out of the diff (bare `pick` take-synonyms, room-alt `Var2` being a
1-based **global** object number, and the 1-2 vs 3+ surface-listing split) —
plus one non-bug that cost a session (see *Verbose* below). The write-up is
the comment block above the `professor_solution.txt` row in
`harness/run_v4_walkthroughs.sh`; the fixes are commit `6b61f2ab`.

Nothing in this file is done yet, apart from Professor itself. It is the
candidate list and the recipe.

**Scope: all four file versions.** The file started as a pre-4.0 list (3.90 /
3.80 / 3.70, 66 rows); the 4.00 pool — 124 further seed-invariant rows, listed
here since 2026-08-23 — is the larger half and is where Professor itself came
from, so it belongs in the same list under the same recipe. The only thing
that changes with the version is **which Runner binary to launch** (next
section but one) and **how the transcript is captured**; the seed-invariance
test, the Verbose/Appearance pre-flight and the diff discipline are identical.

## Why these games and not others

A walkthrough can only be diffed turn-for-turn against the Runner if the game
is *deterministic along that path*. The test, run 2026-08-23 over the whole
v4 harness:

    export SCR_SEED=97      && harness/run_v4_walkthroughs.sh
    export SCR_SEED=424242  && harness/run_v4_walkthroughs.sh

A row that still PASSes its seed-1 golden under **both** alternate seeds has
no visible randomness anywhere on its walkthrough path. One alternate seed is
not enough: the second seed killed nine rows that had survived the first by
luck. Result:

| | rows |
|---|---:|
| wired v4 walkthrough rows | 303 |
| carrying their own `SCR_SEED` / `SCR_ASSUME_COMBAT` (excluded) | 16 |
| seed-invariant under both seeds | **190** |
| — of those, 4.00 | 124 |
| — of those, 3.90 | 54 |
| — of those, 3.80 | 10 |
| — of those, 3.70 | 2 |

Re-run 2026-08-23 against the current 303-row harness: the four counts above
reproduce exactly, so no row has drifted in or out since the list was first
cut.

The 16 excluded rows are excluded because `$ROW_ENV` is applied *after* `env`
in `transcript()`, so a row that pins its own seed overrides the ambient
export and its PASS proves nothing.

Two caveats on the test itself:

- Seed-invariance is measured **in Scarier**. A randomness feature Scarier
  does not implement at all would be invisible to it. The Runner is the
  oracle, not the harness.
- It measures the *walkthrough path*, not the game. A game can be full of
  random events that the walkthrough happens never to trigger.

## Which Runner to launch

Match the Runner to the file's version — 3.9 and 4.0 differ in real
semantics (event task dispatch, take wording, division rounding). The version
is in the .taf header, bytes 8-10, the XOR-obfuscated version characters:

| bytes 8-10 | version | Runner |
|---|---|---|
| `93 45 3E` | 4.00 | `run400.exe` |
| `94 45 37` | 3.90 | `run390.exe` |
| `94 45 36` | 3.80 | `run380.exe` |
| `94 45 39` | 3.70 | `run370.exe` |

Do **not** use "is there a zlib `78 9c` at offset 22" as the test — that is
true only of 4.00 and silently classifies every older file as unknown/4.0
depending on which way the check is written. All four Runners live in
`~/adrift-battle/runner/wine/pfx/drive_c/adrift/`.

## Capturing a transcript

The Adventure menu splits at 3.90/3.80, and this was measured live, not
assumed:

**run390 (and run400) — "Start Transcript".** Live streaming to
`C:\adrift\Adrift_<N>.txt`, plain text, opened *before* play. Use the
existing helper unchanged, passing the Runner as `$3`:

    cd ~/adrift-battle/runner/wine
    sh runner_transcript.sh <game.taf> <cmdfile> run390.exe

⚠️ **For run400, use the `_safe` helpers instead.** run400 pops a modal
"Cannot play sounds" alert on games that carry sound (the prefix runs with
`mmdevapi=d` because Wine audio soft-locks the whole Mac), and that modal
**eats one Return** — the fed command it swallows is silently lost and every
later line lands one turn early, which reads exactly like an engine
divergence:

    sh runner_transcript_safe.sh <game.taf> <cmdfile> run400.exe
    sh drive_ckpt_safe.sh ...        # instead of drive_ckpt.sh

A startup sound alert can itself *be* the "cascaded window" a retry loop is
chasing, so dismiss it before concluding the window stack is wrong.

⚠️ **The transcript menu is dead until the game has begun.**  While the Runner
sits on a startup "press any key" pause it is in a modal key loop and ignores
the menu bar entirely: the click does nothing, no Save dialog appears, and
`measure.sh` reports "Save-transcript dialog never appeared".  Count the pauses
in the game's opening and pass that count as `measure.sh`'s 4th argument.
`humbug` has two -- `[Press any key]` after the ASCII-art title and `<MORE>`
after the credits -- so it needs `PRE=2`.  These are game text, so they are
visible in the golden; count them there.

⚠️ **Only ever click the menu bar** (window-relative y+43), and pick the item
by its accelerator (`t` for "Start &Transcript").  A click into the window body
that misses an open menu lands in the scrollback, and the Runner **copies the
clicked word into the command entry field** -- so a missed menu click does not
merely fail, it glues a stray word onto the next scripted command.  The menu
cannot be driven from the keyboard alone: Alt+A is swallowed by Wine, so the
top-level menu still needs that one click.  For the same reason
`drive_ckpt_safe.sh` no longer clicks to focus at all by default (fronting the
process is enough); its old hard-coded `CLICK_Y=825` was off the entry field,
which put every focus click into the scrollback.

⚠️ **Close every menu before driving.**  An open menu swallows the first typed
command *and its Return*, so the whole replay runs one turn behind the engine --
which in the diff is indistinguishable from an NPC-walk divergence.  On `humbug`
that cost a 50-minute run: the only visible symptom was Schrodinger the cat
arriving one command late, everywhere.  `drive_ckpt_safe.sh` now takes
`FIRSTCHECK=<transcript path>` and aborts if the first command never reaches the
game; `measure.sh` passes it.  Escape does not reliably close a Runner menu.

**run380 and run370 — "Save Transcript".** No live transcript at all: the
menu item dumps the whole scrollback *at the moment you click it* to
`C:\adrift\Adven_<N>.rtf` and pops a "Transcript saved" MsgBox (no Save-As
prompt; dismissed with key code 36). So the order is reversed — play first,
save last:

    sh runner_savetranscript.sh <game.taf> <cmdfile> run380.exe
    textutil -convert txt -stdout pfx/drive_c/adrift/Adven_1_marooned.rtf

The output is RTF with a colour table, which is a bonus: it preserves the
bold and the colours, so room headings and author styling survive.
`runner_savetranscript.sh` also carries run370's different click point — that
Runner's window is a fixed 559x498 and its entry field sits at window-relative
280,452, where the maximised 3.80/4.0 layout puts it at screen 400,825.

⚠️ `runner_transcript.sh` ends with `ls -t pfx/drive_c/adrift/Adrift_*.txt |
head -1`. Point it at a 3.80/3.70 game and it will happily print the path of
somebody else's hours-old transcript instead of failing — that is how the
"run370 can't save transcripts" wrong conclusion got made. Use
`runner_savetranscript.sh` for those two.

Verified 2026-08-31 on `cave` (216 commands): the scrollback dump is NOT
capped — the Save-at-end `Adven_N.rtf` carried the whole session, from the
intro through command 216, so a full-length 3.80 diff is trustworthy.

⚠️ **The Runner reuses transcript numbers, so old citations rot.** Both
`Adrift_<N>.txt` and `Adven_<N>.rtf` restart their numbering when the prefix's
`adrift` directory is emptied or the Runner is reinstalled, and the new run
silently overwrites the old file. A note that cites "measured in `Adrift_18`"
is therefore only trustworthy if the file's mtime is *later* than the note.

To stop that happening again, every transcript in
`~/adrift-battle/runner/wine/pfx/drive_c/adrift/` was renamed 2026-08-24 to
carry the game it is a transcript of, keeping the original index as a prefix:

    Adrift_22.txt  ->  Adrift_22_xfiles.txt
    Adven_1.rtf    ->  Adven_1_marooned.rtf

Real games were identified by matching the transcript's opening prose against
the golden solutions; the synthetic probes (`pET*`, `srd`, `p39*`, `pwear400`)
by the `.taf` whose mtime immediately precedes the transcript's. Keep the
convention for new captures: `Adrift_<N>_<slug>.txt`.

The slug names the **game**, not the `.taf` that was loaded. Where a game has
two releases, or where a run used a doctored copy, the slug does not say which
-- all six `_sophie` transcripts were recorded from `sa.taf` and its `saF*`
variants, none from `sophie.taf` (see the CLOSED 2026-08-25 sophie section).
The `.taf` is recoverable from the `measure.sh <taf> <cmdfile>` invocation that
produced the run; check it before quoting a transcript at a golden.

`~/adrift-battle/runner/wine/transcript_provenance.py` does that recovery for
every transcript at once: `measure.sh` prints `transcript=<file>` for the run it
just made, so the session logs hold the true pairing, and the script binds each
`measure.sh <taf> <cmdfile>` to the `transcript=` line that followed it and
matches the runs to the files on disk by mtime.  Run 2026-08-25, 60 files:

- **44 verified**, including every transcript a landed port rests on --
  `Adrift_22_xfiles.txt` and `Adrift_31_xfiles.txt` <- `xfiles.taf` (the object
  `seen` model), `Adrift_36_orient_express.txt` <- `orient.taf` (the battle
  alias rule), `Adrift_37_melbourne_beach.txt` <- `melb.taf` and
  `Adrift_38_stardust.txt` <- `stardust.taf` (the walk join),
  `Adrift_27_thepkgirl.txt` <- `pkgirl.taf`, `Adrift_47_p4walkalr.txt` <-
  `p4WALKALR.taf`, `Adven_9_timmy_reid.rtf` <- `tra.taf` (which really is *The
  Timmy Reid Adventure*, and is the `.taf` on that corpus row).
- **1 corrected**: `Adrift_41..46_sophie.txt` <- `saF570/saF575/saFXYZ/saF576/
  saF577/sanoalr.taf`, i.e. doctored copies of `sa.taf`.  See the CLOSED
  2026-08-25 sophie section.
- **16 unverified** -- no `measure.sh` record survives, because they predate it
  or were driven by `drive_ckpt.sh`/`runcmds.sh` by hand: `Adrift_1`,
  `Adrift_9_pET3`, `Adrift_10_pET4`, `Adrift_14_relojero`,
  `Adrift_15_relojero`, `Adrift_16_hauntedhouse`, `Adrift_17_hauntedhouse`,
  `Adrift_18_funhouse`, `Adrift_19_the_cat_in_the_tree`,
  `Adrift_20_maincourse`, `Adrift_28_humbug`, `Adrift_30`, `Adven_1_marooned`,
  `Adven_3_castle_quest`, `Adven_4_superliam`, `Adven_5_arlo`.

`transcript_identify.py`, next to it, is the independent second opinion for
those: it scores a transcript's first 6 KB against the opening of every
`goldens/*.expected.txt` by shared vocabulary.  Run over all sixteen, **every
slug holds** -- each names its own golden at 0.62-1.00 with the runners-up at
0.12-0.37, and the only ones it cannot speak for are the synthetic probes
(`Adrift_9_pET3`, `Adrift_10_pET4`), which are named after their `.taf` to
begin with.  Two useful specifics fall out: `Adrift_16/17_hauntedhouse` really
are *Haunted House* (`hauntedhouse.taf`, **not** `haunted.taf` -- see the
pairing warning above; only their **cmdfile** pairing was wrong), and
`Adven_5_arlo.rtf` is `alices_restaurant_solution`, which is the `arlo.taf`
row.  What the score cannot tell you is which RELEASE was loaded -- that is
precisely what the sophie mispairing turned on -- so it confirms a slug, never
a `.taf`.

Citations in the tree that still resolve were updated to the new names. These
ones were **not** -- their files have since been overwritten by a later run and
the measurement they describe is no longer reproducible from disk:

    sclibrar.cpp:5646, :6196          Adrift_8   (ALEXIS/iachini, 2026-08-22)
    scrunner.cpp:1251, :1279          Adrift_14/15 (now relojero)
    scrunner.cpp:1271, :1272          Adrift_18/19 (now funhouse / cat-in-the-tree)
    scrunner.cpp:1761, :2304          Adrift_18
    scrunner.cpp:2488, :2489          Adrift_20/18 (now maincourse)
    RUNNER_TESTS_TODO.md:778-780      Adven_2 cited as the run380 haunt.taf run
    harness/make_39_doneprobe.py:4    Adrift_14
    goldens/life_solution.txt:55      Adrift_22 (string is in neither Adrift_22 nor Adven_8)

Re-measure before relying on any of them.

### The back-catalogue sweep, 2026-08-25

Once the provenance table existed, every verified (`.taf`, cmdfile,
transcript) triple that had **never been through
`compare_wine_transcript.py`** was swept offline.  Eight of them, and the
sweep is finished -- do not redo it:

| transcript | rule 2 | verdict |
|---|---|---|
| `Adrift_34_unraveling_god` | all echoed | **clean pass, identical on every turn** -- already in the corpus row |
| `Adrift_31_xfiles` | all echoed | 19 of 20 turns byte-identical after the constant one-turn `Nope!` shift; the 20th is `burn memo`, since FIXED (the 4.0 `%object%` case rule) |
| `Adrift_30_humbug` | all echoed | ten differing turns, **all rule 3** -- see the humbug "Still open" list |
| `Adrift_35_cybercow` | 1 lost | no divergence before the loss |
| `Adrift_32/33_unraveling_god` | 2 lost / 1 lost | earlier attempts at the run `Adrift_34` completed; superseded |
| `Adrift_29_humbug` | 2 lost | superseded by the two-phase splice (`Adrift_30`) |
| `Adrift_27_thepkgirl` | known feed-broken | already recorded as such |

Two of the eight carried something new; the other six carried nothing.  That
ratio is the argument for sweeping a transcript the moment it is captured,
rather than only when its own lead is being chased -- both finds here sat on
disk unread for two days.

### FIXED 2026-08-25 -- the inverse census, and an empty room description

Run the census backwards -- every Runner string that Scarier never prints --
and most of what comes back is dialogue boxes, registry errors and map-zoom
chatter.  Strip those and two groups remain.  The larger one is the
disambiguation family (` would you like to take.  `, `That is still
ambiguous!`, `It is not clear which object you are referring to.`, `Who do you
want to attack?`, `Where do you want to put that?`), which is already the
biggest known gap and has its own TODO entry.  The other is one sentence:

> There is nothing of interest here.

**What the Runners do.**  It is what a room with no description of its own
says.  run380 does it at LOAD: `447FEE` reads the room's Long line and, if it
is empty, substitutes the sentence, so the room simply *has* that description
from then on.  run390 does it at PRINT: `4478CA` appends it when the LastDesc
is empty and the Long is empty and nothing else has described the room (the
branch two lines up at `4478A8` takes the LastDesc instead and skips it).
run370 does not have the string at all and leaves such a room blank.  run400
carries the constant in its pool but no call site references it anywhere in
the decompile.

**What Scarier does.**  Nothing -- `lib_print_room_description()` prints the
Long only `if (!scr_strempty (description))` and has no fallback, for any
version.

**Exposure, measured against the corpus.**  Patching it in locally as "if
nothing described the room and the version is 3.80 or 3.90, print the
sentence" moves exactly two goldens, `yeh` and `richard`, both 3.90, and
nothing else in the 303 rows.  (The naive placement -- an `else` on the Long
itself -- moves sixteen, because it fires ahead of every alt and LastDesc.
That difference is the whole content of the run390 branch, and it is a good
reminder to read the guard, not just the literal.)

**Measured 2026-08-25**, `p39EXAM.taf`'s third room -- an empty Long, no alts,
no objects.  run390, `Adrift_41_p39exam.txt` and `Adrift_43_p39exam.txt`, all
19 + 29 commands echoed:

    e
    You move east.
    Void Room
    There is nothing of interest here.  You can only move west.

and `look` repeats it.  The sentence is joined to the exits with the ordinary
two-space clause gap -- the Runner appends it to the message with no `pspace()`
of its own, so the exits sentence supplies the separator (contrast `4478AB`,
where the LastDesc branch *does* call `pspace()` first).  The 4.00 twin
`p4EXAM.taf`, `Adrift_1_p4exam.txt`, prints the exits alone, confirming 4.0
dropped it.

**Ported** in `lib_print_room_description()`, gated `version == TAF_VERSION_380
|| version == TAF_VERSION_390` and on `!is_described` -- exactly the guard
above, not the Long alone.  `yeh` and `richard` moved and were re-blessed; the
corpus is back to 303 PASS.

### The four-exe string census, 2026-08-25

An offline substitute for a Wine run, and the thing that found the `x <unknown
noun>` split.  Every wording Scarier prints is a literal in `sclibrar.cpp`;
every wording a Runner prints is a literal in its `.exe`.  Dumping all four
constant pools and asking which of Scarier's phrases no Runner carries turns a
1,300-line source file into a short, ranked list of candidate version splits --
no game, no transcript, no desktop needed.

**Dumping a pool.**  VB6 stores each string as a **uint32 little-endian byte
length** immediately followed by the UTF-16LE bytes.  Regex-scan the image for
runs of `(?:[\x20-\x7e]\x00){3,}` and keep a run only when the four bytes
before it equal its length; that validation is what makes the dump clean (an
unvalidated scan mis-anchors and truncates -- it was returning `see no such
thin` before the prefix check went in).  run370 753 strings, run380 864,
run390 1119, run400 1309.

**Matching.**  Compare *fragments*, not sentences: the Runners build a line out
of pieces (`<player>` + `" see no such thing."`), and the older ones build more
of it than the newer ones.  For each Scarier phrase, find the longest
contiguous word-run that appears verbatim in some pool, and record which exes
carry it.  A run of three or more words missing from at least one exe is a
candidate.

**Three false-positive modes, all real:**

* *Composition.*  `"You can't read "` is in run370.exe alone, but run380 says
  the same thing as `MemVar & " can't read " & obj` (`43CEFA`) -- the phrase
  exists, just not as one string.  Anything whose missing half is a pronoun or
  a name is suspect.
* *Already gated.*  `"There is nothing worth taking here."` is run400-only and
  Scarier prints it -- correctly, from inside the `lib_is_version_400()` arm
  that the take-wording split already added.  Skip any phrase whose enclosing
  function already mentions `lib_is_version_400`.
* *Prose, not output.*  The exposure half of the census greps the goldens, and
  a golden is mostly the **game's** text.  ` a part of ` matched eleven lines
  across seven goldens and every one of them was authored prose ("I am a part
  of your mind", "a part of the coast") -- the Runner message it belongs to is
  reached by none of them.  Only count a hit that stands alone on its line as
  a whole Runner sentence.
* *Prefix.*  Matching a phrase against the *start* of a golden line catches
  longer sentences that merely begin the same way -- `sommeril` L706 "You
  can't read it from here." is not Scarier's `"You can't read "`, it is the
  game's own text.  Check that what follows the phrase is the object name.

After both filters, 74 phrases remain, and cross-referencing them against the
goldens (golden -> `.taf` signature -> Runner generation) leaves the handful
that a pre-4.0 game actually reaches today:

| sclibrar | phrase | exes carrying it | exposed golden | verdict |
|---|---|---|---|---|
| `lib_cmd_examine_object` | `You see nothing special about ` | 400 | `ms_mobius` (3.90) | **live lead**, written up above |
| `lib_cmd_read_other` | `You see no such thing.` | 400 | `cybercow_win`, `panic` (3.90) | **closed 2026-08-29** -- run400 prints it third-person (`<name> see no such thing.`, 471F02/4801E1); ported with the `is_admin` flag |
| `lib_cmd_locate_object` | ` a part of you!` | 400 | -- | prose false positive |
| `lib_cmd_buy_other` | `I don't think that is for sale.` | 370, 380 | `circus` (3.90) | composition, not a split |
| `lib_put_in_is_valid` | `You can't put anything inside ` | 370, 380, 390 | `sophie_comp` (4.00) | composition, not a split |

Three of the five are closed already, and closing them is most of what the
census is for -- each is a wording someone would otherwise have "fixed":

* **buy.**  run370 `43E515` and run380 `445037` write `"I don't think "` then
  branch: `If var_88 = "that"` append `"that is for sale."`, else append
  `var_88 & " is for sale."`.  run390 (`45E68F`) and run400 (`489A09`) dropped
  the branch and keep only the else arm -- which, when the noun *is* `that`,
  produces the identical sentence.  The constant vanished; the output did not.
* **put in.**  run400 `46DE47` pushes `" can't put anything "` and then
  concatenates `var_98` (`inside` / `on`), a space, the object and `"!"`.
  Same sentence as run370/380/390's one-piece literal.  `sophie_comp` is safe.
* **locate.**  The four "exposed" goldens are the prose false positive above.
  ` is a part of ` really is run400-only -- run370's locate routine
  (`42F9FF`) branches on the object's position with no part-of arm at all --
  but no golden reaches it, so there is nothing to measure and nothing at
  risk.  Left ungated deliberately.

**The rest of the list is swept.**  Re-running the exposure pass over every
candidate -- substituting `You`/`I`/`%player%` into each phrase, requiring a
whole-line or object-name-followed match, and skipping any golden whose `.taf`
signature names an exe that *does* carry the phrase -- leaves seven hits, and
five of them are the closures above plus two more of the same kind:

* `lib_cmd_read_object`'s `"You can't read "` is run370-only as one literal;
  380/390/400 build it from `" can't read "` (run380 `43CEFA`).  The `sommeril`
  hit is the prefix false positive.
* `"You are not holding "` is 370/380-only as one literal; run390 `45D5C0` and
  run400 `463B1D` push `" not holding "` and prepend the player and `is`/`are`
  separately.  `colony` (3.90) and `adriftorama` (4.00) are safe.

So the whole 74-row list reduces to the two live leads, and both are answered
by the one probe.  The list is still worth keeping: it is the map of where 4.0
reworded the library, and the `x` row proves the map is worth reading.

**The pre-4.0 exposure, exhaustively.**  Re-grepping every golden for the
whole-line Runner sentences in this family, then resolving each golden to its
`.taf` signature, the complete list of pre-4.0 games that reach any of them is
three: `ms_mobius` (3.90) for `You see nothing special about the hole.`, and
`cybercow_win` + `panic` (3.90) for `You see no such thing.` -- both of those
from a `read`, not an `x`.  Every other sighting (`cellar`, `trabula`,
`yak_shaving`, `goldilocks`, `man_overboard`, `princess_in_the_tower`,
`professor`, `sandy`, `topaz`, `valley`) is a 4.00 game printing the 4.00
wording correctly.  So the whole open family is worth exactly three golden
lines -- which is the point: it is cheap to be wrong here for a long time.

## Before measuring anything

- **Turn Verbose ON** (Options → Verbose, Ctrl+V). It resets to OFF on every
  launch and never persists. With it OFF, re-entering a visited room prints
  only `RoomName.` and NPC walker lines are *absent entirely*. Scarier models
  the Verbose-ON Runner, and author transcripts are Verbose-ON sessions.
  Measured in run400; assumed but **not yet verified** for run390/380/370 —
  check the Options menu on the first game of each version.
- **Check, do not assume, the Appearance checkboxes.** An earlier note here
  said "all five default OFF and never persist". Both halves are wrong, and
  the correction is sourced twice over (2026-08-24):
  * run400's options loader (`Proc_21_24_4747F8`, `run400.bas:89290`) reads
    each one through `Proc_21_25_44AC08(key, default)` -- args are pushed in
    reverse, so the byte pushed *before* the key string is the default. The
    defaults are `Myfont` 0, `Sound` 1, `Graphics` 1, **`showbrackets` 1**,
    `showgt` 0, **`showshortroom` 1**, `autopause` 1. So "Room names in
    descriptions" and "References in brackets" default **ON**, not off.
  * they *do* persist: this prefix's `pfx/user.reg` carries
    `[Software\\VB and VBA Program Settings\\ADRIFT\\Runner]` with
    `"showshortroom"="1"`, `"Graphics"="0"`, `"Sound"="1"`, `"Verbose"="False"`.
    (`run390`'s `m_showshortroom_Click` is a plain `SaveSetting`.) The old
    "nothing records it" reading was taken before anything had ever toggled
    the box, when the key simply did not exist yet.
  What this means in practice: room headings are **on** in this prefix, which
  is why `measure.sh` -- which only sends Ctrl+V for Verbose and never touches
  Appearance -- still matches Scarier's headings (FunHouse, 0/18 commands
  differ). Verbose is the only box that really does reset every launch.
  Read the key out of `user.reg` before a measurement rather than trusting
  either claim.
- **Policy since 2026-08-29: measure with all three Appearance boxes ON and
  Verbose ON, and Scarier models that Runner.**  "References in brackets"
  (`showbrackets`), "Prompt for typed commands" (`showgt`) and "Room names in
  descriptions" (`showshortroom`) are forced to `1` in `pfx/user.reg` by
  `measure.sh` before every launch, and Verbose is forced `True` the same way
  (plus the Ctrl+V belt-and-braces).  The consequence for the engine is that
  every line the 4.0 Runner gates on `showbrackets` is now printed: the
  `(Getting off X first)` / `(Standing up first)` pair, the pronoun echo
  `(a trophy)`, the `again` echo, and -- still to port -- the `ask about` /
  `talk about` and `give X` rewrites.  See FIXED 2026-08-29 below; the CLOSED
  2026-08-24 section further down describes the brackets-OFF world it
  replaced.
- **Look for randomised puzzle state before splicing a command file.**  The
  Runner rolls its own numbers, so any walkthrough that types a combination,
  a code or a count back at the game will break in the Runner even when the
  engines agree perfectly.  `humbug` is the worked example: it randomises a
  four-digit lock at game start and shows it on a slate as one roman numeral
  (`lock1` thousands ... `lock4` units).  Scarier at `SCR_SEED=1` rolls 3446,
  which is why the walkthrough says `Turn dial to 3/4/4/6`; run400 rolled 4937
  on the launch that mattered.  Fed the golden's digits, the Runner's case
  simply never opens and every later command runs against a different world.
  Grep the `.taf` for `%var%` inside object descriptions if you are unsure --
  humbug's slate reads
  `The numerals read [lock1=%lock1%][lock2=%lock2%][lock3=%lock3%][lock4=%lock4%].`
- **Fresh process per measurement.** Adventure → Restart game does not
  reliably reset NPC walk state.
- Feed with `drive_ckpt.sh`, which echo-verifies each line. Wine mangles
  input; read the echo, never assume the line landed.
- Rows marked **waitkey** below contain a "press any key" pause that swallows
  one fed keystroke. They are usable but need the feeder to account for it —
  prefer a non-waitkey game first.
- Real-time pauses (the Professor pie) only manifest live. They are not a
  divergence.
- Kill Wine afterwards, properly:
  `pkill -9 -f wine; pkill -f wineserver; pkill -9 -f 'C:\\'; pkill -9 -f 'start\.exe /exec'`
  then verify with `ps aux | grep -iE 'wine|\.exe'`. `pkill -f wine` alone
  matches nothing — Wine's Windows processes carry Windows command lines.

## Measured so far

Everything below was settled between 2026-08-02 and 2026-08-24.  The
walkthroughs themselves were never touched; where the Runner disagreed, the
engine changed and the golden was re-blessed, with the evidence written into
the row's comment block in `harness/run_v4_walkthroughs.sh`.

| game | version | how it was settled | outcome |
|---|---|---|---|
| `Professor.taf` | 4.00 | full run400 replay | the worked example; walk phase, arrival lines, presence lines |
| `FunHouse.taf` | 4.00 | full run400 replay, 0/18 commands differ | an **empty game-start walk preempts for ever**: NPC 3 WALK 1 and NPC 5 WALK 1 stay shut all game |
| `TheCatintheTree.taf` | 4.00 | full run400 replay | corroborates the same rule -- the boy (NPC 2 WALK 1) never arrives |
| `humbug.taf` | 4.00 | run400 P-code, room lister `Proc_19_63_472CA4`; a two-phase replay to command 373; then (2026-08-29) a **full checkpointed replay** (`#save`/`#restore` in the cmdfile, `Adrift_4_humbug.txt`, secrets 7628 / Pakiwagag / 7076) plus 76 pronoun probes (`Adrift_5_humbug.txt`) | ChangedDesc pick is task-state only, ascending, non-empty wins; the partial replay added the `On X is`, `and carrying` and pronoun-echo findings below.  **Not fully replayable** -- three randomised secrets, see "Still open" |
| `lair-of-the-cybercow.taf` | 3.90 | run390 P-code, viewroom `loc_447D1D` | same lister rule one Runner down; one line changes |
| `great.taf` | 3.80 | run380 P-code, `characters() '441928` | no expiry stamp at all, restart needs `Loop = 1`, preempt has no StoppingTask test |
| `maincourse`, `orient`, `xfiles`, `wamk` | 4.00 | re-blessed under the same two rules | `maincourse` lost its win marker to a faithful preemption |
| `iqsfot.taf` | 4.00 | see the row's comment block | NPC 16 WALK 2 is an empty game-start walk with no stops; it pins the patrol shut and the game cannot be won in run400 |
| `the_pk_girl.taf` | 4.00 | full run400 replay with a 96-command peddler hunt spliced in | the Runner WINS -- and that is what proved a finished 4.0 walk is stamped **-1**, not 255 |
| arena probes EV14/EV15/EV16 | 4.00 | run400, `harness/make_arena_probe.py` (Adrift_1_ev14..16.txt) | **`x <npc>` and a nothing-found examine are administrative turns** -- no turn count, no walk, no event tick; `x me`, `x <object>`, `look`, `i` are normal; "Time passes..." carries its own vbCrLf; "1 turns so far" never singularised |
| `BobBobsly.taf` | 3.90 | run390 (Adrift_1_bob390.txt) | 3.9 counts NPC examine, failed examine and `turns` as turns; `z` = 1 turn under WaitTurns 3 -- see Open leads |
| `CAH.taf` (cruel) | 3.90 | run390 probe (Adrift_1_cruelprobe.txt) | `take it` -> "You can't take the jacket." |
| `man overboard.taf` | 4.00 | full run400 replay, 99/99 identical but the tail | settles the `again` echo, the give/ask rewrites and "(a Cupboard)" |
| `princess1.taf`, `Tear.taf`, `lobster.taf`, `PTGOOD.taf` | 4.00 | full run400 replays | 78/78, 36/36, 54/54, 6/6 (+7/7 ptgood_again) identical |
| `Beanstalk.taf` | 4.00 | full run400 replay, 49/49 | the turn-45 stranger greeting is one command later because `x stranger` is administrative |
| `CIBASS.taf` | 4.00 | partial run400 replay | identical to turn 16, then waitkey prompts desync the script |
| `arlo.taf` | 3.70 | full run370 replay, `Adven_6_arlo.rtf` | the 3.7 walk departure lines, incl. "walks off to not moved."; 3 differing of 85 |
| `tra.taf` | 3.80 | full run380 replay, `Adven_9_timmy_reid.rtf` | "outside" takes no "to" in a departure line |
| `Melbourne Beach.taf` | 3.90 | full run390 replay, `Adrift_37_melbourne_beach.txt` | the 3.9 walk directions, incl. the diagonal a pre-4.0 8-exit scan cannot name |
| `Orient_Express.taf` | 4.00 | full run400 replay, `Adrift_36_orient_express.txt` | the 4.0 walk directions; also the spurious "Gimme Atip enters." arrival |
| `S_Tar_Dus.taf` | 3.90 | full run390 replay, `Adrift_38_stardust.txt` | all 129 walk lines match count for count; pinned the not-a-room-zero arrival gate |
| `asteroid_after.taf` | 4.00 | live run400 probes (six co-present valves) + the corpus' ALR tables + UTF-16 literals in all four exes | the 4.00 object-ambiguity rule, its wording, its follow-up prompt, and that NPCs share the object message -- see the MEASURED section below |
| `p4ALR` / `p4ALRSRC` / `p4WALKCOUNT` / `p4VARFREEZE` (built probes) | 4.00 + 3.90 | run400 and run390 replays of four packed probe games | the whole **4.0 output filter**: walk = repeat a length-descending pass until nothing changes, self-containing ALRs retired per walk, one walk per completing task plus the flush, variables frozen by each walk -- see the FIXED section below |
| `3monkeys.taf` | 4.00 | live run400 replay of the solution's first 36 commands, `Adrift_16.txt` | the Runner really does print the raw `CHIMPSIGNAL=0`; the variable freeze is not a port artefact |
| `Oh_Human.taf` | 4.00 | full run400 replay, `Adrift_1_ohhuman.txt` (feed `cmdfile_ohhuman.txt` -- no `_w_`, which is why the 2026-08-30 re-sweep missed it) | 9/9 identical on every turn; compared 2026-08-30 |
| `wingman1.taf` | 3.90 | full run390 replay, `Adrift_3_wingman1.txt` (POPUP_ANSWERS name dialog, feed `cmdfile_wingman1.txt`) | 32/32 identical but the tail -- once the 3.9 `(Getting off ...)` correction below landed |
| `gamma.taf` | 3.90 | full run390 replay, `Adrift_3_gamma.txt` (POPUP_ANSWERS name dialog, feed `cmdfile_gamma.txt` -- 185 commands, the golden's `#` comment lines stripped) | 185/185 identical but the tail, all 4 walks and 10 NPCs in step -- once the pre-4.0 openness-line fix below landed |
| `tcom.taf` | 3.90 | full run390 replay, `Adrift_3_tcom.txt` (feed `cmdfile_tcom.txt`) | 13/13 identical but the tail; the three walk scenes line up |
| `windy2.taf` | 3.90 | full run390 replay, `Adrift_3_windy2.txt` (POPUP_ANSWERS name dialog, feed `cmdfile_windy2.txt` -- 147 commands) | 147/147 identical but the tail; 8 NPCs, both walks and the fixed skinny-dip event all in step |
| `Richard.taf` | 3.90 | full run390 replay, `Adrift_3_richard.txt` (feed `cmdfile_richard.txt` -- 70 commands) | 70/70 identical but the tail -- once the 3.9 WinText pspace join below landed; 1000/1000 |
| `cleft.taf` | 3.90 | full run390 replays, `Adrift_3_cleft.txt` + `Adrift_3_cleft2.txt` (feed `cmdfile_cleft.txt`) | first drive 90/90 echoed with 3 divergent turns -- the 3.9 event-move seen-byte split below; re-drive with the `look` added 91/91 identical but the tail, Runner wins 100/100 |
| `sa.taf` (`sophie`) | 4.00 | live run400 replay, `Adrift_41_sophie.txt`..`Adrift_45_sophie.txt` (five runs of the solution's first fifty commands), plus the game's own 488-entry ALR table | the walk announcement is **joined into the turn's paragraph**, so 12 of sa.taf's 65 join-spanning ALRs fire and delete the arrivals they match -- see the FIXED section below |
| `p4WALKALR` (built probe) | 4.00 | run400 replay, `Adrift_47_p4walkalr.txt` | the join itself, in isolation: an ALR whose Original starts with the two-space separator matches |
| `The_X-Files_A_New_Beginning.taf` (`xfiles`) | 4.00 | live run400 replay of the solution's first 40-odd commands, `Adrift_22_xfiles.txt` | a **"The" prefix is never lower-cased**, and **what is *on* an object is listed before what is *in* it, in one sentence** -- see the two FIXED sections below.  Also closed the `knock` lead (a feed artefact) and pinned `burn memo` on the 4.0 `%object%` case rule (FIXED) |
| `p4BURN` (built probe) + an `xfiles` bisect | 4.00 | four run400 probe replays (`Adrift_6_p4burn.txt` thirteen restriction cells, `Adrift_2_p4burn.txt` Repeatable, `Adrift_12/13_p4burn.txt` case) and nine replays of edited `xfiles` builds (`Adrift_1/3/4/5/7/8/9/10/11_xfilesbisect.txt`) | **4.0 substitutes an object's Short or Alias into a `%object%` task command verbatim** and compares it to the lower-cased input, so a capitalised Short can never bind and no article, Prefix or partial name binds either.  `%character%` lowers the name first and is unaffected.  See the FIXED section below |
| `p4STATE` (built probe) | 4.00 | run400 replay, `Adrift_1_p4state.txt` (29 commands) | **only `%state_<obj>%` lower-cases an object's state name, and it folds the whole string**; the examine lister and `%obstate%` print it verbatim.  One golden, three lines |
| `p39CASE` (built probe) | 3.90 | run390 replay, `Adrift_1_p39case.txt` (19 commands) | the 3.90 half of the same rule: **strict binding starts at 3.90, the case fold is only lost at 4.0**.  Moved five rows in Scarier and no goldens |
| `p4WALKCAP` (built probe) | 4.00 | run400 replay, `Adrift_1_p4walkcap.txt` (4 commands) | **4.0 capitalises a walk announcement's Name wherever the sentence lands** -- joined mid-paragraph and opening a line both print `Bob` for an NPC named `bob`.  Confirmed the ported reading; no change |
| `p4PALR` (built probe) | 4.00 | run400 replay, `Adrift_1_p4palr.txt` (8 commands) | **punctuation in an ALR changes nothing**: all seven cells fire, leading `, `/` `/`: ` Originals and pure-punctuation Replacements alike.  Confirmed `sophie.taf`'s `[, and] -> [:]`; no change |
| `p39EXAM` / `p4EXAM` (built probes) | 3.90 + 4.00 | run390 replays `Adrift_41/43_p39exam.txt` (29 + 19 commands) and the run400 twin `Adrift_1_p4exam.txt` (32) | the whole **examine / read / open / close refusal family**, plus the empty room description: four splits found and ported, and 3.90 now agrees with Scarier on all 48 rows.  See the FIXED sections below |
| `hauntedhouse.taf` | 4.00 | run400 replay of the game's **own** 42-command solution, `Adrift_1_hauntedhouse.txt`, Verbose ON, all 42 echoed | **clean: 41 of 42 turns identical, and the 42nd differs only by the Runner's `[Press any key to end]` tail**, which Scarier emits as a waitkey pause rather than as text.  Supersedes the mispaired `Adrift_16/17` run except for the two engine bugs that one found |
| `goldilocks.taf` | 4.00 | run400 replay of the full 252-command solution, `Adrift_1_goldilocks.txt`, Verbose ON, all 252 echoed | one real divergence in 252 turns, and it was an engine bug: **an event's look text is gated on the room being described, not on the room the player is standing in** -- see the FIXED section below.  After the fix, 251 of 252 identical, the 252nd only the `[Press any key to end]` tail |
| `lair-of-the-cybercow.taf` | 3.90 | run390 replay, `Adrift_1_cybercow.txt` (127 commands, 3 lost) | the *other* direction of the same rule: the Runner **does** print the day/night event's look text in the Chapel Yard a ShowRoomDesc task shows, while the player is still at the bottom of the well.  Not a clean row -- see its comment block for the three feed problems it exposed |
| `Monsters_r2.taf` | 4.00 | full run400 replay, `Adrift_1_monsters.txt`, Verbose + brackets ON, 38/38 echoed | **brackets ON prints `(Getting off Sissy's four poster bed first)` on its own line** (turns 5, 23); after the port 37/38 identical, the 38th is the `[Press any key to end]` tail |
| `ADRIFTMaze.taf` | 4.00 | run400 replay, `Adrift_1_adrift_maze.txt`, name via `POPUP_ANSWERS`, 26/26 echoed | **the 4.0 pronoun echo `(a trophy)`** on turns 24-25; otherwise identical bar the echoed name |
| `BlackSheepsGold.taf` | 4.00 | full run400 replay, `Adrift_1_black_sheeps_gold.txt`, 99/99 echoed | clean: 98/99 identical, the 99th cut off at the Runner's last `(press any key to continue)`.  Needs `--offset 0` |
| `Space Boy's First Adventure.taf` | 4.00 | full run400 replay, 133/133 echoed | clean: 132/133, the tail only |
| `angeldevilhuman`, `cyber`, `demonhunter`, `plunder_gargoyle`, `renegade_brainwave`, `ptgood`, `srsintro`, `imagination` | 4.00 | full run400 replays, every command echoed | clean: all turns identical except the `[Press any key to end]` tail (and the echoed name for `imagination`) |
| `cyber2.taf` | 4.00 | full run400 replay, 29/29 echoed | 26/29; turns 15 and 26 differ by one **battle roll** line each (rule 3), 28 is the tail |
| `dragonshrine`, `through_time`, `invasion_shirts`, `qui_a_tue_dana`, `whitterscap`, `hyper_b_s`, `cibass`, `allhallowseve` | 4.00 | partial run400 replays -- a cutscene, real-time pause or waitkey eats a fed command part-way (rule 2) | identical up to the loss (105, 12, 13, 15, 13, 3, 3 and 3 turns respectively); nothing after it is comparable |
| `Vardock Bates.taf` | 4.00 | full run400 replay (Adrift_1_vardock_bates.txt), then a checkpointed probe (Adrift_3_vardock_bates.txt) | `<waitkey 4>` is a zero-second wait; co() feeds the generic verbs; a finishing event's task re-checks LOWER-indexed events in the same tick; the SYNONYM table is a sequence of whole-string rewrites -- see the two FIXED 2026-08-29 sections |
| `ECOD3.taf` | 3.90 | full run390 replay, `Adrift_3_ecod3.txt` (feed `cmdfile_w_ecod3.txt` -- 11 commands, the solution's 15 `#` comment lines stripped) | clean: 11/11 echoed, 10/11 turns identical and the Usher's walk in step; the 11th is the tail -- the transcript stops mid-epilogue at the final pause, so the alley arrival and score summary never flush.  Measured 2026-08-31 |
| `largo-winch.taf` | 3.90 | full run390 replay, `Adrift_3_largo_winch.txt` (feed `cmdfile_w_largo_winch.txt` -- all 323 commands, `#save lw100`/`#save lw200` checkpoints, PRE=0) | clean: 323/323 echoed, 322/323 turns identical, all 42 NPCs and 22 events in step; the tail is the Runner's `[Press any key a end]` only.  Second drive: the first (`Adrift_3_largo_winch_drive1.txt`) desynced at command 204 because `composer "9002472832"` broke the AppleScript keystroke -- a driver bug, fixed |
| `mudergreatfalls.taf` | 3.90 | full run390 replay, `Adrift_3_murder_great_falls.txt` (feed `cmdfile_w_mgf.txt` -- 103 commands, the golden's `Sam`/`male` answers stripped and given as `POPUP_ANSWERS="Sam|male"`, PRE=2; compare `--start 2`) | clean: 101/101 echoed, every turn identical; the tail is the winning `accuse ken` cut at the Runner's endgame pause |
| `report.taf` | 3.90 | full run390 replay, `Adrift_3_report.txt` (feed `cmdfile_w_report2.txt` -- 165 commands, `Sam`/`male` via `POPUP_ANSWERS`, PRE=0; compare `--start 2`) | clean: 165/165 echoed, every turn identical but the `[Press any key to end]` tail; 100/100 |
| `Archie's Birthday V 1-2.taf` | 3.90 | full run390 replay, `Adrift_3_archie.txt` (feed `cmdfile_w_archie.txt` -- 205 commands, PRE=0; .taf copied as `ArchiesBirthday.taf`) | 205/205 echoed; two engine divergences, both fixed: run390 echoes `(a camcorder)` on `take it` (Scarier's 3.9 gate was wrong) and appends `.` to a PlayerDesc that lacks one; clean after the fix but the `[Press any key to end]` tail; 50/50 |
| `veteran.taf` | 3.90 | 5-command run390 probe, `Adrift_3_veteran_probe.txt` | `take it` then `open it` both echo `(a bag)`: 3.9 keeps the authored article after a take, unlike 4.0's `(the bag)` |
| `yak_shaving.taf` | 4.00 | 1-command run400 probe, `Adrift_4_yak_probe.txt` | `x me` answers `...after your journey.` -- 4.0 appends the full stop too |
| `croft.taf` | 3.90 | full run390 replay, `Adrift_5_croft.txt` (feed `cmdfile_w_croft.txt` -- 101 commands, PRE=0; the table's 193 counted the golden's comment lines) | 101/101 echoed; zero engine divergences -- the only diff is the Runner's `[Press any key to end]` after the final score summary; 150/150 |
| `DarkTower.taf` | 3.90 | full run390 replay, `Adrift_6_darktower.txt` (feed `cmdfile_w_darktower.txt` -- 121 commands, PRE=0) | 121/121 echoed; zero engine divergences -- only the Runner's `[Press any key to end]` after the 0/0 score summary; "restored power to the building." |
| `FarFromHome.taf` | 3.90 | full run390 replay, `Adrift_8.txt` (feed `cmdfile_w_ffh_nock.txt` -- 71 commands, `POPUP_ANSWERS="Sam"`, PRE=0); the earlier checkpointed drive `Adrift_7.txt` is superseded | 71/71 echoed; zero engine divergences -- the only diff is the Runner transcript stopping at the `<waitkey>` inside the ending text.  The checkpointed drive's six "divergences" (puff at p30, pirate at turn 39, four tide lines) were two extra event ticks, one at each `#save`; 50/50 |
| `EnqueteAHautsRisques.taf` | 3.90 | full run390 replay, `Adrift_9_enquete.txt` (feed `cmdfile_w_enquete.txt` -- 145 commands, PRE=0, no popups, no waitkeys) | 145/145 echoed; zero engine divergences -- 144 of 145 turns byte-identical (French, CP1252) and the 145th, the winning `se coucher`, differs only by the Runner's `[Press any key a end]`.  All seven events are fixed-length and the game has no walks, so nothing on the path can roll; 59/59 |
| `Captive.taf` | 3.90 | full run390 replay, `Adrift_9_captive.txt` (feed `cmdfile_w_captive.txt` -- 57 commands, PRE=0, no popups, no waitkeys) | 57/57 echoed; zero engine divergences -- 56 of 57 turns byte-identical and the 57th, the winning `put diamond on pedestal`, differs only by the Runner's `[Press any key to end]`.  The one rollable thing on the route, EVENT 12 [Serpent] (`time1=3 time2=5`, started by `tie rope to ledge`), is confined to rooms 25-27 and the next command climbs out of them, so neither side prints a Serpent line; 100/100 |
| `superliam.taf` | 3.80 | full run380 replay, `Adven_1_superliam.rtf` (feed `cmdfile_w_superliam.txt` -- 86 commands, Save Transcript at the 85th) | 85/85 echoed; three divergent turns, two engine rules, both FIXED: the run380 **AdditionalMessage double-space suppression reaches through a ShowRoomDesc room description**, and **names are matched RAW** -- object Short `"necko wafers "` (trailing space) is unreferenceable, `take necko wafers` answers "Take what?".  After the fixes every echoed turn matches; both sides win 3250/3250.  Measured 2026-08-31 |
| `cave.taf` | 3.80 | three full run380 replays, `Adven_1_cave.rtf` / `Adven_1_cave2.rtf` / `Adven_1_cave3.rtf` (final feed `cmdfile_w_cave3.txt` -- 216 commands, Save Transcript flow) | 215/215 echoed each time; FOUR engine findings, all FIXED: `z` is not 3.80 vocabulary (whole-line `= "z"` test only exists from run390_3 45FCB0 -- seven `z` -> `wait`); 3.8 substitutes "There is nothing of interest here." into empty room Longs at LOAD (447FEE), so it prints BEFORE LastDesc alts; a 3.8 task can never move the PLAYER to the game's FIRST room (tasks() 44D1D4 stores Var2 pre-decremented behind "If Var2 > 1" -- second `climb down` re-derived to `down`; 3.7's encoding sits one higher so run370 441E55 is correct, arlo the counterexample; is_v370 gate in sctafpar.cpp); and the single-named held-take refusal is "You've already got X!" pre-4.0 (43E03E -- 4.0's "already carrying" @462D25 gated on lib_is_version_400, four 3.9 goldens re-blessed -- thewoods' own ALR "I've already got the" -> "<br>I'm already carrying the" confirms the base from the game side).  Third drive replays CLEAN: every comparable turn identical, `score` at 900/1000 both sides; only the winning `read parchment` is uncapturable in the Save-at-end flow (Scarier finishes 1000/1000).  Still open from the pre-fix stuck-tail census (never reached by the corrected feed): 3.8 answers a matched-task-wrong-room with "You can't do that here." (21x) and names unseen/unheld objects in refusals ("You can't see X from here!" / "You don't have X!") where Scarier says "Take what?" etc -- a 3.8 referenceability/where-fail model not yet ported.  Measured 2026-08-31 |
| `haunt.taf` | 3.80 | full run380 replay, `Adven_1_haunt.rtf` (feed `cmdfile_w_haunt.txt` -- 85 commands, `measure38.sh`: Save Transcript at the 84th, the winning `down` sent after it) | 84/84 echoed; 40 divergent turns, then 1, then 0 -- two pre-3.9 engine rules, both fixed: **no startup event tick before 3.90** (a StarterType 2 delay of N starts on turn N, uncompensated) and **no administrative turns before 3.90** (`score` ticks NPCs and events).  Seven 3.80 goldens re-blessed, wrecked re-pinned to seed 106; full suite 428/428 PASS. |
| `jb2000.taf` | 3.80 | three run380 replays, `Adven_1_jb2000.rtf` / `_jb2000b` / `_jb2000c` (final feed `cmdfile_w_jb2000c.txt` -- 23 commands, the walkthrough plus four `take` probes, Save Transcript at the 22nd) | 22/22 echoed; ONE engine finding, FIXED: run380's generaltasks rewrites the typed `take` to `get` before matching (441C61), a **3.80-only** rewrite (run370 has only everything->all and slap->hit, run390 has no change() rewrite at all, run400 rewrites take->get only inside its get handler after task matching).  This game's tasks are all written `get X`, so Scarier had been printing the library's "You take the X." where run380 ran the task.  Ported in `pf_filter_input` (BUILTIN table, version-gated); after the port 0 differences.  Golden re-blessed (the probes) |
| `Crime_Adventure.taf` | 3.80 | full run380 replay, `Adven_1_crime.rtf` (whole solution, Save Transcript at the penultimate command) | every command echoed; 0 engine differences once the take->get rewrite was in.  Re-blessed: 65/95 finish (the `score` before `stand on chair` ticks the events in 3.8) |
| `mikes.taf` | 3.80 | second full run380 replay, `Adven_1_mikesb.rtf` (feed `cmdfile_w_mikesb.txt`) | cmd 27 `take truck keys` -> `Which keys.  The mustang keys or the truck keys?` -- the end-of-turn co() prompt, now **PORTED for 3.7/3.8** (see the DIAGNOSED section's 2026-09-04 addendum); identical through cmd 52 after the port; the shift from cmd 53 on is the "auction" event's RANDOM 2..10 length, not a consequence of 27 |
| `great.taf` | 3.80 | five run380 replays: `Adven_1_greatx1.rtf` (6-command probe), `_greatb` / `_greatc` (old walkthrough, survived the chase, 1240), `_greatd` (new walkthrough + dummy `look` after the winning `hide` -- the end-of-game modals wipe the scrollback, 2870-byte .rtf, useless), `_greate` (new walkthrough, DIED in the chase -- a death restarts the game and wipes the scrollback, 3240 bytes, useless), `_greatf` (feed `cmdfile_w_greatf.txt`: 121 commands up to `break into car`) | 121/121 echoed, **0 engine differences** (turns 5/102/109 differ only by the .rtf's `Â£` mojibake).  Line 41 is now `steal picasso`: the 3.80 take->get rewrite turns `take picasso` into `get picasso`, which matches none of the theft task's patterns and falls to "You can't take the picasso."  Everything after `break into car` is the car chase, whose four events roll RANDOM lengths (police arrival 1..6, small road 1..15, police 3 / police 4 1..5): greatc survived and greate died on the same feed, so the chase is unmeasurable and only the pre-chase turns count.  Even turn 121's tail shows the roll: the "sirens" event prints PrefText1 ("grows steadily louder", run380) or PrefText2 ("getting much closer", Scarier) on its start turn depending on the length it drew |
| `akron.taf` | 3.80 | the 2026-08-24 run380 replay `Adven_7_akron.rtf` (feed `cmdfile_akron.txt`, 44 commands), re-compared 2026-09-05 with `compare_wine_transcript.py` against the engine as of 9c7c1691 | still clean: 43/43 echoed, 0 differences (the 44th, `knock`, wins and is never echoed).  No events in the game, so the 2026-09-04 tick changes could not have moved it |
| `microwaveman.taf` | 3.80 | full run380 replay, `Adven_1_microwaveman.rtf` (feed `cmdfile_w_microwaveman.txt` -- the 9-command solution, Save Transcript at the 8th, the winning `shoot man` last) | clean: 8/8 echoed, 0 differences.  Its one event is fixed-length (5) and StarterType 3 |
| `duck.taf` | 3.80 | full run380 replay, `Adven_1_duck.rtf` (feed `cmdfile_w_duck.txt` -- 13 commands, the winning `jump` last) | clean: 12/12 echoed, 0 differences |
| `first.taf` | 3.80 | full run380 replay, `Adven_1_first.rtf` (feed `cmdfile_w_first.txt` -- the 18-command solution plus a dummy `look`, because `read book` prints the ending text without an EndGame, so the game is still at a prompt and the Save Transcript can follow it) | clean: 18/18 echoed, identical on every turn |
| `haunted.taf` | 3.80 | full run380 replay, `Adven_1_haunted.rtf` (feed `cmdfile_w_haunted.txt` -- the 116-command solution, the winning `open gate` last) | clean: 115/115 echoed, 0 differences.  Both of its events (rain 15..20 delay / 10..15 length, chains 20..50 delay) are RNG-timed but carry no room list, so their texts never show; nothing to diverge on |
| `castle.taf` | 3.70 | full run370 replay, `Adven_1_castle.rtf` (feed `cmdfile_w_castle.txt` -- the 17-command solution, the winning `take treasure chest` last; `measure38.sh ... run370.exe`) | clean: 16/16 echoed, 0 differences.  The older `Adven_3_castle_quest.rtf` (723 bytes, 2026-08-23, driven by hand before `measure38.sh`) holds no turns at all and is superseded |

Three of these -- `xfiles`, `wamk` and `humbug` -- are **not measurable by
full replay**.  For `xfiles` and `wamk` the reason is RNG-timed event lines, so
the Runner's stream cannot be aligned against ours command for command; for
`humbug` it is randomised puzzle answers that the walkthrough hard-codes.  For
those, argue from the P-code and from a short targeted probe instead.
`the_pk_girl` looked like a fourth
until 2026-08-24, and it is worth knowing why it was not: what blocked it was
one randomly-placed NPC, and brute-forcing him out of the way (see
`cmdfile_pkhunt.txt`) made the whole game replayable.  Its transcript still
carries RNG-timed lines, so a command-for-command diff is noisy -- but the
*outcome* lines are not noisy at all, and the outcome was the whole question.

## Candidates

Sorted by NPC **walk** count first, then by length. Walks are the payload:
every Professor-class divergence found so far lived in walk phase, walk
arrival announcements, or walker presence lines. `walks`/`NPCs`/`events` come
from `SCR_DUMP_TASKS=1 harness/scare <game>`. `cmds` is the walkthrough
length. Solution files are `goldens/<solution>_solution.txt`.

The dump is one-shot and fires from the first task check, so it needs a turn
to be taken: `printf 'look\nquit\ny\n' | SCR_DUMP_TASKS=1 harness/scare
games/<game>` on stderr. Twelve of the 4.00 games open on a keypress-gated
intro that swallows that `look` and print nothing at all — feed them their own
solution file (with `SCR_SKIP_WAITKEY=1` where the row uses it) instead of
concluding the game has no tasks. Counts are `^NPC `, `^  WALK ` and `^EVENT `
lines.

### 4.00 — 124 games

Professor is in this table (marked **done**) so the exemplar sits next to its
peers. 122 distinct .taf files; `Sandy.taf` and `unravel.taf` each carry two
rows, and `sa.taf` / `sophie.taf` are the two releases of *Sophie's
Adventure*.

Shape of the pool: 29 rows author at least one walk, 88 author at least one
event, 59 need the waitkey allowance, and the lengths are strongly bimodal —
12 rows of 100+ commands against 52 of 20 or fewer. So there are two ways in:
a short row to calibrate the feeder cheaply, then a long walk-rich row for
the payload.

⚠️ The `walks` column counts **authored** walks, not walks the walkthrough
traverses, and at 4.00 that gap can be total: `To_Hell_And_Beyond` heads the
table on 19 walks but its row is a 3-command partial that reaches Oran and
stops, so it exercises essentially none of them. Read `walks` against `cmds`
before picking.

The four best targets, by walks x length:

- `goldilocks` — 252 commands, 8 walks, 10 events. The strongest row in the
  4.00 pool, and it strictly dominates Professor (86 / 2 / 4).
- `sophie` (`sa.taf`) — 255 commands, 7 walks, and **73 NPCs**, far more than
  anything else here; NPC presence lines are exactly where the Professor
  divergences lived. `sophie_comp` (`sophie.taf`) replays the comp release of
  the same game, so the pair also cross-checks a re-release.  The first fifty commands were replayed
  2026-08-25 and pinned the walk-announcement join; the rest of the row is
  still open.
- `cibass` — 40 commands, 8 walks, 8 events. Short enough to finish in one
  session at full walk density.
- `vardock_bates` — 103 commands, 2 walks, waitkey; the closest structural
  match to Professor, useful as a control.

| game | solution | cmds | walks | NPCs | events | waitkey | notes |
|---|---|---:|---:|---:|---:|---|---|
| `To_Hell_And_Beyond.taf` | `to_hell_and_beyond` | 3 | 19 | 41 | 7 | -- | [To_Hell_And_Beyond_walkthrough](To_Hell_And_Beyond_walkthrough.md) |
| `goldilocks.taf` | `goldilocks` | 252 | 8 | 6 | 10 | -- | [Goldilocks_walkthrough](Goldilocks_walkthrough.md) |
| `CIBASS.taf` | `cibass` | 40 | 8 | 2 | 8 | yes | [CIBASS_walkthrough](CIBASS_walkthrough.md) |
| `FunHouse.taf` | `funhouse` | 18 | 8 | 9 | 0 | -- | **done** 2026-08-24 -- see "Measured so far" |
| `sa.taf` | `sophie` | 255 | 7 | 73 | 13 | yes | **partly done** 2026-08-25 (first 50 commands) -- see "Measured so far"; [Sophies_Adventure_walkthrough](Sophies_Adventure_walkthrough.md) |
| `sophie.taf` | `sophie_comp` | 255 | 6 | 72 | 13 | yes | [Sophies_Adventure_walkthrough](Sophies_Adventure_walkthrough.md) |
| `Oh_Human.taf` | `ohhuman` | 9 | 6 | 3 | 5 | -- | -- |
| `TheCatintheTree.taf` | `the_cat_in_the_tree` | 8 | 5 | 4 | 1 | yes | **done** 2026-08-24 -- see "Measured so far" |
| `Monsters_r2.taf` | `monsters` | 38 | 3 | 3 | 4 | -- | -- |
| `The Angel the Devil and the Human.taf` | `angeldevilhuman` | 25 | 3 | 3 | 3 | -- | -- |
| `Through time.taf` | `through_time` | 18 | 3 | 10 | 3 | -- | [Through_time_walkthrough](Through_time_walkthrough.md) |
| `Vardock Bates.taf` | `vardock_bates` | 103 | 2 | 4 | 4 | yes | [Vardock_Bates_walkthrough](Vardock_Bates_walkthrough.md) |
| `Professor.taf` | `professor` | 86 | 2 | 9 | 4 | -- | **done** -- the worked example |
| `cyber2.taf` | `cyber2` | 29 | 2 | 8 | 1 | -- | [cyber2_walkthrough](cyber2_walkthrough.md) |
| `ADRIFTMaze.taf` | `adrift_maze` | 26 | 2 | 5 | 5 | -- | [ADRIFT_Maze_walkthrough](ADRIFT_Maze_walkthrough.md) |
| `cyber.taf` | `cyber` | 20 | 2 | 3 | 1 | -- | [Cyber_walkthrough](Cyber_walkthrough.md) |
| `DragonShrineR43.taf` | `dragonshrine` | 136 | 1 | 1 | 7 | yes | [The_Curse_of_DragonShrine_walkthrough](The_Curse_of_DragonShrine_walkthrough.md) |
| `BlackSheepsGold.taf` | `black_sheeps_gold` | 99 | 1 | 11 | 1 | yes | -- |
| `QuiATueDana.taf` | `qui_a_tue_dana` | 63 | 1 | 4 | 0 | yes | -- |
| `plunder_gargoyle.taf` | `plunder_gargoyle` | 43 | 1 | 3 | 4 | -- | [Pirates_Plunder_walkthrough](Pirates_Plunder_walkthrough.md) |
| `demonhunter.taf` | `demonhunter` | 40 | 1 | 2 | 2 | -- | [Apprentice_of_the_Demonhunter_walkthrough](Apprentice_of_the_Demonhunter_walkthrough.md) |
| `Invasion of the Second-Hand Shirts.taf` | `invasion_shirts` | 39 | 1 | 3 | 0 | -- | [Invasion_of_the_Second-Hand_Shirts_walkthrough](Invasion_of_the_Second-Hand_Shirts_walkthrough.md) |
| `Imagination.taf` | `imagination` | 35 | 1 | 1 | 0 | -- | [Just_My_Imagination_walkthrough](Just_My_Imagination_walkthrough.md) |
| `hyper_b_s.taf` | `hyper_b_s` | 34 | 1 | 2 | 1 | -- | [hyper_b_s_walkthrough](hyper_b_s_walkthrough.md) |
| `Renegade_Brainwave.taf` | `renegade_brainwave` | 25 | 1 | 5 | 3 | -- | [Renegade_Brainwave_walkthrough](Renegade_Brainwave_walkthrough.md) |
| `whitterscap.taf` | `whitterscap` | 21 | 1 | 3 | 4 | -- | -- |
| `All Hallows Eve.taf` | `allhallowseve` | 16 | 1 | 4 | 0 | yes | -- |
| `SRSintro.taf` | `srsintro` | 13 | 1 | 2 | 3 | -- | [SRSintro_walkthrough](SRSintro_walkthrough.md) |
| `competition2006__adrift__ptgood__PTGOOD.taf` | `ptgood` | 6 | 1 | 1 | 0 | -- | -- |
| `The Plague - Redux.taf` | `plague` | 266 | 0 | 10 | 20 | yes | [The_Plague_Redux_walkthrough](The_Plague_Redux_walkthrough.md) |
| `vetknow.taf` | `vetknow` | 228 | 0 | 15 | 38 | yes | [Veteran_Knowledge_walkthrough](Veteran_Knowledge_walkthrough.md) |
| `TheCellar.taf` | `cellar` | 176 | 0 | 1 | 1 | yes | [TheCellar_walkthrough](TheCellar_walkthrough.md) |
| `mysteryofcaves.taf` | `mysteryofcaves` | 146 | 0 | 6 | 1 | yes | [mysteryofcaves_walkthrough](mysteryofcaves_walkthrough.md) |
| `Space Boy's First Adventure.taf` | `space_boy` | 145 | 0 | 1 | 1 | -- | [Space_Boy_walkthrough](Space_Boy_walkthrough.md) |
| `vetknow2.taf` | `vetknow2` | 141 | 0 | 15 | 38 | yes | [Veteran_Knowledge_walkthrough](Veteran_Knowledge_walkthrough.md) |
| `shardsofmemory.taf` | `shardsofmemory` | 122 | 0 | 6 | 5 | yes | [Shards_of_Memory_walkthrough](Shards_of_Memory_walkthrough.md) |
| `man overboard.taf` | `man_overboard` | 99 | 0 | 5 | 0 | yes | [Man_Overboard_walkthrough](Man_Overboard_walkthrough.md) |
| `relojero.taf` | `relojero` | 88 | 0 | 0 | 2 | -- | [La_hija_del_relojero_walkthrough](La_hija_del_relojero_walkthrough.md) |
| `salutations.taf` | `salutations` | 88 | 0 | 3 | 2 | yes | [Salutations_walkthrough](Salutations_walkthrough.md) |
| `CBN.taf` | `cbn` | 82 | 0 | 1 | 0 | yes | [The_Revenge_Of_Clueless_Bob_Newbie_walkthrough](The_Revenge_Of_Clueless_Bob_Newbie_walkthrough.md) |
| `forum2.taf` | `forum2` | 82 | 0 | 1 | 0 | yes | [Forum_2_walkthrough](Forum_2_walkthrough.md) |
| `asdfa.taf` | `asdfa` | 80 | 0 | 4 | 0 | yes | [ASDFA_walkthrough](ASDFA_walkthrough.md) |
| `mortality.taf` | `mortality` | 78 | 0 | 4 | 5 | yes | [Mortality_walkthrough](Mortality_walkthrough.md) |
| `princess1.taf` | `princess_in_the_tower` | 78 | 0 | 4 | 1 | -- | [Princess_In_The_Tower_walkthrough](Princess_In_The_Tower_walkthrough.md) |
| `Private Eye.taf` | `private_eye` | 74 | 0 | 0 | 0 | yes | [Private_Eye_walkthrough](Private_Eye_walkthrough.md) |
| `AFDFR.taf` | `afdfr` | 73 | 0 | 32 | 17 | yes | [A_Fine_Day_For_Reaping_walkthrough](A_Fine_Day_For_Reaping_walkthrough.md) |
| `chooseyourown.taf` | `chooseyourown` | 72 | 0 | 0 | 0 | yes | [chooseyourown_walkthrough](chooseyourown_walkthrough.md) |
| `hauntedhouse.taf` | `hauntedhouse` | 72 | 0 | 4 | 1 | -- | [The_Haunted_House_of_Hideous_Horror_walkthrough](The_Haunted_House_of_Hideous_Horror_walkthrough.md) |
| `valley.taf` | `valley` | 72 | 0 | 6 | 0 | yes | [HappyValley_walkthrough](HappyValley_walkthrough.md) |
| `yak_shaving.taf` | `yak_shaving` | 71 | 0 | 5 | 3 | yes | [Yak_Shaving_walkthrough](Yak_Shaving_walkthrough.md) |
| `unravel.taf` | `unraveling_god_lou` | 70 | 0 | 4 | 10 | yes | -- |
| `unravel.taf` | `unraveling_god` | 70 | 0 | 4 | 10 | yes | -- |
| `lobster.taf` | `lobster` | 65 | 0 | 1 | 4 | -- | -- |
| `Tear.taf` | `Tear` | 62 | 0 | 0 | 3 | -- | [Tears_of_a_Tough_Man_walkthrough](Tears_of_a_Tough_Man_walkthrough.md) |
| `cbn2.taf` | `cbn2` | 60 | 0 | 2 | 0 | yes | [The_Revenge_Of_Clueless_Bob_Newbie_2_walkthrough](The_Revenge_Of_Clueless_Bob_Newbie_2_walkthrough.md) |
| `imagi.taf` | `imagidroids` | 60 | 0 | 0 | 7 | yes | [ImagiDroids_walkthrough](ImagiDroids_walkthrough.md) |
| `saffire.taf` | `saffire` | 58 | 0 | 0 | 1 | -- | [Saffire_walkthrough](Saffire_walkthrough.md) |
| `CD.taf` | `crimsondetritus` | 53 | 0 | 1 | 0 | yes | [CrimsonDetritus_walkthrough](CrimsonDetritus_walkthrough.md) |
| `exercise.taf` | `too_much_exercise` | 51 | 0 | 0 | 0 | -- | [Too_Much_Exercise_walkthrough](Too_Much_Exercise_walkthrough.md) |
| `marika.taf` | `marika` | 50 | 0 | 0 | 1 | yes | -- |
| `second chance.taf` | `second_chance` | 50 | 0 | 23 | 9 | yes | [Second_Chance_walkthrough](Second_Chance_walkthrough.md) |
| `Beanstalk.taf` | `beanstalk` | 49 | 0 | 3 | 1 | -- | -- |
| `goblinhunt.taf` | `goblinhunt` | 48 | 0 | 2 | 0 | yes | [Goblin_Hunt_walkthrough](Goblin_Hunt_walkthrough.md) |
| `shore.taf` | `shore` | 46 | 0 | 1 | 1 | -- | [The_Farthest_Shore_walkthrough](The_Farthest_Shore_walkthrough.md) |
| `chicken.taf` | `chicken` | 45 | 0 | 2 | 0 | -- | [The_Evil_Chicken_of_Doom_walkthrough](The_Evil_Chicken_of_Doom_walkthrough.md) |
| `buried.taf` | `buried_alive` | 43 | 0 | 1 | 1 | -- | [Buried_Alive_walkthrough](Buried_Alive_walkthrough.md) |
| `Percy.taf` | `percy` | 41 | 0 | 1 | 1 | -- | [The_Saga_of_Percy_the_Viking_walkthrough](The_Saga_of_Percy_the_Viking_walkthrough.md) |
| `marlin_affair.taf` | `marlin_affair` | 40 | 0 | 0 | 1 | yes | [Marlin_Affair_Prologue_walkthrough](Marlin_Affair_Prologue_walkthrough.md) |
| `microbe_willie.taf` | `microbe_willie` | 40 | 0 | 2 | 2 | -- | [Microbe_Willie_vs_The_Rat_walkthrough](Microbe_Willie_vs_The_Rat_walkthrough.md) |
| `pyramid.taf` | `pyramid` | 38 | 0 | 0 | 2 | yes | [The_Pyramid_of_Hamaratum_walkthrough](The_Pyramid_of_Hamaratum_walkthrough.md) |
| `Confession(1).taf` | `confession` | 37 | 0 | 1 | 3 | yes | [Confession_walkthrough](Confession_walkthrough.md) |
| `togetyou.taf` | `togetyou` | 34 | 0 | 1 | 8 | yes | [We_Are_Coming_To_Get_You_walkthrough](We_Are_Coming_To_Get_You_walkthrough.md) |
| `Griswold.taf` | `griswold` | 33 | 0 | 0 | 1 | yes | [Griswold_walkthrough](Griswold_walkthrough.md) |
| `endgame.taf` | `endgame` | 32 | 0 | 1 | 0 | -- | [The_Game_To_End_All_Games_walkthrough](The_Game_To_End_All_Games_walkthrough.md) |
| `frog.taf` | `frog` | 27 | 0 | 3 | 0 | -- | [The_Green_Princess_walkthrough](The_Green_Princess_walkthrough.md) |
| `SPAM.taf` | `spam` | 27 | 0 | 2 | 3 | yes | [SPAM_walkthrough](SPAM_walkthrough.md) |
| `I am the Law.taf` | `law` | 26 | 0 | 5 | 3 | yes | [IAmTheLaw_walkthrough](IAmTheLaw_walkthrough.md) |
| `topaz.taf` | `topaz` | 23 | 0 | 0 | 4 | yes | [Topaz_walkthrough](Topaz_walkthrough.md) |
| `Wreckage.taf` | `wreckage` | 23 | 0 | 0 | 2 | -- | [Wreckage_walkthrough](Wreckage_walkthrough.md) |
| `ARGH_sGreatEscape.taf` | `argh` | 22 | 0 | 0 | 1 | -- | [ARGHs_Great_Escape_walkthrough](ARGHs_Great_Escape_walkthrough.md) |
| `ShadricksTravels.taf` | `shadricks_travels` | 22 | 0 | 3 | 0 | -- | -- |
| `1HRGAME.taf` | `masochists_heaven` | 20 | 0 | 0 | 0 | -- | [Masochists_Heaven_walkthrough](Masochists_Heaven_walkthrough.md) |
| `Pieces of eden.taf` | `pieces_of_eden` | 20 | 0 | 1 | 3 | -- | [Pieces_of_eden_walkthrough](Pieces_of_eden_walkthrough.md) |
| `longbarrow.taf` | `longbarrow` | 19 | 0 | 0 | 2 | -- | -- |
| `Vagabond.taf` | `vagabond` | 19 | 0 | 3 | 2 | yes | [Vagabond_walkthrough](Vagabond_walkthrough.md) |
| `agent_4F[1].A.taf` | `agent4f` | 18 | 0 | 0 | 5 | -- | [Agent_4-F_from_Mars_walkthrough](Agent_4-F_from_Mars_walkthrough.md) |
| `dancingevenhim.taf` | `dancing_even_him` | 17 | 0 | 0 | 1 | yes | -- |
| `Undefined1.taf` | `undefined` | 17 | 0 | 0 | 0 | -- | [Undefined_walkthrough](Undefined_walkthrough.md) |
| `outline.taf` | `outline` | 16 | 0 | 0 | 0 | -- | -- |
| `Pilfers.taf` | `pilfers` | 16 | 0 | 0 | 1 | yes | -- |
| `QuestI.taf` | `questi` | 16 | 0 | 0 | 1 | -- | [QuestI_walkthrough](QuestI_walkthrough.md) |
| `raccoon.taf` | `raccoon` | 16 | 0 | 0 | 0 | yes | -- |
| `The_Stowaway.taf` | `stowaway` | 16 | 0 | 2 | 2 | -- | -- |
| `herrdoktor.taf` | `herrdoktor` | 15 | 0 | 0 | 1 | -- | -- |
| `InMemory.taf` | `inmemory` | 15 | 0 | 0 | 9 | yes | [InMemory_walkthrough](InMemory_walkthrough.md) |
| `MurderMansionntro.taf` | `murdermansionntro` | 15 | 0 | 0 | 0 | yes | -- |
| `Sandy.taf` | `sandy` | 15 | 0 | 0 | 0 | -- | -- |
| `shreddem.taf` | `shred_em` | 15 | 0 | 0 | 1 | -- | [Shred_Em_walkthrough](Shred_Em_walkthrough.md) |
| `rollingthedough.taf` | `rollingthedough` | 13 | 0 | 1 | 3 | yes | -- |
| `Witness_Demon_vs_Vampire.taf` | `witnessdemon` | 13 | 0 | 0 | 0 | yes | -- |
| `TheAmulet.taf` | `the_amulet` | 12 | 0 | 0 | 3 | -- | -- |
| `The Dangers of Driving at Night.taf` | `dangersdrivingnight` | 11 | 0 | 4 | 0 | yes | -- |
| `MammothVacuum.taf` | `mammoth` | 11 | 0 | 1 | 0 | yes | [MammothVacuumButtonOfDeath_walkthrough](MammothVacuumButtonOfDeath_walkthrough.md) |
| `headless.taf` | `headless` | 10 | 0 | 4 | 4 | yes | [TeenageHeadlessExperiment_walkthrough](TeenageHeadlessExperiment_walkthrough.md) |
| `Sandy.taf` | `sandy_meta_number` | 10 | 0 | 0 | 0 | -- | -- |
| `The_Shuffling_Room.taf` | `shufflingroom` | 10 | 0 | 0 | 8 | -- | -- |
| `smote.taf` | `smote` | 9 | 0 | 0 | 0 | -- | -- |
| `The Foggy Banana Adventure.taf` | `foggybanana` | 8 | 0 | 3 | 1 | -- | -- |
| `The Fly Human.taf` | `flyhuman` | 7 | 0 | 0 | 3 | -- | -- |
| `hungry.taf` | `hungry` | 7 | 0 | 2 | 1 | -- | -- |
| `zombiecow.taf` | `zombiecow` | 7 | 0 | 0 | 2 | yes | -- |
| `asteroid_after.taf` | `asteroidafter` | 6 | 0 | 11 | 3 | yes | -- |
| `door.taf` | `door` | 5 | 0 | 0 | 1 | -- | [Door_walkthrough](Door_walkthrough.md) |
| `Existence.taf` | `existence` | 5 | 0 | 1 | 1 | yes | -- |
| `Newton.taf` | `newton` | 5 | 0 | 0 | 1 | -- | -- |
| `Way Out.taf` | `wayout` | 5 | 0 | 0 | 0 | -- | -- |
| `zacksmackfoot.taf` | `zacksmackfoot` | 5 | 0 | 0 | 2 | yes | -- |
| `P2P.taf` | `p2p` | 4 | 0 | 0 | 4 | yes | -- |
| `hiker.taf` | `hiker` | 3 | 0 | 1 | 5 | -- | -- |
| `rift.taf` | `rift` | 3 | 0 | 0 | 1 | -- | -- |
| `Phoneb.taf` | `phoneb` | 2 | 0 | 0 | 0 | -- | -- |
| `ptbad.taf` | `ptbad` | 1 | 0 | 1 | 0 | -- | -- |
| `Cut_the_Red_Wire.taf` | `redwire` | 1 | 0 | 1 | 0 | yes | [CutTheRedWire_walkthrough](CutTheRedWire_walkthrough.md) |
| `The Vault.taf` | `vault` | 1 | 0 | 1 | 1 | -- | -- |

### 3.90 — 54 games

| game | solution | cmds | walks | NPCs | events | waitkey | notes |
|---|---|---:|---:|---:|---:|---|---|
| `Merry_Murders.taf` | `merry_murders` | 181 | 8 | 8 | 2 | yes | [Merry_Murders_walkthrough](Merry_Murders_walkthrough.md) |
| `Vampire.taf` | `vampire` | 205 | 7 | 11 | 11 | yes | [The_Vampire_With_A_Conscience_walkthrough](The_Vampire_With_A_Conscience_walkthrough.md) -- **measured 2026-08-31**, Runner walls at 70/100 (T61 spent-claim), see section below |
| `gamma.taf` | `gamma` | 315 | 4 | 10 | 0 | -- | -- |
| `S_Tar_Dus.taf` | `stardust` | 199 | 4 | 6 | 0 | -- | [S_Tar_Dus_T_walkthrough](S_Tar_Dus_T_walkthrough.md) |
| `wingman1.taf` | `wingman1` | 33 | 3 | 3 | 0 | -- | -- |
| `tcom.taf` | `tcom` | 13 | 3 | 1 | 0 | -- | [tcom_walkthrough](tcom_walkthrough.md) |
| `windy2.taf` | `windy2` | 200 | 2 | 8 | 1 | -- | -- |
| `Richard.taf` | `richard` | 189 | 2 | 5 | 13 | yes | [WhereIsRichard_walkthrough](WhereIsRichard_walkthrough.md) |
| `cleft.taf` | `cleft` | 115 | 1 | 2 | 1 | -- | [The_Cleft_in_the_Rock_walkthrough](The_Cleft_in_the_Rock_walkthrough.md) |
| `ECOD3.taf` | `ecod3` | 26 | 1 | 1 | 0 | -- | [ECOD3_walkthrough](ECOD3_walkthrough.md) -- **measured 2026-08-31**, clean (tail only) |
| `BobBobsly.taf` | `bob_bobsly` | 25 | 1 | 2 | 0 | -- | [Bob_Bobsly_walkthrough](Bob_Bobsly_walkthrough.md) |
| `largo-winch.taf` | `largo_winch` | 323 | 0 | 42 | 22 | -- | [Largo_Winch_walkthrough](Largo_Winch_walkthrough.md) -- **measured 2026-09-05**, clean (tail only) |
| `mudergreatfalls.taf` | `murder_great_falls` | 255 | 0 | 0 | 0 | yes | [Murder_in_Great_Falls_walkthrough](Murder_in_Great_Falls_walkthrough.md) -- **measured 2026-09-05**, clean (tail only) |
| `report.taf` | `report` | 254 | 0 | 0 | 0 | -- | [Report_Espionage_walkthrough](Report_Espionage_walkthrough.md) -- **measured 2026-09-05**, clean (tail only) |
| `Archie's Birthday V 1-2.taf` | `archie` | 240 | 0 | 8 | 0 | yes | [Archies_Birthday_walkthrough](Archies_Birthday_walkthrough.md) -- **measured 2026-09-05**, two engine fixes (3.9 pronoun echo, examine-self full stop) |
| `croft.taf` | `croft` | 193 | 0 | 4 | 1 | -- | -- -- **measured 2026-09-05**, clean |
| `DarkTower.taf` | `darktower` | 174 | 0 | 0 | 0 | -- | [The_Dark_Tower_walkthrough](The_Dark_Tower_walkthrough.md) -- **measured 2026-09-05**, clean |
| `FarFromHome.taf` | `farfromhome` | 167 | 0 | 0 | 0 | yes | [Far_From_Home_walkthrough](Far_From_Home_walkthrough.md) -- **measured 2026-09-05**, clean (tail only); do not checkpoint a measurement drive |
| `EnqueteAHautsRisques.taf` | `enquete_a_hauts_risques` | 145 | 0 | 13 | 7 | -- | **measured 2026-09-05**, clean (tail only) |
| `Captive.taf` | `captive` | 141 | 0 | 2 | 19 | -- | [Captive_Universe_walkthrough](Captive_Universe_walkthrough.md) -- **measured 2026-09-05**, clean (tail only); 57 real commands, not 141 |
| `The Screen Savers On Planet X.taf` | `screen_savers` | 133 | 0 | 10 | 19 | -- | [The_Screen_Savers_On_Planet_X_walkthrough](The_Screen_Savers_On_Planet_X_walkthrough.md) |
| `thewoods.taf` | `thewoods` | 133 | 0 | 0 | 0 | yes | [The_Woods_Are_Dark_walkthrough](The_Woods_Are_Dark_walkthrough.md) |
| `Chosen.taf` | `chosen` | 123 | 0 | 0 | 0 | yes | [Chosen_walkthrough](Chosen_walkthrough.md) |
| `Renuntio.taf` | `renuntio` | 118 | 0 | 0 | 3 | yes | [Renuntio_walkthrough](Renuntio_walkthrough.md) |
| `as.taf` | `asylum` | 102 | 0 | 1 | 0 | yes | [Asylum_walkthrough](Asylum_walkthrough.md) |
| `A_Morning_with_a_Headache.taf` | `morning_headache` | 88 | 0 | 3 | 8 | -- | [A_Morning_with_a_Headache_walkthrough](A_Morning_with_a_Headache_walkthrough.md) |
| `sleaze.taf` | `sleaze` | 86 | 0 | 0 | 0 | -- | [Sleaze_City_walkthrough](Sleaze_City_walkthrough.md) |
| `Wheel105.taf` | `wheels_must_turn` | 77 | 0 | 4 | 15 | yes | [The_Wheels_Must_Turn_walkthrough](The_Wheels_Must_Turn_walkthrough.md) |
| `tq3.taf` | `tq3` | 76 | 0 | 2 | 4 | -- | [The_Quest_Moody_walkthrough](The_Quest_Moody_walkthrough.md) |
| `mhpquest.taf` | `mhpquest` | 68 | 0 | 2 | 0 | -- | [MHP_Quest_walkthrough](MHP_Quest_walkthrough.md) |
| `everything.taf` | `everything` | 68 | 0 | 0 | 0 | yes | [Everything_Emanuelle_walkthrough](Everything_Emanuelle_walkthrough.md) |
| `ECOD2.taf` | `ecod2` | 61 | 0 | 0 | 0 | yes | [ECOD2_walkthrough](ECOD2_walkthrough.md) |
| `chicago.taf` | `chicago` | 60 | 0 | 3 | 0 | -- | [Chicago_walkthrough](Chicago_walkthrough.md) |
| `hangover.taf` | `the_hangover` | 56 | 0 | 16 | 0 | -- | -- |
| `veteran.taf` | `veteran` | 47 | 0 | 3 | 0 | -- | [Veteran_Experience_walkthrough](Veteran_Experience_walkthrough.md) |
| `lostsouls.taf` | `lost_souls` | 47 | 0 | 0 | 0 | -- | [Lost_Souls_walkthrough](Lost_Souls_walkthrough.md) |
| `CRM.taf` | `crm` | 46 | 0 | 0 | 0 | -- | [That_Crazy_Radioactive_Monkey_walkthrough](That_Crazy_Radioactive_Monkey_walkthrough.md) |
| `Villains_And_Kings.taf` | `villains_and_kings` | 44 | 0 | 0 | 0 | -- | [Villains_And_Kings_walkthrough](Villains_And_Kings_walkthrough.md) |
| `DFU.taf` | `dfu` | 44 | 0 | 1 | 0 | -- | [Dance_Fever_USA_walkthrough](Dance_Fever_USA_walkthrough.md) |
| `impulso.taf` | `impulso` | 43 | 0 | 0 | 0 | -- | [Impulso_walkthrough](Impulso_walkthrough.md) |
| `Colony.taf` | `colony` | 40 | 0 | 3 | 3 | -- | [Colony_walkthrough](Colony_walkthrough.md) |
| `LOST.TAF` | `lost` | 38 | 0 | 3 | 11 | yes | [Albert_is_Lost_walkthrough](Albert_is_Lost_walkthrough.md) |
| `LOST.TAF` | `lost_down` | 38 | 0 | 3 | 11 | yes | -- |
| `amonkeytoomany.taf` | `amonkeytoomany` | 34 | 0 | 1 | 0 | -- | [A_Monkey_Too_Many_walkthrough](A_Monkey_Too_Many_walkthrough.md) |
| `Phoenix_Destiny.taf` | `phoenix_destiny` | 33 | 0 | 0 | 0 | -- | [Phoenix_Destiny_walkthrough](Phoenix_Destiny_walkthrough.md) |
| `CAH.taf` | `cruel` | 30 | 0 | 0 | 0 | -- | [Cruel_and_Hilarious_Punishment_walkthrough](Cruel_and_Hilarious_Punishment_walkthrough.md) |
| `forest.taf` | `forest_on_the_norm` | 27 | 0 | 4 | 0 | -- | [Forest_On_The_Norm_walkthrough](Forest_On_The_Norm_walkthrough.md) |
| `Locked_door_with_water_trap.taf` | `locked_door` | 21 | 0 | 0 | 0 | yes | -- |
| `Theannihilationofthink2.taf` | `think2` | 19 | 0 | 0 | 0 | -- | [Theannihilationofthink2_walkthrough](Theannihilationofthink2_walkthrough.md) |
| `lifesimulation.taf` | `lifesimulation` | 19 | 0 | 0 | 0 | -- | [lifesimulation_walkthrough](lifesimulation_walkthrough.md) |
| `Insane.taf` | `escape_from_insanity` | 16 | 0 | 0 | 0 | -- | [Escape_from_Insanity_walkthrough](Escape_from_Insanity_walkthrough.md) |
| `Toxically_Earth.taf` | `toxically_earth` | 11 | 0 | 17 | 0 | -- | [Toxically_Earth_walkthrough](Toxically_Earth_walkthrough.md) |
| `Dreams.taf` | `dreamland` | 10 | 0 | 0 | 1 | -- | [Dreamland_walkthrough](Dreamland_walkthrough.md) |
| `Matt's House.taf` | `matts_house` | 8 | 0 | 5 | 0 | -- | [Matts_House_walkthrough](Matts_House_walkthrough.md) |

### 3.80 — 10 games (ALL MEASURED as of 2026-09-05)

| game | solution | cmds | walks | NPCs | events | waitkey | notes |
|---|---|---:|---:|---:|---:|---|---|
| `haunt.taf` | `haunt` | 88 | 5 | 4 | 3 | -- | **done** 2026-09-04 -- see "Measured so far" |
| `superliam.taf` | `super_liam` | 86 | 5 | 11 | 0 | -- | **done** 2026-08-31 -- see "Measured so far" |
| `cave.taf` | `cave` | 216 | 2 | 5 | 12 | -- | **done** 2026-08-31 -- see "Measured so far" |
| `akron.taf` | `akron` | 44 | 2 | 4 | 0 | -- | **done** 2026-08-24 (`Adven_7_akron.rtf`, 0/44), re-checked 2026-09-05 against the current engine: still 43/43 identical -- see "Measured so far" |
| `jb2000.taf` | `james_bond` | 20 | 1 | 1 | 0 | -- | **done** 2026-09-04 -- see "Measured so far" (take->get rewrite, 3.80 only) |
| `haunted.taf` | `haunted_house` | 116 | 0 | 0 | 2 | -- | **done** 2026-09-05 -- clean, see "Measured so far" |
| `Crime_Adventure.taf` | `crime_adventure` | 90 | 0 | 2 | 3 | -- | **done** 2026-09-04 -- see "Measured so far"; [Crime_Adventure_walkthrough](Crime_Adventure_walkthrough.md) |
| `first.taf` | `fistandantalus` | 18 | 0 | 1 | 0 | -- | **done** 2026-09-05 -- clean, see "Measured so far" |
| `duck.taf` | `duck_mccloud` | 13 | 0 | 0 | 0 | -- | **done** 2026-09-05 -- clean, see "Measured so far" |
| `microwaveman.taf` | `microwave_man` | 9 | 0 | 1 | 1 | -- | **done** 2026-09-05 -- clean, see "Measured so far" |

### 3.70 — 2 games

| game | solution | cmds | walks | NPCs | events | waitkey | notes |
|---|---|---:|---:|---:|---:|---|---|
| `arlo.taf` | `alices_restaurant` | 85 | 11 | 9 | 9 | -- | [ADRIFT_370](ADRIFT_370.md) |
| `castle.taf` | `castle_quest` | 17 | 0 | 1 | 0 | -- | **done** 2026-09-05 -- clean in run370, see "Measured so far"; [ADRIFT_370](ADRIFT_370.md) |

`arlo.taf` is the single best target in the pre-4.0 half: 11 walks in 85
commands, and 3.70 is the least-exercised parse schema in the engine. Across
the whole file `goldilocks` and `sophie` (4.00) are denser, but they test a
schema Professor has already been through — arlo tests one nothing has.

## What to do with a diff

Same discipline as Professor:

1. Rule out Verbose and the Appearance checkboxes first. Two of the three
   apparent Professor divergences were display settings.
2. Rule out the feed. Scroll the Runner to the session top and confirm the
   commands actually landed in order before believing an output difference.
3. Only then treat it as an engine bug — fix the engine, never the
   walkthrough, re-bless the golden, and record the measurement that
   justifies any deliberate deviation in the row's comment block in
   `harness/run_v4_walkthroughs.sh`.

Step 2 is now mechanical.  `harness/compare_wine_transcript.py` takes the
game, the command file that was driven in and the Runner's own
`Adrift_N.txt`, replays the same feed through `harness/scare`, and splits
both sides into turns — the Runner's by its echoed command line, scarier's by
the `>` prompt:

    python3 harness/compare_wine_transcript.py \
        --taf games/The_X-Files_A_New_Beginning.taf \
        --feed goldens/xfiles_solution.txt \
        --runner ~/adrift-battle/runner/wine/pfx/drive_c/adrift/Adrift_22_xfiles.txt

It prints, before any diff, **every feed command the Runner never echoed**,
and marks every later turn as past that point; then it diffs the turns that
did line up, whitespace-normalised so the Runner's own hard wrapping is not a
difference, and says so explicitly when the two streams re-synchronise a
prompt out.  Its self-test is the X-Files row, where it finds `feed[23] look`
— the one lost command that produced two apparent engine bugs (`take knife`
and `knock`) and cost a day of argument before anyone counted the echoes.
`burn memo` at turn 4, which is upstream of the loss, is the only real
divergence it reports there.

## Open leads

Things a measurement turned up that are **not** walk bugs and have not been
chased yet.  Each needs its own investigation; none of them should be folded
into a walk-related change.

- **run390 turn accounting** (BobBobsly.taf, Adrift_1_bob390.txt, 2026-08-29):
  3.9 counts an NPC examine, a failed examine and `turns` itself as turns, and
  `z` advances one turn under WaitTurns 3 where 4.0 loops WaitTurns times
  (48ABFB).  Scarier keeps the 4.0 behaviour for 4.0 games only; which 3.9
  commands are administrative is unmeasured.
- **`lib_cmd_examine_absent` admin status** is unmeasured; `look in <absent>`
  going through the administrative `examine_other` refusal is inferred from
  the 4801E1/471F02 flag, not measured (sandy_meta_number "turn 6").
- **run400 refuses commands Scarier accepts** (found while replaying
  `The_X-Files_A_New_Beginning`, 4.00, 2026-08-23).  `take knife` and
  `take directions` get "Take what?" from run400 while Scarier takes the
  object; `burn memo` gets "I don't understand what you want me to do with
  The Memo."; `look at camera` gets "You see no such thing."; `knock` gets a
  task refusal.  These are task-matching / parser divergences and they are
  what makes that row unmeasurable by full replay.  Worth its own probe --
  the common shape is a multi-word object name matched on a later word.
  **FIXED 2026-08-24**: the two `Take what?` hits and the `look at camera`
  refusal are the object `seen` gate, not the noun matcher -- ported and
  landed, see the seen-model section at the end of this file (`take knife` in
  `Adrift_22_xfiles.txt` is one of the two live measurements the port rests on).
  **CLOSED 2026-08-25: `knock` was never a divergence, and neither was the
  `take knife` difference.**  Both are downstream of one command the feed lost.
  The walkthrough reads `use key` / `look` / `take knife`; the transcript reads

      use key
      The garage door begins to groan open ...
      take knife
      Take what?

  -- no `look` echoed between them.  So the Runner entered Garage 5 through a
  `ShowRoomDesc = 0` task, nothing ever listed the Small Pocket Knife, and its
  `seen` byte stayed clear; Scarier *did* get the `look`, listed the knife and
  took it.  `knock` then follows mechanically: task 9 `Knock` has four ANDed
  restrictions (`#A#A#A#`) and the fourth is **Knife held**
  (`RESTR type=0 v1=26 v2=1 v3=0`), so the Runner answers with that
  restriction's own FailMessage, "You should check out the warehouse first."
  One lost command, two apparent bugs.  Note this does **not** weaken the
  seen-model measurement -- losing the `look` is exactly what left the Runner
  on the bare `ShowRoomDesc = 0` entry the model predicts a refusal for.  It
  does mean the walkthrough's own comment at `goldens/xfiles_solution.txt:25`
  overstates the case: the `look` on the next line would have listed the knife,
  and the Runner simply never saw it.
  **CLOSED 2026-08-25: `burn memo` is a capital letter.**  4.0 substitutes an
  object's Short into a `%object%` task command **verbatim** and compares it to
  the lower-cased input, so a capitalised Short can never bind -- and every
  object in xfiles is capitalised.  Ported, and the golden re-blessed; see the
  FIXED section below.
- **FIXED 2026-08-25: the 4.0 battle narration names an NPC by its alias,
  not by its Name.**  Found by re-running `compare_wine_transcript.py` over
  `Adrift_36_orient_express.txt`, a transcript that was measured for the walk
  work and whose battle lines had been written off as noise.  run400 fights
  "the large man" and "BIG BOSS"; Scarier printed "Igotta Bigbottom" and
  "Ivill Getyou".  The rule, from `Battles.bas`: given a first alias, a blow
  names the NPC `"<Prefix> <Alias[0]>"` -- the player's blow (Proc_11_1,
  @45E1CE) from any NPC, an NPC's blow (Proc_11_2, @464F20 attacker /
  @464FF2 target) only from a combatant whose **current** `Battle.Attitude`
  is enemy (the record byte at +172, tested `= 2`).  Nothing else follows it:
  the corpse line reads the Name field raw (@44B115), and so does every room
  listing -- the very same transcript says "Igotta Bigbottom is here." one
  line above "You manage to avoid the large man's attack."  The dump confirms
  each name it produces (`SCR_DUMP_TASKS=1 SCR_DUMP_BATTLE=1`, which now
  prints an NPC's prefix and aliases): npc 3 `prefix=[the large] alias=[man]
  attitude=2`, npc 6 `prefix=[BIG] alias=[BOSS] attitude=2`, and npc 5
  `Thug ` with no alias at all -- which run400 duly fights as "Thug ",
  trailing space and all.  Ported as `battle_print_npc_name()` in
  `scbattle.cpp`, guarded by `battle_legacy` because the pre-4.0 battle
  system is a different set of strings and names by Name (run390 `Form1.frm`
  @4595DB).  Seven goldens moved: `orient_express` (now matching the Runner
  transcript on every battle line), `trabula`, `shadowpeak` x3, `cyber2` and
  `light_up`.  **Still unmeasured:** whether run400 capitalises a battle line
  that now *starts* with a lowercase alias -- `trabula` gives "a soldier
  attacks you with the rapier", and no saved transcript has an NPC-initiated
  blow to check it against.  The decompile concatenates raw, so the port does
  too; a probe on a game with a lowercase-prefixed enemy would settle it.
- **FIXED 2026-08-25: a bare Return is a parser complaint, not silence.**
  Found by rule 2 rather than in spite of it.  `cmdfile_stardust.txt` and
  `cmdfile_xfiles.txt` are the only CRLF feeds in
  `~/adrift-battle/runner/wine/` (`grep -c $'\r'`: 117 of 117 lines and 21 of
  21), so every command in those two runs was driven in followed by an extra
  empty Return -- and both Runners answered every one of them.  run390's
  `Adrift_38_stardust.txt` carries 115 copies of that game's ALR for
  DontUnderstand, "I are confused.  DURHH!"; run400's `Adrift_31_xfiles.txt`
  carries 22 of xfiles' "Nope!", two of them before the first command is even
  echoed.  Nothing else comes with the message -- no walk line, no event line
  -- so an empty command does **not** tick the turn.  Scarier printed nothing
  at all: upstream SCARE guarded the not-understood block with
  `if (!scr_strempty (command))`.  That guard is gone (`scrunner.cpp`); the
  no-tick half was already right, since the complaint returns FALSE and
  `run_main_loop()` ticks only on TRUE.  An empty `line_element` can only come
  from a genuinely empty input line -- the element splitter already takes the
  first character even when it is a separator, so `.` and `i. .` were
  complaining before this and still are.
  **38 goldens moved, every one of them purely additive** (192 lines added, 0
  removed, no route change, no win marker lost), because a blank line in a
  solution file is a turn like any other.  Two kinds of blank line turn out to
  live in the corpus, and the corpus now shows which is which: real
  empty-command turns (`cbn`'s five leading blanks, whose header already
  carried a FOOTGUN saying so -- one of them is what a `*` task turns into the
  move out of room 0) and mere layout (`iachini` and `wonderwombat` separate
  their commented sections with blanks; `wes_ghn` has 77 blank lines, 45 of
  which reach the parser).  The layout ones now print the game's complaint,
  which is faithful to that feed and is *why* they are worth seeing.  Cleaning
  them out of the solution files is optional and was NOT done: for a row
  without `SCR_SKIP_WAITKEY=1` the blanks are load-bearing -- the `<waitkey>`
  read eats them in file order -- so deleting the wrong one desyncs the route.
- **Games whose transcripts carry RNG-timed lines** (`xfiles`, `wamk`) need a
  *targeted* Runner probe rather than a full replay.  There is no harness for
  that yet; the p4WK* probe .taf files in
  `~/adrift-battle/runner/wine/pfx/drive_c/adrift/` were built by hand in
  gen400 and there is no script that regenerates them.  Note that RNG-timed
  lines do not by themselves make a game unmeasurable -- see `the_pk_girl`
  below, where the diff is noisy but the *outcome* lines are not.
- **FIXED 2026-08-24: a dead NPC still walks in Scarier** -- ported; see the
  section at the end of this file for the full P-code case and the Azra
  fallout.  Original note (read out of run400 while chasing the
  PK Girl walk counters, 2026-08-24):  run400's walk ticker opens with
  `Proc_19_1_468DA0` @0004685B6: `If npc.Room = &HFB Then GoTo 468D61`, i.e.
  it skips *every* walk of an NPC whose room is 251.  251 is the battle
  system's "dead" marker (`Battles.bas` @00044B127, right after the
  " falls down, dead." line).  Scarier has no such marker: `battle_npc_die()`
  in `scbattle.cpp` puts the corpse in location 0, which is "Hidden", and
  `npc_tick_npc()` goes on ticking its walks -- so a walk can march a dead
  NPC back into play.  A faithful fix needs a *separate* dead flag, because
  run400 does keep ticking the walks of a merely hidden NPC (that is how a
  hidden walker comes back); reusing location 0 for both would break that.
  (`&HFB` is really **-5**, not 251: `LitI2_Byte` sign-extends.)
- **FIXED 2026-08-24: "On X is", never "On X are"** (measured on `humbug`,
  4.00, then confirmed in P-code for 3.90 as well).  run400 prints
  "On the triangular table **is** some swimming goggles, a watch, a musket and
  a china doll."  Scarier printed "are": `lib_list_on_object_normal()` and
  `lib_list_in_object_normal()` in `sclibrar.cpp` chose the verb with
  `lib_select_plurality (game, list[0], ...)` -- i.e. from the plurality of the
  *first listed item*.  The Runner does have an is/are helper
  (`isare`, `Proc_19_69_4507BC` @4507BC, a string heuristic on the article and
  the noun's last letter) and calls it for "Also here is/are", but these two
  listings do not: the verb is a literal, run400 @46A31F ("On ") and @46A7C7
  ("Inside "), run390 @443944.  3.70 and 3.80 have no such listing at all, so
  there is no version split.  Both sites now emit `" is "` unconditionally.
- **FIXED 2026-08-24: "... is wearing a hat, and carrying a document."**
  (measured on `humbug`, 4.00; P-code checked in all four Runners).  Scarier
  printed "and **is** carrying".  `lib_list_npc_inventory()` in `sclibrar.cpp`
  emitted `", and"` and then `" is carrying "` unconditionally; the Runner puts
  the `" is"` in the *subject* clause, not the verb clause, so it appears only
  when there is no preceding "wearing" clause:
      run400 @45B901   worn count > 0  ->  var_AA = 1, "  " & np & " is wearing "
      run400 @45BA5F   If var_AA = 1 Then  MemVar_4941B0 &= ", and" : GoTo 45BAA2
      run400 @45BA76   Else                MemVar_4941B0 &= "  " & np & " is"
      run400 @45BAA2                       MemVar_4941B0 &= " carrying "
  run390 is byte-for-byte the same shape (@382E1 `", and"`, @382FF `" is"`,
  @38311 `" carrying "`).  **3.80 differs** and repeats the whole subject:
  @2CA67 appends `", and "` and then falls through to its own `np & " is"`,
  giving "... is wearing a hat, and Grandad is carrying a document."  run370
  has no NPC worn/carried listing at all (its only `" wearing "`/`" carrying "`
  literals, @2B457/@2B5CD, are the player's own inventory).  So the fix is
  gated at `TAF_VERSION_390`.  Corpus movers: `humbug`, `vague`, `target`
  (all 4.00); no 3.80 golden exercises the lister, so that branch rests on the
  P-code alone.

- **FIXED 2026-08-29: the pronoun echo's article** (measured on `humbug`,
  4.00, `Adrift_5_humbug.txt`).  The 4.0 antecedent is a string, and its article is
  whichever handler last composed it: examine gives "(a shovel)", `get`/`drop`/
  `open`/`close`/`unlock` and the generic "I don't understand what you want me
  to do with" give "(the shovel)" ("some gloves" -> "the gloves", "an envelope"
  -> "the envelope"); a take from a character keeps "(a document)"; `put` and
  a failed `get X from Y` leave it alone; `look in <room object>` gives "the"
  through co() itself while a held one is left untouched; and a command that
  used the pronoun never changes it (`drop it`, `x it`).  P-code: composer
  `Proc_21_31_448710` mode 1 vs mode 0 (tense), setter `Proc_21_41_448C24`
  sites listed in `uip_definite_form()` (scparser.cpp).  Re-blessed `humbug`,
  `yak_shaving`, `shred_em`, `provenance`; every changed line is an echo after
  a take or an unlock.  The full checkpointed humbug replay (`Adrift_4_humbug.txt`)
  ended at 1810/2000 -- the Runner's `x chute` after a `#restore` hit a
  "Which chute" ambiguity that a fresh game does not, which is the open
  "restore marks everything seen" lead below.
- **FIXED 2026-08-24: the bracketed pronoun echo is gone** (measured on
  `humbug`, 4.00).  `Drop it` gets a bare "Okay.  I have dropped the paper
  aeroplane." from run400, where Scarier printed an italic
  `[Drop a paper aeroplane]` line first; same for `Read it`.  That echo was
  upstream SCARE's, for synonyms and pronouns alike; an earlier session had
  already removed the synonym half as noise "the Runner never prints" and kept
  the pronoun half on the argument that "it"/"her" are ambiguous and the echo
  is how the player learns what they bound to.  The Runner does not agree, and
  the other three generations back run400 up by string search: **run370 and
  run380 contain no `[` string literal at all, and run390's only one is the
  `[More]` pager** (run400's messages live in a table, so it cannot be checked
  that way -- hence the live measurement).  Removed from `scrunner.cpp`; the
  substitution itself still happens.  14 goldens re-blessed, 94 lines, every
  one of them a bracket line and nothing else: `adrift_maze`, `archie`,
  `cellar`, `cruel`, `humbug`, `iqsfot`, `man_overboard`, `provenance`,
  `shred_em`, `TheADRIFTProject`, `veteran`, `wrecked`, `yak_shaving`,
  `yonastoundingcastle`.  Corpus 303/303.
- **CLOSED 2026-08-25: `the_pk_girl`'s second Detainment visit** (opened
  2026-08-24).  Scarier now prints exactly what the Runner does, at both
  visits, and the closing needed no new measurement -- only re-reading the
  old one against today's engine.  The original note called this "which
  alternate NPC description the room lister picks", and that was wrong on the
  mechanism: there is no selector.  Laurie's line is an **ALR** keyed on a
  variable -- `[[LAURIE_DOING=6]]` -> `is in your arms.`,
  `[[LAURIE_DOING=9]]` -> `is standing here.` -- so the two engines were
  simply printing the room at different values of `laurie_doing`, 6 against 9.
  Both Detainment entries are `ShowRoomDesc` tasks (task 1866 for the first
  visit and task 1898 for the second, both `where=1 room=104 srd=106`), and
  the reunion that sets `laurie_doing = 6` is task 1955, reached from those
  entries.  So the room description has to be printed against the **pre-action**
  world state, which is exactly what the "ShowRoomDesc prints BEFORE the task's
  actions" port established a day later; it fixed this row as a side effect and
  nobody came back to cross it off.  Verified by replaying the golden under
  `SCR_TRACE_TASKS=1` and diffing both visits against
  `Adrift_27_thepkgirl.txt` lines 2807 and 3004: "Laurie is lying on the
  floor." then "Laurie is standing here.", both identical now.
- **NOT A BUG 2026-08-25: a typed task that prints nothing is refused, and
  run400 refuses it too.**  Noticed while building
  `make_400_walkcapprobe.py`: typing `sil1` at `p4WC.taf` (the walk-count
  probe, whose `romeo`/`sil1`/`sil2`/`sil3` have empty CompleteText, no
  ShowRoomDesc and no AdditionalMessage) gets "I don't understand." from
  Scarier, while `kilo` next to it answers "K qqqball."  That looked like a
  matcher gate.  It is not: the Runner's own typed-command task dispatcher,
  `Proc_19_24_44CCE0` (run400 `mdlSpreadTheLoad.bas:21595`, called as `tasks`
  from `generaltasks`), ends

      loc_44CCC0:  If MemVar_4941B0 = "" Then  Result = 0        ' FALSE
                   Else  MemVar_4941B0 = Proc_21_18_47A3DC(MemVar_4941B0)
                         Result = var_86

  -- so however the match went, a turn that left the output buffer empty is
  reported as *not handled*, and the caller falls through to the library and
  then to the unknown-command message.  The match itself does happen
  (`Proc_19_66_454EF0` returns the task index at `loc_44CBDB`, and
  `Proc_19_11_45A3EC` = `execute_task` runs it at `loc_44CC3C`), so the
  task's ACTIONS still run before the refusal is printed.  Scarier's
  `task_run_task_unrestricted()` (`sctasks.cpp`) returns the same FALSE by
  accumulating a per-print `status`, which is why the probe's silent cells
  behaved the way they did; giving each cell a real CompleteText was the
  right workaround, not a workaround for a bug.

  One difference is worth keeping in view and is **not** measured: run400
  tests the WHOLE turn buffer, Scarier tests the task's own output.  They
  differ only if something has already written to the buffer before the verb
  dispatch runs -- `generaltasks` does have the References-in-brackets echo
  ahead of it -- in which case run400 would answer TRUE where Scarier answers
  FALSE.  It needs a command that both triggers that echo and matches a
  silent task; no corpus row is known to.
- **Timed events run a turn out of step** in `the_pk_girl` (2026-08-24), the
  same class already noted on `orient_express`.  Of the 470 replayed commands
  138 differ, and the great majority are an event line landing one command
  early or late.  Nothing about the walk work touches this; it wants its own
  measurement on a small event-heavy game.

## Where the walk work stands, 2026-08-24

The walk rewrite in `scnpcs.cpp` is finished and every claim in it is
live-measured in run400.  (Stale when written and corrected 2026-08-24: this
work *is* committed -- it is in the history up to `1622d8fc` -- and the corpus
is **303** rows, not 304.  As of 2026-08-24 it is 303/303 PASS.)

### The finding: `push &HFF 'Byte` is -1, not 255

VB Decompiler renders a one-byte immediate as `push &HFF 'Byte` and the operand
is **signed**.  P32Dasm shows the same instruction as `F4 LitI2_Byte: 255
(True)`, and VB's `True` is -1.  The unambiguous case sits in the walk ticker
itself: at `468805` the same opcode with the same operand is the `Step` of
`For var_BC = (NumStops - 1) To 0`, a loop that runs at all only if the step is
-1.  So the counter a finished non-looping 4.0 walk is stamped with at `46860B`
is **-1**, a sentinel that compares false against every `> 0` test in the
routine -- not a 255-turn countdown.

That is the whole reason 4.0 could drop the pre-4.0 "only looping walks
restart" test from its restart branch: a spent walk holds itself shut on -1
instead.  Read as 255 it becomes a 256-turn cycle whose walks sit above zero
almost permanently, and because the precedence scan runs over the
*higher-numbered* walks, a handful of spent ones pin every lower walk shut
forever.  Written up in `~/Adrift_decompile/README.md` and in the
`run400 468DA0 npc_walk_tick` row of `~/Adrift_decompile/index/annotations.tsv`.

### Measured, not argued

- `funhouse`: 18/18 commands identical under Wine.  Pins the precedence rule --
  run400 lets a higher-numbered walk with `StartTask 0` shut a lower one down
  with no counter test at all (`Proc_19_1_468DA0` @4686FD-468747), even with no
  stops to walk -- and the task-state (not counter) test in
  `lib_get_npc_inroom_text()`.
- `the_pk_girl`: full replay under run400, and **the Runner wins** --
  `Congratulations! You got Katryn's ending.` / `Your Secret Letter is: E`, with
  24 "Laurie follows you".  This is what pins the sentinel.  A prior reading of
  the P-code had concluded the opposite (that Laurie's spent walks preempt her
  follow walk for good and the game cannot be won in run400); the replay
  disproved it, and the only way to make Scarier agree was -1.  The engine fix
  cut this game's golden diff from 512 lines to 30 and restored the ending.
- `donuts_intro`, `the_cat_in_the_tree`, `maincourse`, `orient_express`: pin the
  room lister's task-state ChangedDesc pick.
- `iqsfot` re-derived (185 -> 178 commands) and re-blessed.  Attribution
  confirmed by construction: suppress the empty walk's precedence and the
  pre-fix route reproduces the pre-fix golden byte for byte.
- `humbug` is the only other corpus row the sentinel change moves: two hunks at
  golden lines 6089/6095 (`Grandad stands nearby.` / `Grandad walks to the
  south.` disappear), at command 844 of 1050.  Under Wine now.

### How `the_pk_girl` was made replayable

Its blocker was never the RNG-timed event lines, it was one NPC.  NPC 26 [the
umbrella peddler] has one walk whose three stops are all the same *room group*
(`dest=119`), so which plaza room he is in is a fresh draw each arrival, and the
walkthrough must meet him ("give money to peddler" / "ask peddler about valley")
to unlock the `j) Wautomec Valley` motorcycle destination.  The first replay
stranded at the Plaza with 90 commands to go.  The fix was brute force:
`cmdfile_pkhunt.txt` (504 lines) splices a 96-command sweep of the plaza rooms,
retrying the meeting in each, in after feed index 308.  Run it with

    cd ~/adrift-battle/runner/wine && sh measure.sh pkgirl.taf cmdfile_pkhunt.txt run400.exe 0

The lesson generalises: a randomly-placed NPC is not an unmeasurable game, it is
a search, and the search is cheap compared to arguing from P-code and getting it
backwards.

### How `humbug` was made replayable

Same shape as `the_pk_girl`, different obstacle: a randomised combination
lock rather than a randomly-placed NPC, and the answer is a **two-phase
drive** rather than a brute-force sweep.  `measure.sh` leaves the Runner
running when its command file is exhausted, so a second file can be driven
into the same live process with `drive_ckpt_safe.sh` directly:

    # phase A -- everything up to the first dial (solution lines 1..165)
    sed -n '1,165p' goldens/humbug_solution.txt > ~/adrift-battle/runner/wine/cmdfile_hb_A.txt
    cd ~/adrift-battle/runner/wine && sh measure.sh humbug.taf cmdfile_hb_A.txt run400.exe 2

    # read the Runner's own slate out of the transcript and rewrite the dials
    python3 <scratch>/hb_partb.py pfx/drive_c/adrift/Adrift_30_humbug.txt \
        <repo>/goldens/humbug_solution.txt cmdfile_hb_B.txt

    # phase B -- into the SAME pid, no relaunch
    FIRSTCHECK=pfx/drive_c/adrift/Adrift_30_humbug.txt sh drive_ckpt_safe.sh <pid> cmdfile_hb_B.txt

`hb_partb.py` parses the roman numeral after `The numerals read`, zero-pads it
to four digits and rewrites the four `Turn dial to N` lines in walkthrough
order (Entrance Hall, East Alcove, South Alcove, North Alcove).  Always pass
`FIRSTCHECK` on the second drive: nothing has verified the process is at a
prompt, and an unnoticed dropped first command puts the whole phase a turn out
of step.

### Still open

- Scarier synthesizes an `NPCWalkAlert` task pair (`sctasks.cpp:1844-1873` ->
  `npc_start_npc_walk()`) for which run400 has no counterpart; in practice it
  only anticipates the ticker's own restart branch by a tick, so nothing in the
  corpus depends on it.  Unresolved, not urgent.
- Scarier has no equivalent of run400's dead-NPC walk gate; see the open lead
  above.
- **`humbug` WAS fully replayed on 2026-08-29** with checkpoint saves
  (`#save NAME` / `#restore NAME` directives in the cmdfile, see
  `drive_ckpt_safe.sh`): each randomised secret is read off the transcript,
  the cmdfile edited, and the run resumed from the last save.  The paragraph
  below is kept for the record of why a plain replay cannot work.  The
  seen-after-restore lead from that replay was chased on 2026-08-29 and
  disproved; the three real splits it hid are in the FIXED section at the end.
- **`humbug` is not measurable by full replay** -- it joins `xfiles` and `wamk`.
  The two-phase splice below gets the dial combination right, but the game
  randomises *three* secrets, not one, and the other two are unreachable the
  same way: the magic word (command 209 `Read runes`, "Jisanajen" here vs
  "Tedikebat" in the Runner) and the keypad code (command 344 `Read wall`,
  "HEL3761" vs "HEL1594").  The keypad is the hard break: at command 373
  `push button 7` the Runner answers only "Beep.  The liquid crystal display
  flickers." with no "The metal door to my west slides open.", so command 376
  `W` fails and the streams part for good.  A three-phase splice (dials, then
  magic word, then keypad) would work and costs about an hour of Wine
  wall-clock; nobody has run it.  Everything the replay *did* reach was worth
  having -- both wording fixes above came out of commands 800/2285 -- but the
  two `NPC_WALK_EXPIRED` lines at golden 6089/6096 sit past the break and stay
  unmeasured here.  They are measured on `the_pk_girl` instead.
  Also: the Grandad absence at command 843 that looked like a confirmation is
  **not** one.  Runner and Scarier Grandad lines agree only through command
  328; the Runner has none after that, and at command 583 `Blow trombone` it
  answers "But I am not carrying the trombone.", so his pub sequence never
  fired and his walk was never started.  The absence proves nothing.
- **The phase-A transcript sweeps clean, 2026-08-25.**  `Adrift_30_humbug.txt`
  had never been through `compare_wine_transcript.py`.  It comes out at 165 of
  165 commands echoed and **ten differing turns, all of them rule 3**:
  * nine are Schrodinger the cat -- eight presence/departure lines (turns 41,
    43, 44, 46, 74, 79, 80, 154) and the room descriptions that do or do not
    carry "There is a small tabby cat here".  This is the divergence the Wine
    warning at the top of this file says is indistinguishable from a swallowed
    first command, and it is *not* that here: rule 2 passes, and `measure.sh`
    drove this run with `FIRSTCHECK`.  It is RNG.  Re-run the feed under
    `SCR_SEED=1/2/3/12345/999` and the cat's **destinations** change every time
    (`to above`/`to the east`/`to the west`/`to the south`/`to the north` in
    the second slot), while the first announced step stays on turn 42 in all
    five.  So the walk *tick* is deterministic and agrees with run400; the
    destination is a die roll, and every visible difference downstream --
    which room the cat is in, and therefore which turns announce it at all --
    follows from that one roll.  Nothing here is measurable without an
    RNG-matched Runner.
  * one is the slate at turn 103, `MMMMCMXXXVII` in run400 against
    `MMMCDXLVI` in Scarier -- the same seed sweep gives four more numerals, so
    it is one of the three randomised secrets above, seen from the other side.
  The turn-164 block, where run400's last chunk carries `Turn dial to 4`, `E`
  and more, is the phase-B drive continuing into the same live process; the
  feed is phase A only.  Expected, not a difference.
- **Three RNG-independent `humbug` divergences, found but not chased.**  All
  three are inside the replayed prefix, so they are real and re-measurable
  cheaply:
  * command 217 `Put sweet on plinth` -- run400 prints "Okay.  Okay.  I put the
    sweet on the plinth." (the "Okay." is *doubled*), Scarier printed one.
    **FIXED 2026-08-24** -- the whole 4.0 output filter, see the section at the
    bottom.  Neither "Okay." is authored.
  * command 254 `W` -- Scarier prints "(Getting off the stool first)", run400
    prints nothing.  **FIXED 2026-08-24, and it was never a bug in the mover**
    -- see "the bracket checkbox governs three more lines" below.
  * command 321 `X Grandad` -- this was the "and is carrying" bug, now FIXED
    (see Open leads).
- Next candidates down the list, in order: `arlo.taf` (3.70, 11 walks / 85
  commands -- the best pre-4.0 target, and it shows up twice in the killer-walk
  scan), then `goldilocks`, `cibass`, `sophie`/`sa.taf`.  (`arlo` and
  `sophie` are both done as of 2026-08-25 -- `sophie` only for its first fifty
  commands -- so the next two down are `goldilocks` and `cibass`.)

## PARKED 2026-08-24 -- pre-3.9 wording rules, round one done

The pre-3.9 round is finished and committed; the suite is **304/304 PASS**
with all nineteen pre-3.9 rows re-blessed.  The five rules, their P-code
evidence and the retracted empty-prefix inference are written up in the
comment block above the `akron_solution.txt` row in
`harness/run_v4_walkthroughs.sh`, which is the place to read them.  What is
left here is what is still *open*.

### Measured this round (Wine, three new replays)

| game | .taf | Runner | transcript | state |
|---|---|---|---|---|
| arlo.taf | 3.70 | run370 | `Adven_5_arlo.rtf` (Verbose off), `Adven_6_arlo.rtf` (Verbose on) | 6 differing of 84, all NPC-walk payload |
| akron.taf | 3.80 | run380 | `Adven_7_akron.rtf` | **0 differing / 44** |
| mikes.taf | 3.80 | run380 | `Adven_8_mikes.rtf` | 5 differing of 103, all downstream of one desync |

cmdfiles are `~/adrift-battle/runner/wine/cmdfile_{arlo,akron,mikes}.txt`.
akron is the first pre-3.9 game to match the real Runner byte for byte.

### Open leads

- **FIXED 2026-08-31 -- the walk step (move included) is exact-tick-gated.**
  Measured live in run390 on `Merry_Murders.taf` (see the dated section at
  the end of this file) and ported: `npc_tick_npc_walk()` now resolves the
  fixed-room and Hidden destinations only when `counter == suffix_sum`
  (run390 gate `loc_45A780`, `&HFF` hide stamp `loc_45ABB8` inside it;
  run400 `loc_468841`).  The meet-task dispatch and the roomgroup refresh
  stay ungated -- "Ticket to No Where" is the canary that a Times>1
  roomgroup stop *does* re-run every tick, and it still passes.  Corpus
  fallout was 9 rows: 7 re-blessed (their changed lines were the re-drag
  artifacts), `merry_murders` and `thetest_win` re-derived.
- **arlo, `get out of bus` at the church** (cmds 37 and 64): run370 ends with
  the task's "You are no longer in the bus." and prints no exits list;
  scarier prints the exits and drops the task line.  **Diagnosed 2026-08-24,
  deliberately not ported** -- see "DIAGNOSED ... the run370 double matcher
  pass" below.
- **mikes replay desync**, for anyone re-running it: cmd 27 `take truck keys`
  hits a disambiguation prompt ("Which keys.  The mustang keys or the truck
  keys?") that scarier used to resolve silently.  **PORTED 2026-09-04 for
  3.7/3.8** (Scarier now prints the same line, and -- like run380 -- still
  takes the keys: the prompt replaces the turn's OUTPUT only).  Corrected the
  same day: the shift from cmd 53 on is NOT a consequence of 27, it is the
  "auction" event's random 2..10 length; `Adven_1_mikesb.rtf` is identical
  to Scarier through cmd 52.  See "DIAGNOSED ... the Runner's co()
  object-ambiguity test" below and its 2026-09-04 addendum.
- **Humbug via SAVE points** (user's suggestion, untried): checkpoint the
  replay with the Runner's own `save`/`restore` so the three randomised
  secrets -- dial combination, magic word at cmd 209, keypad code at cmd 344
  -- can each be read out of the Runner's transcript and spliced in without
  re-driving 1050 commands after a desync.
- **Two logged-but-unchased humbug divergences:** cmd 217 `Put sweet on
  plinth` (run400 doubles "Okay."), cmd 254 `W` (scarier adds "(Getting off
  the stool first)").  Both are now **CLOSED 2026-08-24** -- the second was a
  display setting, not an engine bug (see the section below), and the first
  turned out to be the whole 4.0 output filter (last section).
- **Next candidates** down the list: `goldilocks`, `cibass` (`sophie`/`sa.taf`
  has since been replayed for its first fifty commands, 2026-08-25).
  With the pre-3.9 pool now clean, the remaining 3.90 and 4.00 candidates are
  where the next divergences will come from.

## DIAGNOSED 2026-08-24 -- the Runner's co() object-ambiguity test (mikes)

The second pre-3.9 divergence, and the only one left in the pre-3.9 pool.
run380 answers mikes cmd 27 `take truck keys` with

    Which keys.  The mustang keys or the truck keys?

and -- correction 2026-09-04 -- DOES take them: the prompt replaces the turn's
output, the task's state changes stand (`Adven_8_mikes.rtf` and
`Adven_1_mikesb.rtf` both list the truck keys in the inventory afterwards).
Scarier bound the truck keys silently and printed the take line, which is
where the replay diverged.  **PORTED 2026-09-04 for 3.7/3.8** -- see the
addendum at the end of this section.

### What the Runner does

Our `%object%` matcher is positional: `uip_match_entity()` walks the pattern
and only considers a candidate that starts where the pattern's `%object%`
starts, so `take truck keys` can only ever bind the truck keys.

The Runner has no positional matcher at all.  `co(obnum)` -- run380 @42DE60,
run370 @4261B4, run390 `co(obnum, mode)` @43B6BC, one routine with the same
shape in all three -- does this instead:

1. Scan the **whole typed command** for the object's Short name; failing
   that, for its Alias.  The scanner is `c(search)` (run380 @429048): a
   case-insensitive `InStr` whose hit must begin at the string start or just
   after a space, and must end at the string end, a space, or a comma.  (No
   retry on a failed trailing boundary -- only a failed *leading* boundary
   loops, @42902F.  And `c("")` is False, because the zero-length hit fails
   every trailing test.)
2. Call whichever of the two matched `term`.
3. Count every object **present** (`obhere`) whose Short **or** Alias is
   *exactly* `term` -- string equality, not containment.
4. If that count is > 1, flag the command ambiguous by stamping the object's
   number into `MemVar_44F124` (@42DDC7).  That flag is read at the very end
   of the turn (@4431B0) and **replaces the whole turn's output** with
   `"Which " & term & ".  " & list & "?"` (@443303; the sibling @4432AA uses
   the Short name instead when the player typed it).  The list is built at
   @42DC1E from every present object with that term, `tense(Prefix) & " " &
   Short`, joined with ", " and " or ".
5. One escape hatch: if the player *also* typed the last word of that
   object's own Prefix -- "take **silver** key" -- @42DD4C stamps the
   resolved marker `&HFE` instead.  That marker outranks any ambiguity raised
   by any other object in the same scan, because @42DDC1 only writes an
   object number when the marker is not already set, while @42DD51 writes
   `&HFE` unconditionally.  So one resolvable object suppresses the prompt
   for the whole command.

mikes has no Prefix on either object -- Short "mustang keys"/Alias "keys" and
Short "truck keys"/Alias "keys", both Prefix "" -- so nothing rescues them.
At cmd 27 both are present (the mustang keys were taken at cmd 8 and are
carried), the mustang-keys object matches on its alias "keys", two present
objects answer to "keys", and the Runner asks.  Cmd 8 `take mustang keys` is
*not* ambiguous only because the truck keys were not yet in scope, which is
the transcript's own control.

### The measurement harness

`SCR_TRACE_CO` was added to `sclibrar.cpp` for this and is worth keeping.  It
reproduces `co()` from the player's raw command (via the new
`run_get_dispatch_input()`) at each `lib_disambiguate_object_common()` call
and prints

    CO-AMBIG verb=[take] input=[take truck keys] term=[keys] present=2 ours=1

whenever the Runner's test fires; `ours=` is our own post-filter reference
count, so `ours=1` is a real divergence and `ours>1` means only the *wording*
of the prompt differs.  It changes no behaviour.  Sweep the corpus with

    while IFS='|' read -r sol taf marker envs; do
      env $envs SCR_TRACE_CO=1 harness/scare "games/$taf" < "goldens/$sol" \
        2>&1 >/dev/null | grep '^CO-AMBIG'
    done < <(grep -E '^[A-Za-z0-9_.-]+\.txt\|' harness/run_v4_walkthroughs.sh)

It is a *lower* bound: a command claimed by a task never reaches the library
disambiguator at all, and the Runner's flag can be set from handlers we do
not model.

### Corpus exposure, measured 2026-08-24

**31 commands in 14 games.**  19 of them are commands our own disambiguator
already calls ambiguous (`ours>1`), so only the prompt's wording differs
there -- and that wording is wrong too: no Runner anywhere contains the
string "Please be more clear" (which is SCARE's invention), and only four
lines in the whole corpus print it (`cybercow` x3, `light_up` x1).  The three
Runners spell it "Which <term> would you like to take/drop/examine.  <list>?"
from the take/drop/examine handlers (run380 @43DE41/@438A8B/@43D2F6) and
"Which <term>.  <list>?" from everywhere else.

Of the remaining 12 genuine divergences, **eleven are in 4.00 games**:

| game | version | commands |
|---|---|---|
| mikes | 3.80 | `take truck keys` |
| mysteryofcaves | 4.00 | `get scammin's ring`, `wear scammin's ring` |
| A_Spot_of_Bother | 4.00 | `get metal bar` |
| ShadricksTravels | 4.00 | `x wood` |
| Witness_Demon_vs_Vampire | 4.00 | `get red bottle` |
| easter | 4.00 | `put egg/eggs/chicks in basket` |
| asteroid_after | 4.00 | `open/close {first..fifth} valve` x6 |

### Why it is not ported (superseded 2026-09-04: PORTED for 3.7/3.8, see the addendum below)

4.00 is exactly the generation where the rule is *not* established.  Objects
carry `[1]$Alias` -- one alias, full stop -- in 3.7, 3.8 and 3.9, which is why
`co()` can read a single struct field 8; a 4.00 object carries `V$Alias`, a
list (see `V400_...` vs `V390_...` in `sctafpar.cpp`).  If a 4.00 object's
*every* alias were a `co()` term, `open second valve` would be ambiguous in
asteroid_after -- six valves, each aliased "valve", each with Prefix "the" so
the escape hatch never fires -- and the game would be unplayable.  That is
good evidence run400 does something else; the obvious candidate, narrowing on
the longest match, is wrong, and the section below has what it really does.
It cannot be read off the decompile: run400 keeps
its messages in a table, so neither `run400.bas` nor `run400.p32dasm.txt`
resolves the "Which " literal, although the literal really is in the binary
(UTF-16LE at file offset 0x17a2c in run400.exe, and likewise at 0x9568 /
0xb270 / 0xee08 in run370/run380/run390).

At 3.7/3.8/3.9, where the rule *is* established, mikes cmd 27 is the corpus'
only divergence -- one row, whose walkthrough would then need re-deriving
(drop the mustang keys before taking the truck keys, presumably; there is no
adjective that would disambiguate).  Porting on that alone would mean
inventing a 4.00 rule.

### MEASURED 2026-08-24 -- run400 narrows, but not by the longest match

`open second valve` in run400 answers normally, so 4.00 does narrow and the
"the game would be unplayable" argument above is void.  What it does *not* do
is prefer the longest matching name.  Twenty-odd probes into a live run400 on
`asteroid_after.taf` (pid kept warm, `look` between probes -- see the first
footgun below) give a two-pass rule:

* **Pass 1 -- Short.**  Any present object whose **Short** appears
  word-bounded anywhere in the command resolves the reference outright.
* **Pass 2 -- aliases, last writer wins.**  If no Short hit, the objects are
  walked in object order and every object that hits *overwrites* the term
  with its own last-hitting alias.  Only the final -- highest-numbered --
  hitting object's term survives, and the count of present objects whose
  Short or alias equals that term decides.

The consequence is brutal, and it is the part no amount of reading the
decompile would have suggested: **an alias that uniquely names an earlier
object is unreachable whenever a later object shares any alias with it.**

| typed | run400 | why, under the rule |
| --- | --- | --- |
| `open second valve` | opens it | pass 1, Short `second valve` |
| `x first valve`, `x fifth valve`, `x sixth valve` | resolves | pass 1 |
| `x valve` | ambiguous | obj5 writes last: term `valve`, present 6 |
| `x valve one`, `x valve 1` | **ambiguous** | obj0 has alias `valve one`, but obj5 writes last and its only hit is `valve` |
| `x valve two`, `x valve 2`, `x valve 3`, `x valve four` | ambiguous | same |
| `x valve five`, `x valve 5` | **ambiguous** | obj4 has alias `valve five`, obj5 still writes last |
| `x valve six`, `x valve 6` | sixth valve | obj5's last hit is `valve six` / `valve 6`, present 1 |
| `x safety`, `x safety valve` | sixth valve | obj5's last hit, present 1 |
| `x sixth`, `x six`, `x 6`, `x 6th` | sixth valve | only obj5 hits at all |
| `x one`, `x first` | first valve | only obj0 hits |
| `x 2nd` | second valve | only obj1 hits |

`x valve five` against `x valve six` is the decisive pair: obj4 and obj5 carry
the identical alias shape (`fifth, valve, 5th, 5, five, valve 5, valve five`
vs `sixth, valve, 6th, 6, six, valve 6, valve six, safety, safety valve`) and
only the *later* one resolves.  That kills every "best match wins" reading.

### MEASURED 2026-08-25 -- run390 is not longest-match either, and it costs stardust the game

The 117-command `S_Tar_Dus.taf` / run390 replay (`Adrift_38_stardust.txt`)
was re-read against today's engine.  Once the Verbose brief-mode headings are
set aside (rule 1 -- that session had Verbose OFF, so every re-entry reads
"You move west.  Open Area." against our full description) the whole
transcript holds **exactly one** engine divergence, and it decides the game:

    turn 38  take needle box
      run390   You've already got the sharp needle!
      scarier  You take a needle box from the desk.

The game has `obj3` Short `needle` Prefix `a sharp` and `obj12` Short
`needle box` Alias `box`, both on the desk, and `take needle` at turn 37 has
already taken the needle.  No task matches `take needle box` (the only needle
task is TASK 4 `put needle in box`), so this is the library's own reference
resolution: run390 resolved `needle box` to the **lower-numbered obj3**, whose
Short is merely *contained* in the command, over obj12, whose Short is the
whole of it.  It never prompts -- only one present object answers to the term
`needle`, so the ambiguity count of step 4 above is 1 -- which makes this a
measurement of the *resolution* order rather than of the prompt.  It is the
3.90 twin of the run400 asteroid_after finding: **neither generation takes the
longest match.**

The cost is the ending.  Without the box:

    turn 99  put needle in box
      run390   You don't have the box.  Put the sharp needle inside what?
      scarier  You put the needle in the box.  Smart move. ...

-- TASK 4's first restriction fails with its own message and the command then
falls through to the library, which is the pre-4.0 "library wins over a
restricted task" rule already pinned elsewhere in this file.  TASK 4 stays
unspent, so the `sw` at turn 116 misses T35's gate and falls through to T36:
the Runner's transcript ends "You step through the portal...  Better luck next
time.", where our golden ends "You decide to go with the plant lady and ...
Well done - you scored maximum points!"  **`stardust` is therefore the first
corpus row where this item changes an OUTCOME and not just a line**, and the
walkthrough's starred WIN is, as things stand, a Scarier-only win.

The repair is known and cheap, which is worth recording now so that the port
is not blocked on re-deriving a route: `take box` -- obj12's own alias, which
no other object answers to -- takes the needle box, and the route still
reaches T35 (verified offline, one `sed` over the solution file).  It is
deliberately NOT applied yet: the walkthrough is only wrong once the engine is
right.

### Two footguns, either of which invalidates a measurement

**run400 has a disambiguation follow-up prompt.**  After `Which ...?` the
*next* line is consumed as the answer, not as a fresh command, so a probe file
of back-to-back ambiguous commands measures nothing at all:

```
x valve one   -> Which valve?  The first valve, ... or the sixth valve?
x valve 2     -> That is still ambiguous!            <- eaten as the ANSWER
x valve three -> Which valve?  ...
x valve 4     -> That is still ambiguous!            <- eaten
```

The alternation is the tell.  Put a neutral `look` between probes.  The answer
is re-joined with the pending verb (`first` examines the first valve,
`sixth valve` the sixth); an answer naming nothing falls through to the
ordinary parser (`xyzzy` gets the XYZZY refusal, and the prompt is dropped);
an answer that is itself ambiguous gets `That is still ambiguous!`, a wrong
one `That wasn't one of the options!`.  Scarier has none of this -- it prints
its message and forgets.

**Read the game's own ALR table before recording any library wording.**
asteroid_after carries `ALR [Which valve.] -> [Which valve?]`, so the live
Runner prints `Which valve?  The first valve, ...` and the raw run400 message
is `Which valve.  The first valve, ...`, with a period.  Measuring the
punctuation off that screen would have recorded the game's rewrite as the
Runner's wording.

### The ALR tables are a free, offline oracle for Runner wording

An ALR's *Original* is the author's own transcription of Runner output, typed
while looking at the real thing.  `scdump.cpp` now dumps them one per line as
`ALR [orig] -> [repl]` under `SCR_DUMP_TASKS`.  Across the 253 v4-era corpus
games, 38,582 ALRs:

| shape | rows |
| --- | --- |
| `Which <term>.  <list>?` | 92 |
| `It is not clear which <term> you are referring to.` | 18 |
| `That is still ambiguous!` | 8 |
| `That wasn't one of the options!` | 1 |
| `Please be more clear...` (ours) | **0** |

Message shape, read off those 92 rows: `"Which " & term & ".  " & list & "?"`
-- two spaces after the period, items `tense(Prefix) & " " & Short` joined
`", "` with `" or "` before the last, first item sentence-capitalised.  An
empty Prefix leaves its space in, which is why asteroid_after transcribed
`Which satellite.   satellite,  satellite or  satellite?` (three spaces) and
Vendetta `Which girl.   girl or  girl?`.

The term is whatever *matched*, not a noun.  `cursed` alone supplies
`Which fallen.`, `Which broken.`, `Which small.`, `Which loose.`,
`Which metal.` and `Which red.` -- adjectives lifted straight out of the alias
lists, exactly as pass 2 predicts, alongside the ordinary
`Which armour.  The silver suit of armour or the gold suit of armour?`.

### The version split, read out of the four exes

VB6 keeps these as UTF-16LE literals, so `strings` misses them; extract with
`re.finditer(rb'(?:[\x20-\x7e]\x00){3,}', exe)`.

| literal | 370 | 380 | 390 | 400 |
| --- | --- | --- | --- | --- |
| `Which ` | yes | yes | yes | yes |
| ` would you like to take.  ` | yes | yes | yes | yes |
| ` would you like to drop.  ` | yes | yes | yes | yes |
| ` would you like to examine.  ` | yes | yes | -- | -- |
| `That wasn't one of the options!` | -- | -- | yes | yes |
| `That is still ambiguous!` | -- | -- | -- | yes |
| `It is not clear which ` + ` referring to.` | -- | -- | -- | yes |
| `It is not clear which object you are referring to.` | -- | -- | -- | yes |
| `Sorry, I'm not sure which object you're referring to.` | -- | -- | -- | yes |
| anything with *who*, *character* or *person* | -- | -- | -- | -- |

So the answer state machine arrives at 3.9 and grows `That is still
ambiguous!` at 4.0; the whole `It is not clear which ...` family is 4.0-only.
`Please be more clear` is in no Runner and in no ALR: SCARE invented it.

### NPCs are disambiguated -- by the same message, and by alias

There is no "who" message in any Runner because characters go through the
*object* builder.  Vendetta's `ALR [Which girl.   girl or  girl?]` is two
**characters** sharing a room: Chloe (room 5, aliases `girl, lady, woman,
female, friend`) and Sally (room 5, aliases `sal, woman, girl, female`), both
with an empty Prefix.  Note what is printed -- the *matched alias*, not the
character's Name -- confirmed independently by the same game's
`ALR [I don't think girl would appreciate being handled.] ->
[I don't think she would appreciate being handled.]`, the Runner's NPC-touch
refusal naming the character "girl".

`lib_disambiguate_npc()` is therefore wrong twice: the wording, and rendering
the Name where the Runner renders the matched alias.  The three
`Please be more clear, who do you want to attack?  Red Riven or Blue Riven?`
lines in `light_up_solution.expected.txt` are both.

### `get scammin's ring` turns out to be a non-test

mysteryofcaves obj9 `Blocker's Ring` (aliases `Blockers Ring`, `ring`) is
behind the boulder door in the hidden grotto; obj10 `Scammin's Ring` (aliases
`Scammins Ring`, `ring`) is in the chest on the island.  They are never in the
same room on the walkthrough's path, so the shared `ring` alias never
collides and nothing narrows.  (That the author supplied apostrophe-free
aliases for both is a separate hint -- that run400 strips the apostrophe out
of typed input.)  The valve set already supplies the co-present case the ring
pair was meant to.

### Why it is *still* not ported -- a better reason than before

run400's disambiguation is **per-handler**, and only two handlers have it
(measured the same day, same live Runner):

| typed | run400 |
| --- | --- |
| `x valve`, `read valve`, `look in valve` | `Which valve.  ...?` |
| `take valve`, `drop valve` | `It is not clear which valve you are referring to.` |
| `open valve`, `close valve` | `You can't open/close that.` |
| `turn/move/sit on/touch valve` | `You can't <verb> that.` |
| `wear valve` | binds the FIRST candidate silently: `You are not holding the first valve.` |
| `push valve`, `pull valve` | `You push/pull, but nothing happens.` |
| `eat valve`, `put valve in X` | `I don't understand...` |

Scarier prints its ambiguity message at roughly 25 call sites.  Matching
run400 means deleting it from most of them, adding the two 4.0 messages and
the follow-up-prompt state machine, and version-splitting the lot against
3.7/3.8's `Which X would you like to examine.`  Every corpus walkthrough
passes today (303/303) precisely because they name objects by their Short.
Left unported deliberately -- but this section is now the specification for
doing it, and nothing above needs Wine again.

### Addendum 2026-09-04 -- PORTED for 3.7/3.8, and why 3.9 is different

The 4.00 argument above still holds for 4.00.  It never applied to 3.7/3.8,
where the rule was measured (mikes) and read off the decompile, so the
pre-3.9 prompt is now in the engine:

* `sclibrar.cpp` `lib_co_ambiguity_prompt()` -- the run380 `co()` scan for
  every object (term = Short if the whole command contains it, else the
  Alias; count the present objects whose Short or Alias equals the term;
  >1 = flagged unless the last word of the object's Prefix was typed, the
  `&HFE` escape hatch), then the 4431B0 end-of-turn test: the flag is only
  honoured when no task ran this turn.  Scarier's `run_note_task_ran()` is
  the task-ran flag (`run_co_task_claimed`), and `run_main_loop()` calls the
  prompt after the tick, after which the whole turn's output is replaced by
  `Which <term>.  <Short list>?` -- exactly as 4431B0 replaces the Runner's.
  State changes stand, the same as in the Runner.
* Gated `prop_get_taf_version() < TAF_VERSION_390`.  The first version of
  the port was global and **regressed six 3.90 goldens** (cybercow_win,
  secret_of_lost_world, the_hangover, troll, hhorror, deardiary).  run390
  really does not do this: its `co(obnum, mode)` @43B6BC is reached ONLY from
  `characters()` (45ACD8: 459436 / 45A025 / 45A0F0 -- give / "... with"
  style character commands) and `sitstand()` (444A04: 44413E / 444430 /
  4445F8 / 444827); there is no per-object loop in generaltasks, the flag
  468190 is only ever set inside co(), and the end-of-turn test at
  4607BC/460832 is guarded the same way (`(468190 < 0) Or (468198 = 1)`).
  run390's takes() 455B34 / drops() 445F20 / wears() 43D298 never call co()
  and print their own "Which X would you like to take/drop.  <list>?"
  (454CAD / 4459BD / 43C9D5) -- those are NOT ported either.  The live
  hangover run390 transcript (`open cabinet` with two cabinets in the room:
  no prompt) is the measurement behind the gate.
* run370 differs in reach, not in rule: `co()` @4261B4 is called from
  `therest()` (43EAA8; per-object loop 43CC5D/43CC86) and `insides()`
  (43B118), and `therest()` only runs when nothing has answered the command
  yet (`If MemVar_4460E4 = vbNullString`, 43C647); the end-of-turn test at
  43C8D3 checks the flag alone.  The 3.8 "no task ran" gate approximates
  that; no 3.70 golden moved.
* Corpus: only mikes cmd 27 moved (life_of_mike golden re-blessed, one
  line).  v4 suite 428/428 after the port.

## DIAGNOSED 2026-08-24 -- the run370 double matcher pass (arlo)

The one remaining pre-3.9 divergence, read out of the run370 p-code and
matched line for line against `Adven_10_arlo.rtf`.  **Understood in full, and
deliberately not ported** -- see "Why it is not ported" at the end.

### What the transcript shows

    > get out of bus                                     (arlo, at the church)
    You're on foot.  You are in front of a gothic wooden church. ...
    There is a mailbox here.  You are no longer in the bus.

One RTF paragraph, one `\par`.  Scarier instead prints

    You're on foot.
    You are in front of a gothic wooden church. ... There is a mailbox here.

    You can move north and east.

so two things differ at once: the Runner *loses the exits sentence* and
*gains a second task's CompleteText*.  Both come from one mechanism.

### The mechanism

`generaltasks` runs, in this order (run370 @0003B935 onwards):

    takes()  drops()  inventory()  tasks(0)  wears()  removes()
    ... insides() sitstand() openclose() ... moves(playerroom) ... examines()

`takes()` is entered whenever the command contains `get`, `take`, `pick` or
the game's own take-verb **and does not contain `from`** (@00035D8C-35E28).
Its object scan then walks every object and, on the *first* one whose Short
name or an Alias appears in the command, calls the task matcher with
**mode 1 and the player's original command still in place** and `Exit For`
(@00036CA2-36CB5).  arlo's object 29 is `microbus` with the alias **`bus`**,
so `get out of bus` reaches that call.

The crucial bit: **`takes()` never stores a return value.**  Its p-code opens
with `ZeroRetValVar` and there is no store to the result slot anywhere in the
body -- the `takes = MemVar_4460E4` that VB Decompiler prints at
`loc_436D17` is its rendering of the `ExitProcCbHresult` opcode, not a
statement.  So `takes()` always returns Empty, `CBoolVarNull` is False, and
`generaltasks` **falls through to `tasks(0)` anyway**.  The matcher therefore
runs a second time on the same command, this time in **mode 0**.

Mode is the whole story for the buffer (@00041B90):

    If mode = 1 Then  buffer = <buffer as at this call's entry> & CompleteText
    Else              buffer = CompleteText            ' CLOBBER

So in arlo:

1. `takes()` -> matcher(mode 1) -> **task 72** (`get out of *bus*`,
   Where = room 7 = the bus, Repeatable) completes.  Buffer becomes
   "You're on foot.", then its `ShowRoomDesc = 1` runs `viewroom(room 0)`.
2. `viewroom`'s exits block (@000330FF) sets `command = "exits"`, calls
   `moves(broom)` -- which *replaces* the buffer with the exits sentence --
   and then, because the answer is not "... any direction!", **prints
   everything accumulated so far immediately and with no newline**
   (`Sub_3_27(saved & "  ")`, @0003315C) and leaves the buffer holding
   **only** "You can move north and east."
3. Task 72's Movements then put the player in room 0.
4. `tasks(0)` runs the matcher again.  Task 72's Where gate now fails (the
   player is no longer in the bus); **task 107** -- same five patterns,
   Where = room 0, CompleteText "You are no longer in the bus." -- matches
   instead, and mode 0 **clobbers** the buffer.  The exits sentence is
   destroyed before it is ever flushed.
5. The turn's final flush prints the clobbered buffer plus a newline.

Net screen text: the accumulated prefix (already printed in step 2) followed
by task 107's line, in one paragraph, with no exits.  Exactly the transcript.

Every other `get out of bus` in the same transcript corroborates it:

* at the Dump and at the Parking area the second pass finds nothing (task 107
  needs room 0), so only the first task's text survives -- and the exits are
  still missing there for the *other* reason, `viewroom` running before the
  Movements;
* `take garbage` (task 95, structurally identical to task 72, SRD room 0,
  Movements to room 0) keeps "You can move north and east." because nothing
  matches on the second pass;
* probing `get out of bus` while already on foot at the church prints
  "You are no longer in the bus." alone.

### Why it is not ported

Scarier's turn is "matcher once, then the library".  Reproducing this would
mean running the matcher a second time for every take-family command and
giving the second run clobber-the-buffer semantics that scarier's filter has
no equivalent for.  The preconditions are narrow -- a `get`/`take`/`pick`
command with no `from`, naming an object, matching a **repeatable** task whose
own effects then make a **different** task match -- and arlo is the only game
in a 303-row corpus that hits them.  The golden keeps scarier's single-pass
output; the row in `harness/run_v4_walkthroughs.sh` carries the measurement.

## REFERENCE -- run370 facts established while chasing arlo

Recorded here because two of them reverse working models earlier sessions
were built on.

- **The .bas decompile silently DROPS statements.**  Proven twice above and
  once more below.  `run370.bas` has neither the `var_A4 = 0` at
  `00040B7A` nor the whole `If var_A4 = 0 Then` gate at `00040BE5`, and it
  invents a `takes = MemVar_4460E4` where the p-code has only `ExitProc`.
  It also mis-attributes `Left()`/`Right()`/`InStr()` arguments and prints
  `For i = 0 To 0` where the real limit is a variable.  **Always confirm
  against `run370.p32dasm.txt`** (addresses there are VA - 0x400000; use
  `LC_ALL=C`, and find line numbers with `grep -n '^0004...'` rather than
  `sed -n '/^ADDR:/,/^ADDR:/p'`, which silently matches nothing).
- **One ordinary task per matcher call -- `var_A4` is the latch.**  Zeroed
  once *before* the outer task loop (`00040B7A`), tested at the top of every
  pattern iteration (`00040BE5`, `BranchF 00040ED7` when set), set to 1 the
  instant any task completes (`00041DE7`).  The single exception is the
  landing site itself: a `&&` ("always") pattern with mode 0 and an empty
  entry buffer still matches at `00040ED7`, so `&&` tasks are not latched
  out.  arlo has none.  **This retires the "exhaustive task loop" model**
  that earlier sessions inferred from the .bas, and with it the parked
  multi-task patch -- scarier's existing one-task-per-call behaviour is
  correct.  The two CompleteTexts in arlo come from two *calls*, not one.
- **Room-number offsets.**  `playerroom` is 1-based (scarier room `r` is
  `r+1`), and the rooms array `MemVar_446008` is indexed by that 1-based
  value, while `roombitmap` is indexed by the scarier index.  In a task,
  **`ShowRoomDesc` = scarier room + 1** and a Movement's **`Var2` = scarier
  room + 3**; a Movement moves the player when `Var1 = 1 And Var2 > 1`.
- **`viewroom`'s exits block** (`000330FF`) is gated by a game-header byte,
  `MemVar_44613D`, read at load (`0003F313`) -- game-wide "show exits", not a
  per-call flag.  When on, it stashes the buffer, calls `moves(broom)` (which
  *overwrites* the buffer with the exits sentence), then either restores the
  stash and prints nothing at all (answer ends "any direction!") or prints
  the stash immediately without a newline and leaves the exits sentence in
  the buffer for the turn's final flush.  Note the consequence: after a
  successful `viewroom`, the buffer contains **only** the exits sentence.
- **`moves()` counts the exits of `broom`, not of the player's room**
  (`00033F01`), and counts an exit when its task gate is 0 or when
  `tasks(gate-1).done = 1 - flag`.
- **`tasks(0)` returns the endgame flag**, so `generaltasks` normally *falls
  through* it to `wears`/`removes`/`insides`/`sitstand`/`openclose`/
  `moves(playerroom)`/`examines` before the `characters()` + `events()` tail
  -- those are not alternative branches.
- **`checktask(text)` is a pure predicate** ("would a task matching this text
  pass its restrictions").  It never executes a task.
- **Only ten call sites reach the matcher**: `characters()` x2 (CharTask,
  ObjectTask), `generaltasks` x1 (**the only mode-0 call**), `takes()` x3,
  `drops()` x3, `events()` x1.  Eight of them substitute
  `tasks(N-1).Command[0]` for the command first; the two that do not are
  `takes()` `@00036CAD` and `drops()` `@00030D38`, which re-match the
  player's original words.
- **Retraction: `break *garbage*` does match `break garbage with implement of
  destruction`.**  An earlier session used that command as a single-task
  probe; it never was one.

## CLOSED 2026-08-24 -- `%in_<obj>%` / `%on_<obj>%` listing format

`scvars.cpp` printed the contents of a container or surface named by
`%in_X%` / `%on_X%` / `%onin_X%` in the postfixed form unconditionally --
"A tub of butter, a butter knife and a bottle of milk are inside the fridge."
The library listers in `sclibrar.cpp` have selected the format **by content
count** since the 3.9 wording round, because run400 lists container and
surface contents from a single routine at 0006A418 that counts first and then
branches: `0006A49E` (count == 1) -> "`<obj>` is inside `<cont>`.";
`0006A607` (count == 2) -> "`<a>` and `<b>` are inside `<cont>`.";
`0006A786` otherwise -> "Inside `<cont>` is `<list>`."  Before
TAF_VERSION_390 only the prefixed form exists.  The variables took a
different path and missed all of it.

Measured in run400's `Adrift_23_where_are_my_keys.txt` (WhereAreMyKeys.taf, 4.00):

```
open fridge
You open the fridge and the light comes on.  Well that's something. Inside
the fridge is a tub of butter, a butter knife and a bottle of milk.
```

-- task CompleteText is `"...  Well that's something. %in_fridge%"`, three
objects inside, so the prefixed form.  The two-object control is in the same
transcript and keeps the postfixed form:

```
open unit
A large knife and a jar of coffee are inside the kitchen unit.
```

so this is the count selector, not a blanket rewording.  Fixed with
`var_use_alternate_format()` in `scvars.cpp`, shared by all three variables.
Corpus exposure measured: 18 games use any of the three, `%onin_%` only
`WhereAreMyKeys` and `door`.  Exactly one golden line pair moved corpus-wide;
**303/303 PASS**.

The nested case was deliberately left alone here: when an object is both *on*
and *in* the associate, run400 reaches the same lister with `var_9E == 1` and
prints a prefixed ", and inside is `<list>`", which scarier did not model.
**Ported 2026-08-25** off the xfiles replay, which does exercise it -- see
"what is ON an object is listed before what is IN it" below.

**FIXED 2026-08-25 -- and the rule is narrower than it looked.**  run400 was
seen to print "switched off" and "switch in the on position" where Scarier
capitalised them, but only first-character evidence existed, so `LCase` over
the whole string could not be told apart from lowering the first letter -- and
the corpus has states where the difference is destructive: `in the UP
position` (TheADRIFTProject), `facing South` (The_Hunter), `Sur la gauche`
(Les Feux de l'enfer), `R1..R7` (Oh_Human), `Locked off` (baroo).

`harness/make_400_stateprobe.py` -> `p4STATE.taf` settles it: six stateful
objects in one room carrying exactly those destructive shapes plus `switched
off` as the already-lower-case control, each read through all three callers --
`x <obj>` (the examine lister, `BStateListed` on), `ob <obj>`
(`OB=[%obstate%]`) and `st <obj>` (`ST=[%state_<obj>%]`) -- plus `mid <obj>`,
which puts `%state_<obj>%` mid-sentence in case only a line-opening name is
folded.  `flip` then moves the lever to `In the DOWN Position` so all four
reads repeat on a state the game switched to rather than started in.
`Adrift_1_p4state.txt`, all 29 commands echoed:

| States entry | `x <obj>` | `%obstate%` | `%state_<obj>%` | mid-sentence |
| --- | --- | --- | --- | --- |
| `In the UP position` | `In the UP position` | `In the UP position` | `in the up position` | `in the up position` |
| `Facing South` | `Facing South` | `Facing South` | `facing south` | `facing south` |
| `Locked Off` | `Locked Off` | `Locked Off` | `locked off` | `locked off` |
| `R1` | `R1` | `R1` | `r1` | `r1` |
| `Sur la gauche` | `Sur la gauche` | `Sur la gauche` | `sur la gauche` | `sur la gauche` |
| `switched off` | `switched off` | `switched off` | `switched off` | `switched off` |
| `In the DOWN Position` (after `flip`) | `In the DOWN Position` | `In the DOWN Position` | `in the down position` | `in the down position` |

So **only `%state_<obj>%` is folded, and it is folded whole** -- "UP" and "R1"
both lose their capitals, and it happens mid-sentence too.  The examine lister
and `%obstate%` print the States entry verbatim, in the same transcript, on
the same objects.  `obj_state_name()` in `scobjcts.cpp` is therefore NOT the
place to change: the fold went into the `state_` branch of `scvars.cpp`, which
is the only one of its three callers that wants it.

Confirmed a second way on a real game: `where_are_my_keys_solution.txt`
re-blessed, three lines, and all three are printed by
`Adrift_23_where_are_my_keys.txt` itself -- "a fuse box with a single switch
in the on position" (lines 34 and 50) and "the cooker appears to be switched
off." (line 64).  Corpus 303 PASS.

## CLOSED 2026-08-24 -- the not-a-room-zero arrival gate

The residual left open by the walk-announcement round below, closed the same
day.  20 goldens across 20 games re-blessed, corpus **303/303 PASS**.  Full
write-up in the comment block above the `stardust` row in
`harness/run_v4_walkthroughs.sh`.

3.8/3.9/4.0 gate a walker's arrival line on its **pre-move** location not
being the Runner's never-placed zero (run400 @468A64 is the whole test:
ShowEnterExit AND `old <> playerroom` AND `old <> 0`; run380 @4416F4 and
run390 `loc_45A99B` the same shape).  **3.7 has no such test** (run370
@43955E) -- and indeed no 3.7 row moved.

The Runner spells "not a room" two ways and only one suppresses: `0` for an
NPC the game never placed, `&HFF` for one a walk's Hidden stop just hid
(run400 `loc_468D4A`), the latter still printing a directionless arrival.
Scarier stored both as location 0, so `scr_npcstate_t` gained a `walk_hidden`
flag -- cleared by `gs_set_npc_location()`, set by `npc_tick_npc_walk()` right
after a Hidden stamp.  Deliberately **not** in the `.tas` stream: the Runner's
own save writes a room byte in `0..NumRooms`, so a restored hidden walker
reads back as never-placed at either engine.

Measured live at three generations (3.7 exempt, so none needed):

| Game | Runner | Transcript | What it showed |
|---|---|---|---|
| `tra.taf` | run380 | `Adven_9_timmy_reid.rtf` | no "Sting walks towards you." -- but "Canadian couple walks towards you from the north." *is* there, so the gate is the zero, not the walk |
| `S_Tar_Dus.taf` | run390 | `Adrift_38_stardust.txt` | full 117-command replay: **all 129 walk lines match count for count** across four walkers and six directions, and the bare "Plant Lady prances along." is absent from the Runner while its four directional siblings are in both |
| `Orient_Express.taf` | run400 | `Adrift_36_orient_express.txt` | "Gimme Atip enters." is printed by Scarier and by no Runner -- the divergence that started the item |

Everything removed is a first-ever arrival of a never-placed NPC, one to three
lines per game, and **no game lost a directional arrival** -- a useful shape
check if this is ever revisited.

## CLOSED 2026-08-24 -- NPC walk announcements, all four generations

`scnpcs.cpp`'s walk departure/arrival lines rewritten against the Runner's own
`wherefrom()` (run370 @422F8C, run380 @42800C, run390 @430200, run400
`Proc_19_20` @45234C -- one routine, unchanged across all four).  Twenty
goldens across nineteen games re-blessed, corpus back to **303/303 PASS**.
The canonical write-up is the comment block above the `alices_restaurant` row
in `harness/run_v4_walkthroughs.sh`; the short version:

- The direction names *the other room as seen from the player's*: scan the
  other room's exits for the player's room and print that exit's **opposite**,
  **last match winning** (there is no early break).  SCARE scanned the
  player's room forward and took the first match, which agrees only on a
  symmetric map.
- `EightPointCompass` is never consulted.  Pre-4.0 scans exits 0..7, 4.0 scans
  0..11, so a diagonal move is nameless before 4.0.
- Departure suppression is version-split: **3.7** suppresses only "nowhere"
  (so "Alice walks off to not moved." really does print, twice, in arlo);
  **3.8**/**3.9** suppress both; **4.0** suppresses only "not moved" and
  prints a bare "X walks off." on "nowhere".
- `"outside"` loses the "to" everywhere: "walks off outside." (run380
  @0004160D, run390 `loc_45A840`, run400 `loc_46891E`).
- A **follow-player stop is announced by no Runner**; a **hidden stop** gets a
  directionless, resource-less line in **3.7 and 4.0 only** (run370
  `loc_4397A3`, run400 `loc_468CF9` -- run380 `loc_4418DD` and run390 have
  nothing there but the "stamp the NPC nowhere" assignment).
- Both lines fire only on the exact tick, so a multi-turn stay is announced
  once.

Measured live under Wine, one game per generation:

| Game | Runner | Transcript | What it pinned |
|---|---|---|---|
| `arlo.taf` | run370 | `Adven_6_arlo.rtf` | all three 3.7 departure lines; arlo down to **3 differing of 85** |
| `tra.taf` | run380 | `Adven_9_timmy_reid.rtf` | "Hovey shuffles off outside." -- no "to"; the old golden was wrong |
| `Melbourne Beach.taf` | run390 | `Adrift_37_melbourne_beach.txt` | all four changed sites, incl. the dropped diagonal ("David strolls in.") |
| `Orient_Express.taf` | run400 | `Adrift_36_orient_express.txt` | every 4.0 walk line, incl. "...walks off to the west." and "...wobbles in from the east." |

At 3.9 only the NPC *verb text* ever differed from the Runner (the walk's
random alternate texts), never the direction.

Two residual items came out of this.  The not-a-room-zero **arrival** gate is
closed above, the same day.  The fact that the walk **move** and meet-task
dispatch still sit outside the exact-tick test is still an open lead.

## FIXED 2026-08-25 -- the walk announcement is JOINED into the turn's paragraph

The Runner does not give an NPC walk announcement a line of its own.  It
appends it to the buffer the turn has built so far, with the same two-space
section separator every other run-on uses -- so the announcement is part of
the turn's ONE paragraph, and an author's ALR pass sees the join.  Scarier
started the line instead.  **61 goldens re-blessed, corpus 304/304 PASS.**

### The separator, and its version split

3.9 and 4.0 call the shared "pspace" routine (run390 `loc_45A99E`, run400
`loc_468A67` arrival / `loc_4688D3` departure).  3.7 and 3.8 write it inline
and shorter:

    run370 loc_4395AA / run380 loc_441740
        If Right(buf, 1) <> Chr(10) And Len(buf) > 0 Then buf = buf & "  "

which tests only the LAST character, so a buffer already ending in two spaces
gets two more.  Pre-3.9 really can make four spaces where 3.9/4.0 make two.
Ported as `pf_buffer_join()` (pspace) and `pf_buffer_join_always()` (the
pre-3.9 inline form) in `scprintf.cpp`, split at `TAF_VERSION_390` in
`npc_announce()`.

### The Name is capitalised in 4.0 ONLY

`Proc_21_3_446BB4` (run400 `General.bas:75`) is
`UCase(Left(s,1)) & Right(s, Len(s)-1)`, and run400 pipes the NPC Name
through it at **both** `npc_announce()` sites -- `loc_468A79` (arrival) and
`loc_4688E0` (departure).  It is called at neither site in run370
(`loc_43961A`), run380 (`loc_4417B0`) or run390 (`loc_45AA0F`), which push the
raw Name.  Nor at run400's **hidden**-departure site `loc_468CF9`, which
appends a bare `"  "` with no pspace call and no capitalisation at all (as
does run370 `loc_4397A3`).  So: capitalise in 4.0's two `npc_announce()`
sites, nowhere else.

The corpus case is `baroo.taf` (4.00), whose NPC Names are lower-case
"wizard" and "warlock": the golden reads "Wizard walks off ...".  Gated on
`is_400` in `npc_announce()`.

### Why it matters beyond whitespace: the ALRs span the join

Because the announcement lands in the same buffer, an author who wants to
reword a walker writes an ALR whose Original starts with the separator.
`sa.taf` (sophie, 4.00) has **65** such join-spanning ALRs, **12** of which
fire in the walkthrough -- and each one *deletes* the arrival it matches.
That is the whole substance of the two sophie goldens' 12 lost arrival lines;
every one was matched mechanically back to an ALR Original in the table
(count at 0-based plain-text line 255954 of `taf_pattern_scan.plaintext()`'s
body, pairs on the lines after it).  `circus.taf` corroborates from the other
direction with `'  Joe' -> '  The vendor'`, which only fires once the two
spaces are there.

### What was measured, and where

Runner transcripts confirm the join on **every** generation:

| Game | Runner | Transcript |
|---|---|---|
| `arlo.taf` | run370 | `Adven_6_arlo.rtf` |
| `tra.taf` | run380 | `Adven_9_timmy_reid.rtf` |
| `Melbourne Beach.taf`, `S_Tar_Dus.taf` | run390 | `Adrift_37_melbourne_beach.txt`, `Adrift_38_stardust.txt` |
| `Orient_Express.taf` | run400 | `Adrift_36_orient_express.txt` |

(The melbourne_beach "Kitty comes in" / golden "Kitty saunters in" mismatch is
the walk's random alternate verb texts, already documented on the arlo row --
the direction and the join match.)

The join itself was additionally pinned by a built probe, `p4WALKALR.taf`
(`harness/make_400_walkalrprobe.py`, transcript `Adrift_47_p4walkalr.txt`).

Of the 61 goldens this moved, **57 differ from their predecessors in
whitespace alone**; the other four are `baroo` (the capital), `circus` (the
`'  Joe'` ALR) and the two `sophie` rows (the 12 deleted arrivals).  The
canonical write-up is the dated block above the `arlo.taf` row in
`harness/run_v4_walkthroughs.sh`, with pointer comments on the `circus`,
`sophie`, `sophie_comp` and `baroo` rows.

### Confirmed live 2026-08-25

`harness/make_400_walkcapprobe.py` / `p4WALKCAP.taf` ran under run400
(`Adrift_1_p4walkcap.txt`, all four commands echoed).  It packs two rooms and
one NPC whose Name on disk is the lower-case `"bob"`, and four cells: `e`/`w`
(the announcement joined mid-paragraph onto the exits sentence) and `pb`/`pa`
(a task whose CompleteText ends in `<br><br>`, so pspace adds nothing and the
Name opens a line).  run400 prints **`Bob` in all four** --

    e    The second room.  You can only move west.  Bob wanders in from the west.
    pb   PB.
         (blank)
         Bob wanders in from the west.

-- so the capitalisation is not positional, and the listings' reading (4.0
capitalises at both `npc_announce()` sites, 3.7/3.8/3.9 never do) is right as
written.  Scarier reproduces all four cells verbatim; no change came out of it.

Two harness facts fell out of the failed attempts, both worth keeping:

- **`measure.sh` now forces Verbose ON in the registry**, not with a blind
  Ctrl+V toggle.  `pfx/user.reg` carries `"Verbose"="True"` under
  `VB and VBA Program Settings\ADRIFT\Runner` and it *survives* launches, so
  the old blind toggle flipped a Verbose-ON prefix OFF and made every room
  re-entry look like a divergence (`run400-verbose-toggle` says the value
  resets each launch; it does not).  Decide whether to mirror the
  `~/adrift-battle/runner/wine/measure.sh` changes back into the repo.
- A locked screen fails in a way that looks like a game problem: three runs
  died with "WARNING: 3 pause-dismiss Return(s) sent" / "Save-transcript
  dialog never appeared", and the known-good `p4WALKALR.taf` failed
  identically.  The tell is elsewhere: `swift winlist.swift` shows
  `loginwindow 30000x30000`, `osascript` returns `-1719 Can't get process 1
  whose frontmost = true`, and `screencapture -R`/`-l` both refuse.

## FIXED 2026-08-25 -- a "The" prefix keeps its capital

The X-Files: A New Beginning gives its Memo the Prefix `The` (straight out of
`SCR_DUMP_TASKS=1`: `OBJNAME obj=20 [Memo] prefix=[The]`).  run400 prints it
back with the capital intact, everywhere -- the surface clause of an examine,
the take-all list, and the last-resort refusal:

    x desk
    ... Your Coffee Mug and The Memo are on Your Desk, and inside is ...

    take all from desk
    You take Your Coffee Mug, ... Your Badge and The Memo from Your Desk.

    burn memo
    I don't understand what you want me to do with The Memo.

Scarier printed `the Memo` in the first two of those, because
`lib_print_object_np()` carried a `the` branch alongside its `a`/`an`/`some`
ones and re-emitted the article in lower case.

**The Runner has no such branch.**  Its normalizer is `tense`
(`Proc_21_13_44F474` @44F474), reached from the name builder
`Proc_21_31_448710` in its normalizing mode 0 -- and note that `tense` is
handed `Prefix & " " & Short`, the whole thing, not the prefix alone.  It
tests exactly six things and returns its argument untouched otherwise:

    = "a"        -> "the"
    = "an"       -> "the"
    = "some"     -> "the"
    Left(s,2) = "a "     -> "the " & Right(s, Len(s) - 2)
    Left(s,3) = "an "    -> "the " & Right(s, Len(s) - 3)
    Left(s,5) = "some "  -> "the " & Right(s, Len(s) - 5)

`"The Memo"` matches none of them.  Pre-3.9's `tense` (run370 @420F28, run380
@425FA8, byte-identical) is the same shape with the two `some` tests missing,
so it does not rewrite `the` either -- and that branch of
`lib_print_object_np()` was already right.  3.9 is bracketed rather than read:
the run390 decompilation does not reach its normalizer, but both neighbours
leave `the` alone and the only thing 3.9 is known to have added to `tense` is
the `some` pair.

The fix is a deletion.  Falling through leaves the prefix in `normalized`,
which the tail of the function prints verbatim followed by a space, so the
branch only ever changed the author's capital -- `the foo` and `The foo` came
out of it identically apart from that one letter.

Four goldens moved, in both affected generations:

| golden | version | line |
|---|---|---|
| `xfiles_solution` | 4.00 | `... Your Badge and The Memo from Your Desk.`, `You take The Warehouse Key from Case File 10193.` |
| `cyber2_solution` | 4.00 | `You open The Teleporter.` |
| `afdfr_solution` | 4.00 | `You take The Grim Reaper's Scythe.` |
| `spirits_flight_solution` | 3.90 | nine lines -- `The Spirit Dagger`, `The Orb of Storms`, `The Amber of Flames` |

303/303 after re-blessing.

**Follow-up, settled from the P-code and re-blessed 2026-08-25.**
`lib_print_object_np()` also stripped a leading `a`/`an`/`the`/`some` off the
object's **Short name**, inherited from SCARE on the grounds that "some games
may avoid prefix and do this instead".  No Runner can do that.  The whole path
is readable end to end:

- **The object loader normalizes both fields as it reads them.**  run400
  `loc_4900E3` (`mdlSpreadTheLoad.bas:7594`) `LineInput`s the Prefix,
  substitutes a literal `"a"` for an empty one (`loc_4900EC`), then loops
  stripping trailing spaces (`loc_490100`..`loc_49015C`).  It `LineInput`s the
  Short at `loc_49016C` and loops stripping **leading** spaces
  (`loc_490170`..`loc_4901CC`), then moves straight on to the alias count.
  Nothing anywhere looks at an article.
- **The name is built in one place**, `Proc_21_31_448710`
  (`General.bas:7041`, 232 call sites), as the single string
  `Prefix & " " & Short` (`loc_4486B7`..`loc_4486CF`).
- **tense** (`Proc_21_13_44F474`, `General.bas:1728`) tests **exactly six
  things**: the whole string against `"a"`, `"an"` and `"some"`, and
  `Left(s,2)`/`Left(s,3)`/`Left(s,5)` against `"a "`, `"an "` and `"some "`.
  Everything else comes back untouched.  Callers tense the result a *second*
  time (`Battles.bas:261` then `:265`), which changes nothing.

So the only characters ever inspected are at the head of the concatenation,
and after the loader's substitution the head is **always** the prefix.  An
object with Prefix `The` and Short `the Memo` comes out of run400 as
`The the Memo`.

That loader substitution also settles the empty-prefix question that this note
used to leave open, and corrects a claim in `sclibrar.cpp`: 4.0 was said to
have no `"a"` substitution and to default in the printers instead.  It has
one, at `loc_4900EC`, exactly like run370 `@43F5DA` and run380 `@4481B2`.  It
is simply invisible, because scarier's own two defaults -- `"the "` in
`lib_print_object_np()` and `"a "` in `lib_print_object()` -- reproduce its
effect.  It is what makes run400 answer `coger fenix` in La hija del relojero
with "You take **the** Fenix de laton de el cajon." for an object whose Prefix
really is empty (`OBJNAME obj=6 [Fenix de laton] prefix=[]`): the loader makes
it `a`, and `tense("a Fenix de laton")` is `the Fenix de laton`.  The one
place the default was *missing* was `lib_print_object_raw()`, the pre-3.9
`remove` wording, which concatenated the raw prefix and would have opened its
message with a stray space; it now defaults to `a` as well.

**Four corpus lines move, and they are the proof.**  `Shadowpeak.taf` (4.00)
is the only game in the corpus that reaches this: two objects with an empty
Prefix whose Short opens with an article, `[The horn of the angels]` and
`[The dead Margo]`.  scarier used to print

    You take the  horn of the angels.
    I don't understand what you want me to do with the  dead Margo.

-- with the tell-tale double space left where the stripped `The` had been.
The Runner prints `the The horn of the angels`, and the same shape is
confirmed live in the xfiles replay, where a `The` **prefix** survives as
`The Memo`.  Three goldens re-blessed, corpus **303/303**.

### The loader's whitespace trims, same reading

Reading that loader out also settled a second, smaller thing.  Having
LineInput'd Prefix and substituted `"a"` for an empty one, run400 loops

    loc_490100..loc_49015C:  If Right(s, 1) = " " Then s = Left(s, Len(s) - 1)

stripping every **trailing** space from the prefix; having then LineInput'd
Short, it loops

    loc_490170..loc_4901CC:  If Left(s, 1) = " " Then s = Right(s, Len(s) - 1)

stripping every **leading** space from the short name.  Only those two, and
only in those two directions: a Short written `pictures ` keeps its trailing
space, and the aliases that follow are never touched.  run370 (@43F5DA) and
run380 (@4481B2) carry the same pair of loops, so it is not version-split.

Contrary to what this file said an hour ago, the corpus *does* exercise it --
fourteen objects across nine games, found by dumping every game's `OBJNAME`
lines and looking for a bracketed field that opens or closes with a space:

    Crime_Adventure.taf  obj 0 `[an arcade token ]`, obj 17/20 `[a ]`
    arlo.taf             obj 22 `[the ]`
    first.taf            obj 9 `[fresh ]`, obj 10/12 `[old ]`
    superliam.taf        obj 0 `[red ]`
    tra.taf              obj 6 `[the ]`
    ADRIFTMAS_Party.taf  obj 24 `[ bathroom door]`, 85 `[ rack]`, 154 `[ potted plant]`
    hhorror.taf          obj 51 `[ floorboards]`
    marooned.taf         obj 15 `[ trash]`

Every one of them printed as a **double space** where the name is joined:
`Also here is a pile of  trash.`, `and a  cookery book.`, `Also here is fresh
bread and fresh  turkey.`, `Also here are the  floorboards.`  The trim is now
done in `parse_trim_object_names()`, at parse time rather than in the printers,
because in the Runner it happens in the loader and so the noun matcher sees the
trimmed text as well.  Four goldens re-blessed, corpus **303/303**.

## FIXED 2026-08-25 -- what is ON an object is listed before what is IN it

`x desk`, `Adrift_22_xfiles.txt` line 9:

    run400   Your Desk is open.  Your Coffee Mug and The Memo are on Your
             Desk, and inside is Gun Holster, Your Cell Phone, Neatly Wrapped
             Gift and Your Badge.
    scarier  Your Desk is open.  Inside Your Desk is Gun Holster, Your Cell
             Phone, Neatly Wrapped Gift and Your Badge.  Your Coffee Mug and
             The Memo are on Your Desk.

Two differences in one line: the **order** (surface first, not container
first) and the **join** (one sentence, not two, and the container is not
named the second time).

The Runner does not have a container lister and a surface lister the way
SCARE does.  It has one combined helper, **`whatisinon`**,
`Proc_19_26_46A950` @46A950 (`run400/Project/mdlSpreadTheLoad.bas:21880`, body
46A058-46A94A), and its second argument is a mode:

| guard | half | at |
| --- | --- | --- |
| `arg_14 <> 0` | the ON list | `loc_46A083` |
| `arg_14 <> 1` | the IN list | `loc_46A41E` |

so mode 0 is containers only, mode 1 surfaces only, and mode 2 both.  All four
callers, and what they pass:

| caller | mode | |
| --- | --- | --- |
| `openclose` `Proc_19_3_476468` | 0 | @475852 |
| the room lister, `General.bas` | 0 | @479919 |
| `inventory` `Proc_19_70_45C304` | **2** | @45C2C8 |
| `examines` `Proc_19_87_471F94` | **2** | @471928 |

That is exactly why `open desk`, one command earlier in the same transcript,
already agreed byte for byte (`You open Your Desk.  Inside Your Desk is ...`):
the open path never sees the surface at all.  Only examine and inventory
combine.

The join is a flag, `var_9E`, set to 1 once the ON list has printed something.
The IN half tests it **first**, before any format choice (`loc_46A786`):

    loc_46A786:  If var_9E = 1 Then
    loc_46A795:      MemVar & ", and inside is "
    loc_46A79D:      GoTo loc_46A7E0            ' the plain list loop

-- no container name, no `pspace`, no new sentence.  The single closing `.`
is appended once at the very end of the sub (`loc_46A8C6`, and only if
anything was added at all), which is why the ON clause carries no period of
its own when an IN clause follows it.

**The count-1 and count-2 arms are unreachable when a surface listed.**  The
`"<a> is inside <cont>"` branch is guarded `var_98 = 1 And var_9E = 0`
(`loc_46A49E`-`loc_46A4AE`) and the `"<a> and <b> are inside <cont>"` branch
`var_98 = 2 And var_9E = 0` (`loc_46A607`-`loc_46A617`).  Each of them then
contains an inner `If var_9E = 1` arm (`loc_46A4F1`, `loc_46A66C`) that can
never run -- leftovers of the VB source.  Taken literally, a surface listing
forces `", and inside is <list>"` **whatever the in-count**, and that is what
is implemented.

Ported in `sclibrar.cpp` as `lib_list_in_on_object()`, with
`lib_list_in_object_joined()` for the joined wording; `lib_list_on_object()`
gained an "omit the period" argument and `lib_list_in_object()` a "joined"
one.  Both mode-2 call sites now go through it -- `lib_describe_object()` and
the inventory loop -- while the open handler keeps calling
`lib_list_in_object()` directly, as run400's mode 0 does.

**Pre-3.9 is excluded.**  There is no combined lister there at all: run380 has
`whatisin1` @4297AC and `whatisin2` @42998C as separate subs, and its
`examines` @43D5EC carries its listing inline as an either/or on one field --
`loc_43D07A` prints `"  Inside <obj>"` when it is 1, `loc_43D0D0` prints
`"  On <obj>"` when it is 2, never both.  And the literal `", and inside is "`
is absent from `run370.exe` and `run380.exe`, appearing first in `run390.exe`
-- the same boundary as `" is inside "` and `" is on "` (counted in the four
binaries as UTF-16LE, 2026-08-25).  So a pre-3.9 game keeps the older
container-then-surface pair of sentences.

Three goldens move, corpus back to **303/303 PASS**:

| golden | in-count | new wording |
| --- | --- | --- |
| `xfiles_solution` | 4 | `... are on Your Desk, and inside is Gun Holster, ...` |
| `ADRIFTMAS_Party_solution` | 2 | `The suitcase is on the wardrobe, and inside is a leather jacket and an assortment of shoes.` |
| `yonastoundingcastle_solution` | 1 | `Ye olde desk clutter is on ye alchymist's desk, and inside is ye magic crystal.` |

### MEASURED 2026-08-25 -- the unreachable arms really are unreachable

The other two goldens rested on the disassembly alone, so `p4INON.taf`
(`harness/make_400_inonprobe.py`) was built to settle them: nine cells in one
room covering every combination of on-count and in-count that matters, plus a
closed held crate and a held bag.  run400, `Adrift_1_p4inon.txt`, all ten
commands echoed -- **and scarier matches every row word for word**:

| cell | on / in | run400 |
| --- | --- | --- |
| `desk1` | 1 / 1 | `A pin1 is on the desk1, and inside is a cog1.` |
| `desk2` | 1 / 2 | `A pin2 is on the desk2, and inside is a cog2a and a cog2b.` |
| `desk3` | 1 / 3 | `A pin3 is on the desk3, and inside is a cog3a, a cog3b and a cog3c.` |
| `desk4` | 2 / 1 | `A pin4a and a pin4b are on the desk4, and inside is a cog4.` |
| `desk5` | 0 / 1 | `A cog5 is inside the desk5.` |
| `box` | container only, 1 | `A cogbox is inside the box.` |
| `tray` | surface only, 1 | `A pintray is on the tray.` |

`desk1` is the row the whole probe was for: at in-count **1**, with a surface
listing already printed, run400 says `, and inside is a cog1.` and not a
second sentence.  The inner `If var_9E = 1` arms are dead code after all, the
guard order in `lib_list_in_object()` is right, and `ADRIFTMAS_Party` and
`yonastoundingcastle` now rest on measurement rather than on a reading.
`desk5` is the same flag from the other side -- surface flag set, nothing on
it, `var_9E` stays 0, unjoined wording.

The two mode rows check out as well:

    open crate   You open the crate.  A cogcrate is inside the crate.
    x crate      Furniture.  The crate is open.  A pincrate is on the crate,
                 and inside is a cogcrate.
    i            You are carrying a crate and a bag.  A pincrate is on the
                 crate, and inside is a cogcrate.  A pinbag is on the bag,
                 and inside is a cogbag.

`open` passes mode 0 and prints the in-half alone, unjoined; `x` and `i` pass
mode 2 and join.  Nothing left open in this family.

## OPEN 2026-08-25 -- the rest of the absent-object refusals (P-code only)

Found while chasing hauntedhouse's `melt statue` (CLOSED below).  Everything
here is pinned in the decompiles but **not yet measured**, so none of it is
ported -- except the `x <unknown noun>` row, which the captured transcripts
turned out to answer on both sides, and which is now FIXED below.  Scarier
answers the rest with one of two upstream-SCARE strings: `<Verb> what?`, which
no Runner prints at all, and `You see no such thing.`, which run400 does print
-- that string is in run400.exe and in **none** of run370/380/390.exe.

`open` / `close` -- run400 `Proc_19_3_476468`, the `open` branch at 475694 and
the `close` branch at 4759BE.  The noun is resolved by `Sub_22_66` @463640,
which unlike `co()` does **not** require the seen byte.  Three outcomes when
the object is not present:

| resolver | seen byte (+48) | run400 | where | scarier today |
|---|---|---|---|---|
| object | 1 | `You can't see the statue.` | 475966 / 475C10 | `Open what?` |
| object | 0 | `Open what?` / `Close what?` | 4759AC / 475C51 | `Open what?` |
| -1 | -- | `You can't open that.` | `Exit Sub` 4756BC, then therest 488818 | `Open what?` |

The third row is the one hauntedhouse measures (`open door`, turn 3, no `door`
object in the game).  Scarier used to reach all three through the single
`{"open *", ...}` row (scrunner.cpp:658), because `%object%` is seen-gated
*and* `lib_disambiguate_object()` is room-gated, so a seen-but-absent object
never reached `lib_cmd_open_object()`.  All three rows are now right: the third
by the FIXED entry below, the first two by the seen-but-absent resolver, ported
2026-08-25 -- see "FIXED 2026-08-25 -- the 4.0 seen-but-absent resolver".

#### FIXED 2026-08-25 -- no Runner has ever said `Open what?`

Upstream SCARE routed `open *` to `lib_what()`.  Nothing prints that.  Every
Runner composes the same flat refusal its `close` twin does, and `close *` was
already routed to `lib_cant_do_other()` on the line directly below -- a
one-line asymmetry, not a version split.

Measured on both sides, all commands echoed:

| taf | transcript | `open` | `open door` | `open <seen, absent>` |
|---|---|---|---|---|
| `p39EXAM.taf` 3.90 | `Adrift_41/43_p39exam.txt` | `You can't open that.` | `You can't open that.` | `You can't open that.` |
| `p4EXAM.taf` 4.00 | `Adrift_1_p4exam.txt` | `You can't open that.` | `You can't open that.` | `You can't see the statue.` |

Only the last cell differs, and that is the 4.0 resolver in the table above
speaking one layer up -- not this handler.

Confirmed a **third** way, offline and for free, by the ALR Originals table in
`panic.taf` (the oracle described under "The ALR tables are a free, offline
oracle").  Its author enumerated the library messages exhaustively; the list
carries `Block what?`, `Drop what?`, `Take what?`, `Lock what?`, `Unlock
what?`, `Press what?`, `Pull what?` and twenty-six more -- and **no `Open
what?` and no `Close what?`**.  What it carries instead, at the alphabetical
position where `Open what?` would sit:

    You can't open that.
    {#}[I do not discern the object you want to open.]

`Open what?` / `Close what?` are in the run380/390/400 pools, but reaching them
needs the noun to match an object that is elsewhere *and* that object's byte at
+44 (390) / +40 (380) to be 0 -- run390 `43A266`, run380 `42F1D1`.  No probe row
has ever got there; in 3.90 `co()` did not match the absent statue at all, so
the command fell straight to the generic tail at `45D454` (`push "that"`).

**Ported**: `lib_cmd_open_what()` is gone, replaced by `lib_cmd_open_other()`
next to `lib_cmd_close_other()`, and scrunner.cpp:658 points at it.  All
versions, no gate.  Two goldens moved and were re-blessed, both 4.00:
`xfiles` (`open phone book`) and `cellar` (`open satchel`).  Neither is *right*
yet -- run400 answers `open phone book` with `Your Cell Phone is already
open!`, because its matcher takes `phone` as a partial match on `Your Cell
Phone` where Scarier's does not (`Adrift_22_xfiles.txt` lines 226-231, already
logged under the matcher entries) -- but the library half of both is now the
Runner's.

#### FIXED 2026-08-25 -- `close <present, not closeable>` loses its bang before 4.0

The probe caught a punctuation split inside a pair that looks symmetric and is
not:

    run390, Adrift_43_p39exam.txt        run400, Adrift_1_p4exam.txt
    > open stone                         > open stone
    You can't open the stone!            You can't open the stone!
    > close stone                        > close stone
    You can't close the stone.           You can't close the stone!

The decompile says why.  `openclose()` gives `open` a not-openable branch of
its own, ending in `"!"` (run380 `42F071`, run390 `43A0C5`/`43A0F3`).  It gives
`close` **none at all**: run380 `42F25C..42F322` and run390 `43A2xx` test only
openness 6 and 5, so a present-but-not-closeable object falls out of
`openclose()` with the message still empty and is answered by the generic
can't-do tail further down -- which ends in `"."` (run370 `43D231`, run380
`443D31`, run390 `45D4BE`/`45D4CF`).  Same sentence as `close <nothing>`, same
period.  4.0 finally gave close its own message, with a bang (run400 `475A31`,
`"!"` at `475A5F`).

Ported in `lib_cmd_close_object()`'s tail, gated on `lib_is_version_400()`.
No golden moved.

`examine` -- run400 `Proc_19_87_471F94` @471340, readable verbatim as run370
`examines` (Form1.frm:6892-6949):

| case | run370/run400 | where (run370 / run400) |
|---|---|---|
| named object, seen, absent | `You can't see the statue from here!` | 435937 / 471958 |
| named object, never seen | `You can't see that.` | 435956 / 47199E |
| plural, none here | `You can't see any statues here.` | 435A13 |
| nothing matched, lit | `Nothing special.` | 435BF4 |
| nothing matched, dark | `You can't see that very clearly.` | 435C8F |

Scarier used to print `You see no such thing.` for every one of them
(`lib_cmd_examine_other`, sclibrar.cpp).  The last row of that table -- the
one that most needed measuring, because `Nothing special.` is a startlingly
different answer to `x fjkdlsj`, and because it is exactly the string run370's
own darkness check searches for at 438F2F -- is now measured on **both** sides
and ported.  See the FIXED entry immediately below.  The **first** row is
measured and ported too (the seen-but-absent resolver, 2026-08-25).  The three
rows between them -- never-seen, plural, dark -- are still unmeasured and still
unported.

### FIXED 2026-08-25 -- `x <noun that names nothing>` splits at 4.0

Measured without a Wine run, by mining the 102 transcripts already captured for
the refusal strings.  4.0 rewrote the last line of `examines`:

* **run390**, `Merry_Murders.taf` (3.90 -- .taf bytes 8-10 are `\x94E7`),
  `Adrift_39_merry_murders.txt` lines 37-38, and again in the second replay
  `Adrift_40_merry_murders.txt`:

      > x pocket
      Nothing special.

  `pocket` is no object in that game and the Plaza is lit.  Scarier answered
  `I see no such thing.`

* **run400**, `The_X-Files_A_New_Beginning.taf` (4.00 -- `\x93E>`),
  `Adrift_22_xfiles.txt` lines 186-187 and 232-233:

      > look at camera
      You see no such thing.

  Neither `camera` nor `byers` is an object.

The exes date the change independently.  Scanning the VB6 constant pools
(uint32 byte-length, little-endian, then the UTF-16LE bytes) for `such thing`:

    run370.exe 0    run380.exe 0    run390.exe 0
    run400.exe 1    ' see no such thing.'

while `Nothing special.` is present in all four -- in run400 only outside
`examines` (`loc_48A65C`, `loc_488ECB`).  run370's `examines` reaches it
verbatim at 435BF4, readable in `Project/Form1.frm`; run400's tail branches to
` see no such thing.` at `loc_471EF6`.

Ported in `lib_cmd_examine_other()`: pre-4.0 prints the flat, person-free
`Nothing special.`, 4.0 keeps the person-inflected `You/I/%player% sees no such
thing.`  Three 3.90 goldens moved and were re-blessed -- `veteran` (1 line),
`zombies` (2), `everything` (2, the `I see` first-person form).  No route
changed and no win marker was lost; corpus back to full PASS.

Deliberately **not** ported, because nothing has measured them: 4.0 also sets a
flag beside this message (`MemVar_494281` at `loc_471F02`), and Scarier has no
darkness examine answers at all (`... can't see that very clearly.`, `... can
just make out that ...`, `loc_471F41`).

### FIXED 2026-08-25 -- the OTHER half of that rewrite: `x <object with no description>`

Same routine, one branch earlier.  Measured on `p39EXAM.taf` / `p4EXAM.taf`
2026-08-25 and ported; the reading below was right.

When the object *is* found and its Description is empty, pre-4.0 leaves the
Runner's message string empty and the empty string falls through to the very
tail measured above -- so `x stone` and `x fjkdlsj` give the same flat answer:

| | pre-4.0 | 4.0 |
|---|---|---|
| where | the `If msg = vbNullString` tail: run370 `435BF4`, run380 `43D545`, run390 `44C3DC` | filled in one branch earlier, run400 `loc_471A08` / `loc_471A1C` |
| answer | `Nothing special.` | `<player> see nothing special about <obj>.` |
| scarier | the 4.0 wording, every version (`lib_cmd_examine_object`) | correct |

run370 and run380 decompile to readable VB and can be read straight off:
`Project/Form1.frm` 6943 and 7339 are both `MemVar_...E8 = "Nothing special."`,
inside the same `If ... = vbNullString` / `If Not c("me")` tail the measured
`x pocket` row lands in.  run390's p-code pushes the same literal at
`loc_44C3DC` in the same position.

**Beware the near-miss.**  `There's nothing special about <obj>.` -- run380
`440D4C`, run390 `459EC1`, run400 `480041`, and in run380/390/400.exe but not
run370.exe -- is the **character** default, reached from the `locate`/`x`
character block, *not* from `examines`.  Scarier already prints it, in
`lib_cmd_examine_npc`.  An earlier note here had it as the 3.8/3.9 object
wording; it is not, and porting it would have been wrong.

**Corpus exposure**: `ms_mobius_solution` (ms_mobius.taf, 3.90) is the only
pre-4.0 golden that reaches the line.

**Measured 2026-08-25.**  run390, `Adrift_41_p39exam.txt` (29/29 echoed), first
command:

    x stone
    Nothing special.

and `Adrift_43_p39exam.txt` repeats it with the stone **held** (`take stone` /
`i` / `x stone`) -- same answer, so being carried makes no difference.  The
4.00 twin, `Adrift_1_p4exam.txt`, answers `You see nothing special about the
stone.`  `x crate` behaves as predicted on both: the contents sentence counts
as a description and the tail never fires.

Ported in `lib_cmd_examine_object()`, gated on `lib_is_version_400()`.
`ms_mobius` moved by exactly one line and was re-blessed.

**`read <noun that names nothing>` is the same tail again.**  Pre-4.0 there is
no separate read verb for an unmatched noun: `read` is one of the words that
*enters* `examines`, ORed in with `x`/`examine`/`look at`/`ex`/`exam` --
readable verbatim in run370 `434E2A` and run380 `43C69D`, and pushed one by one
in run390's p-code at `44B7FF`.  So `read eye` with no `eye` object falls
through to the very line `x pocket` measured: `Nothing special.`  4.0 answers
`<player> see no such thing.`, which is what Scarier prints for both.
`cybercow_win` (`read notation`) and `panic` (`read eye`) are the two exposed
3.90 goldens.

**Measured 2026-08-25**, `Adrift_41_p39exam.txt`: `read zzzz` -> `Nothing
special.`, against `Adrift_1_p4exam.txt`'s `You see no such thing.`  The
matching `read stone` (present, not Readable) answers `You can't read the
stone!` in both, so only the unmatched-noun tail splits.  Ported in
`lib_cmd_read_other()`.

Both exposed goldens moved -- and both moved to a **game-supplied** wording,
because each game ALRs the sentence: `cybercow_win` line 690 now reads `I can
tell you nothing about that.` and `panic` line 470 `[I do not discern the
object you want to examine.]`  That is the fix landing correctly, not a second
divergence: the ALR only fires because Scarier finally emits the string the
Runner emits.

**Staged probe -- BUILT and RUN 2026-08-25, `harness/make_39_examprobe.py`.**  A
purpose-built 3.9 file, `p39EXAM.taf`, one 24-command session, one row per
open question.  Two rooms wired north/south; four dynamic objects, so the room
listing SEES all of them (a static is never listed, and an unlisted object is
not referenceable -- that alone would have wrecked a hand-built probe): a
`stone` and an open `crate` with no Description at all, a `coin` inside the
crate, and a `statue` in the North Room, to be seen and then walked away from.

    x stone / x crate / x coin / x zzzz / x me / n / x statue / s /
    x statue / open statue / close statue / x statues / x door /
    open door / take door / open / close / take / drop / read zzzz /
    buy statue / get off / x all / probe

`x zzzz` is a control: it is the row already measured, and it must come back
`Nothing special.` for the rest of the session to be worth reading.  `probe` is
a repeatable no-restriction task that must answer `PROBE OK.`, proving the file
is wired.  The builder's docstring carries what Scarier answers today for each
line, so the run is a straight diff.  Read all of it off the echo.

This supersedes the earlier hauntedhouse staging for 3.9 -- but hauntedhouse
(3.80) is still the cheapest 3.8 replay of the same rows, and settles the
`open door` half of the hauntedhouse measurement left open below.

**RUN 2026-08-25.**  Two run390 sessions, `Adrift_41_p39exam.txt` (29 commands,
all echoed) and `Adrift_43_p39exam.txt` (19 commands, all echoed -- the second
feed adds the open/close-a-real-object rows, the held-object `x`, and a second
reading of the Void Room).  Feeds are `cmdfile_p39exam.txt` and
`cmdfile_p39exam2.txt` in `~/adrift-battle/runner/wine/`.

After the four fixes above, **run390 and Scarier agree on every one of the 48
rows, word for word.**  That is the whole probe closed for 3.90.

**The 4.00 twin -- BUILT and RUN 2026-08-25, `harness/make_400_examprobe.py`.**
run400 will not load a 3.90 file: it puts up a red `Loading... Incorrect
version.` and never starts, and `measure.sh` reports it only as "first command
never reached the game".  Each Runner plays its own file version and no other,
which is itself the argument for keying every split on
`prop_get_taf_version()`.  So the probe was rebuilt against the 4.0 OBJECT
schema (`sctafpar.cpp:115-123`) and packed with `taftool.py` onto a 4.00 donor.
`p4EXAM.taf`, 32 commands, `cmdfile_p4exam.txt`, transcript
`Adrift_1_p4exam.txt`, all echoed.

Two footguns worth keeping:

* **`Capacity 99` is invalid and hangs run400 at "Loading...".**  Capacity packs
  as tens = object count, units = size index (`scobjcts.cpp:674-706`), and size
  index 9 is out of range.  Use `52`.  Bisected with a new
  `~/adrift-battle/runner/wine/loadtest.sh <taf> [exe] [png]`, which launches,
  waits, prints the window title (the title carries the game name only if the
  file actually loaded) and screenshots the top of the window -- the cheap way
  to tell "the .taf is malformed" from "the replay went wrong".
* That same bisect found **`p4INON.taf` has never been loadable** for the same
  reason.  Its queued probe was blocked by a malformed file, not by Wine.
  `make_400_inonprobe.py` now writes `52`; rebuilt, and the file loads (window
  title `ADRIFT Runner - In-On Probe 400`).  That probe is **run and closed**
  -- see "the unreachable arms really are unreachable" above.

What `p4EXAM.taf` left open was only the 4.0 resolver family:

    x statue      You can't see the statue from here!   (scarier then: You see no such thing.)
    open statue   You can't see the statue.             (scarier then: You can't open that.)
    close statue  You can't see a statue.               (scarier then: You can't close that.)
    buy statue    You can't see the statue.             (scarier then: I don't think that is for sale.)

Note the article: `open` takes the definite, `close` the indefinite.  All four
were one port -- the seen-but-absent resolver -- and all four are **FIXED**;
see the entry immediately below.

**Still unmeasured**: the 3.70 and 3.80 halves of every row here.  That needs
3.7/3.8 twins of the probe, i.e. a further taf-format port.  The decompiles say
3.8 tracks 3.9 throughout except that its "nothing of interest" substitution
happens at LOAD; 3.7 differs on at least one row (`open <present, not
openable>` composes with a period at `43D1E0`, where 3.8 and 3.9 have grown a
separate branch ending in a bang).

## FIXED 2026-08-25 -- an event's look text belongs to the room being DESCRIBED

**Found by** the goldilocks measurement (`Adrift_1_goldilocks.txt`, run400,
Verbose ON, 252 commands, all 252 echoed).  It was the only real divergence in
the whole run, and it was ours.

**The turn.**  Command 243 is `u`, the escape from the cellar as it fills with
porridge.  The task that lifts you out has ShowRoomDesc pointing at the hall:

```
turn 243  u
  run400   ...front door is an open trapdoor.  I can move north, west, up,
           down and out.
  scarier  ...front door is an open trapdoor.  Extremely hot porridge is
           gushing down the sides of the porridge pot.  The room is steadily
           beginning to fill up with the stuff.  I can move north, west, up,
           down and out.
```

That sentence is EVENT 4 `[Cellar fills with porridge]`, whose Where is a
some-rooms list of {cellar, dark passage, dungeon}.  The hall is not in it.

**Why we printed it.**  Two rules meet.  A ShowRoomDesc room is displayed
*before* the task's own actions run (`task_show_room_desc()`, and the
ShowRoomDesc-before-actions note), so when the hall was composed the player was
still standing in the cellar.  And the event look-text loop in
`lib_print_room_description()` asked `evt_can_see_event()`, which has only ever
consulted `gs_playerroom()`.  So the room the player was *leaving* decided what
the room they were being *shown* said.  A one-line `SCR_TRACE_LOOKTEXT` over
the whole 252-command run turned up exactly one qualifying event:
`LOOKTEXT ev=4 cansee=1 room=11`.

**What the Runners do.**  Both room listers take the room to describe as an
argument and index the event's room array with *that*:

* run400 `viewroom` (`Proc_19_63_472CA4`), loop at `loc_472B34`: the state byte
  `var_198(74) = 1` ANDed with `var_198(20)(arg_C - 1) = 1`, then the LookText
  `var_198(12)` is appended at `loc_472B7F`.
* run390 `viewroom` (`4481AC`), loop at `loc_448056`: `var_184(70) = 1` -- the
  state byte sits at 70 in 3.9, not 74 -- ANDed with `var_184(20)(broom - 1) = 1`
  at `loc_448075`, LookText appended at `loc_4480A1`.

`arg_C` and `broom` are the room arguments.  Neither routine looks at the
player at all.

**The fix.**  `evt_can_see_event_in_room (game, event, room)` in `scevents.cpp`
carries the whole room list test; `evt_can_see_event()` stays as a
`gs_playerroom()` wrapper, which is the right question for its four tick-path
callers (`scevents.cpp` 454, 546, 1017, 1090 -- those really are asking whether
the player can see it).  Only the description loop passes its own `room`.

**Corroborated in the other direction** on run390, `Adrift_1_cybercow.txt`:
`up` out of the well is a ShowRoomDesc task naming Chapel Yard, and the Runner
prints the day/night event with it --

```
Chapel Yard
You are at the well.   The rope, which is tied to the well quite securely,
leads down. ... Down the hill to the north there is the bus stop.
Vluurinik flits around.
It is daytime.  You can move north, east, south, west and down.
```

-- where Scarier used to print nothing, the player being at the bottom of the
well and outside the event's list.  The correct gate prints *more* here and
*less* in goldilocks; it is the same single condition.

**Corpus fallout**: 11 rows across 9 games, all re-blessed after reading each
hunk -- Shadowpeak x3 and mangiasaur and wax_worx lose an event line from a
task-shown room; cybercow, timmy_reid, fugitive, provenance and baroo gain one;
goldilocks does both.  303 PASS, `scproj_regress.sh` PASS, the a5 suite
untouched at DIVERGE=17.

**Three feed lessons from the cybercow run**, none of them engine differences,
recorded so the row can be driven cleanly one day:

1. A game that asks the player's **name or gender asks it in an InputBox**, not
   at the game prompt.  The answers are the first lines of the walkthrough for
   Scarier -- which prints the questions inline -- but under the Runner they
   must be typed into dialogs before the transcript exists and must NOT be in
   the command file.  `measure.sh` grew `POPUP_ANSWERS="Hero|male"` for this.
   An empty Return is rejected and the box comes back with a **new window id**,
   which the startup-alert dismissal loop mistakes for a stubborn alert, so
   that loop is now skipped when answers are supplied -- and the window
   geometry is re-read afterwards, because the main window is not necessarily
   where it was before the dialogs (the first cybercow run clicked 480 px away
   from its menu bar and reported "Save-transcript dialog never appeared"
   while the menu was perfectly reachable).
2. `measure.sh` now forces **Options -> Sound OFF** in the registry the same
   way it forces Verbose ON.  With `mmdevapi=d` -- which is what keeps Wine
   audio from soft-locking the desktop -- any game that plays a sound raises a
   "Cannot play sounds" modal, and a game that plays one every turn raises it
   again the instant it is dismissed; that modal is indistinguishable from the
   Save-transcript dialog to a window counter.  Sound OFF stops playback and
   changes no text.
3. `catch fairy` is a **random roll** and the walkthrough spams it.  A fixed
   command file cannot be replayed turn for turn against an unseeded Runner,
   so cybercow needs the same treatment as the other RNG rows before it can be
   quoted as a clean measurement.

---

## FIXED 2026-08-25 -- the 4.0 seen-but-absent resolver

The last unported thing `p4EXAM.taf` measured, and the largest of the round.
4.0's noun binder makes a **second pass**: when nothing the noun names is in
the player's room, it looks again over every object the player has *seen*
(the +48 byte, written at run400 `471749` / `46A142`), and if exactly one
matches it answers "can't see" instead of falling through to the flat refusal.
Pre-4.0 Runners have no second pass at all -- `p39EXAM.taf` answers all four
rows with the ordinary library refusals -- so this is a version split, keyed on
the .taf version as always.

Measured on both sides, all commands echoed:

| command | run390 (`Adrift_41/43_p39exam.txt`) | run400 (`Adrift_1_p4exam.txt`) |
|---|---|---|
| `x statue` | `Nothing special.` | `You can't see the statue from here!` |
| `open statue` | `You can't open that.` | `You can't see the statue.` |
| `close statue` | `You can't close that.` | `You can't see a statue.` |
| `buy statue` | `I don't think that is for sale.` | `You can't see the statue.` |

**The article is not a typo.**  `open`, `examine` and `buy` compose with
`the`; `close` composes with the object's own Prefix, i.e. `Prefix & " " &
Short`.  In the decompiles that is the definite path at run400 `475966` vs the
indefinite one at `475C10`, and the split survives because the definite
composer is a separate helper -- it is not the same call with a flag.  Scarier
mirrors it exactly: `lib_print_object_np()` for the definite three,
`lib_print_object()` for `close`.

**Where the calls go matters, and cost five regressions.**  Putting the check
at the top of `lib_cmd_examine_object()` and friends steals the turn from
`lib_cmd_examine_npc()`: humbug's `X robot` (there is a *static* object named
`robot` in the cellar, and a separate bus-stop NPC) and unraveling_god's
`x people` both lost their NPC descriptions, five goldens went red.  The
Runner does not have that problem because its clause is gated on the output
buffer still being empty (`MemVar_4941B0 = vbNullString`, tested at `471933`,
`475952`, `475BFC`, `4887A0`) -- it fires only when nothing else in the whole
turn has spoken.  Scarier reproduces that ordering structurally instead: four
new `_absent` handlers sit in `STANDARD_COMMANDS` **directly above the
catch-all `*` rows they pre-empt**, so every more specific row -- NPCs
included -- still gets first refusal.

`buy` is the odd one out: it has no `_absent` branch of its own in run400.
The sentence comes from `therest()`'s leading `obhere()` clause, which runs
**before** every verb branch, which is why `buy <seen, absent>` never reaches
"is for sale" at all.  Scarier registers `{"buy %object% *", ...}` above
`{"buy %text%", ...}` to the same effect.

**Deliberate deviation**: an object with an empty Prefix.  run400 composes
`" " & Short` and prints a double space; scarier prints the Short alone.  No
corpus game has an empty-Prefix object that can reach this clause, so nothing
measures it, and reproducing a stray space is not worth a special case.

Ported in `sclibrar.cpp` as `lib_absent_seen_object()` (the second pass, which
returns -1 unless *exactly one* seen object matches and none is present) plus
`lib_cant_see_absent_object()` (the composer), with four thin handlers
`lib_cmd_{examine,open,close,buy}_absent()`.  Two goldens moved and were
re-blessed:

* `cellar_solution` -- `x dust` -> `You can't see the dust from here!`,
  `open satchel` -> `You can't see the satchel.`
* `professor_solution` -- `examine contraption` ->
  `You can't see the flying contraption from here!`

Corpus 303/303, `scproj_regress.sh` PASS, a5 suite at its documented
17-DIVERGE baseline.

Still open in the same family, and reached by no probe row yet: the 4.0
"You can't see that." branch at `471995` for a named object the player has
**never** seen, run400's `%object%` two-pass scope filter proper (present-first
then absent-but-seen, tail self-call at `loc_458E64`), and the NPC seen gate
(`npc.global_26`) for `%character%` matching.

## FIXED 2026-08-25 -- `burn memo`: 4.0 binds `%object%` case-sensitively

**run400 refused a task Scarier ran, and the reason is neither the
restrictions nor Repeatable -- it is a capital letter.**

The_X-Files_A_New_Beginning.taf task 24 is `Burn %object%`, restr=2, mask
`#A#`:

    RESTR type=0 var1=1 var2=3 var3=0    "any object visible to the player"
    RESTR type=3 var1=0 var2=2 var3=-1   "the player is alone"

`SCR_TRACE_TASKS=1` showed both PASS in Scarier and the task running, printing
`You incinerate the The Memo with a Zippo ...`.  run400 answered `I don't
understand what you want me to do with The Memo.` -- `generaltasks`' end-of-turn
fallback at `loc_48B1F5` (`mdlSpreadTheLoad.bas:33899`) -- so it refused the
task and printed **nothing at all**.  Reproduced in two independent sessions
(`Adrift_22_xfiles.txt:17-18`, `Adrift_31_xfiles.txt:32-33`), so not a feed
artefact.

### What it took to get there

Four Wine rounds on the synthetic `p4BURN.taf` (`Adrift_2_p4burn.txt`,
`Adrift_6_p4burn.txt`, `Adrift_12_p4burn.txt`, `Adrift_13_p4burn.txt`) and one
on the game itself (`Adrift_1/3/4/5/7/8/9/10/11_xfilesbisect.txt`)
(`harness/make_400_burnprobe.py`, whose docstring carries the full cell table):

  1. **Thirteen restriction shapes** -- one restriction, two restrictions,
     sure-failers, messages present and absent, and `pd`, xfiles task 24's
     *exact* shape.  **All thirteen agreed with Scarier.**  The restrictions
     are not the refusal, and `burn` is not intercepted.  Two facts fell out
     for free: run400 prints the **first** failing restriction's message and
     stops, an empty message included (it does not skip ahead to a later
     failing restriction that has one) -- which is what `restr_lowest_fail`
     (`screstrs.cpp:906`, consumed at `:1176`) already does; and the nowhere
     NPC does not collide with an unnormalised `StartRoom` 0 (`pc` passed both
     before and after the round trip).
  2. **Repeatable** -- task 24 has it OFF, every round-one cell had it ON.
     `pm`/`pn`/`po` re-ran the interesting shapes with it OFF.  All ran.
  3. **Case, verb half and object half.**  Bisecting the game itself -- lower
     the verb at plain line 5929, lower the Memo's Short at 2376, separately
     and together, repack, replay -- showed the task fires only when **both**
     are lowered (`Adrift_11_xfilesbisect.txt`).
  4. **Case, pinned cell by cell** (`Adrift_12_p4burn.txt`, `Adrift_13_p4burn.txt`).

### The rule, measured

    LCase(pattern), with %object% textually Replace()d by the object's Short or
    one of its Aliases VERBATIM, compared for exact equality to LCase(input).

| probe cell | typed | run400 |
| --- | --- | --- |
| `PX %object%`, Short `coin` | `px coin` | PASS |
| `PX %object%`, Short `coin` | `PX coin` | PASS |
| `PY` (literal) | `py` | PASS |
| `PZ *` (wildcard) | `pz anything` | PASS |
| `pa %object%`, Short `Widget` | `pa widget` | refused |
| `pa %object%`, Short `Widget` | `pa Widget` | refused |
| `pa %object%`, Short `brass key`, Prefix `a small` | `pa brass key` | PASS |
| ditto | `pa key` | `I don't understand.` |
| ditto | `pa a brass key` / `pa the brass key` / `pa small brass key` | refused |
| `pa %object%`, Short `gem`, Alias `jewel` | `pa gem` | PASS |
| ditto | `pa jewel` | PASS |
| ditto | `pa a gem` / `pa the gem` | refused |

So the **verb**'s case never matters (both sides are lowered), literals and
wildcards are case-insensitive, but a **capitalised Short can never bind**, and
no article, no Prefix and no partial name is tolerated.  xfiles' objects are
`Memo`, `Coffee Mug`, `Gun Holster` -- every one capitalised -- so task 24 is
dead code in the real Runner, and so is every other `%object%` task in that
game.

### Why it is a Runner bug, and a one-sided one

The listing is `Proc_19_37_458E6C` (`mdlSpreadTheLoad.bas:26591-26845`, body
`loc_458BBC`-`loc_458E69`): guard `InStr(pattern, "%object%") > 0`, loop the
object array, gate on the seen byte `CInt(obj.global_48) = 1`, prefilter
`Proc_21_38_454CB0`, scope-test `Proc_21_53_44B578(i) = arg_14`, then
`Replace(pattern, "%object%", obj.global_4, 1, -1, 0)` and compare.  **There is
no `LCase()` anywhere on the substituted name.**

Its `%character%` twin, `loc_46918C`-`loc_469264` in the same file, is written
the same way but *does* lower the NPC's Name (`loc_4691B4`) and each Alias
(`loc_469207`) before the `Replace()`.  The asymmetry is the whole bug, and it
is one-sided: ADRIFTMAS Party's `[kiss {the} %character%]` over an NPC named
`Mystery` runs in the Runner, and must keep running here.

### Ported

`uip_compare_reference_strict()` in `scparser.cpp`, reached through the
`uip_set_strict_reference()` flag that `run_match_task_commands()` raises with
an RAII guard for `>= TAF_VERSION_400` only, and only for objects (the
`%character%` branch keeps the tolerant matcher, along with its lead-character
prefilter).  Corpus exposure is wide and the blast radius was not: **64 of the
432 .taf are 4.0 games with `%object%` in a task command**, and the change
moved exactly one golden line.

`xfiles_solution.expected.txt` re-blessed, three lines: `burn memo` now answers
`I don't understand what you want me to do with The Memo.` verbatim as the two
Wine transcripts do, and the score falls 296 -> 295, "3 points short" -> "4
points short".

### Version scope -- MEASURED 2026-08-25, and the split is only half a split

`"%object%"` does not appear in run370.exe or run380.exe at all, so before
3.90 such a pattern matches nothing whatever the player types and the tolerant
matcher there is harmless.  run390 implements it somewhere else entirely
(`Form1.frm:13991ff`), through `c()` plus the seen byte at `.global_44`, and
**rewrites the task command in place** with the substituted name.

`p39CASE.taf` (`harness/make_39_caseprobe.py`) is the 4.0 cell table in the
3.90 layout.  `Adrift_1_p39case.txt`, all 19 commands echoed:

| typed | run390 | run400 |
| --- | --- | --- |
| `pa widget`, `pa Widget` (Short `Widget`) | **PASS, PASS** | refused, refused |
| `pa brass key` (Short `brass key`, Prefix `a small`) | PASS | PASS |
| `pa key` | `I don't understand.` | `I don't understand.` |
| `pa a brass key`, `pa the brass key`, `pa small brass key` | refused | refused |
| `pa gem`, `pa jewel` (Alias `jewel`) | PASS, PASS | PASS, PASS |
| `pa a gem`, `pa the gem` | refused | refused |
| `px coin` / `PX coin`, `py` / `PY`, `pz coin` / `PZ coin` | PASS | PASS |

So **strict binding starts at 3.90 and only the case fold is lost at 4.0**.
3.90 refuses the article, the Prefix and the partial name exactly as 4.0 does;
it just lower-cases the name it substitutes first.  Scarier passed five of
those rows, so the port is now gated at `TAF_VERSION_390` for the binding and
at `TAF_VERSION_400` for the case sensitivity -- one `match_case` flag through
`uip_set_strict_reference()`.  **The corpus did not move at all** on the 3.90
half: still 303 PASS.

### By-product, not yet ported

The same listing shows run400 trying the match **twice**: `arg_14` is the
scope answer it demands from `Proc_21_53_44B578`, and the tail self-call at
`loc_458E64` re-runs the whole loop with `arg_14 = 0`.  Present objects bind
first, absent-but-seen objects only if no present object matched.  Still
unported; see the `SCR_TRACE_SCOPE` note.

The `%character%` half gates on `CInt(npc.global_26) = 1`, the NPC's own seen
byte -- set exactly like an object's `global_48` (`General.bas:452E4E` sets an
object's seen byte when the NPC holding it is seen).  That is why
`Adrift_22_xfiles.txt:232` answers `look up byers` with `You see no such
thing.`: at the FBI parking garage the Lone Gunmen have not been met yet, so
`Look up *%character%*` cannot bind.  The golden reaches that command long
after Byers has been listed, so it is not a divergence -- but porting the NPC
seen gate is still open.


## CLOSED 2026-08-25 -- an unhandled command names an object it can't see

`Adrift_16/17_hauntedhouse.txt` are `hauntedhouse.taf` driven with
`haunted.taf`'s solution -- the mispairing the section above warns about.  The
*feed* is wrong, but both engines were given the same nonsense, so the run is
still a valid engine-vs-engine comparison, and 115 of its 116 commands echoed
(first loss at `feed[43] w`).  Two divergences sit before that loss.

**turn 34, `melt statue`.**  The player is on the Front porch; the statue is a
static in the Entrance, seen a dozen turns earlier.  `melt` is not a verb, a
synonym or a task command anywhere in the game.

    run400   You can't see the statue.
    scarier  Hmmm.  Interesting.  No doubt it means something of resounding
             importance where you come from.  ...   <- the game's DontUnderstand

Pinned in the decompiles.  When nothing else has produced output the Runner
falls into `therest()` (run400 `Proc_19_85_489F4C` @4883A0, called from
48AFE4 under `If MemVar_4941B0 = ""`).  Its first act is to resolve a noun --
`Sub_22_66` @463640, then a loop over `co()` -- and *before* it dispatches any
verb it asks `obhere()`:

    If (var_C0 > -1) Then
      If (CInt(obhere(CInt(var_C0))) = 0) Then
        MemVar_4460E4 = Player & " can't see " & tense(prefix) & " " & name & "."
        Exit Sub                                   ' run370 43D169, Form1.frm:3336
      End If
    Else
      var_88 = "that"                              ' run370 43D199
    End If
    If CBool(c("open")) Then ...                   ' the verb chain starts here

So a command the library never handled still answers "<player> can't see <the
object>." whenever it names a resolvable object that is somewhere else, and
the game's DontUnderstand never gets a look in.  The clause is the same in
every Runner -- run370 43D169, run380 443C6A, run400 4887C1
(`mdlSpreadTheLoad.bas:41196) -- so no version gate.  The resolver *is*
version-gated (3.9+ `co()` also requires the seen byte), which is the gate
`lib_matcher_requires_seen()` already applies.

Ported into `lib_cmd_verb_object()`: the existing scan, which requires the
object to be in the player's room, still prints "I don't understand what you
want me to do with ..."; when it finds nothing, a second scan drops the
in-room test and prints "You can't see ..." instead of returning FALSE.  The
two outcomes are disjoint, so the order of the checks doesn't matter.  Corpus
303/303, no golden moved.

**turn 3, `open door` -- CLOSED 2026-08-25 by the p39EXAM/p4EXAM probes.**  There is no `door` object in
hauntedhouse.taf at all (objects 0-20 are knife, sink, severed head, oven,
fridge, meat cleaver, ghostly bomb, comb, statue, key, coffin, light switch,
marble pedestal, fences, trees, chandelier, bed, window, toilet, dust,
footprints; only the oven is openable), and `door` appears in the whole file
only inside "a series of doors leading off".

    run400   You can't open that.
    scarier  Open what?

`therest()` builds that from `var_88 = "that"` and the `c("open")` branch at
run400 488818, setting the message directly rather than through `checkverb()`
(`Proc_19_86_4455F8` @4455F8), which is what turns a *bare* verb into "<Verb>
what?".  Two facts stop the port here.  First, run400's own open handler
(`Proc_19_3_476468`, the `open` branch at 475694) does contain "Open what?"
at 4759AC -- but only on a path that needs the noun to have resolved, and it
`Exit Sub`s outright when it hasn't (4756BC), which is what lets `therest()`
run at all.  Second, the transcript answers `take guttering` with "Take what?"
and `drop fags` with "Drop what?", both unknown nouns, so the rule is not a
blanket "unknown noun -> can't <verb> that".  What run400 says to a bare
`open`, `close`, `take` and `drop` decides it, and that needed a live probe.

The probe answered it.  On **both** run390 and run400 a bare `open` and
`open door` alike print "You can't open that.", so `open` was simply
asymmetric with the `close *` row beneath it in `STANDARD_COMMANDS` and no
Runner has ever printed "Open what?" -- corroborated a third way by the
`panic.taf` ALR Originals table, which enumerates thirty of these refusals
and has no `Open what?` in it.  `take` and `drop` keep their "<Verb> what?"
forms, which the same transcript shows (`take guttering`, `drop fags`), so
the rule really is per-verb and not a class.  Ported, and `hauntedhouse.taf`
has since been re-driven with its own solution: 42 of 42 commands echoed and
41 of 42 turns identical (the 42nd is only the `[Press any key to end]`
tail).
**Staged**: drive any game with `open` / `open door` / `take` / `take
guttering` / `drop` / `drop fags` and read the four answers off the echo.

## CLOSED 2026-08-25 -- sophie's `[, and] -> [:]`, and a transcript-naming trap

**A Wine transcript is named after the GAME, not the `.taf`.**  Every sophie
run under run400 -- `Adrift_41_sophie.txt` .. `Adrift_46_sophie.txt` -- was
launched on `sa.taf` or one of its doctored `saF*` variants
(`measure.sh saF577.taf cmdfile_soph14.txt run400.exe` and friends).  The comp
release `sophie.taf` has **never** been run under a Runner.  Before quoting a
transcript against a golden, check which `.taf` the run actually used; the two
sophie rows point at two different games.

That trap cost a diagnosis.  `sophie.taf` carries ALR **#418 `[, and] -> [:]`**,
`sa.taf` does not, and the string `, and` is common enough that the rule wrecks
eight sentences.  Scarier applies it, six `, and`s survive in the run400
transcripts, and it *looks* like a clean divergence -- but the transcripts are
`sa.taf`'s, where no such rule exists, and they agree with
`goldens/sophie_solution.expected.txt` line for line.  Nothing was measured
against `sophie.taf` at all.

Scarier is nevertheless right to apply #418, on the author's own evidence:

- `sophie.taf` also carries two fixup rules, #455
  `[of the moment: throw yourself at Smunch.] -> [of the moment and throw
  yourself at Smunch.]` and #458 `[inventory: so on] -> [inventory, and so]`.
  Both Originals contain a **colon** at a spot where the raw game text reads
  `, and` (confirmed straight out of the game text: `...on the spur of the
  moment, and throw yourself at Smunch.` and `...look, inventory, and so
  on.`).  The author can only have seen those colons in a Runner, so run400
  does apply #418.
- The fixups are 40 and 16 characters long against #418's 5, so a single
  length-descending pass would run them *first*, find no colon, and leave them
  dead.  They only do anything under 4.0's repeat-until-stable pass -- which is
  what `pf_filter_internal()` (`scprintf.cpp:833`) already implements, and
  which is therefore corroborated here a second time.
- `sa.taf`, the later author release, **deletes #418 and keeps #455/#458**,
  now dead rules.  That is what cleaning up a mistake looks like.

No engine change; both goldens stand; corpus **303/303**.  The reasoning is
recorded on the `sophie_solution.txt|sa.taf` row in
`harness/run_v4_walkthroughs.sh` so it is not re-derived a third time.

The general question this raised -- whether an ALR whose Original starts with
punctuation (`, `, ` `, `: `) or whose Replacement is pure punctuation behaves
any differently from a word-for-word rule -- is now **measured and closed**.
`harness/make_400_punctalrprobe.py` / `p4PALR.taf` ran under run400 on
2026-08-25 (`Adrift_1_p4palr.txt`, all eight commands echoed).  Every cell
fires, and Scarier's output is identical character for character:

    ping     PING FIRED.       (control -- no ALR)
    alpha    AA: BB.           [, zz]  -> [:]     the sophie shape exactly
    beta     CC :.             [zz DD] -> [:]
    gamma    EEGOT3 FF.        [, yy]  -> [GOT3]
    delta    GG GOT4.          [yy HH] -> [GOT4]  control, plain both ends
    epsilon  IIGOT5 JJ.        [ ww]   -> [GOT5]
    zeta     KKGOT6 LL.        [: vv]  -> [GOT6]
    eta      MM , NN.          [tt]    -> [,]

So `Proc_21_20_44C7DC` really is the character-blind `Replace()` walk the
listing shows, punctuation and all, and #418 firing on `sophie.taf` needs no
further defence.  No engine change.

## CLOSED 2026-08-24 -- empty-M1 room alts, and recursive holding

Two `sclibrar.cpp` fixes, three live Wine measurements, 19 goldens across 13
games re-blessed, corpus back to **303/303 PASS**.  The full write-ups live in
the row comment blocks in `harness/run_v4_walkthroughs.sh` (canonical block on
the `lair-of-the-cybercow` rows; see also `xfiles`, `unraveling_god`,
`alices_restaurant`).  In short:

- **A matching method-0/1 room alt is the description's starting point even
  when its own M1 is blank**, and everything accumulated before it is thrown
  away.  `lib_find_starting_alt()` used to skip such an alt and keep scanning
  backwards.  Only the *non*-matching branch is guarded, on M2.  Confirmed at
  all three generations, and deliberately on cases where the new behaviour
  *loses* text, which is the direction that needed proving:
  - **3.7** `arlo.taf` / run370 -- cmd 34 now byte-exact against `Adven_6_arlo.rtf`.
  - **3.9** `lair-of-the-cybercow.taf` / run390 -- `Adrift_35_cybercow.txt`.  Room 7's
    alt 0 is method 2, unconditional, "The end of a rope dangles here."; alt 1
    is method 1 on task 31 with M1 and M2 both empty.  Before `untie rope` the
    Runner prints the rope line; after it, it does not.
  - **4.0** `unravel.taf` / run400 -- `Adrift_34_unraveling_god.txt`.  Every "Outside the
    MagLab" ends at "...is to the south." and never carries the "As nice of a
    day as it is, though, ..." block the old golden had.
- **Room-alt "is/isn't holding" is the Runner's recursive possession
  predicate** (run400 4579C1/4579EB -> `Proc_21_46` @44615C), so an object
  inside or on something carried or worn counts as held.  SCARE tested the
  object's own position only.  `xfiles.taf` is the only corpus row it moves,
  and that replay is now Runner-exact.

### Newly logged, not fixed

- **xfiles cmd 17, article capitalisation.**  run400 prints "You take **The**
  Warehouse Key from Case File 10193."; scarier prints "the Warehouse Key".
  The same object's *examine* message capitalises correctly in both.
  Pre-existing, unrelated to either fix above.
  **FIXED 2026-08-25** -- the Runner's normalizer has no `the` branch at all;
  see "a \"The\" prefix keeps its capital" below.

### Harness lessons from this round

- **Write cmdfiles with plain LF, never CRLF.**  `drive_ckpt_safe.sh` reads
  with `IFS= read -r line` and leaves the `\r` on, so `keystroke "$line"`
  submits the command by itself and the following `key code 36` submits an
  *empty* line.  The tell is a parser refusal ("Nope!", "I'll be dammed if
  that makes any sense.") after every single turn.
- **`drive_ckpt_safe.sh` now takes `TYPE_SLEEP` / `ENTER_SLEEP`** (defaults
  0.25 / 0.45), forwarded by `runner_transcript_safe.sh`.  A game whose
  responses run to several screens can still be laying out text when the next
  line is typed: the keystroke lands before the `(press any key)` pause
  exists, is eaten as an empty command, and the pause then swallows the
  *following* real command -- which reads exactly like the engine dropping a
  turn.  `unravel.taf` needs `TYPE_SLEEP=0.6 ENTER_SLEEP=1.6`.
- **`runner_transcript_safe.sh` now picks the largest window for the pid**,
  not the first non-1x1 one.  A game that opens with its own modal -- e.g.
  `lair-of-the-cybercow.taf`, which asks for a player name and then a gender
  in two separate dialogs (417x162 and 282x127) -- otherwise has that dialog
  chosen as "the window", and the whole Start-Transcript click sequence is
  aimed at it, so nothing in the game is ever clicked and the script reports
  "could not identify our own new transcript file".  Note the modal also
  *blocks the Adventure menu*: answer the game's startup prompts by hand
  first, then start the transcript, then drive the remaining commands.


## FIXED 2026-08-24 -- the object `seen` model (was PARKED on `scarier-seen-flag-port`)

The xfiles replay (`Adrift_22_xfiles.txt`) left one unexplained divergence: run400
answers `take knife` in Garage 5 with **"Take what?"** where Scarier takes the
knife.  Task 7 "Use Key" carries the player into Garage 5 with `ShowRoomDesc`
off, so no room description prints, and the knife is a dynamic that has been
lying there since the load.  The Runner's parser will not resolve a noun to an
object whose `seen` byte is clear, and nothing on that path ever sets it.

Scarier, by contrast, used to mark *everything* in the player's room seen on
every turn (`obj_turn_update`).  That was the bug.  The port was written on
`scarier-seen-flag-port` (commit `de7bffcc`) and landed on master on
2026-08-24, with fifteen walkthroughs re-derived for it (below).

### What run400 actually does

- **The gate.**  `co()` (`Proc_21_39_46486C`, `run400/Project/General.bas:8711`)
  tests `(obhere(obj) Or mode = 4) And obj(48) = 1` in both its counting loop
  (`@00464372`) and its selection loop (`@00464693`).  `takes`
  (`Proc_19_6_47C83C`) reaches `co()` at `@0047B9DC`, `@0047BD9D` and
  `@0047C694` and has no bypass, so the seen byte gates `take` as hard as it
  gates `examine`.
- **The seed.**  `openadv` clears the byte and sets it at `@004909B5` when the
  location field is `0` (held by the player) or `&H9C` (worn).  Statics reach
  that test through the same `location = InitialPosition - 1` mapping at
  `@00490270`, so a static whose `Where/Type` is **ONE_ROOM (1)** lands on 0
  and *starts seen*; some-rooms (`&HF6`), all-rooms (`&HEC`),
  part-of-character (`&HE2`) and hidden (`-1`) statics start unseen.  The
  Runner plainly never noticed it was labelling single-room statics "held".
  This quirk is load-bearing: it is the only reason a game with
  `DispFirstRoom` off -- `ZAC.taf`, `1HRGAME.taf`,
  `secret_of_lost_world` -- can answer `x sand` on turn one, since `tstart`
  calls `viewroom` only when that flag is set (`@0044D68F`).
- **The writers.**  All 47 sites, by containing function: `execute_action` 14,
  `whatisinon` 6, `viewroom` 4, `examines` 3, then two each in `openadv`,
  `obhere`, `inventory`, `charinv`, `insides`, `drops`, `dobattle` and the
  event mover `Proc_19_16_45614C`.  Census both decompiler idioms or the
  answer is wrong: `Dim from_stack_1.global_48 As Byte: ... = from_stack_2`
  **and** `var_XXX(48) = from_stack_1`, across `*.bas` *and* `*.frm`, with
  `grep -a` (General.bas reads as binary).
- **Task player moves reveal statics only.**  `execute_action`'s three
  player-room destinations (`@0048CA32` "to room", `@0048CADD` "to roomgroup
  part", `@0048CB48` "to same room as") sweep the object table and set the byte
  where `global_24 = 1` **and** the static's presence array covers the new
  room.  Dynamics are skipped -- which is precisely the xfiles knife.
- **Task/event object moves reveal only into the player's room.**  Both
  `execute_action` (`@0048C40A` and siblings) and the event mover
  (`@00456124`) compare the object's freshly written location field against
  `unk_409011.global_0` and stamp the byte only on equality.

### How it was settled

The parked note said only a live run400 probe could decide it, and the console
was locked for the whole session.  It never needed one: **the answer was
already in the archived transcripts.**

- **xfiles, `Adrift_22_xfiles.txt` lines 92-93.**  The exact case the branch changes,
  measured live months ago and never read closely:

        take knife
        Take what?

  Task 7 "Use Key" carries `ShowRoomDesc = 0`, the Small Pocket Knife (object
  31, `InitialPosition` 11 = room 7) is lying loose on the floor of Garage 5,
  and the very next command, `out`, moves normally -- so the player really is
  standing in the room and the knife simply does not exist to the parser.  The
  same transcript answers `take directions` and `get in the van` with "Take
  what?" too.
- **humbug, `Adrift_29_humbug.txt`.**  A command-for-command replay of the first 832
  of `cmdfile_humbug.txt` against both master and the branch found **exactly
  one** line where they differ -- `X teeth` at command 723 -- and the branch is
  the one that matches the Runner:

        RUNNER: Nothing Special.
        MASTER: The trouble with being a dentist is that you still have to ...
        BRANCH: Nothing Special.

  Grandad's teeth are a part-of-character static of an NPC the player has never
  had described, so they are unseen and the examine falls through to the
  default.

Two independent live confirmations, zero contradicting evidence, and the
P-code re-read above (`obhere`'s only two `(48) = 1` writes are in its
part-of-character branch; the player-move sweep at `loc_48CA32` is gated on
`global_24 = 1` **and** the static presence array) all agree.  The Renegade
Brainwave probe was never needed -- and on the branch it behaves exactly as
predicted:

    > take crowbar  =>  Take what?
    > look          =>  Yew tree  You stand under the spreading shadow of ...
    > take crowbar  =>  You take the crowbar.

So the note's own decision rule applied: *"Take what?" -> the branch is right,
and the 15 walkthroughs need re-deriving.*

### Landing it

Cherry-picked onto master as `scarier-seen-flag-land`; six files
(`scevents.cpp`, `scgamest.cpp`, `sclibrar.cpp`, `scobjcts.cpp`, `scprotos.h`,
`sctasks.cpp`).  The 15 regressions reproduced unchanged on top of the new
master, and each was repaired by inserting the reveal command a player would
actually type before the first reference:

| row | repair |
| --- | --- |
| renegade_brainwave | `look` before `take crowbar` |
| xfiles | `look` before `take knife` (the measured case) |
| mr_smith | `look` before `take gold key` |
| spirits_flight | `look` before `get cake` |
| spam | `look` first (`DispFirstRoom` off) |
| wreckage | `look` before `take repairbot` |
| imagination | `look` first (`DispFirstRoom` off) |
| valley | `look` before `get gloves` |
| to_hell_in_a_hamper | `look` before `put ear-trumpet in dog's ear` |
| deadman | `look` before `get all` |
| 3monkeys | `look` before `get stone`, **minus** the `z` that followed |
| colony | `look` + one `take all` **replacing** two separate takes |
| lair | `look` before the wake-up `get all`, plus two more `up` |
| wonderwombat | three `look`s, and the maze re-measured 12 -> 15 norths |
| humbug | **no route change** -- only `X teeth` moved |

Two of them could not afford the extra turn and had to stay turn-for-turn
identical: colony's alien kills in two hits (the old route died on the shifted
turn), and 3monkeys' mandrill corners you one turn later.  Folding an existing
turn into the reveal fixed both.

`lair` is the one worth reading.  TASK 313 (`open coffin` in the dream, room
31) moves the cobalt key to room 21 *while the player is still in the dream*,
so no reveal fires -- a task object move only reveals into the player's
**current** room.  The wake-up narration prints no room description, so without
a `look` the `get all` silently misses the key, the chest at the end cannot be
opened, and the game finishes at 221 instead of 226 while still printing its
win marker.  That is exactly the class of quiet loss the marker guard cannot
catch, so **check the score, not just the marker, on every seen-model repair.**
The added turn then desynced the random ruined-stairs collapse, which is why
that row now climbs four times.

Corpus after landing: v4 **303/303 PASS**, a5 unchanged (MATCH 180, DIVERGE 17
all at baseline, NOSCRIPT 2).

The three follow-up probes the parked note listed are now moot for `SPAM.taf`
and `Colony.taf` -- both re-derived and green, and Colony's two pre-existing
"Take what?" lines at golden 218/222 are unchanged, which is the right answer
for dynamics in a described room.  `1HRGAME.taf` (`x little table` then `take
bubbles`) is still worth a live check if a console ever comes back, but it
exercises the `examines` path that was already ported.

## FIXED 2026-08-24 -- `where` / `find` / `locate`, from P-code alone

No golden had ever run this command with an argument the Runner answers
positively, so upstream SCARE's wording had never been checked.  It is wrong
in four places.  Unusually, run370 and run380 decompile to readable VB here
(`run380/run380.bas`, `where`-for-objects at @436F4F, `where`-for-characters at
@440B3E), so the whole handler can be read rather than reconstructed, and
run390/run400 confirm every literal.

- **"<Name> is <lowercased room name>.", never "<Name> -- <Room Name>."**
  The lowercasing is a real `LCase()` call -- `ImpAdCallFPR4 LCase()` at
  run380 dasm @00040B94, and run400 @468143 (objects) / @47FD19 (characters).
  The string `" -- "` occurs in **none** of the four binaries.
  The character branch uses a literal `" is "`; the object branch calls
  `isare()`.
- **"is carrying", not "is holding"** for an object an NPC is holding.  The
  carrying/wearing pair sits together at run400 @467FC1/@46802E and run380
  @4372E6/@43736B.  "holding" was upstream's invention.
- **The object branch drops the "that"**: `"somewhere " & person(5) &
  " haven't been yet."` (run400 @4681B0, run380 @4375D3), where the character
  branch of the same command says `" is somewhere that " & person(5) &
  " haven't been yet."` (run400 @47FD89, run380 @440C11).  An inconsistency of
  ADRIFT's own that all four Runners carry.
- **The smart-alec clause was `#if 0`'d out** upstream.  All four Runners print
  it when the NPC is standing in the player's room, and there is **no comma**
  before "silly" -- the literals are `"  (Right next to "` and `" silly!)"`
  with the perspective pronoun spliced between (run380 @00040BCE/@00040BE2,
  run400 @47FD56/@47FD6A).

`viewroom` opens by copying `room(0)` into `room(4)` (run400 @472053) before
the alt selector runs, so run400's `room(4)` -- the field both branches read --
is the alt-resolved name, i.e. exactly what `lib_get_room_name()` returns.  The
new `lib_print_room_name_lower()` folds that.

Corpus movers: `TheADRIFTProject` ("DARWIN is central communications core.")
and `ticket` ("Young Girl is waiting room."), both re-blessed.  Nothing else in
the v4 corpus reaches these handlers, so nothing else moved.  The lowercasing
reads badly on games that name rooms in title case -- that is what the Runner
does.

### Two leads this turned up

- **`isare()` is not `obj_appears_plural()`.**  The real helper decompiles
  cleanly at run380 @428EAC:

      r = " is "
      If Left(prefix,4) = "some" And Right(name,1) = "s" Then r = " are "
      If Right(name,1) = "s" And Mid(name, Len(name)-1, 1) <> "u" Then r = " are "
      If prefix = "a"  Or Left(prefix,2) = "a "  Then r = " is "
      If prefix = "an" Or Left(prefix,3) = "an " Then r = " is "

  (the first test is redundant; the net rule is *plural iff the short name ends
  in `s` not preceded by `u`, and the prefix is not an `a`/`an` article*).
  SCARE's `obj_appears_plural()` in `scobjcts.cpp` adds a condition the Runner
  does not have: it returns singular for an **empty** prefix, where `isare`
  returns " are ".  Not changed here -- `obj_appears_plural()` feeds 24 call
  sites, several of which the Runner answers with a literal rather than
  `isare`, and VB6's default `Option Compare Binary` makes the `"a"`/`"s"`
  tests case-sensitive in a way SCARE's `scr_compare_word()` is not.  Wants a
  live probe on an object with a blank prefix and a plural short name before
  anyone touches it.
- **"<Name> is dead!"** -- **FIXED, see the section at the end of this file.**
  run390 @459D74 and run400 @47FDB9 answer `where` for
  a character whose room field is `&HFB` (which is **-5**, sign-extended, not
  251 -- `LitI2_Byte`) with that line; run370 and
  run380 have no such branch and no such string.  251 is the battle system's
  corpse marker (run400 sets it at Battles.bas @44B127, right after the
  " falls down, dead." line, and the walk ticker skips every NPC carrying it at
  @4685B6).  Scarier has no dead marker at all -- `battle_npc_die()` hides the
  corpse in location 0, which also means "hidden" -- so this stays with the
  parked *dead NPC still walks* lead above.  Both want the same fix: a separate
  dead flag, because the Runner does keep ticking the walks of a merely hidden
  NPC.

---

## FIXED 2026-08-24 -- a battle-killed NPC is dead for good (the parked *dead NPC still walks* lead)

Closes both halves of the pair above: the `where` answer "<Name> is dead!" and
the corpse that kept walking.  Done from P-code alone -- the console is still
locked, so no live Runner was needed or available.

### What the Runner does

`killchar` -- run390 `run390_3.bas` `@42D410`, run400 `Project/Battles.bas`
`@44B13C`, the same routine either side of the 4.0 rewrite -- does three things
in this order:

1. drops everything the NPC held or wore into the room it died in;
2. **if it has a KilledTask, runs it**, then suppresses the default
   " falls down, dead." line (run390 gates on `var_90(124) > 0` and dispatches
   through the matcher: `MemVar_468118 = tasks(idx-1).cmd(0)` then `tasks(1)`;
   run400 gates on `var_90(206) > 0` and calls the task directly.  `var_90(206)`
   is the **KilledTask index**, not a lives counter -- that was the open
   question from the previous pass and it is now answered);
3. **unconditionally** stamps the NPC's room field:

       loc_42D3FA: push &HFB 'Byte
       loc_42D3FC: var_90(12) = from_stack_1        ' run390, field 12
       loc_44B127: push &HFB 'Byte  ->  var_90(14)  ' run400, field 14

`&HFB` here is **−5**, not 251: it is pushed by `LitI2_Byte`, which
sign-extends.  Same family as the `push &HFF` = −1 already recorded in
`adrift-decompile-signed-byte-literals`.  −1 is the Runner's "hidden"; −5 is
its "dead".

Three independent sites confirm the field is the NPC's room in each build:
the walk-to-hidden write `var_16C(12) = &HFF` (run390 `loc_45ABBA`), the
compare against the player's room `If (var_16C(12) = unk_4082E6.global_0)`
(`loc_45AC74`), and the `where` compare at `loc_459D60`.

Exactly two readers of −5:

* **the walk ticker breaks out of the walk loop** --
  run390 `loc_45A4BC: If (var_16C(12) = &HFB) Then GoTo loc_45ABD0`, and
  `loc_45ABD0` is *after* `Next var_2D4` but before the per-NPC tail, so the
  same NPC still goes through `charbattle`; run400 `loc_4685B6 -> loc_468D61`,
  likewise past `Next var_A0` and before `Next var_94`.  A **break**, not a
  continue -- that distinction was checked, because it decides whether the
  walk's step counter keeps advancing.
* **`where <name>`** -- run390 `loc_459D74`, run400 `@47FDB9`.

And the ADRIFT 4 manual (`~/adrift-battle/runner/manual.txt` l. 2659) states it
outright:

> The default behaviour for when a character is killed (i.e. its stamina
> reaches zero) is for the character to disappear, and any objects it was
> holding are moved to the current room.  Typically you would want to create a
> dead body and have some message notifying the player of the recently
> deceased.

### The port

`NPC_DEAD_LOCATION = -5` in `scgamest.h` plus a `dead` flag on
`scr_npcstate_t`, cleared by `gs_set_npc_location()` so any later move revives:

* `scbattle.cpp` -- `battle_kill()` sets location 0 **and** the dead flag, in
  that order and *after* the KilledTask, matching killchar;
* `scnpcs.cpp` -- `npc_tick_npc()` breaks out of the walk loop on the flag;
* `sclibrar.cpp` -- `lib_cmd_locate_npc()` early-returns "<Name> is dead!";
* `scserial.cpp` -- saves write `NPC_DEAD_LOCATION` and restore special-cases it
  ahead of the range guard, so `.tas` round-trips.

### Fallout: The Town of Azra's economy was never real

Azra (3.90 build) is designed as a renewable hunting sandbox: tasks 19
`#banditkristdies` and 37 `#deerdies` each drop a corpse object, move their NPC
to hidden, and restore its stamina (+30 / +20), plainly expecting the looping
walk (`step0 dest=0`) to bring it back.  It never did in the Runner, so the
author's intro remark -- "you can continue to kill more bandits and sell more
carcasses to gain more money, of course. :)" -- is untested, and **goal 5, the
$7,500 house, is unreachable**: one bandit purse plus one $500 carcass tops out
at $959.68, and Stealth alone costs $800.

The old golden ran 505 turns and sold fifteen carcasses; that route existed only
because Scarier let the corpse keep walking.  Re-derived at `SCR_SEED=26` to
**58 turns**, goals 1/2/3/4/6, wealth $159.68.  Measured en route: 10 attacks
kill the bandit and 4 the deer, and overshooting is free -- `attack` at an
absent or dead NPC is a parser rejection that costs no turn, so the blocks are
self-syncing (11–15 bandit attacks give a byte-identical transcript).

`notes/The_Town_Of_Azra_walkthrough.md` rewritten to match; the harness row
carries the measurement.  Shadowpeak's two routes were re-derived in the same
pass (corpses no longer draw a walk random each turn, which re-threads every
downstream walker and battle roll): `shadowpeak_killwraith` 710 -> **735/790**.

v4 corpus after the port: **303/303**.

## CLOSED 2026-08-24 -- the bracket checkbox governs three more lines

> **Superseded 2026-08-29.**  The measurement policy changed to brackets ON
> (see *Before measuring anything*), so the lines this section removed are
> back for 4.0 and the pre-3.9 echo it declined to restore is restored.  The
> P-code inventory below is still the reference for *which* lines the box
> governs.

The humbug cmd 254 lead ("Scarier prints `(Getting off the stool first)`,
run400 prints nothing") was logged as an engine divergence.  It is not one.
It is rule 1 of *What to do with a diff* -- rule out the Appearance
checkboxes first -- and it was skipped.

**Options -> Display & Media... -> Appearance -> "References in brackets"**
(registry `showbrackets`) does not gate only the pronoun echo already written
up in `RUNNER_TESTS_TODO.md` §4.  From 3.9 on it also gates the mover's two
bracketed lines:

| Runner | `(Getting off X first)` | `(Standing up first)` | gate |
| --- | --- | --- | --- |
| run370 | `loc_42303C` | `loc_423078` | none -- no such menu |
| run380 | `loc_428244` | `loc_428280` | none -- no such menu |
| run390 | `loc_431911` | `loc_4319A0` | `m_showbrackets.Checked`, by name |
| run400 | `loc_450339` | `loc_4503BF` | `MemVar_4942BA = 1` |

`MemVar_4942BA` is `showbrackets`: run400 writes it to the registry under that
key at `4679A1` (`Form1.frm` 6289), and it is the same byte the pronoun echo
is already known to hang on -- `48A095`, the `Sub_20_62` site recorded in §4.
The whole set of nine `MemVar_4942BA` tests in run400 is: six pronoun echoes
(`him`, `he`, `her`, `she`, `it`, `them`, `Proc_19_49_461F38`), the
`ask about`/`talk about` rewrite (`47F15A`, `47F21D`), the general reference
echo (`48A095`), and these two.  Nothing else.

The checkbox starts unticked on every launch and is never restored from the
registry (run400 has a `SaveSetting` for `showbrackets` and no `GetSetting`),
so a default Runner prints neither line.  **Ported**: `lib_go()` in
`sclibrar.cpp` now prints both only below `TAF_VERSION_390`.  43 lines went
across 29 rows -- every one a bracket line, every diff a pure deletion, no
pre-3.9 row touched.  Corpus 303/303.

### The same finding says 7f7349c7 over-reached

`7f7349c7` ("drop the bracketed pronoun echo -- no Runner prints one") is
right about the default and wrong about the mechanism, and the mechanism is
what the commit message argues from.  The Runner *does* print a pronoun echo;
it prints it in **round** brackets, which is why searching run370/run380 for a
`[` literal found nothing and read as proof of absence.  What it really shows
is that upstream SCARE's square brackets are not the Runner's.

run370 `Sub Form1.its` @0002CA9C prints, for each of seven pronouns
(`him`, `he`, `her`, `she`, `it`, `them`, **`one`** -- 4.0 has no `one`),
`"(" & antecedent & ")"` followed by a newline, and it is **not gated**: 3.7
has no Appearance menu.  run380 @000326B4 is the same routine.  The antecedent
is the NPC's Name for the four personal pronouns (`MemVar_4460B4`, seeded
`"Nobody"`) and `tense(Prefix) & " " & Short` for the object ones
(`MemVar_4460AC`, seeded `"Absolutely nothing"`) -- so the pre-3.9 Runner
answers `drop it` with `(the paper aeroplane)`, not with the rewritten
command.

Corpus exposure of the over-reach is one row: `wrecked` (3.80) lost 25 lines
in that commit.  The other thirteen re-blessed rows are 3.90/4.00 and were
right to lose theirs.  Restoring the pre-3.9 half means writing a *new* echo
(round brackets, the antecedent alone, no italics), not reverting.  Not done
here.

## FIXED 2026-08-24 -- the ADRIFT 4.0 output filter (the humbug `Okay.  Okay.`)

The lead was humbug (4.00) command 217 `Put sweet on plinth`:

    run400   Okay.  Okay.  I put the sweet on the plinth.
    Scarier  Okay.  I put the sweet on the plinth.

Neither "Okay." is the author's.  Task 80's CompleteText is a bare "I put the
sweet on the plinth." (`SCR_DUMP_TASKS=1`, which now dumps CompleteText,
AdditionalMessage, RepeatText and ReverseMessage for exactly this reason), and
the game carries one ALR

    [I put ] -> [Okay.  I put ]

whose replacement contains its own original.  That is the only shape in which
the number of times the Runner applies an ALR is observable at all, which is
why it took four probe games to pin down.

### The rule, as measured

Four probe games were built with `harness/make_400_alr*probe.py`,
`make_400_walkcountprobe.py` and `make_400_varfreezeprobe.py`, packed with
`taftool.py`, and replayed in Wine.  Each script's docstring carries its own
cells and the transcript they answered with; the model they add up to is:

1. **A walk of the ALR list** is a full length-descending pass, repeated until
   a pass changes nothing.  An ALR whose replacement contains its own original
   is retired for the rest of the walk it fired in -- but only that walk.
   *(run400 `qqAAA.`, `QQ.`, `done.`; `Adrift_2/3/4.txt`.)*
2. **3.9 is exactly one plain pass** of that list, with no repeat and no
   retirement.  *(run390 `qAAA.`, `PPPP.`, `VVVV.`; `Adrift_5.txt`.)*
3. **A 4.0 turn walks its whole accumulated buffer once at the end of every
   task that completes**, and once more at the flush.  "Every task" means
   every one: tasks an action executes, at any nesting depth, and tasks an
   event's `TaskAffected` runs.  Refusing to repeat a non-repeatable task is
   not a completion and gets the flush walk alone.  *(`Adrift_13/14.txt`:
   `O qqqqqqball.` for four completions plus the event's plus the flush.)*
4. **That pass interpolates variables too**, so each one freezes the values
   then and there.  A task's own change-variable action still reaches text the
   task has already printed -- so 4.0 must *not* checkpoint the buffer before
   a variable change, the way pre-4.0 does -- but a task run by an action
   freezes the text before any action after it runs.  *(`Adrift_15.txt`:
   `B n=9` with the change alone, `A n=5` with a silent task run first.)*

### What it cost, and the one that had to be measured on a real game

Thirty-one goldens moved, all of them consequences of one of three shapes: a
self-containing ALR multiplied once per completing task (sophie's
`[north] -> [north (to the farmhouse)]`, shardsofmemory's
`[I move north.] -> [I move north.<br>]`), a variable frozen one step earlier
(ticket's clock, unauthorized_termination's charge level, the_town_of_azra's
turn counter -- its win marker moved 27 -> 26), and tokens that simply resolve
now where the golden had carried them raw (cursed's `[windmessage=Rixomas]`,
ticket's "telling off about the .").

3monkeys was the one that could not be blessed on a probe's word.  Its "chimp"
task prints `[CHIMPSIGNAL=%signal_to_chimp%]` -- an ALR original built out of
a variable -- then runs a silent bookkeeping task, and only then increments the
variable.  Rule 4 says the text freezes at `CHIMPSIGNAL=0`, no ALR has an
original for that, and the player is shown the raw token while the prose the
author wrote for `=1` arrives one signal late.  That is a bad enough outcome
for a well-liked game to be worth a run of its own, so it got one: run400, the
solution's first 36 commands, every command echoed (`Adrift_16.txt`).

    chimp, get coconut
    CHIMPSIGNAL=0
    The chimpanzee scans the ground immediately near his feet, but there are
    no fallen coconuts to be seen.

The Runner prints it.  Measured, not argued.

### Where it lives

`pf_filter_internal()` and `pf_replace_alrs()` in `scprintf.cpp` hold rules 1
and 2; `pf_refilter()`, called at the end of `task_run_task_unrestricted()` for
4.0 games, holds 3 and 4, and the pre-4.0 checkpoint in
`task_run_change_variable_action()` is now gated `< TAF_VERSION_400`.  4.0 task
actions no longer transfer the turn's buffer out and prepend it back; they hide
it behind a barrier instead (`pf_hide_prefix()` / `pf_reveal_prefix()`), so the
paragraph-spacing helpers still see what they saw before while the filter sees
the whole buffer.  Suite: **303/303 PASS**, and the ADRIFT 5 corpora are
unchanged.

### Left unmeasured

- Whether 3.9 also drops the pre-variable-change checkpoint.  The corpus cannot
  see it either way, so the gate keeps the old behaviour there.
- What run400 does with a mutual `A -> B` / `B -> A` ALR pair.  The repeat loop
  is bounded by the ALR count so it terminates; that bound is a guard, not a
  model of the Runner.

## FIXED 2026-08-25 -- a non-looping walk with StartTask 0 never runs before 4.0

`Adrift_37_melbourne_beach.txt` again, this time for the walk itself rather
than its announcement.

*Melbourne Beach* (3.90) gives Judy a **six-stop, non-looping** walk with
StartTask 0 -- Kitchen 10, Eating area 10, Den 5, Judy's bedroom 15, follow 5,
Outside den 1. Scarier walked her: room 8 on turns 1-10, 14 on 11-20, 5 on
21-25, 3 on 26-40 (`SCR_TRACE_JUDY=1` confirms the suffix-sum arithmetic
exactly). run390 does not. In its transcript Judy is still standing in the
Kitchen at turn 18, and all twenty `give trumpet to judy` typed in her bedroom
on turns 36-55 are refused by task 17's third restriction, "You can't do that
in your present company." (the restriction is *player in the same room as NPC
2*).

Those two observations cannot both be a phase shift. Judy in the Kitchen at
turn 18 needs the walk to start at `s` with 9 <= s <= 18; the bedroom window is
then `s+25 .. s+39`, which always intersects [36, 55]. There is no `s`. The
walk never starts at all.

That matches the P-code. Nothing in run370/380/390/400 seeds a walk counter at
game start; the only thing that ever puts a counter on a walk no task started
is the ticker's *restart a spent walk* branch, and pre-4.0 that branch is
gated on the walk **looping** (run380 441389, run390 45A585). 4.0 made it
unconditional -- which is exactly the version split Scarier already had, but
far too narrowly drawn.

`npc_start_walk_is_390_noop()` used to be `stops == 1 && !loop`, with a comment
naming this very game as the counterexample that proved it could not be wider.
The comment was wrong and the measurement says so: the rule is simply `!loop`,
which subsumes the old one-stop probe result as a special case.

**Cost:** one golden. `melbourne_beach_solution.txt` waited out Judy's walk
with two twenty-turn `give` loops in her bedroom; it now hands her the trumpet
and the music in the Kitchen, where she stands for the whole game, and is 44
lines shorter. Score unchanged, 38/41. Suite **303/303 PASS** -- no other row
in the corpus moved, which is the strongest evidence the wide rule is right.

## FIXED 2026-08-25 -- a catch-all `*` task clears the room refusal

Fixing the walk moved the melbourne replay off-route in two places, which
exposed a second difference the old feed had hidden: `play volleyball` and
`use shower` typed outside their rooms get "I don't understand what you mean!"
from run390 and "You can't do that here!" from Scarier.

run390's `checktask` (run390_3.bas) is the answer. For each task whose command
matched:

    If cmd_matched Then
      If room_ok Then ... GoTo done_with_task        ' the FLAG block is skipped
      If running = 1 And OUT = "" Then               ' loc_44B681
        FLAG = 1                                     ' loc_44B688
        For i = 0 To &H18                            ' the task's 25 command slots
          If task.Command(i) = "*" Then FLAG = 0 : GoTo done_with_task
        Next
      End If
    End If

and at the end of the turn, `If OUT = "" And FLAG = 1 Then OUT = "... can't do
that here!"` (loc_45FFE8/45FFF4). Three things follow, and Scarier had none of
them:

- the scan does **not** stop at the first refusable task. FLAG is overwritten
  by every later out-of-room match, so it is the **last** one that decides.
- a task with a bare `*` in its command list clears FLAG for good. Only the
  forward `Command` list is walked, not `ReverseCommand`.
- the already-done half writes `OUT` as it goes, so the first such task wins
  and ends the scan -- and because the final room message is gated on
  `OUT = ""`, an already-done refusal beats a room refusal raised *earlier* in
  the scan. (The probe-task "theta" ordering still holds: within one task the
  room test comes first, since the done branch sits inside `If room_ok`.)

*Melbourne Beach* has task 94 = `*` confined to room 0, so the room refusal is
suppressed everywhere except room 0.

`run_task_refusal()` now scans every task and `run_task_has_catchall_command()`
does the `*` test. Suite **303/303 PASS** -- nothing else in the corpus has a
catch-all task on a refusal path.

**Where melbourne stands.** With both fixes the 128-command replay is down from
88 differing turns to **29**, and every one of the 29 is rule 1 or RNG: this
Wine run had Verbose OFF, so re-entry is brief ("Entrance hall." against the
full description), `$randwalks` (task 85) picks the NPC enter/exit verb at
random ("Kitty comes in" / "saunters in" / "enters" / "slinks in"), and `play
chess` picks a winner. Nothing left to chase.

## FIXED 2026-08-25 -- the turn an event starts is also a tick

*Orient Express*, run400, `Adrift_36_orient_express.txt`, 53 commands and all
53 echoed. Two turns carried a line we never printed at all:

    turn 43  use phone
      run400   ...  Just as you put the receiver down, the phone rings. [...]
               You tell the man not to hurt Anita, and you want proof that
               she isn't already dead.
      scarier  ...  Just as you put the receiver down, the phone rings. [...]

    turn 46  give card to habibo
      run400   ...  You hear a car approaching. [...]
               You look at Ivanna, and shove her to the ground. She screams
               in horror as the bullets leave holes in the side of the Booze
               barn.
      scarier  ...  You hear a car approaching. [...]

Both extra lines are a `PrefText1`, and both land on the very turn the event
starts:

    EVENT 2 [Phone rings]      starter=3 startTask=16 time1=1 time2=8
       Where type=1 room=16  PrefTime1=2 PrefTime2=1
    EVENT 3 [Driveby Shooting] starter=3 startTask=20 time1=1 time2=5
       Where type=1 room=18  PrefTime1=3 PrefTime2=0

Our `evt_tick_event()` could not print them on any turn. The ES_AWAITING
branch started the event and `break`ed, so the first pref-time comparison
happened a turn later -- and in this game the player leaves the event's single
`Where` room on that next turn, so `evt_can_see_event()` was false and the
text was lost for good.

`checkevent()` settles it, and all four Runners agree. It is one straight run
of `If state = ...` tests over a single event, not a switch, so an event that
moves from "awaiting task" to "running" falls into the running block in the
**same call**: StartText, then decrement, then the two pref-time tests, then
the end-of-event test. The task-started path compensates by rolling the clock
one high --

    run370  431BF0    time = t1 + Int(Rnd * (t2 - t1)) + 1
    run380  439E78    "
    run390  448428    "
    run400  46FE49    "

-- and only that path adds the 1 (the clock-started path, run380 439DE5 and
run400 46FD31, does not). So the +1 and the start turn's decrement cancel, the
event still ends `roll` turns after it started, and our start-turn-does-not-
tick model has always produced the right *end* time. What it could not produce
is a notification whose `PrefTime` equals the whole rolled length: the Runner
compares the post-decrement clock -- the roll itself -- on the start turn, and
we compared nothing at all there.

The ES_AWAITING branch now runs the pauser test first (the Runner tests it
before the decrement and leaves `checkevent()` if it pauses, so a pause on the
start turn suppresses both later tests), then the pref-time notifications,
then the finish test. The clock still holds the plain roll, since our model
never took the +1.

Seven goldens gained a line, every one of them a pref text on an event's start
turn: zombies, orient_express, sun_empire, thepkgirl, great_escape, losttomb,
merry_murders. Suite **303/303 PASS**.

**What is left in the orient replay.** At `SCR_SEED=424242` the run is down to
8 differing turns: 21, 22 and 37-41 are rule 1 (Verbose was OFF, so re-entry
is one brief line), and 52 is the harness eating "[Press any key to end]".
The train-stop texts move with the seed -- event 0 rolls 10..19, events 1..3
roll 1..7 -- so where they land is RNG, not an engine difference. Turns 43 and
46 now match.

**Why only the task-started path was wrong.** `evt_tick_events()` already
re-ticks an event that has just gone from waiting or paused to running -- "a
bit of laziness", as the comment there puts it -- so the clock-started and
resumed paths have always had their start turn's decrement, pref-time tests
and end test, and the ES_WAITING immediate-start hack's `+ 1` exists to
compensate for exactly that re-tick. ES_AWAITING is the one transition the
re-tick does not cover, which is why it alone needed the block added here, and
why it must not be re-ticked: our clock already holds the roll, the value the
Runner reaches only after its start-turn decrement.

The same reading explains the parking rule the EV4 probe measured. The Runner
tests `time = 0`, not `time <= 0`. A clock-started event with `Time1 = Time2 =
0` rolls 0, is decremented to -1 on its start turn and never equals 0 again,
so it runs on forever with its LookText in every room description -- while a
task-started one rolls 0 and then takes the `+ 1`, is decremented to 0, and
finishes on the spot. Two behaviours that had to be special-cased out of the
transcripts fall straight out of that one `+ 1`.

## FIXED 2026-08-29 -- the brackets-ON Runner: `(Getting off ...)`, the pronoun echo, the `again` echo, `<waitN>`

The measurement policy is now "References in brackets", "Prompt for typed
commands" and "Room names in descriptions" ON, Verbose ON (all four forced
into `pfx/user.reg` by `measure.sh`), and Scarier models that Runner.  Four
things changed to match it; 33 goldens were re-blessed, every diff a pure
added line.  Suite 343/343.

**Correction 2026-08-30 (wingman1):** run390 does NOT lack the literal.
`Adrift_3_wingman1.txt` (3.90, brackets ON) answers `in` from the Barstool
with `(Getting off the Barstool first)`, and the decompile agrees: run390
moveroom `431A4C` prints `"(Getting off "` (run390_3.bas:9909) behind
`m_showbrackets.Checked` at `loc_431911`, and `"(Standing up first)"`
behind the same check at `loc_4319A9` -- the same gate as 4.0's
`MemVar_4942BA`.  The census that read 3.9 as having no literal looked in
the wrong pool.  `lib_go()`'s version gate is gone: every version prints
the pair (3.7/3.8 unconditionally, 3.9/4.0 behind the box, which Scarier
models ON).  Nine goldens re-blessed, every diff a pure added line;
suite 395/395.

**`(Getting off X first)` / `(Standing up first)` at 4.0.**  `Monsters_r2.taf`
turns 5 and 23: run400 with the box ticked answers `in` from the bed with
`(Getting off Sissy's four poster bed first)` on its own line before `I move
in.`  `lib_go()`'s gate is now `< 3.90 || >= 4.00` -- 3.7/3.8 print the pair
unconditionally, run390 has no `Getting off` literal at all (its
`showbrackets` tests at `loc_459036/459107` guard only the `ask about` /
`talk about` rewrite), 4.0 prints it behind `MemVar_4942BA`.  28 `Getting
off` and 5 `Standing up` lines came back across the 4.0 rows.

**The pronoun echo.**  `ADRIFTMaze.taf` turns 24-25: `read it` prints
`(a trophy)` on its own line, then the response.  run400
`Proc_19_49_461F38` (him/he/her/she/it/them) tests `MemVar_4942BA` and prints
`"(" & antecedent & ")" & vbCrLf` via `Proc_21_19_47B568`; the antecedent is
the NPC's Name or `tense(Prefix) & " " & Short`, which is exactly the
`replacement` `uip_replace_pronouns()` already builds.  run370 `Sub
Form1.its` @2CA9C and run380 @326B4 print the same line with no gate, so the
echo is gated `< 3.90 || >= 4.00` -- which puts back the 25 `wrecked` (3.80)
lines that `7f7349c7` removed, this time in round brackets.  3.9 prints
nothing.

**The `again` echo.**  run400 `loc_48A058/48A095` prints the recalled command
in the same round brackets behind the same byte; `run_player_input()`'s
`do_again` branch does the same for 4.0.  From P-code only -- no replay has
typed `again` under brackets ON yet (the man_overboard row, 5 echoes, is
staged for it).

**The plumbing: `pf_buffer_reference()`.**  The echo is a paragraph of its
own, and the print filter collapses one leading break when the buffer already
ends in one -- so a task whose text opens with `<br>` lost its blank line
after an echo, and using the `hidden` prefix barrier instead broke the walk
announcement join ("Time passes...  DARWIN enters." split in two).  The fix
is a one-field note, `reference_at`, set by `pf_buffer_reference()` to the
buffer length right after the `)\n`; `pf_buffer_paragraph()` refuses to
collapse when the buffer still ends exactly there.  Nothing else reads it.

**`<wait3>` is a wait tag.**  run400 `loc_47A82C` tests `Left(LCase(tag), 5)
= "<wait"` and takes `Val()` of the rest, in tenths of a second times ten;
`pf_output_tag()` treated `<wait3>` as an unknown tag, so `SCR_MARK_WAIT=1`
never emitted a `[WAIT 3]` mark for it and `make_wine_cmdfile.py` could not
place its `#sleep`.  Now matched by the tag's leading `<wait`, and the
generator reads `[WAIT n]` anywhere on a line.

### Ported 2026-08-29 from the P-code (still unmeasured live): the NPC rewrites

`uip_rewrite_references()` / `uip_note_named_npcs()` in `scparser.cpp`, with a
`last_npc` register in `scr_game_t` (the Runner's `MemVar_494180`, seeded
"Nobody" at `45A7F5`):

- `ask about X` / `talk about X`: `(<npc>)` then a rewrite to `ask <npc>
  about X` (`47F134..47F2BA` in `Proc_19_0_480674`; run390 `459036/459107`
  behind `m_showbrackets`; run370/run380 ungated).
- `give X` with no `to` and no NPC named: `(to <npc>)` then ` to <npc>`
  appended (`48A98A..48AA38`, via `Proc_21_40_45E99C`; run370 `43BED8`;
  NOT in run390, which has no `"(to "` literal).
- The register is assigned at `47F3A2`, inside `Proc_19_0`, for every NPC
  the line names by Name or alias (highest index wins, no presence test),
  AFTER the ask rewrite -- so a rewrite always sees the previous library
  line's character.
- **Ordering is what matters.**  The typed-command task dispatcher
  (`Proc_19_24_44CCE0`, called at `48A481`; run370 `tasks(0)` at `43B97F`)
  runs BEFORE both rewrites, and a matched task jumps past them (`GoTo
  48B4E3`).  So the rewrite lives in `run_all_commands()` after the last task
  pass, and only lines the library answers are noted.  Applying it up front
  cost `sommeril`'s literal task `ask about glass framed page` (rewritten to
  `ask gargoyle about ...`, the task no longer matched) and added echoes to
  seven task-answered give/ask lines (spam, mysteryofcaves, sophie x2,
  sophie_comp, cbn2, asdfa, dayattheoffice); moved, the suite is unchanged
  (343/343), which also means no golden yet exercises a *library* give/ask
  rewrite.  `vardock_bates` turn 16 showed `(to Vagabundo)` live, after a
  lost command; `man_overboard` is staged to measure both rewrites.
- Settled 2026-08-29 by `man_overboard` (Adrift_1_man_overboard.txt): the
  prefix-less object antecedent prints as `(a Cupboard)` (Proc_21_31_448710),
  and the `again` echo and both give/ask rewrites print as Scarier does.

### Tooling

- `harness/make_wine_cmdfile.py <slug> <out>` writes the Runner command file
  from a golden, turns `[WAITKEY]` marks into the PRE count and `[WAIT n]`
  marks into `#sleep n` lines (`drive_ckpt_safe.sh` honours `#sleep N` and
  `#` comments), and prints the `PRE=` to pass to `measure.sh`.
- `harness/compare_wine_transcript.py` in prompted mode; pass `--offset 0`
  when the auto-detect picks a later turn (black_sheeps_gold), and remember a
  room name that equals a later feed command (`Zenes`) is mis-read as an echo.
- `harness/scare` prints no commands without `SCR_ECHO_INPUT=1`.

## FIXED 2026-08-29 -- round two: administrative turns, the take retry, containment before catch-alls

**NPC examine and the nothing-found examine are administrative in 4.0.**
Arena probes EV14/EV15/EV16 in run400 (Adrift_1_ev14..16.txt): `x <present
npc>`, `look at <npc>`, and the "<name> see no such thing." refusal print and
return without a turn count, an NPC walk or an event tick.  In the P-code the
end-of-turn tick at 48B599-48B5C9 runs only when MemVar_494281 = 0 -- the
not-a-turn flag every exit of the NPC block of Proc_19_0_480674 sets, as do
471F02/4801E1 (refusals), `turns`, `score`, "With what?" and open/save.
`x me`, `x <object>`, `look` and `i` are normal turns.  Ported as
`game->is_admin` (sclibrar `examine_npc`/`examine_other`, version 4.0 only;
scrunner skips turns++, NPC/event/battle ticks and updates).  Debug aid:
`SCR_TRACE_ADMIN=1` on the DUMP_TOOLS build prints `ADMIN turn=N after [cmd]`
and the input-line counter.

Seven walkthroughs depended on the extra tick: `humbug` (15 lines), `ticket`
(4), `vague` (2), `escape_to_new_york` (2), `lair` (3),
`yonastoundingcastle` (1), `thelasthour` (3).  Each got a no-op turn after
the administrative examine -- `z` in WaitTurns-1 games, `i` where WaitTurns
is 3 (a `z` there ticks three times).  All seven win again.  Eighteen more
goldens were re-blessed for the event/RNG shift after an admin examine
(yak_shaving, togetyou, confession, trabula, to_hell_in_a_hamper, thepkgirl,
target, perspectives, beanstalk, sandy_meta_number) or for the wait-turn line
break below; the rows carry the evidence.

**"Time passes..." breaks the line in 4.0.**  48ABDA stores the literal
concatenated with Proc_21_4_442418 (vbCrLf), so a walk announcement in the
same turn starts on its own line; run390 45E636 stores the bare literal and
joins.  `pf_buffer_hard_break()` in scprintf keeps that stored newline
through the join filter (4.0 only).  "You have taken N turns so far." is never
singularised (48ACA1).

**Third-person "see no such thing."** -- `lib_cmd_read_other` and the
examine refusals print `<player name> see no such thing.` at 4.0, closing the
two "live lead" rows above.

**The 4.0 take retries per object; the refusal ends in ".".**  `fugitive`
("You stand up from the bed") and `panic` ("You can't take ...", not "!") --
run400 literals.

**Containment before the catch-alls.**  `cellar` `x papers`: run400 finds an
object inside a present container before any catch-all task or "see no such
thing" refusal; the containment pass now runs first.

**3.9 pronoun.**  `cruel` (CAH.taf is 3.90, run390): `take it` answers
"You can't take the jacket." -- the pronoun resolves to the last referenced
object.

Suite re-run after blessing: see the harness row comments for the 25 rows.

## FIXED 2026-08-29 -- Vardock Bates re-drive: `<waitkey 4>` is not a pause, and co() feeds the generic verbs

Third run400 drive of `Vardock Bates.taf` (Adrift_1_vardock_bates.txt; the
first two lost a keystroke each -- feed[26] `examinar el crucifijo` arrived as
`xaminar ...` when typed straight after two keypress pauses, so the cmdfile
now carries a `#sleep 3` there).  Two engine findings came out of the second
drive, before the lost command:

**`<waitkey 4>` is a zero-second `<wait>`, not a keypress pause.**  run400
47A779 compares the whole tag with the literal `<waitkey>`; anything else
whose `Left(LCase(tag), 5)` is `<wait` goes to the timed pause at 47A82C
with `Val()` of the remainder -- `Val("key 4>")` = 0.  The Runner did not
stop after the Jhave wall; Scarier's tag table matched "waitkey" followed by
a space and asked for a key (which is what put a stray blank line, and an
empty-command turn, into the cmdfile).  `scprintf.cpp` now takes `waitkey`
only when nothing follows it; `<waitkey 4>` falls through to the `wait`
entry and the handlers' `sscanf` yields no delay, matching `Val()`.

**The generic verbs see the co() object.**  `tirar de la palanca` is
rewritten by the game's `tirar`->`pull` synonym to `pull de la palanca`; the
`[take/pull/press/move/push]{la}[palanca]` task misses on the "de", and run400
answers `Tiras de  la palanca, pero no pasa nada.` -- "You pull " & prefix &
name (the double space is the game's ` la` prefix), i.e. the generic-verb
routine's object form (48946B, via Proc_19_86_4455F8) with the lever found by
co() up front in generaltasks (Proc_19_85_489F4C, 488478).  Scarier fell to
`lib_cmd_pull_other`.  Two changes: `uip_nothing_follows()` treats a trailing
` *` as nothing (so "pull %object% *" can contain), and
`run_standard_commands()` runs the fallback verb table with containment on,
per row, positional first.  Five other goldens moved by the same rule
(circus, lifesimulation, the_hangover, iachini, foresthouse3 -- see their
row comments); 343 of the suite's 344 rows passed after blessing -- the
remaining one, `vardock_bates` itself, is the next section.

## FIXED 2026-08-29 -- Vardock Bates, round two: the same-turn event re-check, and the SYNONYM table as sequential rewrites

Two more divergences from the full run400 replay of `Vardock Bates.taf`
(Adrift_1_vardock_bates.txt), both now ported; suite 343/344 before, and
the one remaining row is `cursed` (below).

**A finishing event's task re-checks every lower-indexed event it starts,
in the same tick.**  After `decir museo` (TASK 22 pauses EVENT 0 [Jinetes])
the walkthrough's `esperar` printed, in the Runner, the start text of a
second event in the SAME turn, where Scarier printed it a turn later.  The
Runners' checkevent (run400 470754 at 47059C; run390 448EB8 at 448D99) ends
its finish path with

    For i = 0 To n-1
      If events(i).TaskNum = TaskAffected And <game running> Then checkevent i

after running TaskAffected -- so an event of LOWER index whose StarterType-3
task is the one just executed is advanced again inside the finishing
event's own tick, and its start text lands on the same turn.  Higher-indexed
starters see it in the ordinary loop anyway.  `evt_finish_event()` in
scevents.cpp now runs the same loop (through the new
`evt_tick_event_and_settle()`, which is the per-event body the main tick
loop also uses), guarded on `!taskfinished` and on the event having a
starter task at all ("TaskNum" is only present for StarterType 3; reading
it blindly broke every game at load).

Fourteen other goldens moved and were re-blessed, each with a row comment:
ticket, mishmash, fugitive (RNG order), cybercow / cybercow_win (the rain
chain), Glum_Fiddle (one walk line), vendetta (the buzzer cutscene and
"Sally opens the door" now share a turn, so a trailing `wait` is spare),
thepkgirl (filler lines and pauses move; still 55/60), losttomb (the wall
chain), gorxungula (two rabbit lines a turn earlier), shadowpeak and
shadowpeak_killwraith (battle rolls).  Two seed-tuned rows lost their win
and were re-seeded rather than re-derived: `wrecked` (the four train legs
each roll Time 15-20; 234 -> 95, from a 1-700 scan that also found 429, 433,
681) and `shadowpeak_allgargoyles` (Damastus is a random walker the route
waits on; 83 -> 2326, the first win in 1-3000).

**The SYNONYM table is applied as sequential whole-string rewrites.**
`hablar con jason` on the museum terrace got "Nadie escucha tus delirios."
from the Runner (run400 488DB6 via generaltasks, ALR [409]) where Scarier
ran TASK 28 `[talk]{con}[dhirco/jason/jason dhirco]`.  A probe from the
taxi-rank checkpoint (Adrift_3_vardock_bates.txt) settled it:

| input | Runner |
|---|---|
| `hablar con jason` | generic |
| `talk con jason dhirco` | generic |
| `hablar con dhirco` | **TASK 28**, the bastón event starts |
| `hablar con jason dhirco` | generic |

which is exactly what you get if synonym 0 rewrites every whole-word
occurrence of its original in the input, synonym 1 rewrites synonym 0's
output, and so on down the table: with hablar->talk [101],
jason->"jason dhirco" [160] and dhirco->"jason dhirco" [161], only an input
that still lacks "jason" when [160] runs survives; the other three double
the surname.  `pf_filter_input()` used to fire the first matching synonym
per word and let later ones re-match only the whole replacement -- a rule
inferred from Lair of the Vampire (harris<->steve) and Yak Shaving (flags,
line, clothes -> "clothes line").  It is now the sequential rewrite; both
of those games still pass under it (Yak's `x flags` becomes
`x clothes line clothes line line`, which the containment matcher resolves).
The walkthrough's line 93 is now `hablar con dhirco`, the only spelling the
Runner accepts, and the golden was re-blessed; suite 341/344 before the
vardock bless, 342/344 after, with `cursed` still out.

**Harness:** run400 shows the Save dialog only for the first `#save` of a
session and silently re-saves the remembered file after that -- which is
why `ck_vb25.tas` turned out to hold the command-74 state.  `drive_ckpt_safe.sh`
now backs the previous checkpoint up before each save and copies a silent
re-save to its own `ck_NAME.tas`.

**FIXED 2026-08-29 -- `cursed`:** no seed of its own (the row runs unseeded);
a SCR_SEED 1-24 scan found no winning seed, and the earlier diffs (the
shopkeeper's facing for `drag rake`, the king's yes/no order, the
"Nothing seems to make sense" `z`) all turned out harmless.  The real
break was in the mill: the re-check turns warrior 2's seven-room circuit
(events 99-105; 105 -> 99 is the downward step, so event 99 loses a tick)
from 14 ticks into 13, he now leaves the Mill (room 77) the tick before
the third `jump on tray` succeeds, and `bark` found nobody -- task 0b 1523
(`#O#`: warrior 2 next to marker NPC 56 at 77) failed, so `chew rope` gave
"You consider chewing".  Barking from the wheel is not an option (task 1534
kills a player with var 92 == 0), so the walkthrough now waits one lap on
the tray (4 x `z`; WaitTurns is 3, so 12 ticks, bark on tick 13 while he is
outside again), then barks and chews as before.  Re-blessed, 93 points as
before, suite 343/343.  The engine is untouched: run400's checkevent
finish sets state 2 immediately for a task-started event (470747/4706C5/
470734), which is exactly what the port does.


## FIXED 2026-08-29 -- the humbug "seen after restore" lead, and what it was hiding

The lead: after a `#restore` in the checkpointed humbug replay, `x chute`
appeared to behave as if every object had been seen.  It had not.  run400's
loader (46C6BC) restores the seen byte with the rest of the object record,
and no "Which chute" prompt exists anywhere in Adrift_4_humbug.txt.  Lining
the golden up against the transcript instead found three genuine engine
splits, all 4.0, all now ported (`sclibrar.cpp`, `scrunner.cpp`):

1. **Examine of a seen-but-absent noun with several candidates.**  Scarier
   answered only when exactly one seen object matched.  run400's examine
   resolver (Proc_19_88_457034) has a third pass after the present and
   unique-seen ones (456F5D-45702E): split each candidate's Short name on
   spaces, count the words whole-word-present in the typed line
   (Proc_21_38_454CB0), take the unique maximum; a tie is nothing found.
   humbug 1604: `X machine` against the washing machine (Short "machine")
   and two dispensers aliased "machine" -> "I can't see the washing machine
   from here!"; 1602: `X chute` against six chutes all Short "chute" ->
   "I see no such thing." -> ALR'd "Nothing Special.".  Scarier:
   `lib_absent_seen_object()`.
2. **Blocked-exit refusal.**  "You can't go in that direction (at present)."
   is a Scarier invention; no Runner 3.7-4.0 has the literal.  run400's
   movement refusal (475638) counts exits with the door/task gates applied
   (454684, 45459A-45463C) and lists what is open: humbug 1596 `W` -> "I
   can't go in that direction, but I can move north, east and south."
   Scarier `lib_go()`.  Re-blessed xfiles, mangiasaur, fugitive, panic (one
   line each).
3. **`put <unresolvable> ...`.**  The 4.0 put/drop list parser (459DB4,
   entered when the line holds whole-word "put" or "drop") names each piece
   with name_object (46E5D8): the piece is resolved against the objects
   present (463640 mode 2); if nothing, and no task pattern pre-matches the
   line (453C50), it prints "Drop what?" when the line holds "drop", else
   "It is not clear which object you are referring to." (46E165-46E18B).
   humbug 3407/3538/3903.  Scarier printed 'I don't know how to "..."'.
   Ported as the `put *` fallback row `lib_cmd_put_unclear()` (below the
   wear row; defers when the first noun is present -- TheADRIFTProject `put
   batter in remote`, thelasthour `put bowl near spyhole` keep "I don't
   understand what you want me to do with" -- and when a seen-but-absent
   object is named anywhere, which is the p4EXAM `put xyzzy in statue` ->
   "You can't see the statue." control), and `lib_cmd_drop_what()` now
   gives the "not clear" form for 4.0 `put down <unresolvable>`.  The
   literal is 4.0-only (only run400's decompile has it); pre-4.0 unchanged.
   Unmeasured: a seen-but-absent FIRST noun, and an unknown first noun with
   an ambiguous seen-absent second one.

Not splits: every later divergence in the transcript is the game's RNG.
The Viking Contact Society number is `01047%phoneno_bb%` with phoneno_bb
randomised (transcript 010472195080, golden 010473736401), so the golden's
`Type 010473736401 on computer` hits the `type * on * computer` catch-all
("Not numeric format") in the Runner; likewise Olaf's aunty's number, and
from the failed `Say` onwards Olaf stays put, `Get rucksack` is refused and
every later `Put powder ...` lands on split 3.  Suite 344/344 after the
port.  Housekeeping the same day: the Wine transcripts were renamed
`Adrift_N_<game>.txt`.

## Journ2.taf (The Long Journey Home, 3.90) — 2026-08-30, run390

`Adrift_2_journ2_end.txt` (golden + typed endgame probe, 63/64 echoed — the
`#6 start card game` line is skipped by `drive_ckpt_safe.sh`'s `#` rule) and
`Adrift_3_journ2_t5.txt` (21 commands + `i look "x creature" score e "take
shovel" dig fly north w "x card" "x king" i e`, 35/35 echoed).  One split, and
it is the known deliberate one: after `T3 #6 creature looks` fires on entering
the Lair, run390 answers every non-library command there with "You have
already done that." (spent `rep=0` task, patterns include a bare `*`) while
Scarier walks on.  `i` and `x creature` still answer in the Runner.  Not a
new engine question — it is the "task match that says nothing still claims
the command" rule of `adrift4-spent-task-vs-restrictions.md`, deliberately
not imported.  Consequence for the verdict: UNFINISHABLE in the original
Runner as well, ceiling 5/90 there vs our 30/90.

## jailbreakbob.taf (Jailbreak Bob, 4.00) — 2026-08-30, run400

Three sessions (PRE=3, `SCR_SKIP_WAITKEY`-style intro pauses), all commands echoed:

- `Adrift_2_jailbreakbob_golden.txt` — the old 11-command death golden; run400 matches Scarier through "BANG!!! ... Well, he did warn you..."
- `Adrift_2_jailbreakbob_probe.txt` — Hoggins-presence probe (46 cmds). Hoggins is absent in the cell at start and after `n`/`s`, but after the yard-pass `w` from the dining hall and returning `e`,`s` he IS in the cell (`x hoggins`, `talk hoggins` answered; `give comb` → "But I ain't asked you for it yet"). The notes' "moves to the dining hall" claim was a 0/1-based room-index misread.
- `Adrift_2_jailbreakbob_win.txt` — the full chain (38 cmds): comb request fires during the waits, `give comb` accepted, second `talk hoggins` tosses the coin, meeting-room `insert coin` / `4` prank call, `ne` → wife disarms Terry, `get gun`, `n` → "woo-hoo!" `[Press any key to end]`. **The game is winnable in the real Runner**; goldens/jailbreakbob_solution.txt re-derived as a 31-command win (the `look` before `get gun` is required: the gun is not referenceable until listed).

## zelda.taf (The Legend of Zelda: Legacy of a Princess, 4.00) — 2026-08-30, run400

- `Adrift_2_zelda_key.txt` — old 79-command golden through the Graveyard scene, then `get key` / Tree Room `unlock door` / `e`: "The key fits perfectly", Wizrobe Room, 61/197 — above the notes' claimed 59 maximum. Event timing (twig, breathing, OOOOF, CLINK) matches Scarier turn-for-turn on identical input; run400 joins the event StartText onto the room paragraph.
- `Adrift_2_zelda_win.txt` — the full 188-command win (PRE=1, 12 mid-game pauses). Per-command responses match Scarier; ends at `kill ganon with arrow` (EndGame win task) — the Runner closes the transcript at the following keypress so the score summary isn't in the file. First attempt (`Adrift_2_zelda_win_desync1.txt`) lost the `e` after the Moat `z` — the known random cutscene swallow; re-run was clean.

## TenebraeSemper.taf (Tenebrae Semper, 4.00) — 2026-08-30, run400

- `Adrift_1_tenebrae_probe.txt` — old golden as-is: `take pens` falls to the library ("You take the pens from your desk.") and the clock-code chain later blocks at `north` — not a swallowed command, a real split.
- `Adrift_1_tenebrae_probe2.txt` — `take pens` then `get pen`: hands full at `take gold key` (self-inflicted; two pens).
- `Adrift_1_tenebrae_probe3.txt` — `get pens` fires TASK0 `get * pen(s)`; key sequence proceeds. **Task matching is verb-literal**: no take↔get synonymy in run400. Ported as `lib_typed_verb()` in sclibrar.cpp (the library's retry now uses the verb the player typed; the prefix-less retry stays — cobl below).
- `Adrift_1_tenebrae_probe4.txt` — corrected golden + 8 waits: **UNWINNABLE confirmed**. The player is still in the Science Center Hallway, `examine pillow` is the daytime text; rooms 6/7 are only entered by tasks that live in rooms 6/7.

## COBL.taf / GreekSchool.taf — 2026-08-30, run400 (retry-model probes)

- `Adrift_1_cobl_probe.txt` — `look in rubbish`, `take medicine` → "You don't have any. And boy does it show." (TASK `[eat/take] {the} {mind} [medicine…]` matched, failed, and owns the command; no library take). Settles the sclibrar.cpp retry model: keep the prefix-less retry, use the typed verb. Golden `take medicine` → `get medicine`.
- `Adrift_1_greek_probe.txt` (154/154 echoed) — `take exam` → "You'll need the test, first." Golden `take exam` → `get exam`.
- man_overboard `take poster` → `get poster` by the same rule (unmeasured; TASK `Get * poster`). **Retracted 2026-08-30 (evening):** the archive re-sweep showed `Adrift_1_man_overboard.txt:39` running "Get * poster" for `take poster` — the 4.0 *refusal-exit* pre-match is canonical, not verb-literal; see the section at the end of this file. Golden back to `take poster`.

## Del Sol.taf (4.00) — 2026-08-30

No new run. The UNWINNABLE verdict already rests on live run400 probes recorded in notes/Del_Sol_walkthrough.md (win task 26 has Where = room 6 only; the Moreland KilledTask dispatch is gated on its room list). Nothing in the Tenebrae/Hangover work touches those rules.

## hangover.taf (The Hangover, 3.90) — 2026-08-30, run390

- `Adrift_1_hangover_run390.txt` — old golden as-is: turn 2 `x closet` refused (player still on the bed: "can't reach … from your bed"), 4/7.
- `Adrift_1_hangover_run390_standup.txt` (57/57 echoed) — golden with `stand up` prepended: **UNWINNABLE confirmed**, 5/7 like Scarier; both Where=0 endgame tasks answer "You can't do that here!". Golden re-derived with `stand up` first.
- `Adrift_1_hangover_run390_cabinet.txt` (42/42) — `open filing cabinet` / `open cabinet` / `open the cabinet`: the first runs silent +1 TASK8 and prints the game's DontUnderstand text "What you typed doesn't work."; the rest say "You have already done that."; the cabinet stays closed and `take approval form` → "Take what?". Scarier opens it via the library after the task — deliberate deviation (same score), recorded on the harness row. Scarier also lacks the from-the-bed reach rule (not fixed).

## 2026-08-30 (evening) — the archive wipe, the Time Machine restore, and the full re-sweep

Housekeeping first: at 12:00:59 today a session hand-typed
`rm -f pfx/drive_c/adrift/Adrift_*.txt` and wiped all 173 named Runner
transcripts this note cites.  Restored the same evening from the Time
Machine snapshot `2026-08-30-113451` (`tmutil listbackups -m`, `cp -p`, no
clobber); the backup's unnamed `Adrift_1.txt` (a relojero `restore` probe
from 2026-08-29) was kept as `Adrift_1_prewipe.txt`.  Rule going forward:
never `rm` a glob in that directory — `measure.sh` names new files itself.

Then every transcript that pairs with a cmdfile (63 pairs,
`cmdfile_w_<slug>.txt` ↔ `Adrift_N_<slug>.txt`) was re-swept offline
against the current engine with `harness/compare_wine_transcript.py`.
Outcome of the sweep:

- **Clean** (tail-only or zero diffs): the large majority, unchanged.
- **Policy**: pre-08-29 captures ran brackets-OFF/Verbose-OFF
  (goldilocks t94, unravel t11, melbourne/orient re-entry headings).
- **RNG**: orient train events, melbourne walk verbs, cyber2 battle
  rolls, humbug cat — all already recorded above.
- **Feed artefacts**: black_sheeps_gold and cibass show "Do what?" /
  "I don't understand." where the Runner cmdfile carries blank lines for
  cutscene pauses; the goldens (which have no blanks) PASS in the harness.
  Not engine differences.
- **One real lead**, below.

### The lead: `take poster` really does fire "Get * poster" — at the refusal exit

`Adrift_1_man_overboard.txt:39` (`take poster` → the task's authored
blu-tac text) contradicts the morning's re-verb of the man_overboard
golden.  The reconciliation was already in the decompile annotations
(get_piece 473A34 → task_prematch 453C50): run400's per-piece take
consults the tasks **at its refusal exits** with the RESOLVED object and
the canonical `get` — never the typed verb.  Tenebrae's `take pens` never
reached a refusal (the pens are takeable; the library took them), so both
of today's facts hold at once:

- the **pre-action** task pass is verb-literal (`lib_typed_verb()`,
  morning's port — Tenebrae, cobl, greekschool goldens keep `get`);
- the **refusal-exit** pre-match (statics, absent objects) is canonical
  `get <object>` (the 2026-08-29 port of 473A34).

Fix: `lib_try_game_command_common()` gained a `use_typed_verb` flag;
the refusal-exit retry in `lib_take_backend_common` now goes through
`lib_try_game_command_short_canonical()`.  man_overboard's golden is back
to `take poster`, and the full v4 suite is 395/395 PASS.  The
man_overboard replay against its Runner transcript is again clean but for
the expected `[Press any key to end]` tail.

### gamma's fridge: the pre-4.0 examine openness line is "The <Name>", prefix dropped

The gamma drive (run390, 185 commands) came back clean except one turn:
`x mini fridge` answers "This is a small box-shaped fridge.   **The fridge
is open.**  A bottle of rum is on the mini fridge." where we printed "The
mini fridge is open."  The object is Prefix `a mini`, Short `fridge`
(OBJNAME dump), and the openness sentence uses the bare Short name while
the contents sentence in the same message keeps the full name.

The decompile has it as a literal in all three pre-4.0 Runners -- the
line is `"  The " & Name & " is open."`, no prefix anywhere near it:
run370 loc_435629/loc_435659, run380 loc_43CF4A/loc_43CF7A, run390
loc_44BE84/loc_44BEB4.  run400 is different: it composes the name with
the tensed prefix (Proc_21_31_448710, mode 0, at 4717D1/47182B), and
that side was already measured live -- man overboard.taf's drawers are
Prefix `the set of`, Short `drawers`, and run400 says "The set of
drawers is closed."  So 4.0 keeps the prefix exactly where 3.7-3.9 drop
it, and Scarier's single `lib_print_object_np` call was right only for
4.0.

Fix: the openness branch of the examine describer now prints `"the " +
Short` for `< TAF_VERSION_400` and keeps `lib_print_object_np` at 4.0.
Eight goldens re-blessed (cybercow_win, deardiary2, fantasyworld, gamma,
report, the_hangover, villains_and_kings, wrecked) -- every diff is the
openness sentence alone, and several read better for it ("Heavy trunk is
open." -> "The trunk is open.", "Cracked Broken Window" -> "The Broken
Window").  The archived hangover run390 transcript already carried the
proof unnoticed: `Adrift_1_hangover_run390.txt` says "The closet is
open.  A two dollar bill is inside your closet." -- bare name in the
openness sentence, full name in the contents sentence, in one message.
Suite 395/395, wheretest 3/3.

### richard's homecoming: 3.9 alone runs the win text through pspace()

The richard drive (run390, 70 commands) came back clean except the winning
turn: the Runner joins the final task's completion text and the game's
WinText with the two-space separator -- "...you return to the staging
area.  Rich smiles as you hand him the recall beacon..." -- where we
butt-joined them ("area.Rich").  The SCR_DUMP_TASKS dump clears the
authored-spaces theory: COMPLETE ends "staging area." with nothing after
the period, and WINTEXT starts at "Rich".

The decompile pins it to one call: run390's win branch (execute_task
43F72C) at loc_43F252 tests the win flag and, before appending the
WinText global (MemVar_468148), calls pspace() (loc_43F255; the sub at
42C920: append "  " unless the buffer is empty or already ends with "  ",
Chr(10) or "<br>").  The call is unconditional -- it runs even when the
WinText is empty.  It is also 3.9's alone: run380 is the measured
butt-join (microwaveman, "You win the game.You have destroyed Coffee
Man..."), run370 shares 3.8's inline join with no pspace sub at all, and
run400 is the measured butt-join-plus-terminator (ptbad).

Ported as pf_buffer_pspace() (scprintf.cpp), an exact pspace: unlike
pf_buffer_join() it never pops a trailing authored newline (that pop is
what the ECOD3 "<br><br>" two-blank-lines measurement needs from the
join, and pspace has no such removal).  Called from
task_print_end_game_message()'s win branch for 3.90+ pre-4.0 games only.
Twenty-three goldens re-blessed, every diff the join alone -- and two of
them are their own corroboration: the archived tcom transcript prints the
WinText line as "  [This ends the first part..." with the two leading
spaces run390 really emits at the start of a line (the buffer it tested
ended in neither break nor spaces), and deaths/wingman1 gain three-space
runs because the authored text ends in ". " -- pspace only refuses to
stack on an exact trailing "  ".  Suite 395/395, wheretest 3/3 after.

### cleft's packing case: a 3.9 event move never stamps the seen byte

The cleft drive (run390, 90 commands) diverged on three turns, all one
cause.  The klaxon event (Time1=Time2=15, started by the lever task)
moves the packing case to the Loading bay when it finishes -- the player
is standing there, and "The klaxon stops sounding." printed identically
on both sides -- yet the Runner then answers `open case` with "You can't
open that." and `get coin` with "Take what?", where we opened the case
and won the game.  The delivery is silent: nothing ever LISTED the case,
so under the seen model it is unreferenceable -- in 3.9.  Scarier stamped
the seen byte in its event mover whenever the object landed in the
player's room, a rule read out of run400 (@456124: compare the freshly
written location against the player-room global, stamp seen(48)).
run390's mover (448B94-448CE3, inside checkevent 448EB8) has no such
tail: it writes the location (22), the static room-presence array (24)
and the parent (42), and falls off the end on every branch.  The stamp is
now gated to TAF_VERSION_400+ (scevents.cpp evt_move_object); with it,
Scarier's replay reproduces the Runner's refusals word for word.

The route needed the reveal a player would need: a `look` after the nine
`z`s lists the case ("timing" costs nothing -- the klaxon is the game's
only event and it is already finished).  Golden re-derived and
re-blessed, still 100/100.

Open question from the same drive: with the coin never taken, run390
answers `put coin in slot` with "You can't do that!" where Scarier says
"I don't understand what you mean!" (the task matches, its held-coin
restriction fails with an empty FailMessage -- compare the run390
silent-task DontUnderstand rule from the Hangover cabinet, which this
contradicts on the surface).  The turn is off the corrected route, so it
is recorded here rather than chased; 3.7/3.8's event movers are also
unread -- the gate models them as 3.9 (no stamp) pending a measurement.

## Merry_Murders.taf (The Merry Murders, 3.90) — 2026-08-31, run390

Clean full replay: `cmdfile_w_merry_murders.txt` (79 lines -- 64 commands
plus waitkey blanks), transcript `Adrift_3_merry_murders.txt`, every command
echoed.  (The earlier `Adrift_40_merry_murders.txt` probe desynced at command
4 -- the Act II cutscene swallowed a keystroke -- and must not be quoted.)

**The walk step is exact-tick-gated, move included -- MEASURED and PORTED.**
Task 9 starts Trey's walk (counter seeds `1 + ΣTimes`, he arrives in the
Plaza on the next tick and the walk is spent); task 27 `research alex` then
moves him to room 2, the east Hallway (`ACT type=1 v1=7 v2=0 v3=3`).  In the
Runner he then *stays there for the rest of the game* (transcript lines
134/167/192/300/324/361 show him in the east Hallway with his ChangedDesc;
line 251 `show list to trey` in the Plaza gets "Trey's not here!").  Scarier
used to re-resolve the walk destination every tick and warp him back to the
Plaza.  run390 runs the entire walk step -- destination resolve, move, meet
dispatch, announcement, `&HFF` hide stamp -- inside the
`counter == suffix_sum` gate (`loc_45A780` .. `loc_45ABC7`); run400 is the
identical shape at `loc_468841`.  `npc_tick_npc_walk()` now gates the
fixed-room and Hidden destination resolution on `is_exact`; follow-player
was already arrival-gated and the roomgroup refresh is deliberately left
per-tick (the "Ticket to No Where" canary).  Same mechanism as provenance's
butler, whose exit announcements now fire at his real ticks.

Decode confirmations from the same replay: walk-stop dest `N>=2` -> room
`N-2` (Mary stop 3 -> room 1, the janitor Hallway; Nancy walk 4 stop 2 ->
room 0, the Plaza); dest 1 = follow player; dest 0 = hidden.  3.9 walk
struct: `(0)` StartTask, `(4)` Loop, `(8)` stops, `(12)` NumStops, `(13)`
counter, `(18)` StoppingTask.  Seeding really is `1 + total`: run390's
task-completion handler (`loc_43F095`) *scans every walk's StartTask*,
where Scarier reads the task's parse-time `NPCWalkAlert` list
(sctafpar.cpp) -- built by the same scan, so they cannot disagree.

Also exhibited, all previously known:

- **The pre-4.0 spent-task claim** at task 46: after `n` unlocks the
  archives, the Runner answers the second `n` with "I have already done
  that." (game ALR rewrites "You " -> "I ") and never enters the archives,
  ending 100/135.  Deliberate deviation, NOT imported (the journ2 rule);
  Scarier's `n`, `n` walks through to 135/135.
- **The waitkey butt-join**: across authored `<waitkey><cls>` sequences the
  run390 transcript butt-joins the two texts with no separator (feed turns
  2/11/22/31/51); Scarier emits a space.  Transcript artifact -- the `<cls>`
  wipes the live screen, so the join never displays.  Accepted.
- **Open, minor**: feed turns 45/46 (`move pile` / `open panel` typed a room
  early) -- run390 prints its out-of-room FLAG text where Scarier's library
  answers first.  Wording only, no state; same family as the FIXED
  2026-08-25 catch-all-`*` section.

Corpus fallout of the port, all triaged 2026-08-31: shadowpeak x3,
great_escape, textident_evil, thehunter (walker pre-move lines vanish),
provenance (the butler's exit announcements appear at his real ticks --
the old bare "The butler exits." lines were the re-drag bug) -- all still
hit their win markers, re-blessed.  merry_murders re-derived (`show list to
trey` moved to the east Hallway, 66 commands, 135/135).  thetest_win
re-derived (the Robot Guard's storms-in/out schedule shifted; 191 commands,
still 20/25 "Well done!  You won!").
## Vampire.taf (The Vampire With A Conscience, 3.90) -- 2026-08-31, run390

Feed `cmdfile_w_vampire.txt` (84 commands, PRE=1), transcript
`Adrift_3_vampire.txt`; `compare_wine_transcript.py` RULE 2 clean -- every
command echoed.  The seeded Scarier replay of the same feed WINS 100/100.
The Runner ends **walled at 70/100**, stuck in the Bozo backyard.

- **Mechanism**: T61 (`east / e / go east / go e`, where=room 11,
  Repeatable=0, action move-player -> nightclub, **RepeatText=' '**) fires on
  the first exit (feed 63, "You enter the nightclub again.").  At feed 73 --
  the exit after raising Jon -- the spent task claims `e` and run390 prints
  its **RepeatText**, a single space, so the turn is whitespace-only; every
  later `e` is claimed the same way forever.
- **The real run390 cannot finish this game.**  Room 11 has exactly one exit
  (east, and no in/out), T61's four patterns cover every phrasing of it, and
  T114 (the "Hay, you're not supposed to be back here" bounce) is a
  `-`-prefixed system task, event-fired only.  Any route needs two backyard
  exits (stash the corpse; come back to raise Jon), and the second is always
  claimed.  The 70 points on the wall's near side are exactly what the
  transcript's `score` shows (70/100).  Same family as Journ2's Lair brick.
- **New Runner fact**: the pre-4.0 spent-task claim prints the task's
  RepeatText when non-empty, and the default "You have already done that."
  only when RepeatText is empty (Journ2 T3#6 RepeatText="" -> default;
  Vampire T61 RepeatText=' ' -> blank turn).
- **Scarier**: 100/100 stands on the documented deliberate deviation (the
  pre-4.0 spent-task claim is NOT imported -- see the Journ2 section and
  `adrift4-spent-task-vs-restrictions`); no engine change, golden untouched.
- **Benign RNG diffs**: turn 12 "A green porche drives past you" (ranged
  traffic event, fired in the Runner only, 1 vs 0 occurrences); Jon
  Simonsen's club arrival lands one turn apart (the turns-45/46 splitter
  wobble, content identical).  Turns 47-71 byte-identical after whitespace
  normalisation.

## superliam.taf (Super Liam, 3.80) -- 2026-08-31, run380

Feed `cmdfile_w_superliam.txt` (86 commands, PRE=0, identical to the
solution), transcript `Adven_1_superliam.rtf` (Save Transcript at the 85th
command; the 86th, `east`, sent after the save so its output is not in the
.rtf).  RULE 2 clean -- 85/85 echoed.  Three divergent turns, two engine
rules, both fixed; the seeded replay wins 3250/3250 on both sides.

- **AdditionalMessage vs a room description (turns 82/83, FIXED)**: run380
  builds each turn as one flat VB string and appends `"  " & AdditionalMessage`
  only when the string does not already end in two spaces (run380.bas '4D001).
  Rooms 38/39 have Longs ending `"  "`, and tasks 22/23 (`press button`, the
  `up` cutscene) show them via ShowRoomDesc -- so "a cat runs out of a door,
  and sings a song" and "X1 raising his gunlike arms and aims at you.  Do
  something now!" are SUPPRESSED in the Runner, and "The Mermaid follows you."
  joins straight on.  Scarier's 3.80-only `task_suppresses_additional_message()`
  already modelled the rule but could not see the double space through the
  room-description block's terminating `'\n'` -- that newline was a bare
  `pf_buffer_character()`, invisible to `pf_ends_with_double_space()`.  Fixed
  by recording it as an auto-break: new `pf_note_trailing_auto_break()`
  (scprintf.cpp), called from `task_show_room_desc()` (sctasks.cpp).
- **Raw name matching (turn 25, FIXED)**: object 9's Short is
  `"necko wafers "` -- authored with a trailing space.  run380's `c()`
  ('429048) matches the RAW stored Short/Alias case-insensitively by InStr
  and requires the character after the match to be a space, comma or
  end-of-input; `co()` ('42DE60) feeds it the raw obj(4)/obj(8) strings.  A
  trailing-space name can therefore only match input holding two consecutive
  spaces -- never typeable -- so the object is unreferenceable: `take necko
  wafers` answers "Take what?".  The task-matched `eat necko wafers` still
  fires (task patterns, no noun resolution), so the game stays winnable.
  run390's `c()` LCases both strings but keeps the same InStr shape, and the
  4.0 strict comparator already refused the trailing space -- only Scarier's
  tolerant `uip_compare_reference()` forgave it, by collapsing the name's
  trailing whitespace.  Fixed: a name whose whitespace runs to the end never
  matches (scparser.cpp; the containment pass `uip_contains_words()` and the
  3.90+/4.0 strict comparators were already raw).  The 3.80 resolver has no
  seen gate and no ambiguity pass: 75 lines, raw InStr, first hit wins.
- Corpus after both fixes: **408 PASS**, sole golden change
  `super_liam_solution.expected.txt` (3 lines), re-blessed; win marker kept.

## haunt.taf (House of the Damned, 3.80) -- 2026-09-04, run380

Feed `cmdfile_w_haunt.txt` (85 commands, PRE=0, identical to the solution),
transcript `Adven_1_haunt.rtf` (Save Transcript at the 84th command; the
85th, `down`, is the win and was sent after the save, so its output is not in
the .rtf).  Driven with the new `measure38.sh` -- the run380/run370 twin of
`measure.sh`: registry-only Verbose/Sound pre-flight in `pfx/user.reg` with
nothing of ours running, fresh launch, startup-alert dismissal, all-but-last
commands through `drive_ckpt_safe.sh`, then Adventure (menu bar +30/+43) and
`t` for Save Transcript with the "Transcript saved" MsgBox detected as an
extra top-level window before its Return is sent, then the last command.  It
replaces `runner_savetranscript.sh`, whose blind Ctrl+V TOGGLES Verbose (the
registry value is what the Runner starts with -- run380.bas 431963
`GetSetting "Verbose"`, default False), so a prefix already at Verbose=True
came up Verbose OFF and every re-entry heading went missing.  RULE 2 clean
-- 84/84 echoed.  Forty divergent turns before the fix, one after the first
rule, none after the second; the seeded replay wins 84/84 on both sides.

- **No startup event tick before 3.90 (40 turns, FIXED)**: run390's tstart
  (42E940) calls `events()` at 42E90B straight after the opening viewroom and
  run400's tstart calls 449310 the same way -- that is the tick Scarier's
  startup `evt_tick_events()` reproduces, and the `+1` on every StarterType 2
  delay in `evt_start_load_events()` compensates.  run380's `events()`
  (425094) and run370's (432538) have exactly ONE caller each: the tail of
  generaltasks.  Their load code (run380 448DC9 / run370 440083) puts a
  StarterType 1 event straight into RUNNING with its rolled length and no
  StartText, and a StarterType 2 event into WAITING with its rolled delay;
  checkevent (run380 439DA5) then decrements and starts on `= 0`, so a delay
  of N starts on turn N with nothing to compensate.  haunt's Weather (delay
  1, restart 2) and Wolves (delay 1) events therefore started AT LOAD in
  Scarier and on turn 1 in the Runner, and every weather/wolves line sat one
  turn early.  Ported as version gates in scrunner.cpp (the startup
  `evt_finish_load_events()` + `evt_tick_events()` pair) and scevents.cpp
  (the `+1`), both `>= TAF_VERSION_390`.  Immediate events are unaffected:
  bare length, first decrement on turn 1, same finish turn.  A ZERO-length
  immediate parks for ever in 3.8 (the clock goes negative past the `= 0`
  test at 43A474) -- decompile-read, not measured; the corpus has one such
  event (wrecked EVENT 35, which un-completes a task nothing ever completes).
- **No administrative turns before 3.90 (turn 83, `score`, FIXED)**: after
  the first fix the one remaining divergence was `score` at turn 83 -- the
  Runner followed "Your score is 84 out of a maximum of 84.  (100%)" with the
  Weather FinishText and the grandfather-clock line, which Scarier had
  swallowed as an administrative turn.  run390's generaltasks (460D6C) sets
  its flag 468219 for history / score / count / information / end / turns and
  guards the end-of-turn `characters()` + `events()` (460675/46067A) on it;
  run380's generaltasks tail (443160-44317E; run370 43C88D-43C8AB) ticks
  after EVERY command that left output unless the game has ended.  Only
  `opensave()` (save / restore / restart, `GoTo 443326`) and quit (which
  Unloads the form) bypass it, and the turn counter (44F138 at 441A21)
  increments on every command as well.  Ported as `lib_set_admin()` in
  sclibrar.cpp -- `is_admin` only from 3.90 -- on the meta-commands 3.8
  recognises and answers: score, turns, count, hint, help, about, clear,
  history, where.  Verbs 3.8 does not know (version, license, notify,
  status, brief, verbose ...) keep their 4.0 behaviour, since the Runner
  would answer "I don't understand" for them anyway.  `undo` is left admin:
  3.8 has no undo ("I can't undo your blundering.").
- Corpus after both fixes: seven 3.80 goldens moved -- haunt, great_escape
  (its mid-chase `score` now gets the "sirens" event line), and marooned /
  wrecked / twilight / tom_ceader (secret.taf) / timmy_reid (tra.taf), whose
  delayed events all have RANDOM delays and moved only through the RNG
  stream (one draw fewer at startup).  None changed its score or win;
  wrecked's seed-tuned train waits needed a re-pin (95 -> 106; 150 also
  wins).  cave and haunted, the other exposed 3.80 rows, did not move.
  The tra.taf replay (`Adven_9_timmy_reid.rtf`) still agrees with Scarier
  on everything but its RNG-timed lines, as it did before.
- **Open lead, 3.90**: the run390 admin set above is history / score / count
  / information / end / turns only.  Scarier also marks hint, help, clear,
  where and a dozen 4.0-era verbs administrative for 3.9 games; nothing has
  measured `hint` / `help` / `clear` / `where` under run390 yet.

## 2026-09-04 -- jb2000 / Crime_Adventure / mikes / great (run380): the 3.80 `take` -> `get` rewrite, the co() prompt ported

Four more 3.80 replays (rows in "Measured so far").  Two engine findings,
both ported; one correction; one RNG dead end.

- **run380 rewrites the typed command before matching** (generaltasks
  441C3F-441C72): `everything` -> `all`, `slap` -> `hit`, `take` -> `get`,
  `except` -> `but`, each a whole-word replace on the raw input.  run370 has
  only the first two.  run390 has no `change()` function at all, and
  run400 rewrites take->get only inside its get handler, after task
  matching -- so the generic rewrite is **3.80-only**.  The take->get one is the visible one: any 3.80 game whose
  tasks are written with `get` (jb2000: all of them; Crime_Adventure; the
  great.taf picasso task in the other direction) matched in run380 and fell
  to the library in Scarier.  Ported in `scprintf.cpp` `pf_filter_input()`
  as a BUILTIN rewrite table after the game's SYNONYM pass, version-gated
  per row.  jb2000 and Crime then replay with 0 differences; great line 41
  had to become `steal picasso` because the rewritten `get picasso` matches
  no pattern of that task.
- **The co() end-of-turn ambiguity prompt is ported for 3.7/3.8** -- see the
  addendum under "DIAGNOSED 2026-08-24 -- the Runner's co() object-ambiguity
  test".  3.9 is handler-scoped and deliberately unported; the global
  version regressed six 3.90 goldens and the hangover transcript proves
  run390 prints nothing there.
- **Correction**: run380 DOES take the keys at the mikes prompt; the prompt
  replaces the output only.  And the mikes shift from cmd 53 was never a
  consequence of cmd 27 -- it is the random-length "auction" event.
- **great.taf's car chase is RNG** (four events with random lengths); one
  replay survived it, one died on the identical feed.  Only the 121 turns
  before `break into car` are measurable, and they match.  Two Save-Transcript
  footguns learned the hard way: a dummy command after the winning move
  lands the save in the end-of-game modal chain and wipes the scrollback,
  and a mid-run death restarts the game and does the same -- both give a
  ~3 KB .rtf with nothing in it.  The game-ending command must be the LAST
  one in the feed (it is never echoed anyway, RULE 2).
- **Open, unmeasured**: two things seen in the OLD-walkthrough replay
  (`Adven_1_greatc.rtf`) that the new feed does not exercise -- a 3.8
  room-refusal ("You can't do that here.", a task's Where restriction)
  answering where Scarier's library refuses for an unseen object (greatc
  turn 49, and the turns after it cascade from that), and run380's task
  wildcard `get *knives*` matching `get meat`.  Neither is in a wired
  walkthrough today; both need their own probe.

**Parked here 2026-09-04.**  Next candidate is the next unmeasured row in
the "### 3.80" table above; the harness is 428/428.

## 2026-09-05 -- the rest of the 3.80 table (akron, microwaveman, duck, first, haunted): all clean

Five rows, no engine finding.  akron's 2026-08-24 transcript re-compared
against the engine as of 9c7c1691 (43/43); microwaveman (8/8), duck
(12/12), first (18/18, dummy `look` after the non-EndGame ending) and
haunted (115/115) each replayed fresh in run380 with `measure38.sh` and
matched on every echoed turn.  The 3.80 candidate table is therefore
complete: 10/10 measured, and the 2026-09-04 engine changes (no startup
tick, admin turns tick, take->get rewrite, co() prompt) hold on all of them.

**castle.taf followed the same day**: 16/16 clean in run370 (`Adven_1_castle.rtf`,
`measure38.sh ... run370.exe` works unchanged for 3.70).  That closes the
pre-4.0 half: every 3.70 and 3.80 candidate is measured, and the only
pre-4.0 divergences left are arlo's two diagnosed, deliberately unported
run370 ones (the walker departure lines and `get out of bus`; see the arlo
bullet).  Next candidate: the first unmeasured row of the 3.90 table.

## largo-winch.taf (Largo Winch, 3.90) -- 2026-09-05, run390

Clean full replay, `Adrift_3_largo_winch.txt`: 323/323 commands echoed,
322 turns byte-identical (French, CP1252), and the last differs only by the
Runner's `[Press any key a end]` after the 97/97 score line.  No engine
change.  `measure.sh ... run390.exe 0` unchanged; `#save` works in run390
exactly as in run400 (dialog for the first save, silent re-save after).

Two DRIVER findings on the way, both fixed in
`~/adrift-battle/runner/wine/drive_ckpt_safe.sh` (now types every line
through the new `type_line.py`):

* **AppleScript `keystroke` drops accents.**  `regarder sous le canapé`
  reached the Runner as `regarder sous le canape` (first drive's
  FIRSTCHECK abort, `Adrift_3_lwprobe.txt`).  The Swedish layout's dead
  keys DO reach Wine, so accented letters are now typed as dead key +
  base letter (key code 24 = acute, shift = grave; key code 30 =
  diaeresis, shift = circumflex; `ç` types as itself).  Probed live:
  é è à û î ï ç all arrive as their CP1252 bytes.  **Caveat for the
  earlier French drive**: `Adrift_1_qui_a_tue_dana.txt` lines 142/145
  show `prendre telephone` / `x telephone` -- that run400 drive typed
  the accents away too, and any accent-sensitive command in it was not
  measured as fed.  Its rule-2 desync happened later anyway.
* **Embedded double quotes broke the osascript** (`composer
  "9002472832"` -> "A real number can't go after this" -> nothing typed,
  an EMPTY command submitted, everything after it out of step).
  `type_line.py` escapes quotes and backslashes.

`compare_wine_transcript.py` needs no change: the transcript is CP1252 and
it already decodes it.  The FIRSTCHECK grep in the driver now converts the
UTF-8 feed line to CP1252 before looking for the echo.

Next candidate: `mudergreatfalls.taf` (255 commands, 3.90).

## mudergreatfalls.taf (Murder in Great Falls, 3.90) -- 2026-09-05, run390

Clean: `Adrift_3_murder_great_falls.txt`, 101/101 echoed, all turns
identical, the last (`accuse ken`) cut at the Runner's endgame pause.  The
table's 255 is the golden's line count; the feed is 103 commands.  Two
harness lessons, no engine finding:

* The name/gender dialogs come **at load, before any intro text** (a black
  window under the gender box), and `measure.sh` used to send the PRE
  pause-dismissals before the popup answers, so they only hit the dialog.
  The popup block now runs first; PRE=2 then dismisses the two intro
  pauses as intended.
* `type_line.py` called osascript with no script for a waitkey BLANK line,
  and osascript read the rest of the command file from stdin as its
  script -- the first drive (`Adrift_3_mgf_drive1.txt`) stopped dead at
  the first blank.  Fixed (an empty line types nothing, stdin is
  /dev/null).

Next candidate: `report.taf` (3.90).

## report.taf (Report of Espionage, 3.90) -- 2026-09-05, run390

Clean: `Adrift_3_report.txt`, 165/165 echoed, every turn identical, the
winning `burn report` differs only by the Runner's `[Press any key to end]`.
Name/gender dialogs again (`POPUP_ANSWERS="Sam|male"`, feed stripped of
the two answer lines, compare `--start 2`).

## Archie's Birthday V 1-2.taf (3.90) -- 2026-09-05, run390

`Adrift_3_archie.txt`, 205/205 echoed (the .taf copied into the prefix as
`ArchiesBirthday.taf` -- the apostrophe and spaces do not survive the
command line).  Two turns differed, and both were Scarier's fault:

- **`x me` (turn 0):** the game's PlayerDesc is the ALR key
  `[player=%player val%]`, and run390 printed the substituted paragraph
  ending `...self-delusions.` where Scarier ended it without the stop.
  run390 `examines()` @44C488 (loc_44C1F1-44C21E) appends `.` to the reply
  when `Right$(text, 1) <> "."` -- tested on the raw text, before the ALR
  pass, so the key gets the stop and the paragraph inherits it.  run400
  `Proc_19_87_471F94` (loc_471C6D-471D40) does the same and also lets `!`,
  `)`, `%` and `?` stand.  Ported to `lib_cmd_examine_self()` for 3.9+ (the
  position clause already ends with `.`, so only the bare description
  needs it).  The 4.0 half was then measured directly: a one-command run400
  probe on `yak_shaving.taf` (`Adrift_4_yak_probe.txt`) answers `x me` with
  `You are somewhat raggedy looking after your journey.` for a PlayerDesc
  that has no stop.  3.7/3.8 build the reply from the position alone
  (run370 @435C9C, run380 @43D5EC -- no PlayerDesc text at all in the
  P-code) and are left as they were; see RUNNER_TESTS_TODO.md -- no 3.80
  corpus game has a PlayerDesc and no pre-3.9 golden examines the player,
  so nothing is exposed.
- **`take it` (turn 8):** run390 printed `(a camcorder)` on its own line
  before `You take a camcorder from the desk.`  Scarier's pronoun echo was
  gated `< 3.90 || >= 4.00` on a misreading of run390 ("keeps showbrackets
  only for the ask/talk-about rewrite").  run390 `Sub its()` @43D968 tests
  `m_showbrackets` (loc_43D6C2 for `it`, 43D7BF `them`, 43D884 `one`) and
  prints `"(" & MemVar_46811C & ")"`; the menu item is read from
  `ADRIFT\Runner showbrackets` at loc_44526F with default True.  Gate
  removed -- every Runner echoes.  The article is where 3.9 and 4.0 part:
  3.9's `takes` (@455067) and `drops` (@445BE6) compose the antecedent in
  mode 1 (authored Prefix), so there is no `(the X)` after a take.  Measured
  on `veteran.taf` (run390 probe, `Adrift_3_veteran_probe.txt`): `x bag`,
  `take it`, `open it` echo `(a bag)` both times, exactly the re-blessed
  golden.  `uip_definite_form()` stays 4.00-only.

Suite after the port: `archie`, `veteran`, `cruel` gain bracket lines
(`(a camcorder)`, `(a bag)` x2, `(a jacket)`), `archie` and `yak_shaving`
gain the full stop on `x me`; all four re-blessed, 428/428.

Next candidate: `croft.taf` (3.90).

## croft.taf (3.90) -- 2026-09-05, run390

Full run390 replay, `Adrift_5_croft.txt` from `cmdfile_w_croft.txt` (101
real commands -- the candidate table's 193 is the golden's line count
including `#` comments; PromptName 0, PRE=0).  101/101 echoed.  Zero
engine divergences: the compare's only diff is the Runner's own
`[Press any key to end]` after the end-of-game score summary, which
Scarier never prints.  The 3.9 examine-self full stop port from Archie's
Birthday is not exercised here (the walkthrough never examines the
player), but the PlayerDesc ends in `.` anyway.  Golden unchanged,
150/150.

Next candidate: `DarkTower.taf` (3.90).

## DarkTower.taf (3.90) -- 2026-09-05, run390

Full run390 replay, `Adrift_6_darktower.txt` from `cmdfile_w_darktower.txt`
(121 commands, PromptName 0, PRE=0).  121/121 echoed.  Zero engine
divergences: again only the Runner's `[Press any key to end]` after the
end-of-game summary (a 0-of-0 game, so "100% ... maximum points" on both
sides -- the summary wording for MaxScore 0 already matches).  Golden
unchanged.

Next candidate: `FarFromHome.taf` (3.90).

## FarFromHome.taf (Far From Home, 3.90) -- 2026-09-05, run390

Clean in the end, but only the second drive proved it, and the first one
is the lesson.

**Result.**  `Adrift_8.txt` from `cmdfile_w_ffh_nock.txt` (71 commands,
PromptName 1 -- `POPUP_ANSWERS="Sam"` -- PRE=0): 71/71 echoed, zero engine
divergences.  The compare's only diff is turn 70, the winning `lost`,
where the Runner's transcript stops at the `<waitkey>` in the middle of
the ending text ("...siphoned into it through his magic...") and never
reaches the score summary -- a transcript tail, not a difference.  The
golden stays 50/50.

**The first drive lied.**  `Adrift_7.txt` came from the same feed with
two `#save` checkpoints in it (`#save ffh25` after feed command 26,
`#save ffh50` after 51) and showed six divergences: the seagull's
`A puff of wind...` one prompt early, the pirate one walk step ahead at
turn 39, and four tide lines (`The tide washes in`, `The tide is coming
in fast`) shifted by one at turns 52/55/56/62.  All six are the same
artefact: the two `> save` turns really are echoed turns in that
transcript (72 echoes = 70 commands + 2 saves), and the event clock is
exactly two ticks ahead of ours -- one gained in the window
p21..p30, one in p32..p56, i.e. one at each checkpoint.  A single global
shift was impossible, which is what gave it away: event 2 (the tide) does
not start until p31, so the p30 puff and the p56 tide-in cannot both be
explained by one offset.

Do not compare turn-for-turn across a `#save` in a run390 drive.  The
un-checkpointed re-drive is the measurement; the checkpointed one is only
good for getting back to a position.

**Not settled: why.**  run390_3.bas calls `opensave()` at loc_45FD7C,
*before* the per-turn `Call characters()` / `Call events()` at
460675/46067A, and `If opensave() Then GoTo loc_4608DE` jumps past them
into the score-panel/map epilogue -- on paper `save` skips the tick.  The
driver sends no stray Return either (there is not one bare `> ` line in
`Adrift_7.txt`).  And `largo-winch.taf` was driven with the same two
`#save` directives and came out 322/323 identical with 42 NPCs and 22
events in step -- but its transcript contains no `> save` turn at all,
so the two results do not actually contradict each other: where the save
was echoed as a turn, the clock moved.  Worth one probe some day
(`x`, `save`, `x` in a game with a 2-turn event); not worth blocking on.

**Diagnostic added.**  `scdump.cpp`'s `SCR_DUMP_TASKS` event dump now
prints **PrefTime1/PrefText1 and PrefTime2/PrefText2** and the event's own
`Where` room list.  Those two texts are the only event strings that are
neither Start, Look nor Finish -- they fire N turns before the event ends,
against the post-decrement clock -- and a measurement that does not know
an event carries one reads its wording as an unexplained divergence.  This
session spent most of itself on "The tide washes in" before finding it
there.  The room list matters for the same reason: FarFromHome's tide
event is `SOME_ROOMS` = {12 The lighthouse, 24 Western Beach}, and the
player stands in room 13 (Behind the lighthouse) at p52/p58/p62, so the
Runner's shifted tide lines are simply not shown there.  (The old block
also read the room list at the wrong prop path and wrote past the end of
a 3-element `scr_vartype_t ek[3]`; both fixed.)

Next candidate: `EnqueteAHautsRisques.taf` (3.90).

## EnqueteAHautsRisques.taf (Enquête à hauts risques, 3.90) -- 2026-09-05, run390

Clean, and the fastest row in weeks: `Adrift_9_enquete.txt` from
`cmdfile_w_enquete.txt` (all 145 commands of the wired solution, PRE=0, no
popup dialogs, no waitkeys).  145/145 echoed, 144 of the 145 turns
byte-identical, and the 145th -- the winning `se coucher` -- differs only by
the Runner's `[Press any key a end]` after the score summary.  Zero engine
divergences.  Golden unchanged, 59/59.

Two things worth recording, neither an engine finding:

* **The harness row's comment block called it a "French 4.0 game".**  It is
  not: the header bytes 8-10 are `94 45 37`, i.e. 3.90, which is also what
  this file's candidate table has always said.  It sits directly under
  `QuiATueDana.taf` (4.00) in `run_v4_walkthroughs.sh` and evidently inherited
  the neighbour's version in prose.  Corrected in place -- and it is the
  cheapest possible way to drive a whole measurement into the wrong Runner, so
  read the bytes, never the comment.
* **Predicting the row was measurable took one dump and no replay.**
  `SCR_DUMP_TASKS=1` shows all seven events with `start=N..N` and
  `time1 == time2` -- every one fixed-length -- and zero walks against 13 NPCs.
  A game whose events cannot roll and whose NPCs never walk has nothing on the
  path for the RNG to move, which is the same thing the double-seed test says
  but costs one second instead of two corpus runs.  Worth doing first on every
  remaining candidate: it also tells you, before the drive, whether a
  divergence you later see *could* be RNG at all.

The accented commands (`prendre sac à dos`, `x canapé`, `ouvrir
réfrigérateur`, `rez-de-chaussée`, `déverrouiller porte`, `couper câble vert`,
`détacher erica`) all arrived as their CP1252 bytes through `type_line.py`'s
dead-key path -- the first non-largo confirmation that the 2026-09-05 driver
fix generalises.  The feed is byte-for-byte the CP1252 solution file, so
`compare_wine_transcript.py` was pointed straight at
`goldens/enquete_a_hauts_risques_solution.txt` as its `--feed`; its latin-1
reader then agrees with the Runner's CP1252 transcript.  Handing it the UTF-8
`cmdfile_w_enquete.txt` instead reports **25 of the 145 commands as never
echoed** -- exactly the accented ones, listed back as `prendre sac Ã  dos` --
and the scarier side comes out one turn long, because scare is fed the same
mojibake.  The right `--feed` for a non-ASCII game is the CP1252 solution
file; the UTF-8 cmdfile is for the driver alone.

Next candidate: `Captive.taf` (Captive Universe, 3.90 -- 141 commands, 0
walks, 2 NPCs, 19 events).

## Captive.taf (Captive Universe, 3.90) -- 2026-09-05, run390

Clean.  `Adrift_9_captive.txt` from `cmdfile_w_captive.txt`, PRE=0, no popup
dialogs, no waitkeys: 57/57 echoed, 56 of the 57 turns byte-identical, and the
57th -- the winning `put diamond on pedestal`, which runs the whole Thossler
epilogue -- differs only by the Runner's `[Press any key to end]` after the
score summary.  Both sides score 100/100.  Zero engine divergences; golden
unchanged.

**The candidate table's "141 commands" is 84 comment lines plus 57 real
ones.**  Third row in a fortnight to be mis-sized this way (croft's "193",
mudergreatfalls' "255"), because the table counts golden *lines*.  The number
that matters is what `make_wine_cmdfile.py` writes, and it prints it: check
`wc -l` on the cmdfile before estimating a drive, not the table.  At the
measured ~2 s/command this row took under two minutes rather than the five the
table implies.

**The pre-drive RNG screen paid off here in a way it has not before.**  The
`SCR_DUMP_TASKS=1` dump shows 19 events, of which eighteen are fixed-length
and one is not:

    EVENT 12 [Serpent] starter=3 startTask=14 affTask=29(fin=0) restart=1
             start=0..0 time1=3 time2=5 texts=-L-
       L: A whispering in the grass makes you feel afraid, ...
       P1(at 2): It's night time and you're at the stream. ...
       P2(at 1): You hear a hissing sound. ...
       where type=2 rooms[62]: 25 26 27

TASK 14 is `tie rope to ledge`, which *is* on the route (feed line 48), so the
3..5 roll really does happen mid-measurement -- this is not a game whose RNG
never wakes.  What makes the row measurable anyway is the `where` list: the
Look and both PrefTime texts only print in rooms 25-27, and the very next
command after the rope is `u`, which climbs out of them.  Neither transcript
contains any of the three strings.  So the roll is real but invisible, which
is exactly the case the double-seed corpus test cannot tell apart from "no RNG
at all" -- the dump can, and it says in advance which turns to distrust if a
divergence does show up.  Generalise the screen that way: a spread in
`time1..time2` is only a hazard if the event's texts can reach a room the
route visits while it is running.

The two traps the harness comment records both held under the real Runner:
`Globals.WaitTurns` is 3, so the four `z`s at feed 17-20 are twelve turns of
arrest-clock, and EVENT 18 [Timedoor] un-finishes TASK 39 one turn after it
runs, so `w` immediately follows `use crowbar` at feed 50-51.  Had either been
wrong the Runner would have arrested the player or shut the steel door and the
transcript would have ended long before the epilogue.

Next candidate: `The Screen Savers On Planet X.taf` (3.90 -- 133 commands, no
comment lines this time, 0 walks, 10 NPCs, 19 events, and the dump says all
nineteen are `start=0..0 time1=1 time2=1`, so nothing on that path can roll).
