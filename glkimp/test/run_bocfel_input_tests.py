#!/usr/bin/env python3
"""Line input in Bocfel's upper window: preloaded text, terminating keys and
meta commands.

Beyond Zork's DEFINE menu prints a function key's current definition into
the upper window and then reads a line there with that text preloaded and
the function keys as terminating characters.  Two things went wrong with it
upstream:

  garglk #383  A line ended by a terminating key left the cursor too far to
               the right, by the length of the preloaded text, so the menu
               redrew itself with a duplicated definition and put the next
               input in the wrong place.
  garglk #399  A line starting with "/" is a Bocfel meta command.  Its
               output went into the menu, the typed text stayed on screen
               (or a "/aaaa" longer than the definition left its tail
               behind), a ">" prompt was printed into the menu and the
               input came back at the left edge of the line below.

The story assembled here (define_story) does what the menu does, once per
row, and prints "|<terminator>" straight after each read, at wherever
Bocfel left the cursor.  It runs under Scarier's test/glkdrive.py fake app,
extended (GridSession) to keep the text grid the way the app does: a
cursor, unput, a line request at the cursor holding the preloaded text, and
the typed line printed into the grid when the input ends.  The checks are
on what the grid looks like and on where each line request was made.

Usage:
  python3 run_bocfel_input_tests.py [-v] [--build] [substring]

  --build  xcodebuild the Debug bocfel target first.
  -v       print the grid and the main window's text for every case.
"""

import os
import struct
import sys
import tempfile
import threading

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import run_autosave_tests as suite          # noqa: E402
from run_autosave_tests import glkdrive     # noqa: E402

TEXTGRID = 4                        # glk wintype_TextGrid
SIGNATURE = "bocfel-input-test"

# "f1:text" in a script ends the line with that key instead of Return.
KEYCODES = {"f1": 0xffffffef, "f2": 0xffffffee}     # keycode_Func1, _Func2
ZSCII = {"f1": 133, "f2": 134, "return": 13}


class GridSession(glkdrive.Driver):
    """glkdrive.Driver with the text grid windows modelled after
    GlkTextGridWindow: MOVETO, PRINT and CLRWIN, UNPRINT (which blanks the
    text before the cursor if it matches, ignoring case, and steps back
    over it), line requests with their preloaded text and terminators, and
    typedEnter's echo of the finished line."""

    def __init__(self, *args, **kwargs):
        self.grids = {}             # peer -> {"rows": {y: [chars]}, x, y}
        self.requests = []          # (x, y, preloaded) per grid line request
        self.terminators = {}       # peer -> set of keycodes
        self.buffer_text = ""       # everything printed outside the grids
        super().__init__(*args, **kwargs)

    def put(self, grid, text):
        for ch in text:
            if ch == "\n":
                grid["x"], grid["y"] = 0, grid["y"] + 1
                continue
            row = grid["rows"].setdefault(grid["y"], [])
            row.extend(" " * (grid["x"] + 1 - len(row)))
            row[grid["x"]] = ch
            grid["x"] += 1

    def row(self, peer, y):
        return "".join(self.grids[peer]["rows"].get(y, [])).rstrip()

    def dispatch(self, cmd, a1, a2, a3, a4, a5, payload):
        grid = self.grids.get(a1)
        if cmd == glkdrive.NEWWIN and a1 == TEXTGRID:
            self.grids[a2] = dict(rows={}, x=0, y=0)
        elif cmd == glkdrive.PRINT and grid is None:
            self.buffer_text += payload.decode("utf-16-le", "replace")
        elif grid is None:
            pass
        elif cmd == glkdrive.PRINT:
            self.put(grid, payload.decode("utf-16-le", "replace"))
        elif cmd == glkdrive.MOVETO:
            grid["x"], grid["y"] = a2, a3
        elif cmd == glkdrive.CLRWIN:
            grid.update(rows={}, x=0, y=0)
        elif cmd == glkdrive.UNPRINT:
            text = payload.decode("utf-16-le", "replace")
            row = grid["rows"].get(grid["y"], [])
            start = grid["x"] - len(text)
            found = (start >= 0 and "".join(row[start:grid["x"]]).upper()
                     == text.upper())
            if found:
                row[start:grid["x"]] = " " * len(text)
                grid["x"] = start
            self.reply(glkdrive.OKAY, len(text) if found else 0)
            return
        elif cmd == glkdrive.INITLINE:
            self.requests.append((grid["x"], grid["y"],
                                  payload.decode("utf-16-le", "replace")))
        elif cmd == glkdrive.TERMINATORS:
            self.terminators[a1] = set(
                struct.unpack("<%dI" % a2, payload[:4 * a2]))
        super().dispatch(cmd, a1, a2, a3, a4, a5, payload)

    def reply(self, cmd, a1=0, a2=0, a3=0, a4=0, a5=0, payload=b""):
        if cmd == glkdrive.EVTLINE and a1 in self.grids:
            grid = self.grids[a1]
            self.put(grid, payload.decode("utf-16-le"))
            grid["x"], grid["y"] = 0, grid["y"] + 1
        super().reply(cmd, a1, a2, a3, a4, a5, payload)

    def next_event(self, block):
        key = (self.script[0].split(":", 1)[0]
               if self.script and not self.arrange_pending else None)
        if key in KEYCODES and self.line_peer is not None:
            text = self.script.pop(0).split(":", 1)[1]
            peer, self.line_peer = self.line_peer, None
            code = KEYCODES[key]
            if code not in self.terminators.get(peer, ()):
                self.log.append("[%s is not a terminator in peer %d]"
                                % (key, peer))
                code = 0
            self.log.append("> %s <%s>" % (text, key))
            self.reply(glkdrive.EVTLINE, peer, len(text),
                       struct.unpack("<i", struct.pack("<I", code))[0],
                       payload=text.encode("utf-16-le"))
        else:
            super().next_event(block)


def run_session(terp, game, script, workdir):
    os.environ["SPATTERLIGHT_AUTOSAVE_SIGNATURE"] = SIGNATURE
    suite.clean_autosave(terp, SIGNATURE)
    s = GridSession(terp, game, workdir, script)
    timer = threading.Timer(suite.SESSION_TIMEOUT, s.p.kill)
    timer.start()
    try:
        s.rc, s.err = s.run()
    except (BrokenPipeError, ConnectionResetError, EOFError) as e:
        s.p.kill()
        s.p.wait()
        s.rc, s.err = s.p.returncode, type(e).__name__
    finally:
        timer.cancel()
        suite.clean_autosave(terp, SIGNATURE)
    return s


# ---- the story ------------------------------------------------------------

LABEL = "F1 "
DEFINITION = "look"
MAXCHARS = 40
TERMINATORS = suite.V5_SCRATCH + 0x70


def define_story(rounds):
    """A V5 story that, `rounds` times over, does what Beyond Zork's DEFINE
    menu does for one key: on every other row of the upper window it prints
    "F1 look", reads a line with "look" preloaded and F1/F2 as terminating
    characters, and then prints "|" and the terminator read returned, with
    no set_cursor in between.  It ends on a read in the lower window."""
    text = suite.V5_SCRATCH
    data = {text: bytes([MAXCHARS]),
            TERMINATORS: bytes([ZSCII["f1"], ZSCII["f2"], 0])}
    preload = bytes([len(DEFINITION)]) + DEFINITION.encode("ascii")
    code = bytes([0xEA, 0x7F, 2 * rounds])          # split_window
    code += bytes([0xEB, 0x7F, 1])                  # set_window 1
    for n in range(rounds):
        code += bytes([0xEF, 0x5F, 2 * n + 1, 1])   # set_cursor row 1
        code += b"\xB2" + suite.zstring(LABEL + DEFINITION)     # print
        for i, value in enumerate(preload):         # storeb text i+1 value
            code += (bytes([0xE2, 0x17]) + struct.pack(">H", text)
                     + bytes([i + 1, value]))
        code += (bytes([0xE4, 0x1F]) + struct.pack(">H", text)
                 + bytes([0, 0x11]))                # aread text 0 -> G01
        code += b"\xB2" + suite.zstring("|")        # print "|"
        code += bytes([0xE6, 0xBF, 0x11])           # print_num G01
    code += bytes([0xEB, 0x7F, 0])                  # set_window 0
    code += b"\xB2" + suite.zstring("Done")         # print "Done"
    code += bytes([0xE2, 0x17]) + struct.pack(">H", text) + bytes([1, 0])
    code += (bytes([0xE4, 0x1F]) + struct.pack(">H", text)
             + bytes([0, 0x11]))                    # aread text 0 -> G01
    code += b"\xBA"                                 # quit
    return suite.v5_story(code, data, TERMINATORS)


# ---- cases ----------------------------------------------------------------

def expected_rows(answers):
    """What the upper window must hold after `answers`, a list of (text,
    key) with one entry per round: the label and the line as typed, then
    the marker -- straight after the line for a terminating key, at the
    start of the next row for Return."""
    rows = []
    for text, key in answers:
        marker = "|%d" % ZSCII[key]
        if key == "return":
            rows += [LABEL + text, marker]
        else:
            rows += [LABEL + text + marker, ""]
    return rows


def play(terp, stage, script, rounds, res, verbose):
    game = os.path.join(stage, "define%d.z5" % rounds)
    with open(game, "wb") as f:
        f.write(define_story(rounds))
    s = run_session(terp, game, script + ["end"], stage)
    if s.rc != 0:
        res.fail("exit status %r" % (s.rc,), s.err[-1500:])
    for line in s.log:
        if "not a terminator" in line or "dropped" in line:
            res.fail(line)
    if s.script:
        res.fail("%d script lines left over" % len(s.script))
    if len(s.grids) != 1:
        res.fail("%d grid windows, expected 1" % len(s.grids))
        return s, []
    peer = next(iter(s.grids))
    rows = [s.row(peer, y) for y in range(2 * rounds)]
    if verbose:
        res.detail.append("\n".join(
            ["grid:"] + ["  %2d |%s" % (y, r) for y, r in enumerate(rows)]
            + ["requests: %r" % (s.requests,),
               "main window: %d characters, ending %r"
               % (len(s.buffer_text), s.buffer_text[-60:])]))
    return s, rows


def check_rows(res, rows, want):
    for y, (got, exp) in enumerate(zip(rows, want)):
        if got != exp:
            res.fail("row %d is %r, expected %r" % (y, got, exp))


def check_requests(res, s, rounds_of):
    """Every line request in the upper window must sit straight after the
    label of its round's row and hold the definition."""
    want = [(len(LABEL), 2 * n, DEFINITION) for n in rounds_of]
    if s.requests != want:
        res.fail("line requests (x, y, preloaded) were %r, expected %r"
                 % (s.requests, want))


def case_terminator(terp, stage, res, verbose):
    """garglk #383: after a line ended by a terminating key, the cursor
    sits straight after what was typed -- whether the preloaded text was
    kept, extended, replaced or deleted -- and each new request starts
    after its own label."""
    answers = [("look", "f1"), ("look north", "f2"), ("x", "f1"),
               ("", "f2"), ("look", "return")]
    script = [text if key == "return" else "%s:%s" % (key, text)
              for text, key in answers]
    s, rows = play(terp, stage, script, len(answers), res, verbose)
    check_rows(res, rows, expected_rows(answers))
    check_requests(res, s, range(len(answers)))


def case_meta(terp, stage, res, verbose):
    """garglk #399: a meta command typed into the upper window leaves no
    trace there.  The line is blanked however long it was and whichever
    key ended it, no prompt is printed, the command's output goes to the
    main window, and input is requested again in the same place with the
    same preloaded text."""
    script = ["/help", "/aaaaaaaaaaaaaaaaaaaaaa", "f1:/help", "wait",
              "/help", "f2:look"]
    s, rows = play(terp, stage, script, 2, res, verbose)
    check_rows(res, rows, expected_rows([("wait", "return"), ("look", "f2")]))
    check_requests(res, s, [0, 0, 0, 0, 1, 1])
    if "/undo" not in s.buffer_text:
        res.fail("the main window did not get /help's list of commands")
    if ">" in s.buffer_text:
        res.fail("a prompt was printed after a meta command typed into the"
                 " upper window")


CASES = [("bocfel/define-terminating-key", case_terminator),
         ("bocfel/define-meta-command", case_meta)]


def main():
    argv = sys.argv[1:]
    verbose, build = False, False
    while argv and argv[0].startswith("-"):
        opt = argv.pop(0)
        if opt == "-v":
            verbose = True
        elif opt == "--build":
            build = True
        else:
            sys.exit(__doc__)
    pattern = argv[0] if argv else ""

    suite.TERPS = {"bocfel": suite.TERPS["bocfel"]}
    if build:
        suite.xcodebuild()

    failed = 0
    with tempfile.TemporaryDirectory() as stage:
        terp = suite.stage_terps(stage)["bocfel"]
        for label, case in CASES:
            if pattern not in label:
                continue
            res = suite.Result()
            case(terp, stage, res, verbose)
            failed += bool(res.problems)
            print("%-32s %-8s %s" % (label, "FAIL" if res.problems else "ok",
                                     res.problems[0] if res.problems else ""),
                  flush=True)
            for msg in res.problems[1:]:
                print("%-32s          %s" % ("", msg))
            for d in res.detail:
                print("\n".join("    " + l for l in d.splitlines()))
    print("%d failed" % failed if failed else "all passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
