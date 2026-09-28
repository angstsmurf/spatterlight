#!/usr/bin/env python3
"""Spatterlight autosave/autorestore regression for both of Scarier's
engines: ADRIFT <=4 (gsc_main + the SCARAUTO4 container in glk/os_glk_autosave.cpp) and
ADRIFT 5 (gsc_a5_main + the SCARAUTO5 container).

Unlike everything else in this directory it does NOT use harness/scare: the
autosave code is compiled only into the Spatterlight build of the terp
(#ifdef SPATTERLIGHT, plus scarier-autosave.mm and libglkimp).  Each session
runs that terp under test/glkdrive.py's fake app, which feeds scripted input
and ends the session with EVTQUIT -- the window-closed exit that keeps the
autosave -- so the next session with the same game autorestores.

The main check is equivalence: a script played in ONE session must print,
command for command, exactly what it prints when the process is killed and
relaunched after chosen commands.  Determinism is on (the app's testing
mode), so this also proves the RNG, events, NPC walks and the undo history
come back exactly.  No goldens -- the single-session run is the reference.
On top of that every relaunch must

  - emit zero NEWWIN/DELWIN (see gsc_autorestore_wanted: a window the terp
    opens before adopting the archived list is a window the player loses),
  - print nothing into a text-buffer window before its first input (no intro
    replay, no second prompt),
  - have autosaved at the prompt it was closed on.

Cases marked xfail pin down state the autosave is known not to carry; an
XPASS fails the run so the list gets updated when one is fixed.  (There are
none at present.)

State that lives outside the saved game -- the line `again` repeats, command
history, pronouns, an open "Which ...?" question, a question prefix, brief/
verbose and score notification -- travels in the container's session section
(run_session_state).  A case's "expect" strings must appear in the
single-session output, proving the script really reached that state.  With
AUTOSAVE_TEST_STRIP_SESSION=1 the section is cut out of every autosave between
sessions; the cases that depend on it must then FAIL.

Autosaves are keyed by SPATTERLIGHT_AUTOSAVE_SIGNATURE (glkimp's
getautosavedir), set per case to a name no real game can have, and removed
before and after each case.  They still land in the REAL
~/Library/Application Support (NSApplicationSupportDirectory ignores $HOME).

Usage:
  python3 run_autosave_tests.py [-v] [--terp PATH] [--build] [substring]

  --terp PATH  a scarier binary with ../Frameworks/libglkimp.dylib beside it
               (e.g. build/Debug/Spatterlight.app/Contents/MacOS/scarier).
               Default: build/Debug/scarier + build/Debug/libglkimp.dylib,
               staged into a temporary bundle layout.
  --build      xcodebuild the Debug scarier target first.
  -v           print the first differing command's output on a mismatch.
"""

import os
import pwd
import shutil
import subprocess
import sys
import tempfile
import threading

HERE = os.path.dirname(os.path.abspath(__file__))
ADRIFT4 = os.path.dirname(HERE)
SCARIER = os.path.normpath(os.path.join(ADRIFT4, "..", ".."))
REPO = os.path.normpath(os.path.join(SCARIER, "..", ".."))
GAMES = os.path.join(ADRIFT4, "games")
GOLDENS = os.path.join(ADRIFT4, "goldens")
ADRIFT5 = os.path.join(SCARIER, "test", "adrift5")
A5_GAMES = os.path.join(ADRIFT5, "games")
A5_PROBES = os.path.join(ADRIFT5, "probes")
A5_GOLDENS = os.path.join(ADRIFT5, "goldens")

sys.path.insert(0, os.path.join(SCARIER, "test"))
import glkdrive  # noqa: E402

AUTOSAVE_ROOT = os.path.join(
    pwd.getpwuid(os.getuid()).pw_dir,
    "Library", "Application Support", "Spatterlight", "SCARE Files",
    "Autosaves")
SESSION_TIMEOUT = 120.0
TEXTBUFFER = 3                      # glk wintype_TextBuffer


# ---- one session ----------------------------------------------------------

class Session(glkdrive.Driver):
    """glkdrive.Driver that also keeps an ordered event list, so the output
    can be cut into one chunk per command."""

    def __init__(self, *args, **kwargs):
        self.events = []
        self.wintypes = {}
        self.newwin = 0
        self.delwin = 0
        super().__init__(*args, **kwargs)

    def reply(self, cmd, a1=0, a2=0, a3=0, a4=0, a5=0, payload=b""):
        if cmd == glkdrive.EVTLINE:
            self.events.append(("in", payload.decode("utf-16-le")))
        elif cmd == glkdrive.EVTKEY:
            self.events.append(("key", a2))
        elif cmd == glkdrive.EVTQUIT:
            self.events.append(("quit",))
        super().reply(cmd, a1, a2, a3, a4, a5, payload)

    def dispatch(self, cmd, a1, a2, a3, a4, a5, payload):
        if cmd == glkdrive.PRINT:
            self.events.append(("out", a1,
                                payload.decode("utf-16-le", "replace")))
        elif cmd == glkdrive.NEWWIN:
            self.newwin += 1
            self.wintypes[a2] = a1
            self.events.append(("newwin",))
        elif cmd == glkdrive.DELWIN:
            self.delwin += 1
            self.events.append(("delwin",))
        elif cmd == glkdrive.AUTOSAVE:
            self.events.append(("autosave",))
        super().dispatch(cmd, a1, a2, a3, a4, a5, payload)

    def chunks(self):
        """(pre, [chunk per line input]): pre is [(window, text)] printed
        before the first line input; each chunk is the text printed from one
        line input up to the next, window switches marked, keypresses
        inlined.  Nothing after EVTQUIT is kept."""
        pre, chunks, cur, win = [], [], None, None
        for ev in self.events:
            if ev[0] == "quit":
                break
            if ev[0] == "in":
                cur = ["> %s\n" % ev[1]]
                chunks.append(cur)
                win = None
            elif ev[0] == "key":
                if cur is not None:
                    cur.append("[key %d]" % ev[1])
            elif ev[0] == "out":
                if cur is None:
                    pre.append((ev[1], ev[2]))
                    continue
                if ev[1] != win:
                    cur.append("\n{win %d}\n" % ev[1])
                    win = ev[1]
                cur.append(ev[2])
        return pre, ["".join(c) for c in chunks]

    def windows_before_input(self):
        """(opened, closed) before the first line input: the autorestore's
        own window traffic, not that of a RESTART played later on."""
        opened = closed = 0
        for ev in self.events:
            if ev[0] == "in":
                break
            opened += ev[0] == "newwin"
            closed += ev[0] == "delwin"
        return opened, closed

    def autosaved_last_prompt(self):
        """True when an AUTOSAVE came after the last line input (i.e. at the
        prompt the session was closed on)."""
        for ev in reversed(self.events):
            if ev[0] == "autosave":
                return True
            if ev[0] == "in":
                return False
        return False


def run_session(terp, game, script, signature, workdir, savepath=None):
    os.environ["SPATTERLIGHT_AUTOSAVE_SIGNATURE"] = signature
    s = Session(terp, game, workdir, script, savepath=savepath)
    killed = []
    timer = threading.Timer(SESSION_TIMEOUT,
                            lambda: (killed.append(True), s.p.kill()))
    timer.start()
    try:
        s.rc, s.err = s.run()
    except (BrokenPipeError, ConnectionResetError, EOFError) as e:
        # The terp died (or was killed) mid-exchange; report it as a failed
        # session rather than letting it take the whole run down.
        s.p.kill()
        s.p.wait()
        s.rc = s.p.returncode
        s.err = "%s\n%s" % (type(e).__name__, s.p.stderr.read().decode(
            "utf-8", "replace") if s.p.stderr else "")
    finally:
        timer.cancel()
    if killed:
        s.rc = "killed after %ds" % SESSION_TIMEOUT
    s.leftover = list(s.script)
    return s


def autosave_dir(signature):
    return os.path.join(AUTOSAVE_ROOT, signature)


def clean_autosave(signature):
    shutil.rmtree(autosave_dir(signature), ignore_errors=True)


def strip_session_section(signature):
    """Cut the session section out of an autosave, leaving the container an
    older build would have written.  With AUTOSAVE_TEST_STRIP_SESSION set
    every case that depends on the section must fail -- a check that the
    cases are sensitive to it."""
    path = os.path.join(autosave_dir(signature), "autosave.glksave")
    if not os.path.exists(path):
        return
    with open(path, "rb") as f:
        data = f.read()
    pos = data.index(b"\n") + 1
    eol = data.index(b"\n", pos)
    pos = eol + 1 + int(data[pos:eol])
    if data[pos:pos + 1] != b"S":
        return
    eol = data.index(b"\n", pos + 1)
    end = eol + 1 + int(data[pos + 1:eol])
    with open(path, "wb") as f:
        f.write(data[:pos] + data[end:])


def split_script(script, cuts):
    """Cut a script into sessions after the given counts of LINE entries.  A
    "key:" entry stays with the line before it: the keypress it answers
    comes before the next prompt."""
    sessions, cur, lines = [], [], 0
    cuts = sorted(set(c for c in cuts if c > 0))
    for item in script:
        if (not item.startswith("key:") and cuts and lines == cuts[0]):
            sessions.append(cur)
            cur = []
            cuts.pop(0)
        cur.append(item)
        if not item.startswith("key:"):
            lines += 1
    sessions.append(cur)
    return sessions


def line_count(script):
    return sum(1 for item in script if not item.startswith("key:"))


# ---- cases ----------------------------------------------------------------

class Result:
    def __init__(self):
        self.problems = []
        self.detail = []

    DETAIL_LIMIT = 4000

    def fail(self, msg, detail=None):
        self.problems.append(msg)
        if detail:
            if len(detail) > self.DETAIL_LIMIT:
                detail = detail[:self.DETAIL_LIMIT] + "\n[... %d more chars]" \
                    % (len(detail) - self.DETAIL_LIMIT)
            self.detail.append(detail)


def crash_text(s):
    err = s.err or ""
    for word in ("Assertion failed", "AddressSanitizer", "Segmentation",
                 "terminating", "abort"):
        if word in err:
            return err.strip()[-800:]
    return None


def check_process(res, s, label):
    if s.rc != 0:
        res.fail("%s: exit status %s" % (label, s.rc), s.err.strip()[-800:])
    elif crash_text(s):
        res.fail("%s: engine complaint on stderr" % label, crash_text(s))


def case_equivalence(terp, case, res, verbose):
    """The control run plays the whole script in one session; the split run
    relaunches after each cut.  Their per-command output must agree."""
    game, script, cuts = case["game"], case["script"], case["cuts"]
    sig = "scarier-autosave-test-" + case["name"]
    savepath = case.get("savepath")

    with tempfile.TemporaryDirectory() as work:
        if savepath:
            savepath = os.path.join(work, savepath)

        clean_autosave(sig)
        control = run_session(terp, game, script, sig, work, savepath)
        clean_autosave(sig)
        check_process(res, control, "control")
        if control.leftover:
            res.fail("control: script not consumed, %d entries left (%r...)"
                     % (len(control.leftover), control.leftover[:3]))
        _, want = control.chunks()
        for text in case.get("expect", []):
            if text not in "".join(want):
                res.fail("control: never printed %r, so the case does not"
                         " test what it says" % text)
        if savepath and os.path.exists(savepath):
            os.remove(savepath)

        bufwins = {w for w, t in control.wintypes.items() if t == TEXTBUFFER}
        got = []
        sessions = split_script(script, cuts)
        for index, part in enumerate(sessions):
            s = run_session(terp, game, part, sig, work, savepath)
            label = "session %d" % (index + 1)
            check_process(res, s, label)
            pre, chunks = s.chunks()
            if index > 0:
                opened, closed = s.windows_before_input()
                if opened or closed:
                    res.fail("%s: autorestore opened %d and closed %d windows"
                             % (label, opened, closed))
                printed = "".join(t for w, t in pre if w in bufwins).strip()
                if printed:
                    res.fail("%s: printed into a buffer window before input"
                             % label, printed[:400])
            if index < len(sessions) - 1 and not s.autosaved_last_prompt():
                res.fail("%s: no autosave at the prompt it was closed on"
                         % label)
            if s.leftover:
                res.fail("%s: script not consumed, %d entries left"
                         % (label, len(s.leftover)))
            got.extend(chunks)
            if os.environ.get("AUTOSAVE_TEST_STRIP_SESSION"):
                strip_session_section(sig)
        clean_autosave(sig)

    for n, (w, g) in enumerate(zip(want, got)):
        if w != g:
            cmd = w.split("\n", 1)[0]
            res.fail("output differs at command %d (%s), %s"
                     % (n + 1, cmd, session_of(cuts, n)),
                     "--- one session ---\n%s\n--- relaunched ---\n%s"
                     % (w.rstrip(), g.rstrip()) if verbose else None)
            break
    else:
        if len(want) != len(got):
            res.fail("command count differs: %d in one session, %d relaunched"
                     % (len(want), len(got)))


def session_of(cuts, n):
    before = sum(1 for c in cuts if c <= n)
    first = (n == 0) or any(c == n for c in cuts)
    return "session %d%s" % (before + 1,
                             ", its first command" if first and n else "")


def case_no_autosave_at_name_prompt(terp, case, res, verbose):
    """A game closed at its startup name prompt must not autosave: resuming
    there would replay the intro over the restored transcript.  The next
    launch must start fresh."""
    sig = "scarier-autosave-test-" + case["name"]
    game = case["game"]
    with tempfile.TemporaryDirectory() as work:
        clean_autosave(sig)
        s1 = run_session(terp, game, [], sig, work)
        check_process(res, s1, "session 1")
        if "autosave" in [e[0] for e in s1.events]:
            res.fail("session 1: autosaved at the name prompt")
        if os.path.exists(os.path.join(autosave_dir(sig), "autosave.glksave")):
            res.fail("session 1: autosave.glksave left behind")
        s2 = run_session(terp, game, [case["name_answer"], "look"], sig, work)
        check_process(res, s2, "session 2")
        if not s2.newwin:
            res.fail("session 2: opened no windows, so it did not boot fresh")
        if case["intro"] not in "".join(s2.transcript):
            res.fail("session 2: intro text %r missing" % case["intro"])
        clean_autosave(sig)


def case_damaged_container(terp, case, res, verbose):
    """autosave.glksave damaged between sessions.  A container whose engine
    state is unreadable must be discarded (gsc_main's bad-autosave exit),
    after which the game boots fresh; one that only lost its undo tail must
    still restore the game state, with no undo past the restore point."""
    sig = "scarier-autosave-test-" + case["name"]
    game, first = case["game"], case["first"]
    path = os.path.join(autosave_dir(sig), "autosave.glksave")
    with tempfile.TemporaryDirectory() as work:
        clean_autosave(sig)
        s1 = run_session(terp, game, first, sig, work)
        check_process(res, s1, "session 1")
        if not os.path.exists(path):
            res.fail("session 1: no autosave.glksave written")
            return
        with open(path, "rb") as f:
            data = f.read()
        magic = data[:data.index(b"\n") + 1]     # SCARAUTO4 or SCARAUTO5

        if case["damage"] == "garbage":
            with open(path, "wb") as f:
                f.write(magic + b"12\nnot a game!!0\n0\n")
            s2 = run_session(terp, game, ["look"], sig, work)
            check_process(res, s2, "session 2")
            if s2.events and any(e[0] == "in" for e in s2.events):
                res.fail("session 2: took input from a corrupt autosave")
            for name in ("autosave.glksave", "autosave.plist"):
                if os.path.exists(os.path.join(autosave_dir(sig), name)):
                    res.fail("session 2: %s not discarded" % name)
            s3 = run_session(terp, game, case["fresh"], sig, work)
            check_process(res, s3, "session 3")
            if not s3.newwin:
                res.fail("session 3: did not boot fresh")
            if case["intro"] not in "".join(s3.transcript):
                res.fail("session 3: intro text %r missing" % case["intro"])

        elif case["damage"] == "no-undo-tail":
            # Keep the magic line and the engine chunk only.
            eol = data.index(b"\n", len(magic))
            length = int(data[len(magic):eol])
            with open(path, "wb") as f:
                f.write(data[:eol + 1 + length])
            s2 = run_session(terp, game, case["second"], sig, work)
            check_process(res, s2, "session 2")
            if s2.newwin or s2.delwin:
                res.fail("session 2: autorestore opened/closed windows")
            text = "".join(s2.transcript)
            for want in case["expect"]:
                if want not in text:
                    res.fail("session 2: %r missing" % want,
                             text[-600:] if verbose else None)
        clean_autosave(sig)


def solution(name):
    """An ADRIFT 4 walkthrough.  os_ansi (harness/scare) skips a line whose
    first non-blank character is '#' -- the solution files carry prose
    comments -- so the same lines must not be typed here, where the terp is
    os_glk and would parse the prose (haunt's three comment lines were three
    spare turns, and its wolves caught the player).  Blank lines ARE turns."""
    with open(os.path.join(GOLDENS, name), encoding="latin-1") as f:
        lines = [line.rstrip("\r\n") for line in f]
    return [l for l in lines if not l.lstrip(" \t").startswith("#")]


def game(name):
    return os.path.join(GAMES, name)


def a5_solution(name):
    """An ADRIFT 5 walkthrough: a5run_dump skips blank and '#' lines, so a
    blank line here is not a turn (unlike an ADRIFT 4 script)."""
    with open(os.path.join(A5_GOLDENS, name), encoding="utf-8") as f:
        lines = [line.rstrip("\r\n") for line in f]
    return [l for l in lines if l and not l.startswith("#")]


def a5_game(name):
    return os.path.join(A5_GAMES, name)


def a5_probe(name):
    return os.path.join(A5_PROBES, name)


def spread(script, parts):
    n = line_count(script)
    return [n * i // parts for i in range(1, parts)]


def build_cases():
    cases = []

    def equiv(name, gamefile, script, cuts, **kw):
        cases.append(dict(kind=case_equivalence, name=name, game=gamefile,
                          script=script, cuts=cuts, **kw))

    def walk(name, gamefile, sol, parts=4, stop=None, **kw):
        if not os.path.exists(os.path.join(GOLDENS, sol)):
            cases.append(dict(kind=None, name=name, game=gamefile,
                              missing=sol))
            return
        script = solution(sol)[:stop]
        equiv(name, gamefile, script, spread(script, parts), **kw)

    maze = game("ADRIFTMaze.taf")
    probe39 = os.path.join(HERE, "p39ADMIN.taf")

    # Whole walkthroughs, relaunched at three points, one per engine era.
    walk("maze-4.00", maze, "adrift_maze_solution.txt")
    # Routes that depend on the RNG (Les Feux's fights, Great Escape's car
    # chase) need their stream: run_v4_walkthroughs.sh plays them under
    # SCR_RNG=xoshiro with each row's SCR_SEED, and the Spatterlight build
    # reads both variables, so the cases set them.  Its default generator is
    # a different one (erkyrath_random, scutils.cpp), under which Les Feux's
    # character rolls differ from the very first command and the route dies
    # in the shop; a seed alone replays nothing.
    # The `expect` strings are the rows' win markers: the route must reach
    # its ending here too, or the stream is not the one it was derived on.
    walk("les-feux-4.00-battle", game("Les Feux de l'enfer.taf"),
         "les_feux_solution.txt", env={"SCR_SEED": "45", "SCR_RNG": "xoshiro"},
         expect=["Votre score est 75 sur un maximum de 115."])
    walk("great-escape-4.00", game("great.taf"), "great_escape_solution.txt",
         env={"SCR_SEED": "2", "SCR_RNG": "xoshiro"},
         expect=["cry of joy, you have made it, you have escaped!!"])
    # WesGHN plays ./phantasm.mid at its first prompt, which is how its
    # SIGTRAP (glkimp's loadsound snprintf size) was found.
    walk("wesghn-4.00-sound", game("WesGHN.taf"), "wes_ghn_solution.txt")
    walk("zombies-3.90", game("ZAC.taf"), "zombies_solution.txt")
    walk("haunt-3.80-events", game("haunt.taf"), "haunt_solution.txt")
    walk("wrecked-3.80-random", game("wrecked.taf"), "wrecked_solution.txt")
    # marooned's pill starts inside an unset (-1) parent, which once aborted
    # the Glk build's bounds-checked parse at startup.
    walk("marooned-3.80-parent", game("marooned.taf"), "marooned_solution.txt")
    walk("castle-3.7x", game("castle.taf"), "castle_quest_solution.txt")

    # Relaunch after every single command.
    script = solution("adrift_maze_solution.txt")[:10]
    equiv("maze-every-command", maze, script, list(range(1, len(script))))

    # Undo history: the memo ring and the one-turn-back buffer both cross
    # the relaunch, including after undos already taken.
    equiv("undo-across-relaunch", maze,
          ["adventurer", "n", "e", "e", "undo", "undo", "look", "undo",
           "look", "undo", "look"],
          [4, 6, 8])

    # 3.9's turn counter counts every line element, undo keeps it, and the
    # probe's event prints TICK on every ticking turn.
    equiv("turns-3.90", probe39,
          ["turns", "", "wait", "turns", "score", "hint", "turns", "undo",
           "turns", "wait. wait", "turns"],
          [2, 4, 7, 9])

    # Everything below lives outside the saved game and crosses a relaunch
    # in the container's session section (run_session_state()).  `expect`
    # proves the control run really reached the state being carried.

    # `again` repeats the previous line element; 4.0 echoes it in brackets.
    equiv("again-3.90", probe39, ["wait", "turns", "again", "turns"], [2])
    equiv("again-4.00", maze,
          ["adventurer", "n", "again", "s", "again", "look"], [2, 4],
          expect=["(n)"])

    # The command history, past the 64 entries its ring holds.
    equiv("history", maze,
          ["adventurer"] + ["look"] * 70 + ["history", "n", "history"],
          [40, 71, 72], expect=["  71 -- Time"])

    # Pronouns, in the game and in the one-turn undo buffer.
    maze_route = solution("adrift_maze_solution.txt")
    equiv("pronouns", maze, maze_route[:24] + ["read it"], [24],
          expect=["(a trophy)"])
    equiv("undo-pronouns", maze,
          maze_route[:24] + ["look", "undo", "read it"], [25, 26],
          expect=["(a trophy)"])

    # A 4.0 "Which tree?  ...?" still open when the game is closed, answered
    # after the relaunch.
    equiv("which-question", os.path.join(HERE, "p4CO.taf"),
          ["look", "chop tree", "red", "chop tree", "zzz", "x tree",
           "chop keys", "x keys", "chop rock", "x rock", "look"],
          [2, 4, 6, 7, 9], expect=["Which tree?"])

    # The question prefix: "Wear what?", "...with?", "Give what?" and the
    # battle's "Who do you want to attack?" all make the next line a
    # continuation of this one.
    equiv("wear-what-prefix", game("ptbad.taf"),
          ["i", "wear zzz", "goggles", "remove zzz", "goggles", "wear zzz",
           "wield zzz", "goggles", "i"],
          [2, 4, 6, 7], expect=["Wear what?"])
    equiv("with-prefix", os.path.join(HERE, "p4WITHQ.taf"),
          ["probe", "cut rope", "knife", "saw rope", "knife", "hum", "knife",
           "whittle rope", "knife", "cut rope", "probe", "knife", "push",
           "button", "give", "coin", "dave", "give zzz", "coin", "break",
           "button", "i"],
          [2, 4, 8, 10, 13, 15, 16, 18, 20], expect=["with?"])
    equiv("battle-who-prefix", os.path.join(HERE, "p4BATTLEHASH.taf"),
          ["attack", "gargoyle #1", "attack", "look", "gargoyle #2",
           "attack", "attack", "gargoyle #3", "attack with blaster",
           "gargoyle #1", "kick", "nonsense words", "gargoyle #2", "hit",
           "kick", "gargoyle #3", "attack", "blaster", "turns",
           "attack then gargoyle #2", "turns"],
          [1, 3, 4, 6, 7, 9, 11, 12, 15, 17],
          expect=["Who do you want to attack?"])

    # The player's settings: brief/verbose and score notification.
    equiv("verbose-notify", maze,
          ["adventurer", "brief", "n", "s", "n", "notify on", "s", "notify",
           "verbose", "n", "notify off", "s", "notify"],
          [2, 5, 6, 9, 11], expect=["brief", "is now"])

    # A name typed at the startup prompt lives in the property bundle, not
    # the saved game; the container carries it.  The trophy engraving
    # (command 25) prints it.
    equiv("player-name", maze, solution("adrift_maze_solution.txt")[:25], [1])

    # An in-game save/restore before the relaunch, and a restore after it.
    # Both ask "Do you really want to..." first.
    equiv("save-restore", maze,
          ["adventurer", "n", "save", "key:y", "e", "e", "restore", "key:y",
           "look", "e", "restore", "key:y", "look", "undo", "look"],
          [3, 6, 8, 9], savepath="checkpoint.sav")

    # Restart, then relaunch into the restarted game.  Not closed at the
    # name prompt the restart asks: like the startup one it never autosaves.
    equiv("restart", maze,
          ["adventurer", "n", "e", "restart", "key:y", "adventurer", "look",
           "n", "look"],
          [3, 6, 7])

    cases.append(dict(kind=case_no_autosave_at_name_prompt,
                      name="name-prompt", game=maze,
                      name_answer="adventurer",
                      intro="Welcome to the ADRIFT Maze"))
    cases.append(dict(kind=case_damaged_container, name="corrupt-container",
                      game=maze, damage="garbage",
                      first=["adventurer", "n"], fresh=["adventurer", "look"],
                      intro="Welcome to the ADRIFT Maze"))
    cases.append(dict(kind=case_damaged_container, name="truncated-undo-tail",
                      game=maze, damage="no-undo-tail",
                      first=["adventurer", "n", "e"],
                      second=["undo", "look"],
                      expect=["I can't undo",
                              "You can move north, east and west"]))

    # ---- ADRIFT 5 --------------------------------------------------------
    # Same checks against gsc_a5_main and the SCARAUTO5 container.  The
    # committed synthetic probes (test/adrift5/probes) carry most of it, so
    # the cases run without the downloaded corpus; the corpus walks SKIP
    # when their game is absent.

    def a5_walk(name, gamefile, sol, parts=4, stop=None, **kw):
        if not os.path.exists(os.path.join(A5_GOLDENS, sol)):
            cases.append(dict(kind=None, name=name, game=gamefile,
                              missing=sol))
            return
        script = a5_solution(sol)[:stop]
        equiv(name, gamefile, script, spread(script, parts), **kw)

    events = a5_probe("events.taf")
    walkprobe = a5_probe("walk.taf")
    ambiguity = a5_probe("ambiguity.taf")

    # Events (start/pause/resume/stop, the after-delay and turn-based kinds),
    # NPC walks, and RAND-driven variables and moves: each lives in the save
    # XML, and the RNG state with it.
    a5_walk("a5-events", events, "ProbeEvents_walkthrough.txt")
    a5_walk("a5-walk", walkprobe, "ProbeWalk_walkthrough.txt")
    a5_walk("a5-randomness", a5_probe("randomness.taf"),
            "ProbeRandomness_walkthrough.txt", parts=3)
    a5_walk("a5-variables", a5_probe("variables.taf"),
            "ProbeVariables_walkthrough.txt")
    # Whole games from the corpus: a Blorb with graphics and a status line,
    # and Hunt the Wumpus, whose bats, pits and arrows are all RAND.
    a5_walk("a5-4rooms", a5_game("4rooms.blorb"), "4rooms_walkthrough.txt",
            parts=3)
    a5_walk("a5-alien-diver", a5_game("AlienDiver.blorb"),
            "AlienDiver_walkthrough.txt")
    a5_walk("a5-wumpus-random", a5_game("Wumpus.taf"),
            "Wumpus_walkthrough.txt")
    a5_walk("a5-beginners-cave", a5_game("BeginnersCave.taf"),
            "BeginnersCave_walkthrough.txt")

    # Relaunch after every single command.
    script = a5_solution("ProbeWalk_walkthrough.txt")[:12]
    equiv("a5-walk-every-command", walkprobe, script,
          list(range(1, len(script))))

    # The undo stack (a5run_undo_peek / a5run_undo_push_blob) crosses the
    # relaunch, including after undos already taken, and drains to "can't
    # undo" at the same point either way.
    equiv("a5-undo-across-relaunch", events,
          ["wait", "start after", "wait", "wait", "undo", "undo", "look",
           "undo", "look", "undo", "undo", "undo", "undo", "undo", "look"],
          [3, 5, 7, 9, 12])
    # UNDO at the game-over prompt, relaunched just before the fatal command
    # and just after the undo; the undos after that reach back into
    # snapshots taken in earlier sessions.  No cut lands on the game-over
    # prompt itself: that one never autosaves.  (The container's last-turn
    # text, a5run_get/set_turn_text, has no visible effect here: the Glk
    # build answers a successful UNDO with a fresh room view,
    # gsc_a5_undo_look, and only a5run_dump's endgame path replays it.)
    equiv("a5-undo-after-end", a5_probe("undo_after_end.taf"),
          ["wait", "pause", "wait", "win", "undo", "undo", "undo", "wait",
           "wait"],
          [3, 5, 6], expect=["undone"])

    # The pronoun a bare "it" resolves to, set before the relaunch and
    # used after it.
    rooms = a5_game("4rooms.blorb")
    equiv("a5-pronouns", rooms,
          ["west", "x chest", "open it", "close it", "look in it"], [2, 3],
          expect=["(the wooden chest)"])
    # An in-game save/restore before the relaunch, and a restore after it.
    equiv("a5-save-restore", rooms,
          ["west", "open chest", "save", "east", "restore", "look", "east",
           "restore", "look", "undo", "look"],
          [3, 5, 6, 7, 8], savepath="checkpoint.sav",
          expect=["Game saved.", "Game restored."])
    # Restart, then relaunch into the restarted game.
    equiv("a5-restart", rooms,
          ["west", "open chest", "restart", "look", "west", "look in chest"],
          [2, 3, 4], expect=["closed"])

    # Answered "Which key?" questions, relaunched only at settled prompts.
    equiv("a5-ambiguity", ambiguity, a5_solution(
        "ProbeAmbiguity_walkthrough.txt"), [2, 3, 5, 9])
    # Relaunched while the question is open (cuts 2 and 6 fall between
    # "get key" and its answer): the container's pending chunk carries the
    # question, so the answer is taken as one, not as a fresh command.
    equiv("a5-which-open", ambiguity,
          ["i", "get key", "blue", "i", "drop key", "get key", "red", "i"],
          [2, 6], expect=["Which"])
    # ... and when the question is the very first thing typed.
    equiv("a5-which-open-first", ambiguity,
          ["get key", "blue", "i"], [1], expect=["Which"])
    # A bare verb remembered across the relaunch ("Get what?").
    equiv("a5-bare-verb-open", ambiguity,
          ["get", "blue key", "i", "drop", "blue key", "i"], [1, 4],
          expect=["what?"])

    cases.append(dict(kind=case_damaged_container,
                      name="a5-corrupt-container", game=events,
                      damage="garbage", first=["wait", "wait"],
                      fresh=["look"], intro="Events Test"))
    cases.append(dict(kind=case_damaged_container,
                      name="a5-truncated-undo-tail", game=events,
                      damage="no-undo-tail",
                      first=["wait", "start after", "wait"],
                      second=["undo", "wait", "wait", "wait"],
                      expect=["no more undo is available",
                              "Delayed event is ending."]))
    return cases


# ---- driver ---------------------------------------------------------------

def stage_default_terp(tmp):
    build = os.path.join(REPO, "build", "Debug")
    terp = os.path.join(build, "scarier")
    lib = os.path.join(build, "libglkimp.dylib")
    for path in (terp, lib):
        if not os.path.exists(path):
            sys.exit("run_autosave_tests: %s missing; pass --build or --terp"
                     % path)
    newest = max(os.path.getmtime(os.path.join(d, f))
                 for d in (SCARIER, os.path.join(SCARIER, "adrift5"))
                 for f in os.listdir(d)
                 if f.endswith((".cpp", ".inc", ".h", ".mm")))
    if os.path.getmtime(terp) < newest:
        sys.exit("run_autosave_tests: %s is older than the engine sources;"
                 " pass --build (or --terp)" % terp)
    os.makedirs(os.path.join(tmp, "MacOS"))
    os.makedirs(os.path.join(tmp, "Frameworks"))
    shutil.copy2(terp, os.path.join(tmp, "MacOS", "scarier"))
    shutil.copy2(lib, os.path.join(tmp, "Frameworks", "libglkimp.dylib"))
    return os.path.join(tmp, "MacOS", "scarier")


def xcodebuild():
    cmd = ["xcodebuild", "-project", os.path.join(REPO, "Spatterlight.xcodeproj"),
           "-target", "scarier", "-configuration", "Debug", "-arch", "arm64",
           "ONLY_ACTIVE_ARCH=YES", "CLANG_ENABLE_EXPLICIT_MODULES=NO",
           "SYMROOT=" + os.path.join(REPO, "build"), "build"]
    print("building scarier (Debug)...", file=sys.stderr)
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if r.returncode != 0:
        sys.stderr.write(r.stdout.decode("utf-8", "replace")[-3000:])
        sys.exit("run_autosave_tests: build failed")


def main():
    argv = sys.argv[1:]
    verbose, terp, build = False, None, False
    while argv and argv[0].startswith("-"):
        opt = argv.pop(0)
        if opt == "-v":
            verbose = True
        elif opt == "--terp":
            terp = os.path.abspath(argv.pop(0))
        elif opt == "--build":
            build = True
        else:
            sys.exit(__doc__)
    pattern = argv[0] if argv else ""

    if build:
        xcodebuild()

    failed = 0
    with tempfile.TemporaryDirectory() as stage:
        if terp is None:
            terp = stage_default_terp(stage)
        for case in build_cases():
            if pattern not in case["name"]:
                continue
            if case["kind"] is None:
                print("%-28s NOSCRIPT (%s)" % (case["name"], case["missing"]))
                continue
            if not os.path.exists(case["game"]):
                print("%-28s SKIP     (%s)"
                      % (case["name"], os.path.basename(case["game"])))
                continue
            res = Result()
            # A case's own environment (a route's SCR_SEED) reaches the terp
            # through glkdrive, which passes os.environ on.
            saved = {k: os.environ.get(k) for k in case.get("env", {})}
            os.environ.update(case.get("env", {}))
            try:
                case["kind"](terp, case, res, verbose)
            finally:
                for k, v in saved.items():
                    if v is None:
                        os.environ.pop(k, None)
                    else:
                        os.environ[k] = v
            if case.get("xfail"):
                status = "XFAIL" if res.problems else "XPASS"
                bad = not res.problems
            else:
                status = "FAIL" if res.problems else "ok"
                bad = bool(res.problems)
            failed += bad
            note = res.problems[0] if res.problems else case.get("xfail", "")
            print("%-28s %-8s %s" % (case["name"], status, note))
            if bad or verbose:
                for msg in res.problems[1:]:
                    print("%-28s          %s" % ("", msg))
                for d in res.detail:
                    print("\n".join("    " + l for l in d.splitlines()))
    print("%d failed" % failed if failed else "all passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
