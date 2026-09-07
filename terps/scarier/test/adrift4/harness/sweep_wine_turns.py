#!/usr/bin/env python3
"""Sweep every archived Runner transcript for WORD-STREAM divergences.

`sweep_wine_breaks.py` answers where the two engines put their line breaks,
and can only look at turns that already agree word for word.  This is the
other half: for every archived row it replays the same feed through
`harness/scare`, aligns the turns the way `compare_wine_transcript.py` does,
and counts the turns whose word streams DIFFER.

That is the census the `## Measured` tables in `notes/WINE-TRANSCRIPTS-TODO.md`
were built by hand, one `compare_wine_transcript.py` invocation at a time.
Those tables go stale the moment the engine changes, and after a day of ports
a stale table is worse than none -- it sends you chasing rows that closed.
Re-run this instead:

    sh build.sh && python3 sweep_wine_turns.py          # every row
    python3 sweep_wine_turns.py --only trabula --limit 6
    python3 sweep_wine_turns.py --lost                  # only rows that lost
                                                        # a feed command

Columns are the row tag, how many feed commands the Runner echoed before the
first loss (a loss desynchronises everything after it and is a HARNESS
problem, not an engine one -- see the feed-loss traps in the notes), and the
number of differing turns among those.  `--limit` example turns follow, each
as the turn number, the command, and the first 72 characters of each side.
"""
import os, re, sys, io, argparse, contextlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import compare_wine_transcript as C
import sweep_wine_breaks as B

WINE = B.WINE


ENDTAIL = "[Press any key to end]"


def endtail_only(runner_turn, scarier_turn):
    """The Runner's own `[Press any key to end]` and nothing else.

    Every ending row carries it: the Runner waits for a key where the harness
    simply stops.  It is not an engine difference and it swamps the census.
    """
    rw = " ".join(runner_turn.split())
    if not rw.endswith(ENDTAIL):
        return False
    return rw[:-len(ENDTAIL)].strip() == " ".join(scarier_turn.split()).strip()


def excerpt(runner_turn, scarier_turn, before=5, after=9):
    """The two turns from the first word they disagree on, not from the top.

    Truncating both sides at a fixed width shows the shared opening and hides
    the difference, which is the one thing worth printing.
    """
    rw, sw = runner_turn.split(), scarier_turn.split()
    at = 0
    while at < len(rw) and at < len(sw) and rw[at] == sw[at]:
        at += 1
    head = max(0, at - before)
    lead = "..." if head else ""
    return (lead + " ".join(rw[head:at + after]),
            lead + " ".join(sw[head:at + after]), at)


def compare(tag, taf, cmd, popups, env, limit):
    feed_path = os.path.join(WINE, cmd)
    runner = B.transcript_for(tag)
    if not runner or not os.path.exists(feed_path) or not os.path.exists(taf):
        return None
    env = list(env) + ["SCR_WRAP_WIDTH=100000"]
    feed, encoding = C.read_feed(feed_path, taf, env, popups)
    _, runner_turns, losses = C.split_runner(C.read_lines(runner), feed, 4, 0)
    _, scarier_turns = C.split_scarier(C.scarier_run(taf, feed, encoding,
                                                     env, popups))

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
    aligned = differ = endtail = 0
    hits = []
    for i in range(0, min(len(feed), first_loss)):
        rt = runner_turns[i]
        pos = i + offset
        if rt is None or not (0 <= pos < len(scarier_turns)):
            continue
        aligned += 1
        st = scarier_turns[pos]
        if C.normalise(rt) == C.normalise(st):
            continue
        if endtail_only(rt, st):
            endtail += 1
            continue
        differ += 1
        if len(hits) < limit:
            hits.append((i, feed[i]) + excerpt(rt, st))
    return aligned, differ, endtail, len(feed), len(losses), first_loss, hits


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--limit", type=int, default=1)
    p.add_argument("--only", default=None)
    p.add_argument("--lost", action="store_true",
                   help="list only the rows that lost a feed command")
    a = p.parse_args()

    rows, harness = B.jobs(), B.harness_rows()
    tags = [t for t in sorted(rows) if t in harness]
    if a.only:
        tags = [t for t in tags if a.only in t]
    clean = dirty = lost = 0
    for tag in tags:
        _, cmd, popups = rows[tag]
        taf, env = harness[tag]
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                r = compare(tag, taf, cmd, popups, env, a.limit)
        except Exception as exc:                        # noqa: BLE001
            print("%-32s ERROR %s" % (tag, exc))
            sys.stdout.flush()
            continue
        if r is None:
            continue
        aligned, differ, endtail, feedlen, losses, first_loss, hits = r
        if losses:
            lost += 1
        elif differ:
            dirty += 1
        else:
            clean += 1
        if a.lost and not losses:
            continue
        print("%-32s %4d/%-4d aligned  %3d differ%s%s"
              % (tag, aligned, feedlen, differ,
                 " (+%d endtail)" % endtail if endtail else "",
                 "  LOST at %d (%d)" % (first_loss, losses) if losses else ""))
        for turn, command, rline, sline, at in hits:
            print("    t%-4d %-22s w%-4d runner  %s"
                  % (turn, repr(command)[:22], at, rline))
            print("    %33s scarier %s" % ("", sline))
        sys.stdout.flush()
    print("\n%d rows: %d clean, %d differing, %d lost a feed command"
          % (len(tags), clean, dirty, lost))
    print("(clean counts a row whose only differing turn was the ending's"
          " `%s`)" % ENDTAIL)


if __name__ == "__main__":
    main()
