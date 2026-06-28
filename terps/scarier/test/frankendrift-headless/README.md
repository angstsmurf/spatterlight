# FrankenDrift.Headless — ADRIFT 5 ground-truth runner

A minimal stdin/stdout console frontend for the **FrankenDrift** engine (the
VB.NET reimplementation of the official ADRIFT 5 Runner,
<https://github.com/awlck/frankendrift>).  It drives the same engine the GUI/Glk
runners use — `Adrift.SharedModule.UserSession.Process(cmd)` — but with a
text-only frontend, so its transcripts can be diffed against Scarier's headless
`a5run_dump` harness to validate the v5 port (`terps/scarier/a5*`).

These two files (`Program.cs`, `FrankenDrift.Headless.csproj`) are **kept here
for preservation only**.  FrankenDrift is a separate third-party checkout; this
runner is *not* part of upstream and is not pushed there.  To use it, copy both
files into a `FrankenDrift.Headless/` directory at the root of a frankendrift
checkout (so the `..\FrankenDrift.Adrift` / `..\FrankenDrift.Glue` project
references resolve) and build:

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
  the private `SharedModule.r` so runs are **reproducible**.
- Does **not** pump real-time timer events, so runs are turn-deterministic and
  comparable to Scarier's turn loop.

## RNG caveat (important)

FrankenDrift uses .NET `System.Random`; Scarier uses the shared erkyrath
xoshiro128\*\* (`a5rand.cpp`).  Even with the same seed the two sequences differ,
so **RAND-selected text will not match** (dream/epigraph variants, combat rolls,
the Six Silver Bullets `Roller==1` catch-all).  Diff the RAND-independent
portions; treat RAND blocks as expected divergence.
