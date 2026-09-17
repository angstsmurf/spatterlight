#!/usr/bin/env python3
"""Graft onto an archived Runner transcript the tail only its window still had.

run370 and run380 have no live transcript: `Adventure -> Save Transcript` dumps
the scrollback *when it is clicked*, so drive.exe plays every command but the
last, saves, and only then sends the final one -- and the winning move's output
lands in the Runner's window and nowhere else.  All 19 of those rows carried
"RULE 2 -- 1 command(s) the Runner never echoed" for exactly that reason, with
`win_marker` stuck at `no` although the run really had won.

The window can be read back exactly, with no screenshot and no OCR:
`DUMP_SCROLLBACK=<file> ./fast.sh ...` asks drive.exe for the RichTextBox's own
text (WM_GETTEXTLENGTH + WM_GETTEXT) AFTER the feed.  `dump_par.sh` next to
`xoshiro_par.sh` drives a whole job file that way into `par/dumps/<tag>.txt`.

This checks the archived transcript is a character-for-character prefix of a
fresh dump -- which also proves the re-drive followed the same route -- and
appends only what the dump has beyond it, as `\\par` lines inside the .rtf (or
CRLF lines in a 3.9/4.0 .txt).  A row it grafts gets `+tail` on its manifest
`source` and a recomputed `win_marker`; recompare it afterwards.

    python3 harness/graft_scrollback_tail.py --batch ~/adrift-battle/runner/wine/par/dumps
    python3 harness/graft_scrollback_tail.py --batch <dir> --apply akron cave
    python3 harness/graft_scrollback_tail.py runner_transcripts/akron.rtf <dump>

Nothing to graft is the normal answer for 3.9 and 4.0, whose transcript is
written live: measured 2026-09-17 over the 14 rows whose losses were at the end
of the feed, nine dumps held nothing the archived .txt lacked and five were
EMPTY, the window having closed with the game.  Those losses are the Runner
sitting on its "[Press any key to end]" wait, which eats the next keystroke --
there is no text to recover, by this route or any other.
"""
import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import compare_wine_transcript as C                             # noqa: E402
import runner_transcripts as R                                  # noqa: E402


def read_dump(path):
    """drive.exe writes the control text as UTF-16, newlines and all."""
    with open(path, "rb") as handle:
        text = handle.read().decode("utf-16-le")
    if text.startswith("\ufeff"):
        text = text[1:]
    return text.replace("\r\n", "\n").replace("\r", "\n")


def nonspace(text):
    """(the characters that are not whitespace, where each one was).

    The two sides may not be aligned on lines or even on words.  The .rtf's
    `\\par` structure and the control's own newlines disagree -- fast.sh's
    note: the transcript file "loses formatting-only breaks; the control text
    does not" -- and dertf() glues the .rtf's font table onto the first visible
    word ("...Wine Riched20 2.0;Microwave Man!").  Non-whitespace characters
    are the one stream both sides really share.
    """
    chars, where = [], []
    for index, char in enumerate(text):
        if not char.isspace():
            chars.append(char)
            where.append(index)
    return "".join(chars), where


def graft(transcript, dump_path):
    """(what happened, the tail to append) -- the tail is None if there is none."""
    have = "\n".join(C.read_lines(transcript))
    dump = read_dump(dump_path)
    archived, _ = nonspace(have)
    window, offsets = nonspace(dump)

    if not window:
        return ("the dump is EMPTY -- the Runner's window was already gone"
                " (the game's ending closed it), so there is nothing to read"
                " off it, by WM_GETTEXT or by screenshot", None)

    # Neither side reliably starts where the other does: the .rtf's font table
    # is text the window never showed, and a live .txt only starts once the
    # intro has settled (fast.sh --intro-quiet) while the control still holds
    # it.  Try both directions before calling it a mismatch.
    skip = archived.find(window[:80])
    if skip >= 0:
        archived = archived[skip:]
    else:
        skip = dump_skip = window.find(archived[:80])
        if skip < 0:
            return ("MISMATCH: neither text starts inside the other"
                    " (archived %r / dump %r)"
                    % (archived[:60], window[:60]), None)
        window, offsets = window[dump_skip:], offsets[dump_skip:]

    if not window.startswith(archived):
        at = 0
        while at < len(archived) and at < len(window) \
                and archived[at] == window[at]:
            at += 1
        return ("MISMATCH at character %d of %d: archived %r vs dump %r"
                % (at, len(archived), archived[at:at + 40],
                   window[at:at + 40]), None)
    if len(window) == len(archived):
        return ("nothing to graft (the dump has nothing the transcript lacks)",
                None)

    tail = dump[offsets[len(archived) - 1] + 1:]
    return ("+%d characters, %d line(s)"
            % (len(window) - len(archived), tail.strip("\n").count("\n") + 1),
            tail)


def rtf_escape(text):
    out = []
    for char in text:
        if char in "\\{}":
            out.append("\\" + char)
        elif ord(char) < 128:
            out.append(char)
        elif ord(char) < 256:
            out.append("\\'%02x" % ord(char))
        else:
            out.append("\\u%d?" % ord(char))
    return "".join(out)


def append_to_rtf(path, tail):
    """Put the tail back the way Save Transcript would have, as `\\par` lines.

    The blank line at the seam has to come from the DUMP: the alignment is on
    non-whitespace characters, so whatever `\\par` the save left dangling after
    the last visible line is exactly what the dump's own leading newlines
    replace.  Here that run is `\\cf0\\fs16 \\par`, formatting and a break
    together, so drop only the breaks and keep the rest -- the file's
    formatting state at the end is then what it always was.

    Everything here reads and writes with `newline=""`: these files are CRLF
    and the archive is the Runner's own bytes, so letting Python translate the
    line endings would rewrite every line of a file we only mean to append to.
    """
    with open(path, encoding="latin-1", newline="") as handle:
        text = handle.read()
    eol = "\r\n" if "\r\n" in text else "\n"
    closed = text.rstrip()
    assert closed.endswith("}"), path
    body, trailing = closed[:-1], text[len(closed):]
    # Only a literal space delimits an RTF control word, so tokenise with " ?"
    # and not "\s?": the CR of the file's own CRLF is not part of `\par`, and
    # eating it would leave LF-only lines in a CRLF file.
    run = re.search(r"((?:\\[a-zA-Z]+-?\d* ?|\s)+)$", body)
    if run:
        kept = "".join(token for token
                       in re.findall(r"\\[a-zA-Z]+-?\d* ?|\s", run.group(1))
                       if not re.match(r"\\par\b", token))
        body = body[:run.start(1)] + kept
    lines = tail.split("\n")
    body += rtf_escape(lines[0])
    body += "".join("\\par" + eol + rtf_escape(line) for line in lines[1:])
    with open(path, "w", encoding="latin-1", newline="") as handle:
        handle.write(body + "}" + trailing)


def append_to_txt(path, tail):
    """The 3.9/4.0 twin: a live transcript, CRLF, no markup.

    Same seam rule -- cut the archived file at its last visible character and
    let the dump's own newlines supply the gap.
    """
    with open(path, encoding="latin-1", newline="") as handle:
        text = handle.read()
    with open(path, "w", encoding="latin-1", newline="") as handle:
        handle.write(text[:len(text.rstrip())] + tail.replace("\n", "\r\n"))


def append(path, tail):
    (append_to_rtf if path.endswith(".rtf") else append_to_txt)(path, tail)


def batch(dumpdir, tags, apply_it):
    entries = R.read_manifest()
    plan = {p["tag"]: p for p in R.read_plan()}
    grafted = []
    for tag in sorted(entries):
        if tags and tag not in tags:
            continue
        dump = os.path.join(dumpdir, tag + ".txt")
        if not os.path.exists(dump):
            continue
        transcript = os.path.join(R.OUT, entries[tag]["file"])
        what, tail = graft(transcript, dump)
        print("%-24s %s" % (tag, what))
        if tail is None or not apply_it:
            continue
        append(transcript, tail)
        if not entries[tag]["source"].endswith("+tail"):
            entries[tag]["source"] += "+tail"
        entries[tag]["win_marker"] = \
            "yes" if R.has_marker(transcript, plan[tag]["marker"]) else "no"
        grafted.append(tag)
    if grafted:
        R.write_manifest(entries)
        print("\ngrafted %d row(s) -- recompare them now:" % len(grafted))
        print("  python3 harness/runner_transcripts.py recompare %s"
              % " ".join(grafted))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--batch", metavar="DUMPDIR",
                        help="a directory of <tag>.txt scrollback dumps")
    parser.add_argument("--apply", action="store_true",
                        help="write the tails (the default only reports them)")
    parser.add_argument("rest", nargs="*",
                        help="tags to limit --batch to, or <transcript> <dump>")
    args = parser.parse_args()
    if args.batch:
        batch(os.path.expanduser(args.batch), set(args.rest), args.apply)
        return
    transcript, dump = args.rest
    what, tail = graft(transcript, dump)
    print(what)
    if tail is None:
        return
    print("---- tail ----")
    print(tail)
    if args.apply:
        append(transcript, tail)
        print("appended to", transcript)


if __name__ == "__main__":
    main()
