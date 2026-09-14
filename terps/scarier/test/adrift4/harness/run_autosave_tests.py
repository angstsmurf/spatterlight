#!/usr/bin/env python3
"""Spatterlight autosave/autorestore regression for Scarier's ADRIFT <=4
engine (gsc_main + the SCARAUTO4 container in os_glk.cpp).

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
        elif cmd == glkdrive.DELWIN:
            self.delwin += 1
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
                if s.newwin or s.delwin:
                    res.fail("%s: autorestore opened %d and closed %d windows"
                             % (label, s.newwin, s.delwin))
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

        if case["damage"] == "garbage":
            with open(path, "wb") as f:
                f.write(b"SCARAUTO4\n12\nnot a game!!0\n0\n")
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
            eol = data.index(b"\n", len(b"SCARAUTO4\n"))
            length = int(data[len(b"SCARAUTO4\n"):eol])
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
    with open(os.path.join(GOLDENS, name), encoding="latin-1") as f:
        return [line.rstrip("\r\n") for line in f]


def game(name):
    return os.path.join(GAMES, name)


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
    # great.taf is out: its route, seeded for harness/scare, dies in the
    # car chase here and the session never finishes.  Les Feux's route is seeded for
    # harness/scare too: here the ghoul survives its five attacks and kills
    # the player on the next move, so the walk stops after the fights before
    # it (assassin, ogre and the ghoul's own).
    walk("les-feux-4.00-battle", game("Les Feux de l'enfer.taf"),
         "les_feux_solution.txt", stop=91)
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

    # A 4.0 "Which tree.  ...?" still open when the game is closed, answered
    # after the relaunch.
    equiv("which-question", os.path.join(HERE, "p4CO.taf"),
          ["look", "chop tree", "red", "chop tree", "zzz", "x tree",
           "chop keys", "x keys", "chop rock", "x rock", "look"],
          [2, 4, 6, 7, 9], expect=["Which tree."])

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
    newest = max(os.path.getmtime(os.path.join(SCARIER, f))
                 for f in os.listdir(SCARIER)
                 if f.endswith((".cpp", ".h", ".mm")))
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
            case["kind"](terp, case, res, verbose)
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
