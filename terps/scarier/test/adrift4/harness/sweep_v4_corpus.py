#!/usr/bin/env python3
"""Sweep one whole-corpus Runner capture for word-stream divergences.

`sweep_wine_turns.py` reads the ad-hoc `jobs_*.txt` files, which name a
different cmdfile per row and only ever covered the rows someone had driven
by hand.  A whole-corpus capture is laid out differently and needs no job
files at all:

    transcripts_v4_corpus_<date>/MANIFEST_tag_transcript_exe.txt
        tag|Adrift_N_tag.txt|runNNN.exe          -- one row per walkthrough
    transcripts_v4_corpus_<date>/Adrift_N_tag.txt
        the Runner's own transcript
    v4_full_rerun_cmds/<tag>.txt
        the command file that was driven in
    par/<tag>.log
        the driver's log, whose "InputBox ... <- X" lines are the answers to
        the built-in name/gender questions (POPUP_ANSWERS -- they are asked
        before the transcript exists and are NOT in the command file)

The .taf and the row env still come from `run_v4_walkthroughs.sh`.

    python3 sweep_v4_corpus.py                       # every row, 8 jobs
    python3 sweep_v4_corpus.py --only topaz --limit 20
    python3 sweep_v4_corpus.py --jobs 1 --lost       # only rows that lost a
                                                     # feed command

Columns are the row tag, the Runner exe, aligned/feed turn counts and the
number of differing turns.  Rule 2 first: a row that LOST a feed command is
desynchronised from that point and is a harness report, not an engine one.
"""
import argparse
import contextlib
import io
import os
import re
import sys
from concurrent.futures import ProcessPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import compare_wine_transcript as C          # noqa: E402
import sweep_wine_breaks as B                # noqa: E402
import sweep_wine_turns as T                 # noqa: E402

WINE = B.WINE
DEFAULT_BATCH = os.path.join(WINE, "transcripts_v4_corpus_2026-09-08")
DEFAULT_FEEDS = os.path.join(WINE, "v4_full_rerun_cmds")
PARLOGS = os.path.join(WINE, "par")


def manifest(batch):
    """[(tag, transcript path, exe)] from the batch's own manifest."""
    rows = []
    path = os.path.join(batch, "MANIFEST_tag_transcript_exe.txt")
    for line in open(path, encoding="utf-8", errors="replace"):
        parts = line.strip().split("|")
        if len(parts) < 3 or not parts[0]:
            continue
        rows.append((parts[0], os.path.join(batch, parts[1]), parts[2]))
    return rows


def popups_for(tag):
    """The answers the driver typed into the Runner's InputBox dialogs.

    make_wine_cmdfile.py keeps those out of the command file, so a replay
    that does not put them back answers them with an empty line and runs the
    whole game in a different state (imagination's "Jenny", 2026-09-06).
    """
    log = os.path.join(PARLOGS, tag + ".log")
    if not os.path.exists(log):
        return []
    answers = []
    for line in open(log, encoding="utf-8", errors="replace"):
        # The NAME question is an InputBox.  The GENDER question is not: the
        # Runner puts up a form of option buttons, which the driver logs as
        # `gender form '...' [buttons] <- male <- OK` -- the answer is the
        # first `<-` field, the second being the button it then clicked.
        # Reading only the InputBox line left scarier's own inline gender
        # question unanswered, and it re-asked it on every later command:
        # 68 turns of `Please answer "male" or "female".` across
        # secret_of_lost_world, TheADRIFTProject, lifesimulation and life.
        match = re.match(r"\s*InputBox\s+'[^']*'\s*<-\s*(.*)$", line)
        if match:
            answers.append(match.group(1).rstrip())
            continue
        match = re.match(r"\s*gender form\s+.*?<-\s*([^<]*?)\s*<-", line)
        if match:
            answers.append(match.group(1).rstrip())
    return answers


def compare(tag, taf, feed_path, runner, popups, env, limit):
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
    rows = []
    last_aligned = -1
    for i in range(0, min(len(feed), first_loss)):
        if runner_turns[i] is not None and 0 <= i + offset < len(scarier_turns):
            last_aligned = i
    for i in range(0, min(len(feed), first_loss)):
        rt = runner_turns[i]
        pos = i + offset
        if rt is None or not (0 <= pos < len(scarier_turns)):
            continue
        aligned += 1
        st = scarier_turns[pos]
        if C.normalise(rt) == C.normalise(st):
            continue
        if T.endtail_only(rt, st):
            endtail += 1
            continue
        differ += 1
        rows.append((i, feed[i], C.normalise(rt), C.normalise(st),
                     i == last_aligned))
        if len(hits) < limit:
            hits.append((i, feed[i]) + T.excerpt(rt, st))
    return (aligned, differ, endtail, len(feed), len(losses), first_loss,
            hits, rows)


def run_row(job):
    tag, runner, exe, taf, env, limit = job
    feed_path = os.path.join(DEFAULT_FEEDS, tag + ".txt")
    if not os.path.exists(feed_path) or not os.path.exists(taf):
        return tag, exe, None, "no feed or no .taf"
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            result = compare(tag, taf, feed_path, runner, popups_for(tag),
                             env, limit)
    except Exception as exc:                            # noqa: BLE001
        return tag, exe, None, "ERROR %s" % exc
    return tag, exe, result, None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--batch", default=DEFAULT_BATCH)
    parser.add_argument("--limit", type=int, default=1)
    parser.add_argument("--only", default=None)
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--lost", action="store_true")
    parser.add_argument("--clean-too", action="store_true",
                        help="print the clean rows as well")
    parser.add_argument("--tsv", default=None,
                        help="write every differing turn to this file as"
                             " tag/exe/turn/last?/command/runner/scarier, so a"
                             " whole-corpus run can be clustered afterwards")
    args = parser.parse_args()

    harness = B.harness_rows()
    jobs = []
    for tag, runner, exe in manifest(args.batch):
        if args.only and args.only not in tag:
            continue
        if tag not in harness:
            print("%-34s no run_v4_walkthroughs.sh row" % tag)
            continue
        taf, env = harness[tag]
        jobs.append((tag, runner, exe, taf, env, args.limit))

    clean = dirty = lost = skipped = 0
    tsv = open(args.tsv, "w", encoding="utf-8") if args.tsv else None
    with ProcessPoolExecutor(max_workers=args.jobs) as pool:
        for tag, exe, result, note in pool.map(run_row, jobs):
            if result is None:
                skipped += 1
                print("%-34s %-6s %s" % (tag, exe[3:6], note))
                sys.stdout.flush()
                continue
            (aligned, differ, endtail, feedlen, losses, first_loss, hits,
             rows) = result
            if tsv:
                for turn, command, rtext, stext, is_last in rows:
                    tsv.write("\t".join([
                        tag, exe, str(turn), "LAST" if is_last else "-",
                        "LOSS" if losses else "-", command, rtext, stext]) + "\n")
                tsv.flush()
            if losses:
                lost += 1
            elif differ:
                dirty += 1
            else:
                clean += 1
            if args.lost and not losses:
                continue
            if not (losses or differ) and not args.clean_too:
                continue
            print("%-34s %-6s %4d/%-4d aligned  %3d differ%s%s"
                  % (tag, exe[3:6], aligned, feedlen, differ,
                     " (+%d endtail)" % endtail if endtail else "",
                     "  LOST at %d (%d)" % (first_loss, losses) if losses else ""))
            for turn, command, rline, sline, at in hits:
                print("    t%-4d %-22s w%-4d runner  %s"
                      % (turn, repr(command)[:22], at, rline))
                print("    %33s scarier %s" % ("", sline))
            sys.stdout.flush()

    print("\n%d rows: %d clean, %d differing, %d lost a feed command, %d skipped"
          % (len(jobs), clean, dirty, lost, skipped))


if __name__ == "__main__":
    main()
