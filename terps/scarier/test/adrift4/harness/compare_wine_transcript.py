#!/usr/bin/env python3
"""Align a Wine Runner transcript against a scarier replay of the same feed.

Every measurement in `notes/WINE-TRANSCRIPTS-TODO.md` rests on the same three
rules, in order:

  1. rule out Verbose and the Appearance checkboxes;
  2. rule out the FEED -- confirm every command echoed;
  3. only then is a difference an engine bug.

Rule 2 has been done by eye on every row so far, and it has cost real time:
the X-Files `knock` lead and the `take knife` lead were both a single lost
`look`, argued about for a day before anyone counted the echoes.  This does
the counting.

Given the game, the command file that was driven into the Runner and the
Runner's own `Adrift_N.txt`, it

  * replays the same feed through `harness/scare` (or takes a replay you have
    already made, with `--scarier`),
  * splits both sides into turns -- the Runner's by its echoed command line,
    scarier's by the `>` prompt,
  * reports every feed command the RUNNER NEVER ECHOED, which is a lost
    command and not an engine difference,
  * and then diffs the turns that did line up, whitespace-normalised, so that
    the Runner's own hard wrapping does not show up as a difference.

A lost command desynchronises everything after it, so the echo report comes
first and the turn diff is only worth reading down to the first loss.

Usage:
    python3 compare_wine_transcript.py --taf ../games/the_pk_girl.taf \\
        --feed ~/adrift-battle/runner/wine/cmdfile_pk.txt \\
        --runner ~/adrift-battle/runner/wine/pfx/drive_c/adrift/Adrift_27_thepkgirl.txt

    python3 compare_wine_transcript.py --scarier out.txt --feed cmds.txt \\
        --runner Adrift_22_xfiles.txt --limit 20

Exit status is 1 if anything differed, so it can gate a shell loop.
"""
import argparse
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def read_lines(path):
    with open(os.path.expanduser(path), encoding="latin-1") as handle:
        text = handle.read()
    return text.replace("\r\n", "\n").replace("\r", "\n").split("\n")


def read_feed(path):
    """The commands as they were driven in, blank lines and comments dropped."""
    feed = []
    for line in read_lines(path):
        stripped = line.strip()
        if stripped and not stripped.startswith("#"):
            feed.append(stripped)
    return feed


def normalise(text):
    """Whitespace-collapsed words, so hard wrapping is not a difference."""
    return re.sub(r"\s+", " ", text).strip()


def split_runner(lines, feed, lookahead, start=0):
    """Split the Runner transcript on its echoed command lines.

    Returns (turns, losses).  turns[i] is the output the Runner printed for
    feed[i]; a command the Runner never echoed gets None and is listed in
    losses.  The echo is matched case-insensitively because the Runner's Auto
    complete rewrites what it echoes, and a command is looked for a few feed
    entries ahead so that ONE lost command does not derail the whole file.
    """
    turns = [None] * len(feed)
    losses = []
    pending = []
    intro = None
    index = start
    # A drive checkpointed with drive_ckpt_safe.sh's "#save NAME" /
    # "#restore NAME" carries "> save" / "> restore" echoes and their
    # "Saving current game... done." lines; neither is a game turn, so drop
    # them (a restore's "Loading game..." plus the room description that
    # follows land in the intro, which is never compared).
    skipping = False
    # "Prompt for typed commands" (registry showgt) makes the Runner echo
    # every command as "> cmd".  When the transcript shows that prompt, ONLY
    # prompt lines can be echoes: game text that happens to equal a feed
    # entry ("Zenes" against feed "zenes", whitterscap 2026-08-29) otherwise
    # matches through the lookahead and reports four commands as lost.
    prompted = any(l.lstrip().startswith("> ") for l in lines)

    for line in lines:
        stripped = line.strip()
        if stripped.startswith(">"):
            stripped = stripped[1:].strip()
            if stripped.lower() in ("save", "restore"):
                skipping = True
                continue
            skipping = False
        elif skipping:
            continue
        elif prompted:
            pending.append(line)
            continue
        hit = None
        if stripped:
            for ahead in range(0, lookahead + 1):
                if index + ahead >= len(feed):
                    break
                if stripped.lower() == feed[index + ahead].strip().lower():
                    hit = index + ahead
                    break
        if hit is None:
            pending.append(line)
            continue

        if intro is None:
            intro = "\n".join(pending)
        elif index - 1 >= 0:
            turns[index - 1] = "\n".join(pending)
        pending = []

        for lost in range(index, hit):
            losses.append((lost, feed[lost]))
        index = hit + 1

    if intro is None:
        intro = "\n".join(pending)
    elif index - 1 >= 0:
        turns[index - 1] = "\n".join(pending)

    for lost in range(index, len(feed)):
        losses.append((lost, feed[lost]))

    return intro, turns, losses


def split_scarier(lines):
    """Split a scarier replay on its `>` prompts.

    The harness prints the prompt and then, on the same line, whatever the
    command produced, so the text after the Nth `>` is the Nth turn.
    """
    turns = []
    pending = []
    intro = None

    for line in lines:
        if line.startswith(">"):
            if intro is None:
                intro = "\n".join(pending)
            else:
                turns.append("\n".join(pending))
            pending = [line[1:]]
        else:
            pending.append(line)

    if intro is None:
        intro = "\n".join(pending)
    else:
        turns.append("\n".join(pending))

    return intro, turns


def run_scarier(taf, feed_path, env_extra):
    scare = os.path.join(HERE, "scare")
    if not os.path.exists(scare):
        sys.exit("no harness at %s -- run `sh build.sh` first" % scare)
    env = dict(os.environ)
    env["SCR_SKIP_WAITKEY"] = "1"
    for assignment in env_extra:
        name, _, value = assignment.partition("=")
        env[name] = value
    # `#sleep N` is a directive to the DRIVER, not a command: measure.sh
    # sleeps on it so the Runner does not drop keystrokes during a real-time
    # <wait>.  Piping it into scare types it at the game, which answers "I
    # don't understand what you mean!" and shifts every later turn by one --
    # which is exactly how lostsouls first read as an engine divergence
    # (2026-09-05).  Blank lines are kept: they are real empty commands here,
    # because SCR_SKIP_WAITKEY is on and nothing eats them.
    with open(os.path.expanduser(feed_path), "rb") as handle:
        raw = handle.read()
    kept = b"\n".join(l for l in raw.replace(b"\r\n", b"\n").split(b"\n")
                      if not l.strip().startswith(b"#"))
    done = subprocess.run([scare, os.path.expanduser(taf)],
                          input=kept, stdout=subprocess.PIPE,
                          stderr=subprocess.DEVNULL, env=env)
    return done.stdout.decode("latin-1").replace("\r\n", "\n").split("\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--feed", required=True,
                        help="the command file driven into the Runner")
    parser.add_argument("--runner", required=True,
                        help="the Runner's Adrift_N.txt transcript")
    parser.add_argument("--taf", help="game to replay through harness/scare")
    parser.add_argument("--scarier", help="a replay you already have")
    parser.add_argument("--env", action="append", default=[],
                        help="NAME=VALUE for the scarier replay, repeatable")
    parser.add_argument("--start", type=int, default=0,
                        help="first feed index to diff (a checkpointed drive"
                             " that opened with #restore starts here)")
    parser.add_argument("--offset", type=int, default=None,
                        help="scarier turn holding feed[0]; auto-detected")
    parser.add_argument("--limit", type=int, default=40,
                        help="stop after this many differing turns")
    parser.add_argument("--lookahead", type=int, default=4,
                        help="how many feed entries ahead an echo may match")
    args = parser.parse_args()

    if not args.taf and not args.scarier:
        sys.exit("need --taf to replay, or --scarier for a replay you have")

    feed = read_feed(args.feed)
    runner_intro, runner_turns, losses = split_runner (
        read_lines(args.runner), feed, args.lookahead, args.start)

    if args.scarier:
        scarier_lines = read_lines(args.scarier)
    else:
        scarier_lines = run_scarier(args.taf, args.feed, args.env)
    scarier_intro, scarier_turns = split_scarier(scarier_lines)

    # scarier's stream can open with prompts of its own -- a skipped waitkey,
    # a name or gender question -- so feed[0] is not always its turn 0.  Pick
    # the shift that lines the most turns up rather than assuming one.
    if args.offset is None:
        best, args.offset = -1, 0
        for offset in range(0, 8):
            score = 0
            for index in range(0, min(len(feed), len(scarier_turns) - offset)):
                if runner_turns[index] is None:
                    continue
                if normalise(runner_turns[index]) == normalise(scarier_turns[index + offset]):
                    score += 1
            if score > best:
                best, args.offset = score, offset

    print("feed        %d commands" % len(feed))
    print("runner      %d turns echoed" % sum(1 for t in runner_turns if t is not None))
    print("scarier     %d turns, feed[0] is scarier turn %d"
          % (len(scarier_turns), args.offset))
    print()

    if losses:
        print("RULE 2 -- %d command(s) the Runner never echoed.  Everything"
              % len(losses))
        print("after the first of them is out of step and is NOT an engine")
        print("difference until the feed is fixed and the row re-driven:")
        for index, command in losses[:20]:
            print("    feed[%d]  %s" % (index, command))
        if len(losses) > 20:
            print("    ... and %d more" % (len(losses) - 20))
        print()
    else:
        print("RULE 2 -- every feed command was echoed.")
        print()

    differences = 0
    first_loss = losses[0][0] if losses else len(feed)
    shift = args.offset

    def scarier_at(index, with_shift):
        position = index + with_shift
        if 0 <= position < len(scarier_turns):
            return normalise(scarier_turns[position])
        return None

    for index in range(args.start, len(feed)):
        if runner_turns[index] is None:
            continue
        runner_text = normalise(runner_turns[index])
        scarier_text = scarier_at(index, shift)
        if scarier_text is None:
            continue
        if runner_text == scarier_text:
            continue

        # A lost command, or a prompt one side printed and the other did not,
        # slides the two streams apart for good.  Before calling this turn a
        # difference, see whether it IS this turn one or two prompts along --
        # and if so say so, because a re-synchronisation is a feed report, not
        # an engine one.
        if runner_text:
            for candidate in (shift + 1, shift - 1, shift + 2, shift - 2):
                if scarier_at(index, candidate) == runner_text:
                    print("turn %d  %s -- streams re-synchronised, scarier"
                          " turn %+d" % (index, feed[index], candidate - shift))
                    print()
                    shift = candidate
                    scarier_text = runner_text
                    break
        if runner_text == scarier_text:
            continue

        differences += 1
        if differences > args.limit:
            print("... stopping after %d differing turns" % args.limit)
            break
        flag = "  (past the first lost command)" if index > first_loss else ""
        print("turn %d  %s%s" % (index, feed[index], flag))
        # Print the turn whole.  These used to be cut at 400 characters, which
        # is shorter than a single ADRIFT room block once the object list, the
        # character lines and an event's look text are all run together -- and
        # the difference is as often as not in the tail (goldilocks turn 243
        # was 380 characters in before it diverged).  A truncated diff sends
        # you looking for a divergence that the tool has hidden.
        print("  run400   %s" % runner_text)
        print("  scarier  %s" % scarier_text)
        print()

    if normalise(runner_intro) != normalise(scarier_intro):
        print("(the openings differ too -- banner, graphics notice or the")
        print(" Runner's own startup lines; usually not an engine difference)")

    if differences == 0 and not losses:
        print("identical on every turn.")
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
