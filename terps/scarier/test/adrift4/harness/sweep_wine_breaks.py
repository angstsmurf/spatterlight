#!/usr/bin/env python3
"""Sweep every archived Runner transcript for LINE-STRUCTURE divergences.

compare_wine_transcript.py collapses all whitespace, so every comparison made
with it is blind to where the two engines put their line breaks.  The archive
can answer that too, but only in one direction:

  runner-only break   Scarier joined where the Runner broke   -> REAL
  scarier-only break  Scarier broke where the .txt has none   -> SUSPECT

`Adrift_N.txt` DROPS line breaks the RichTextBox really has (the <centre>
alignment artefact) but never invents one, so a break present in the .txt and
absent from Scarier's output is ground truth, and the other direction is
mostly artefact.  Both are counted; only the first is worth chasing.

The engine is run with SCR_WRAP_WIDTH set wide, which turns the harness's
78-column word wrap off entirely (os_ansi.cpp).  With no wrapping there is
nothing to infer: every newline in the output is one the engine meant.

    sh build.sh                             # the harness scare this uses
    python3 sweep_wine_breaks.py            # every row, 4 examples each
    python3 sweep_wine_breaks.py --only ghosttown --limit 40
    python3 sweep_wine_breaks.py --scarier  # show the suspect direction

Each row prints how many of its turns aligned word for word (only those can
be compared at all) and the break counts in each direction; `--limit` example
breaks follow, as `t<turn> '<command>' k<1 newline|2 blank line>` and the
words on either side of the break.
"""
import os, re, sys, glob, argparse, io, contextlib

HERE = os.path.dirname(os.path.abspath(__file__))
GAMES = os.path.join(os.path.dirname(HERE), "games")
WINE = os.path.expanduser("~/adrift-battle/runner/wine")
ADRIFT = os.path.join(WINE, "pfx/drive_c/adrift")

sys.path.insert(0, HERE)
import compare_wine_transcript as C


def tidy(lines):
    """Squeeze runs of spaces and of blank lines, and trim the ends."""
    out = [re.sub(r"[ \t]+", " ", l).strip() for l in lines]
    while out and not out[0]:
        out.pop(0)
    while out and not out[-1]:
        out.pop()
    squeezed = []
    for line in out:
        if not line and squeezed and not squeezed[-1]:
            continue
        squeezed.append(line)
    return squeezed


def harness_rows():
    """solution tag -> (game .taf path, env list) from the walkthrough table."""
    out = {}
    for line in open(os.path.join(HERE, "run_v4_walkthroughs.sh"),
                     encoding="latin-1"):
        line = line.rstrip("\n")
        if line.startswith("#") or "|" not in line:
            continue
        parts = line.split("|")
        if not parts[0].endswith("_solution.txt"):
            continue
        env = [e for e in (parts[3].split() if len(parts) > 3 else [])
               if "=" in e]
        out[parts[0][:-len("_solution.txt")]] = (os.path.join(GAMES, parts[1]),
                                                 env)
    return out


def jobs():
    """tag -> (taf, cmdfile, popups) from the Wine job files; later wins."""
    rows = {}
    for path in sorted(glob.glob(os.path.join(WINE, "jobs_*.txt"))):
        for line in open(path, encoding="utf-8", errors="replace"):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split("|")
            if len(parts) < 3:
                continue
            popups = parts[5].split("~") if len(parts) > 5 and parts[5] else []
            rows[parts[0]] = (parts[1], parts[2], popups)
    return rows


def transcript_for(tag):
    """The highest-numbered archived transcript for a row, if any."""
    hits = sorted(glob.glob(os.path.join(ADRIFT, "Adrift_*_%s.txt" % tag)),
                  key=lambda p: int(os.path.basename(p).split("_")[1]))
    return hits[-1] if hits else None


def breaks(lines):
    """(words, {word index: 1 newline | 2 blank line}) for one turn."""
    words, marks, pending = [], {}, 0
    for line in lines:
        if not line:
            if words:
                pending = 2
            continue
        if words:
            marks[len(words)] = max(pending or 1, marks.get(len(words), 0))
        pending = 0
        words.extend(line.split())
    return words, marks


def classify(rlines, slines):
    """Which side has a break the other lacks, at the same word."""
    rw, rm = breaks(rlines)
    sw, sm = breaks(slines)
    if rw != sw:
        return None
    return (rw,
            {i: k for i, k in rm.items() if sm.get(i, 0) < k},
            {i: k for i, k in sm.items() if rm.get(i, 0) < k})


def context(words, idx, before=6, after=6):
    return "%s  ||  %s" % (" ".join(words[max(0, idx - before):idx]),
                           " ".join(words[idx:idx + after]))


def compare(tag, taf, cmd, popups, env, limit, show_scarier):
    feed_path = os.path.join(WINE, cmd)
    runner = transcript_for(tag)
    if not runner or not os.path.exists(feed_path) or not os.path.exists(taf):
        return None
    env = list(env) + ["SCR_WRAP_WIDTH=100000"]
    feed, encoding = C.read_feed(feed_path, taf, env, popups)
    _, runner_turns, losses = C.split_runner(C.read_lines(runner), feed, 4, 0)
    _, scarier_turns = C.split_scarier(C.scarier_run(taf, feed, encoding,
                                                     env, popups))

    # The same start-offset search compare_wine_transcript.py makes.
    best, offset = -1, 0
    for cand in range(0, 12):
        score = 0
        for i in range(0, min(len(feed), len(scarier_turns) - cand)):
            if runner_turns[i] is None:
                continue
            if C.normalise(runner_turns[i]) == C.normalise(scarier_turns[i + cand]):
                score += 1
        if score > best:
            best, offset = score, cand

    first_loss = losses[0][0] if losses else len(feed)
    same = n_r = n_s = 0
    hits = []
    for i in range(0, min(len(feed), first_loss)):
        rt = runner_turns[i]
        pos = i + offset
        if rt is None or not (0 <= pos < len(scarier_turns)):
            continue
        st = scarier_turns[pos]
        if C.normalise(rt) != C.normalise(st):
            continue                      # only word-identical turns compare
        same += 1
        got = classify(tidy(rt.split("\n")), tidy(st.split("\n")))
        if got is None:
            continue
        words, only_r, only_s = got
        n_r += len(only_r)
        n_s += len(only_s)
        for idx in sorted(only_s if show_scarier else only_r):
            if len(hits) < limit:
                kind = (only_s if show_scarier else only_r)[idx]
                hits.append((i, feed[i], kind, context(words, idx)))
    return same, len(feed), n_r, n_s, hits


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--limit", type=int, default=4)
    p.add_argument("--only", default=None)
    p.add_argument("--scarier", action="store_true",
                   help="show the suspect direction instead")
    a = p.parse_args()

    rows, harness = jobs(), harness_rows()
    tags = [t for t in sorted(rows) if t in harness]
    if a.only:
        tags = [t for t in tags if a.only in t]
    total_r = total_s = 0
    for tag in tags:
        _, cmd, popups = rows[tag]
        taf, env = harness[tag]
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                r = compare(tag, taf, cmd, popups, env, a.limit, a.scarier)
        except Exception as exc:                        # noqa: BLE001
            print("%-32s ERROR %s" % (tag, exc))
            sys.stdout.flush()
            continue
        if r is None:
            continue
        same, nfeed, n_r, n_s, hits = r
        total_r += n_r
        total_s += n_s
        print("%-32s aligned %3d/%3d  runner-only %3d  scarier-only %3d"
              % (tag, same, nfeed, n_r, n_s))
        for turn, command, kind, ctx in hits:
            print("      t%-4d %-18r k%d  %s" % (turn, command, kind, ctx))
        sys.stdout.flush()
    print("TOTAL runner-only %d  scarier-only %d" % (total_r, total_s))


if __name__ == "__main__":
    main()
