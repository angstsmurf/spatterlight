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


def read_lines(path, encoding="latin-1"):
    with open(os.path.expanduser(path), encoding=encoding) as handle:
        text = handle.read()
    return text.replace("\r\n", "\n").replace("\r", "\n").split("\n")


def read_cmdfile_lines(path):
    """A Wine cmdfile as drive.exe reads it -- UTF-8, latin-1 only as a fallback.

    drive.cs does File.ReadAllLines(cmds, Encoding.UTF8); reading the same file
    as latin-1 here turned an accented command into mojibake and made the
    comparison report commands the Runner had echoed perfectly as lost
    (qui_a_tue_dana, 2026-09-06).
    """
    try:
        return read_lines(path, "utf-8")
    except UnicodeDecodeError:
        return read_lines(path)


def cmdfile_lines(path):
    """The command file as scarier is fed it: comments dropped, blanks kept.

    Returns (lines, encoding) so pauses_eat() can hand the very same bytes
    back to the harness.
    """
    for encoding in ("utf-8", "latin-1"):
        try:
            with open(os.path.expanduser(path), encoding=encoding) as handle:
                text = handle.read()
        except UnicodeDecodeError:
            continue
        lines = text.replace("\r\n", "\n").replace("\r", "\n").split("\n")
        return [l for l in lines if not l.strip().startswith("#")], encoding
    raise SystemExit("cannot decode %s" % path)


def scarier_run(taf, feed, encoding, env_extra, popup_answers, markers=False):
    """Replay a list of commands through harness/scare, one prompt per entry.

    SCR_SKIP_WAITKEY is forced ON.  A <waitkey> is pure output -- skipping it
    changes no game state -- but it READS A LINE, and the command file has no
    line for it to read: make_wine_cmdfile.py hands the Runner's startup
    pauses to the driver's PRE and strips those blanks from the file.  Replayed
    without SKIP, scarier's own startup pauses then swallow the first real
    commands instead, scarier turn 0 is feed[PRE], and no forward --offset can
    put the two sides back together.  Forcing SKIP makes scarier consume
    exactly one line per prompt, so feed[i] is always scarier turn i
    (2026-09-06; it is why iachini, the_town_of_azra and wes_ghn all read as
    whole-game divergences from turn 0).
    """
    scare = os.path.join(HERE, "scare")
    if not os.path.exists(scare):
        sys.exit("no harness at %s -- run `sh build.sh` first" % scare)
    env = dict(os.environ)
    for assignment in env_extra:
        name, _, value = assignment.partition("=")
        env[name] = value
    env["SCR_SKIP_WAITKEY"] = "1"
    if markers:
        env["SCR_MARK_WAITKEY"] = "1"
    # The two built-in questions are asked by the Runner in InputBox dialogs
    # before the transcript exists, so make_wine_cmdfile.py keeps them OUT of
    # the command file and reports them as POPUP_ANSWERS instead.  scarier
    # asks them inline and reads the answers off stdin like any other command,
    # so replaying the command file alone answers them with an empty line and
    # the whole game runs in a different state -- which is exactly how
    # imagination's "Jenny" read as a turn-0 engine divergence (2026-09-06).
    # Put them back at the head of scarier's stdin; the offset auto-detection
    # absorbs the extra prompts they create.
    stdin = "\n".join(list(popup_answers) + list(feed)).encode(encoding, "replace")
    done = subprocess.run([scare, os.path.expanduser(taf)], input=stdin,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT if markers
                          else subprocess.DEVNULL, env=env)
    return done.stdout.decode("latin-1").replace("\r\n", "\n").split("\n")


def pause_counts(lines, popups):
    """How many <waitkey> pauses each command's own output printed.

    SCR_MARK_WAITKEY prints "[WAITKEY]" on stderr in transcript order, so with
    stderr folded in the markers fall inside the span of the command that
    printed them -- starting ON the prompt line itself, because a pause printed
    by the first line of a turn's output lands there (Vardock Bates' newspaper,
    2026-08-29).
    """
    counts = []
    for line in lines:
        if line.startswith(">"):
            counts.append(0)
        if counts:
            counts[-1] += len(re.findall(r"\[WAITKEY\]", line))
    return counts[popups:]


def read_feed(path, taf=None, env_extra=(), popup_answers=(), skip_wired=True):
    """The commands as they were driven in: what the Runner treated as a turn.

    A blank line in the command file is a bare Return, and what that IS depends
    on where it lands.  When a <waitkey> is waiting for a key the Return
    answers the pause -- the Runner never prompts for it, so it is not a turn
    and not a feed entry.  When nothing is waiting it is a REAL EMPTY TURN:
    run400 echoes "> " and answers it.

    Under SKIP the question does not arise: make_wine_cmdfile.py ADDED one
    blank per pause purely so the Runner has a key to eat, and every one of
    them is a pause answer.  Without SKIP the blanks are the solution's own and
    can be either -- lobster's ten all answer real pauses, sommeril's four are
    empty commands ("Much like a dream, that never happened.") in a game with
    no pauses at all -- so measure it: replay the candidate feed with markers,
    see how many pauses each command printed, and let those pauses eat the
    blanks that follow.  Iterated to a fixed point, because dropping a blank
    changes the replay that classifies the next one (all 2026-09-06).
    """
    lines, encoding = cmdfile_lines(path)
    if skip_wired or taf is None:
        return [l.strip() for l in lines if l.strip()], encoding
    feed = None
    candidate = [l.strip() for l in lines]
    while candidate and not candidate[-1]:
        candidate.pop()
    for _ in range(6):
        if candidate == feed:
            break
        feed = candidate
        counts = pause_counts(scarier_run(taf, feed, encoding, env_extra,
                                          popup_answers, markers=True),
                              len(popup_answers))
        candidate, index, prompt = [], 0, 0
        while index < len(lines):
            candidate.append(lines[index].strip())
            index += 1
            # a pause eats the next line -- but only if it is blank; a pause
            # sitting on a real command is a mis-wired solution, and the
            # Runner will have eaten it too, so leave it in the feed and let
            # the lost-command report say so
            for _ in range(counts[prompt] if prompt < len(counts) else 0):
                if index < len(lines) and not lines[index].strip():
                    index += 1
                else:
                    break
            prompt += 1
        while candidate and not candidate[-1]:
            candidate.pop()
    return feed, encoding


def raw_lines_all_echoed(feed_path, runner_lines):
    """Did every line of the command file come back as an echo, in order?

    read_feed drops the blanks it believes answered a pause, so a row where
    the two engines pause differently reads as lost commands even though the
    Runner took every line the driver sent.  Comparing the RAW file with the
    raw echoes tells the two apart.
    """
    lines, _ = cmdfile_lines(feed_path)
    raw = [l.strip() for l in lines]
    while raw and not raw[-1]:
        raw.pop()
    echoes = []
    for line in runner_lines:
        stripped = line.strip()
        if stripped.startswith(">"):
            echoes.append(stripped[1:].strip())
    while echoes and not echoes[-1]:
        echoes.pop()
    return len(raw) == len(echoes) and all(
        a.lower() == b.lower() for a, b in zip(raw, echoes))


# The Runner's own end-of-session prompt.  It is not game text: the host
# appends "[Press any key to end]" when it is about to block on a keypress
# (Form1.endmessage, and the `endgame` branch at run400 loc_48AAC9 quoted in
# sclibrar.cpp lib_cmd_endgame), and the headless harness has no keypress to
# block on -- so it can only ever read as a divergence on the very turn a
# replay wins.  It also passes through the game's ALR table on its way to the
# transcript, so it is not always in English: Vardock Bates rewrites it to
# "[Pulsa cualquier tecla para terminar]".  Hence the shape, not the wording,
# is what is recognised -- a trailing [...] fragment, and only when removing it
# makes the whole turn match, so nothing else can hide behind it.
RUNNER_TRAILING_BRACKET = re.compile(r"\s*(\[[^\[\]]*\])\s*$")


def normalise(text):
    """Whitespace-collapsed words, so hard wrapping is not a difference."""
    return re.sub(r"\s+", " ", text).strip()


def strip_runner_keyprompt(runner_text, scarier_text):
    """Return the [...] the Runner appended to an otherwise identical turn."""
    match = RUNNER_TRAILING_BRACKET.search(runner_text)
    if match and normalise(runner_text[:match.start()]) == scarier_text:
        return match.group(1)
    return None


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
            # ... unless the WALKTHROUGH types it.  `save` is a verb a game
            # can give a task of its own -- The Crooked Estate answers it with
            # "The estate is decayed beyond saving." -- and dropping that echo
            # reported a command the Runner had echoed perfectly as lost
            # (crookedestate feed[44], 2026-09-07).
            expected = any(feed[index + ahead].strip().lower() == stripped.lower()
                           for ahead in range(0, lookahead + 1)
                           if index + ahead < len(feed))
            if stripped.lower() in ("save", "restore") and not expected:
                skipping = True
                continue
            skipping = False
        elif skipping:
            continue
        elif prompted:
            pending.append(line)
            continue
        hit = None
        # An empty prompt is a real turn, but only a prompted transcript can
        # tell one from an ordinary blank line of game text -- so match it
        # only there, and only against a blank feed entry.
        if stripped or prompted:
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
            if feed[lost]:
                losses.append((lost, feed[lost]))
        index = hit + 1

    if intro is None:
        intro = "\n".join(pending)
    elif index - 1 >= 0:
        turns[index - 1] = "\n".join(pending)

    for lost in range(index, len(feed)):
        if feed[lost]:
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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--feed", required=True,
                        help="the command file driven into the Runner")
    parser.add_argument("--runner", required=True,
                        help="the Runner's Adrift_N.txt transcript")
    parser.add_argument("--taf", help="game to replay through harness/scare")
    parser.add_argument("--scarier", help="a replay you already have")
    parser.add_argument("--popup", action="append", default=[],
                        help="answer to a built-in name/gender question, in"
                             " order; make_wine_cmdfile.py prints these as"
                             " POPUP_ANSWERS and leaves them out of the feed")
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

    # The Runner side and the scarier side must index the same way, so a blank
    # line counts as a turn only where no pause eats it.
    skip_wired = any(a.split("=", 1)[0] == "SCR_SKIP_WAITKEY" for a in args.env)
    feed, encoding = read_feed(args.feed, args.taf, args.env, args.popup,
                               skip_wired)
    runner_intro, runner_turns, losses = split_runner (
        read_lines(args.runner), feed, args.lookahead, args.start)

    if args.scarier:
        scarier_lines = read_lines(args.scarier)
    else:
        scarier_lines = scarier_run(args.taf, feed, encoding, args.env,
                                    args.popup)
    scarier_intro, scarier_turns = split_scarier(scarier_lines)

    # scarier's stream can open with prompts of its own -- a skipped waitkey,
    # a name or gender question -- so feed[0] is not always its turn 0.  Pick
    # the shift that lines the most turns up rather than assuming one.
    if args.offset is None:
        best, args.offset = -1, 0
        for offset in range(0, 12):
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

    if losses and raw_lines_all_echoed(args.feed, read_lines(args.runner)):
        # Every line of the command file, blanks included, came back as an
        # echo in order -- nothing was lost.  What differs is which of those
        # blanks was a pause answer: read_feed classifies them from SCARIER's
        # pauses, so a game where run400 pauses where scarier does not (or the
        # other way round) turns the surplus blanks into turns on one side
        # only and the alignment reports them as losses (mould, 2026-09-07).
        print("RULE 2 -- every line of the command file was echoed, in order.")
        print("The %d \"lost\" command(s) below are a PAUSE-COUNT difference:"
              % len(losses))
        print("run400 and scarier disagree about which blanks answered a")
        print("<waitkey>, so the two sides number their turns differently.")
        print()
    elif losses:
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
    whitespace_only = 0
    keyprompt_only = 0
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

        # The host's keypress prompt, appended to a turn that is otherwise
        # identical.  Counted apart rather than hidden, and the fragment is
        # printed, because the same shape would catch a real trailing bracket.
        keyprompt = strip_runner_keyprompt(runner_text, scarier_text)
        if keyprompt is not None:
            keyprompt_only += 1
            if keyprompt_only <= 3:
                print("turn %d  %s -- the Runner appended %s, which is the"
                      " host's own keypress prompt (ALR-rewritten in some"
                      " games), not game text.  The turn is identical either"
                      " side of it." % (index, feed[index], keyprompt))
                print()
            continue

        # Whitespace-only, after the wrap-collapse above, means one side put a
        # separator where the other put none.  That is USUALLY not an engine
        # difference: the Runner's .txt transcript is written by its own tag
        # converter, which drops a line break that exists only as a paragraph
        # alignment change -- `<centre>X</centre>Y` reaches the file as "XY"
        # while the RichTextBox itself holds "X\nY".  Measured on YADFA with
        # fast.sh's DUMP_SCROLLBACK, 2026-09-06; six 4.00 rows were reading as
        # divergences on nothing else.  It is not ALWAYS harmless -- a real
        # missing join looks the same -- so say so and count it apart rather
        # than hiding it; DUMP_SCROLLBACK settles any individual case.
        if runner_text.replace(" ", "") == scarier_text.replace(" ", ""):
            whitespace_only += 1
            if whitespace_only <= 3:
                print("turn %d  %s -- WHITESPACE ONLY (a separator one side has"
                      " and the other has not).  The .txt transcript drops"
                      " alignment-only breaks; re-check with fast.sh"
                      " DUMP_SCROLLBACK before calling it an engine bug."
                      % (index, feed[index]))
                print("  run400   %s" % runner_text)
                print("  scarier  %s" % scarier_text)
                print()
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

    if whitespace_only:
        print("%d turn(s) differed by whitespace only%s -- see the note above."
              % (whitespace_only,
                 " (%d not shown)" % (whitespace_only - 3)
                 if whitespace_only > 3 else ""))
        print()

    if keyprompt_only:
        print("%d turn(s) differed only by the Runner's trailing keypress"
              " prompt%s."
              % (keyprompt_only,
                 " (%d not shown)" % (keyprompt_only - 3)
                 if keyprompt_only > 3 else ""))
        print()

    if normalise(runner_intro) != normalise(scarier_intro):
        print("(the openings differ too -- banner, graphics notice or the")
        print(" Runner's own startup lines; usually not an engine difference)")

    if differences == 0 and not losses:
        aside = [note for note, flag in
                 (("whitespace", whitespace_only),
                  ("the Runner's keypress prompt", keyprompt_only)) if flag]
        print("identical on every turn%s."
              % (" apart from " + " and ".join(aside) if aside else ""))
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
