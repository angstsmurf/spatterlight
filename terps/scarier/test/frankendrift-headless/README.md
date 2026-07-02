# FrankenDrift.Headless — ADRIFT 5 ground-truth runner

A minimal stdin/stdout console frontend for the **FrankenDrift** engine (the
VB.NET reimplementation of the official ADRIFT 5 Runner,
<https://github.com/awlck/frankendrift>).  It drives the same engine the GUI/Glk
runners use — `Adrift.SharedModule.UserSession.Process(cmd)` — but with a
text-only frontend, so its transcripts can be diffed against Scarier's headless
`a5run_dump` harness to validate the v5 port (`terps/scarier/a5*`).

These two files (`Program.cs`, `FrankenDrift.Headless.csproj`) are **kept here
for preservation only**.  FrankenDrift is a separate third-party checkout; this
runner is *not* part of upstream.  It is also version-controlled in our fork
(<https://github.com/angstsmurf/frankendrift>, branch `scarier-headless`) — the
authoritative copy — so a clobbered `~/frankendrift` can be recovered with
`git -C ~/frankendrift checkout scarier-headless`.  Keep this in-tree copy and
the fork in sync when either changes.  To use it, copy both files into a
`FrankenDrift.Headless/` directory at the root of a frankendrift checkout (so the
`..\FrankenDrift.Adrift` / `..\FrankenDrift.Glue` project references resolve) and
build:

    cp Program.cs FrankenDrift.Headless.csproj ~/frankendrift/FrankenDrift.Headless/
    dotnet build ~/frankendrift/FrankenDrift.Headless/FrankenDrift.Headless.csproj -c Release

Then diff against Scarier:

    make -f Makefile.headless a5run
    test/a5_groundtruth.sh "<game.blorb>" test/a5_bullets_script.txt

## What it does

- Implements the tiny `UIGlue` / `frmRunner` / `RichTextBox` / `Map` interfaces
  (see `FrankenDrift.Glue/UIGlue.cs`); the engine's sole text sink is
  `UIGlue.OutputHTML`, which we strip to plain text the way `GlkHtmlWin` shows
  it (`<br>`→newline, style/media tags dropped, `<cls>` clears, entities
  decoded), suppressing the Wingdings command-echo glyph.
- Forces a fixed RNG seed (default 1234, override `FD_SEED`) via reflection on
  the private `SharedModule.r` so runs are **reproducible**.  With `FD_RNG=xoshiro`
  it swaps in `XoshiroRandom`, drawing from the same erkyrath xoshiro128\*\* stream
  as Scarier so RAND-selected text matches byte-for-byte (see below).
- Does **not** pump real-time timer events, so runs are turn-deterministic and
  comparable to Scarier's turn loop.
- Wires the engine's save/restore prompts to the environment so a script's bare
  `save` / `restore` commands round-trip through a known `.tas` file:
  `QuerySavePath()` returns `$FD_SAVE_PATH`, `QueryRestorePath()` returns
  `$FD_RESTORE_PATH`. Used to cross-validate Scarier's FrankenDrift-compatible
  save format (`TODO_a5_frankendrift_save_compat.md`):

      # FD writes a .tas Scarier can read (FD -> Scarier)
      FD_SAVE_PATH=/tmp/fd.tas dotnet .../fd-headless.dll game.blorb prefix_then_save.txt
      # FD reads a .tas Scarier wrote (Scarier -> FD)
      FD_RESTORE_PATH=/tmp/sc.tas dotnet .../fd-headless.dll game.blorb restore_then_look.txt

## RNG modes

By default FrankenDrift uses .NET `System.Random`; Scarier uses the shared
erkyrath xoshiro128\*\* (`a5rand.cpp`).  Even with the same seed the two sequences
differ, so **RAND-selected text will not match** (dream/epigraph variants, combat
rolls, the Six Silver Bullets `Roller==1` catch-all) — diff the RAND-independent
portions and treat RAND blocks as expected divergence.

Set **`FD_RNG=xoshiro`** to remove this caveat: `XoshiroRandom` draws from a
generator byte-identical to Scarier's erkyrath stream under the same seed, so the
diff becomes a full every-line conformance check.  Any residual divergence then
points at a real draw-order/count difference (a Scarier bug), not RNG noise.  The
`run_a5_walkthroughs.sh` harness runs both modes and tracks them separately;
`FD_RNG_TRACE=1` (mirroring Scarier's `A5_TRACE_RAND=1`) echoes each draw as
`RAND(lo,hi)=r` for aligning the two streams.
