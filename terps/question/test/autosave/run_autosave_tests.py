#!/usr/bin/env python3
"""Spatterlight autosave/autorestore regression for both Question engines: the
Quest 4 frontend (questionglk.cc, the GEASAUTO1 container) and the Quest 5 one
(quest5/aslxglk.cc, the SAVE-format game plus the ASLXGLK-AUTOSAVE blob).

Unlike everything else under test/ it does NOT use the in-repo harnesses: the
autosave code is compiled only into the Spatterlight build of the terp
(#ifdef SPATTERLIGHT, questionglk-autosave.mm and libglkimp).  Each session runs
that terp under test/glkdrive.py's fake app, which feeds scripted input and
ends the session with EVTQUIT -- the window-closed exit that keeps the
autosave -- so the next session with the same game autorestores.

The main check is equivalence: a script played in ONE session must print,
command for command, exactly what it prints when the process is killed and
relaunched after chosen commands.  Determinism is on (the app's testing
mode), so this also proves the RNG, the timers, the variables and the undo
history come back exactly.  No goldens -- the single-session run is
the reference.  Only the main text window is compared: the status line and
the side pane are repainted at boot on a relaunch, in the pre-input stretch,
where the control run has no such repaint.  On top of that every relaunch must

  - close every window it opened at boot (the archived windows come back
    without NEWWIN; a boot window that survives the restore is a window the
    player would see twice, one that is deleted but not reopened one they lose),
  - print nothing into the main window before its first input (no intro
    replay, no second prompt, no menu redraw),
  - have autosaved after the last thing the previous session did: a typed
    line, a timer tick that fired, a pane click.

A game closed on a prompt of its own resumes in that prompt: the Quest 5 verb
menu popped by clicking an object link, which is recorded in the blob and
reopened; and every menu (the parser's own "which one do you mean?" among
them), yes/no question, typed answer -- Quest 4's `enter`, Quest 5's `get
input` -- and "press a key" pause (`wait`) that a turn or the start script
puts up.  Those autosave the state from the start of the turn plus a replay
record (the command line and the answers to earlier prompts of the same turn,
a keypress counting as one; for the start script the answers alone, the boot
itself being what is re-run); the relaunch re-runs the turn muted up to the
prompt and waits there without redrawing it.
case_pending_prompt covers all of these.

A damaged autosave (garbage where the game state should be) must be
discarded without taking input, after which the next launch boots fresh; a
Quest 4 container that only lost its undo history must still restore.

Autosaves are keyed by SPATTERLIGHT_AUTOSAVE_SIGNATURE (glkimp's
getautosavedir), set per case to a name no real game can have, and removed
before and after each case.  They still land in the REAL
~/Library/Application Support (NSApplicationSupportDirectory ignores $HOME).

Usage:
  python3 run_autosave_tests.py [-v] [--terp PATH] [--build] [substring]

  --terp PATH  a Question binary with ../Frameworks/libglkimp.dylib beside it
               (e.g. build/Debug/Spatterlight.app/Contents/MacOS/question).
               Default: build/Debug/question + build/Debug/libglkimp.dylib,
               staged into a temporary bundle layout.
  --build      xcodebuild the Debug Question target first.
  -v           print the first differing command's output on a mismatch.

The Quest 5 terp needs the Core library: ASLX_CORE is pointed at
terps/question/quest5/aslx-core unless already set.  The corpus walks
(quest4/games, quest5/games) are skipped when the games are not there.
"""

import os
import pwd
import shutil
import subprocess
import sys
import tempfile
import threading

HERE = os.path.dirname(os.path.abspath(__file__))
TEST = os.path.dirname(HERE)
QUESTION = os.path.dirname(TEST)
REPO = os.path.normpath(os.path.join(QUESTION, "..", ".."))
Q4GAMES = os.path.join(TEST, "quest4", "games")
Q4GOLDENS = os.path.join(TEST, "quest4", "goldens")
Q5GAMES = os.path.join(TEST, "quest5", "games")
Q5GOLDENS = os.path.join(TEST, "quest5", "goldens")
ASLX_CORE = os.path.join(QUESTION, "quest5", "aslx-core")

sys.path.insert(0, TEST)
import glkdrive  # noqa: E402

AUTOSAVE_ROOT = os.path.join(
    pwd.getpwuid(os.getuid()).pw_dir,
    "Library", "Application Support", "Spatterlight", "Quest Files",
    "Autosaves")
SESSION_TIMEOUT = 120.0
SIG_PREFIX = "question-autosave-test-"

# Script entries that are not typed lines.  A key: answers a char request
# the previous line opened, and a tick: is time passing at the prompt the
# previous line left, so both stay with that line when a script is cut; a
# click: is an interaction in its own right and can be cut after.
GLUED = ("key", "tick")
NONLINE = ("key", "tick", "click", "link")


def kind_of(item):
    head = item.split(":", 1)[0]
    return head if ":" in item and head in NONLINE else "line"


# ---- one session ----------------------------------------------------------

class Session(glkdrive.Driver):
    """glkdrive.Driver that also keeps an ordered event list, so the output
    can be cut into one chunk per command."""

    def __init__(self, *args, **kwargs):
        self.events = []
        self.newwin = 0
        self.delwin = 0
        self.main_peer = None
        super().__init__(*args, **kwargs)

    def reply(self, cmd, a1=0, a2=0, a3=0, a4=0, a5=0, payload=b""):
        if cmd == glkdrive.EVTLINE:
            self.events.append(("in", payload.decode("utf-16-le")))
        elif cmd == glkdrive.EVTKEY:
            self.events.append(("key", a2))
        elif cmd == glkdrive.EVTHYPER:
            self.events.append(("click", a1, a2))
        elif cmd == glkdrive.EVTTIMER:
            if self.timer and self.waiting_for_input():
                self.events.append(("tick",))
        elif cmd == glkdrive.EVTQUIT:
            self.events.append(("quit",))
        super().reply(cmd, a1, a2, a3, a4, a5, payload)

    def dispatch(self, cmd, a1, a2, a3, a4, a5, payload):
        if cmd == glkdrive.PRINT:
            self.events.append(("out", a1,
                                payload.decode("utf-16-le", "replace")))
        elif cmd == glkdrive.INITLINE:
            if self.main_peer is None:
                self.main_peer = a1
        elif cmd == glkdrive.NEWWIN:
            self.newwin += 1
        elif cmd == glkdrive.DELWIN:
            self.delwin += 1
        elif cmd == glkdrive.AUTOSAVE:
            self.events.append(("autosave",))
        super().dispatch(cmd, a1, a2, a3, a4, a5, payload)

    def chunks(self):
        """(pre, [chunk per input]): pre is the main-window text printed
        before the first input; each chunk is the main-window text printed
        from one input -- a typed line or a hyperlink click, either of which
        can start a relaunched session -- up to the next, with keypresses and
        timer ticks inlined.  A keypress ahead of every line and click opens
        a chunk of its own: it answers a pause the start script put up, or
        (self.continues) the pause the previous session was closed on, and
        that chunk is then the rest of that session's last one.  Nothing
        after EVTQUIT is kept."""
        pre, chunks, cur = [], [], None
        self.continues = False
        for ev in self.events:
            if ev[0] == "quit":
                break
            if ev[0] == "in":
                cur = ["> %s\n" % ev[1]]
                chunks.append(cur)
            elif ev[0] == "click":
                cur = ["[click peer %d link %d]\n" % (ev[1], ev[2])]
                chunks.append(cur)
            elif ev[0] in ("key", "tick"):
                mark = "[%s]" % " ".join(str(x) for x in ev)
                if cur is None and ev[0] == "key":
                    cur = [mark]
                    chunks.append(cur)
                    self.continues = True
                elif cur is not None:
                    cur.append(mark)
                else:
                    pre.append(mark)
            elif ev[0] == "out" and ev[1] == self.main_peer:
                if cur is None:
                    pre.append(ev[2])
                else:
                    cur.append(ev[2])
        return "".join(pre), ["".join(c) for c in chunks]

    def autosaved_since_last_input(self):
        """True when an AUTOSAVE came after the last input-like event -- the
        line, click or firing tick the session was closed on."""
        for ev in reversed(self.events):
            if ev[0] == "autosave":
                return True
            if ev[0] in ("in", "click", "tick"):
                return False
        return False

    def autosaved_after_last_line(self):
        for ev in reversed(self.events):
            if ev[0] == "autosave":
                return True
            if ev[0] == "in":
                return False
        return False


def run_session(terp, game, script, signature, workdir, sa_delays=0):
    os.environ["SPATTERLIGHT_AUTOSAVE_SIGNATURE"] = signature
    os.environ.setdefault("ASLX_CORE", ASLX_CORE)
    s = Session(terp, game, workdir, script, sa_delays=sa_delays)
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


def autosave_file(signature, name="autosave.glksave"):
    return os.path.join(autosave_dir(signature), name)


def clean_autosave(signature):
    shutil.rmtree(autosave_dir(signature), ignore_errors=True)


def split_script(script, cuts):
    """Cut a script into sessions after the given counts of top-level
    entries (lines and clicks).  Glued entries (key:, tick:) stay with the
    entry before them."""
    sessions, cur, count = [], [], 0
    cuts = sorted(set(c for c in cuts if c > 0))
    for item in script:
        if kind_of(item) not in GLUED and cuts and count == cuts[0]:
            sessions.append(cur)
            cur = []
            cuts.pop(0)
        cur.append(item)
        if kind_of(item) not in GLUED:
            count += 1
    sessions.append(cur)
    return sessions


def entry_count(script):
    return sum(1 for item in script if kind_of(item) not in GLUED)


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
                 "terminating", "abort", "autosave"):
        if word in err:
            return err.strip()[-800:]
    return None


def check_process(res, s, label):
    if s.rc != 0:
        res.fail("%s: exit status %s" % (label, s.rc), s.err.strip()[-800:])
    elif crash_text(s):
        res.fail("%s: engine complaint on stderr" % label, crash_text(s))


def check_relaunch(res, s, label):
    """The invariants of a session that autorestored."""
    if s.newwin != s.delwin:
        res.fail("%s: autorestore opened %d windows at boot but closed %d"
                 % (label, s.newwin, s.delwin))
    pre, _ = s.chunks()
    if pre.strip():
        res.fail("%s: printed into the main window before input" % label,
                 pre[:400])


def compare_chunks(res, want, got, cuts, verbose):
    for n, (w, g) in enumerate(zip(want, got)):
        if w != g:
            cmd = w.split("\n", 1)[0]
            res.fail("output differs at command %d (%s), %s"
                     % (n + 1, cmd, session_of(cuts, n)),
                     "--- one session ---\n%s\n--- relaunched ---\n%s"
                     % (w.rstrip(), g.rstrip()) if verbose else None)
            return
    if len(want) != len(got):
        res.fail("command count differs: %d in one session, %d relaunched"
                 % (len(want), len(got)))


def session_of(cuts, n):
    before = sum(1 for c in cuts if c <= n)
    first = (n == 0) or any(c == n for c in cuts)
    return "session %d%s" % (before + 1,
                             ", its first command" if first and n else "")


def control_run(terp, case, res, sig, work):
    """The whole script in one session: the reference output."""
    control = run_session(terp, case["game"], case["script"], sig, work,
                          case.get("sa_delays", 0))
    clean_autosave(sig)
    check_process(res, control, "control")
    if control.leftover:
        res.fail("control: script not consumed, %d entries left (%r...)"
                 % (len(control.leftover), control.leftover[:3]))
    _, want = control.chunks()
    for text in case.get("expect", []):
        if text not in "".join(want):
            res.fail("control: never printed %r, so the case does not test"
                     " what it says" % text)
    return want


def case_equivalence(terp, case, res, verbose):
    """The control run plays the whole script in one session; the split run
    relaunches after each cut.  Their per-command output must agree."""
    game, script, cuts = case["game"], case["script"], case["cuts"]
    sig = SIG_PREFIX + case["name"]
    sa_delays = case.get("sa_delays", 0)

    with tempfile.TemporaryDirectory() as work:
        clean_autosave(sig)
        want = control_run(terp, case, res, sig, work)

        got = []
        sessions = split_script(script, cuts)
        for index, part in enumerate(sessions):
            s = run_session(terp, game, part, sig, work, sa_delays)
            label = "session %d" % (index + 1)
            check_process(res, s, label)
            if index > 0:
                check_relaunch(res, s, label)
            if index < len(sessions) - 1 and not s.autosaved_since_last_input():
                res.fail("%s: no autosave after the last input it took"
                         % label)
            if s.leftover:
                res.fail("%s: script not consumed, %d entries left (%r...)"
                         % (label, len(s.leftover), s.leftover[:3]))
            got.extend(s.chunks()[1])
        clean_autosave(sig)

    compare_chunks(res, want, got, cuts, verbose)


def case_pending_prompt(terp, case, res, verbose):
    """The `first` script's last line opens a host prompt (a question, a
    menu, `get input`, `wait`) and the game is closed there.  No autosave may
    follow that line: the relaunch resumes at the turn prompt before it, and
    `second` (the line again, then its answer) must produce what the single
    session produced.  With resume=True the prompt IS meant to survive (the
    verb menu of a clicked object, a disambiguation menu, a pause): `second`
    then starts with the answer -- for a pause the keypress, which the
    driver supplies by itself.  An empty `first` closes the game on the
    first prompt of the start script."""
    game, first, second = case["game"], case["first"], case["second"]
    resume = case.get("resume", False)
    sig = SIG_PREFIX + case["name"]
    case = dict(case, script=first + second if resume
                else first[:-1] + second)
    with tempfile.TemporaryDirectory() as work:
        clean_autosave(sig)
        want = control_run(terp, case, res, sig, work)

        s1 = run_session(terp, game, first, sig, work,
                         case.get("sa_delays", 0))
        check_process(res, s1, "session 1")
        if s1.leftover:
            res.fail("session 1: script not consumed (%r left)" % s1.leftover)
        if resume:
            if not s1.autosaved_since_last_input():
                res.fail("session 1: no autosave under the menu it was"
                         " closed on")
        elif s1.autosaved_after_last_line():
            res.fail("session 1: autosaved under the host prompt (a relaunch"
                     " would resume inside it)")
        elif not any(e[0] == "autosave" for e in s1.events):
            res.fail("session 1: never autosaved at all")

        s2 = run_session(terp, game, second, sig, work,
                         case.get("sa_delays", 0))
        check_process(res, s2, "session 2")
        check_relaunch(res, s2, "session 2")
        if s2.leftover:
            res.fail("session 2: script not consumed (%r left)" % s2.leftover)
        clean_autosave(sig)

    _, c1 = s1.chunks()
    _, c2 = s2.chunks()
    if resume and c1 and c2 and s2.continues:
        # Closed under a keypress pause: session 2 opens with the key, and
        # what follows is the rest of the command session 1 was closed in.
        c1, c2 = c1[:-1], [c1[-1] + c2[0]] + c2[1:]
    # Without resume the closing line's chunk (the prompt's own text) is
    # replaced by session 2's retyping of it.
    got = c1 + c2 if resume else c1[:-1] + c2
    compare_chunks(res, want, got, [len(c1) - (0 if resume else 1)], verbose)


def case_damaged_container(terp, case, res, verbose):
    """autosave.glksave damaged between sessions.  A container whose game
    state is unreadable must be discarded (the bad-autosave exit: no input
    taken), after which the game boots fresh; a Quest 4 one that only lost
    its undo tail must still restore the game state, with no undo past the
    restore point."""
    sig = SIG_PREFIX + case["name"]
    game, first = case["game"], case["first"]
    path = autosave_file(sig)
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
                f.write(case["garbage"])
            s2 = run_session(terp, game, ["look"], sig, work)
            if s2.rc != 0:
                res.fail("session 2: exit status %s" % s2.rc,
                         s2.err.strip()[-800:])
            if any(e[0] == "in" for e in s2.events):
                res.fail("session 2: took input from a corrupt autosave")
            for name in ("autosave.glksave", "autosave.plist",
                         "autosave-bak.glksave", "autosave-bak.plist"):
                if os.path.exists(autosave_file(sig, name)):
                    res.fail("session 2: %s not discarded" % name)
            s3 = run_session(terp, game, case["fresh"], sig, work)
            check_process(res, s3, "session 3")
            if s3.delwin >= s3.newwin:
                res.fail("session 3: did not boot fresh (opened %d windows,"
                         " closed %d)" % (s3.newwin, s3.delwin))
            text = "".join(s3.transcript)
            if case["intro"] not in text:
                res.fail("session 3: intro text %r missing" % case["intro"],
                         text[:600] if verbose else None)

        elif case["damage"] == "bad-undo":
            # Keep the magic line and the engine chunk; replace the undo
            # history chunk with garbage of a matching declared length.
            magic = b"GEASAUTO1\n"
            if not data.startswith(magic):
                res.fail("session 1: autosave.glksave is not a GEASAUTO1"
                         " container")
                return
            eol = data.index(b"\n", len(magic))
            length = int(data[len(magic):eol])
            junk = b"not an undo history"
            with open(path, "wb") as f:
                f.write(data[:eol + 1 + length] + b"%d\n" % len(junk) + junk)
            s2 = run_session(terp, game, case["second"], sig, work)
            ignored = "undo history was not usable"
            if ignored not in (s2.err or ""):
                res.fail("session 2: the damaged undo history was not"
                         " reported as ignored", s2.err.strip()[-800:])
            elif s2.rc != 0:
                res.fail("session 2: exit status %s" % s2.rc,
                         s2.err.strip()[-800:])
            check_relaunch(res, s2, "session 2")
            text = "".join(s2.transcript)
            for want in case["expect"]:
                if want not in text:
                    res.fail("session 2: %r missing" % want,
                             text[-600:] if verbose else None)
        clean_autosave(sig)


# ---- scripts --------------------------------------------------------------

def quest4_script(name):
    """A quest4/goldens command script: a prose header up to a "Start:"
    line, then one command per line with optional "  (note)" tails -- the
    reading question_walkthrough_runner.cc does."""
    with open(os.path.join(Q4GOLDENS, name), encoding="utf-8",
              errors="replace") as f:
        raw = [line.rstrip("\r\n") for line in f]
    begin = 0
    for i, line in enumerate(raw):
        if line.strip().lower().startswith("start:"):
            begin = i + 1
            break
    out = []
    for line in raw[begin:]:
        t = line.strip()
        p = t.find("  (")
        if p >= 0:
            t = t[:p].strip()
        if t:
            out.append(t)
    return out


def quest5_script(name):
    """A quest5/goldens .cmd: '#' comments and the replay harness's
    bookkeeping directives (tick:/menu:/save:/assert:) dropped.  Scripts with
    a typing clock or ticks are not offered here: the Glk frontend's timers
    run on wall-clock events, not on typed commands."""
    with open(os.path.join(Q5GOLDENS, name), encoding="utf-8") as f:
        raw = [line.rstrip("\r\n") for line in f]
    out = []
    for line in raw:
        if not line.strip() or line.startswith("#"):
            continue
        if line.split(":", 1)[0] in ("tick", "menu", "save", "assert"):
            continue
        out.append(line)
    return out


def spread(script, parts):
    n = entry_count(script)
    return [n * i // parts for i in range(1, parts)]


def probe5_as(stage, version):
    """probe5.aslx with its <asl version> changed, written into the staging
    directory.  From ASL 600 on `ask` and `show menu` draw their options
    inline in the transcript instead of through the host dialog; below 540
    `request (Wait, "")` is still a keypress pause."""
    with open(os.path.join(HERE, "probe5.aslx"), encoding="utf-8") as f:
        text = f.read()
    old = '<asl version="580">'
    if text.count(old) != 1:
        sys.exit("run_autosave_tests: probe5.aslx no longer declares %s"
                 % old)
    path = os.path.join(stage, "probe5-v%d.aslx" % version)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text.replace(old, '<asl version="%d">' % version))
    return path


def build_cases(stage):
    cases = []
    probe4 = os.path.join(HERE, "probe4.asl")
    probe4intro = os.path.join(HERE, "probe4-intro.asl")
    probe5 = os.path.join(HERE, "probe5.aslx")
    probe5intro = os.path.join(HERE, "probe5-intro.aslx")
    probe6 = probe5_as(stage, 600)
    probe53 = probe5_as(stage, 530)

    def equiv(name, gamefile, script, cuts, **kw):
        cases.append(dict(kind=case_equivalence, name=name, game=gamefile,
                          script=script, cuts=cuts, **kw))

    def pending(name, gamefile, first, second, **kw):
        cases.append(dict(kind=case_pending_prompt, name=name, game=gamefile,
                          first=first, second=second, **kw))

    def walk(name, gamefile, script, parts=4, **kw):
        equiv(name, gamefile, script, spread(script, parts), **kw)

    # ---- Quest 4 --------------------------------------------------------

    # Relaunch after every single command: rooms, inventory, the pane.
    q4tour = ["look", "take lamp", "n", "x desk", "s", "i", "drop lamp",
              "take book", "look", "i"]
    equiv("q4-every-command", probe4, q4tour, list(range(1, len(q4tour))))

    # The RNG position under determinism.
    equiv("q4-rng", probe4, ["roll"] * 6, [1, 2, 4], expect=["You roll"])

    # A numeric variable.
    equiv("q4-variable", probe4, ["count"] * 5, [2, 4],
          expect=["Count is 5."])

    # The undo history, including after undos already taken.
    equiv("q4-undo", probe4,
          ["take lamp", "n", "take book", "s", "undo", "undo", "i", "look",
           "undo", "i", "undo", "i"],
          [3, 4, 6, 8, 10], expect=["You are carrying"])

    # The real-time timer: three ticks per firing.  Every tick re-saves,
    # whether it fires or only counts, so a relaunch between two firings
    # (the midcycle case) resumes with the ticks already counted.
    equiv("q4-timer", probe4,
          ["look", "tick:3", "look", "tick:3", "look", "tick:3", "look"],
          [1, 2, 3], sa_delays=1, expect=["[pulse]"])
    equiv("q4-timer-midcycle", probe4,
          ["look", "tick:2", "look", "tick:1", "look"],
          [1], sa_delays=1, expect=["[pulse]"])

    # Closed under a question, a selection menu, an `enter` question or the
    # parser's "which ball?" menu: each autosaves, and the relaunch replays
    # the command silently back to it.  juggle rolls and asks a question
    # before the menu, so the replay must draw the same number and answer
    # the question the same way.
    pending("q4-question", probe4, ["look", "question"], ["1", "look"],
            resume=True, expect=["[yes]"])
    pending("q4-menu", probe4, ["look", "menu"], ["2", "look"],
            resume=True, expect=["[two]"])
    pending("q4-enter", probe4, ["look", "sign"], ["Ann", "look"],
            resume=True, expect=["Signed Ann."])
    pending("q4-which", probe4, ["look", "take ball"], ["2", "i"],
            resume=True, expect=["blue ball"])
    pending("q4-which-juggle", probe4, ["look", "roll", "juggle", "1"],
            ["1", "i", "roll"], resume=True, expect=["red ball", "You roll"])

    # Closed under a `wait`, the first of the command and then the second:
    # the pause autosaves like the questions above, and the keypress that
    # passed the first is part of the record.  Each roll must come out as in
    # the single session.
    pending("q4-wait", probe4, ["look", "roll", "nap"], ["roll", "look"],
            resume=True, expect=["You stir", "You wake up.", "You roll"])
    pending("q4-wait-second", probe4, ["look", "roll", "nap", "key:32"],
            ["roll", "look"], resume=True,
            expect=["You stir", "You wake up.", "You roll"])

    # Closed under a question the startscript asks, before any turn: the
    # autosave is the answers so far, and the relaunch boots with them.  (The
    # very first question needs none: nothing has been typed yet, and a
    # relaunch is a fresh start.)
    pending("q4-intro-gender", probe4intro, ["Ann"], ["2", "who", "look"],
            resume=True, expect=["You are Ann, female, lucky number"])
    # The pause ahead of them, with nothing typed at all.
    pending("q4-intro-wait", probe4intro, [], ["Ann", "2", "who"],
            resume=True, expect=["You are Ann, female, lucky number"])

    # Pane hyperlinks: live after a relaunch (cut 1), and the unfolded verb
    # menu -- a re-save without a turn -- comes back unfolded (cut 2).
    equiv("q4-pane-links", probe4,
          ["look", "click:Lamp", "click:Take", "i", "click:Lamp",
           "click:Drop", "i"],
          [1, 2, 4, 5], expect=["You are carrying"])

    # Corpus walkthroughs, relaunched at three points.
    for name, gamefile, sol in (
            ("q4-walk-bear", "Bear Campsite.cas",
             "Bear Campsite - command script.txt"),
            ("q4-walk-mansion", "mansion.asl",
             "The Mansion - command script.txt"),
            ("q4-walk-gathered", "Gatheredindarkness.cas",
             "Gathered in Darkness - command script.txt")):
        if os.path.exists(os.path.join(Q4GOLDENS, sol)):
            walk(name, os.path.join(Q4GAMES, gamefile), quest4_script(sol))

    cases.append(dict(kind=case_damaged_container, name="q4-corrupt",
                      game=probe4, damage="garbage",
                      garbage=b"GEASAUTO1\n12\nnot a game!!0\n\n",
                      first=["take lamp", "n"], fresh=["look"],
                      intro="A hall."))
    cases.append(dict(kind=case_damaged_container, name="q4-bad-undo",
                      game=probe4, damage="bad-undo",
                      first=["take lamp", "n"], second=["undo", "look"],
                      expect=["A study."]))

    # ---- Quest 5 --------------------------------------------------------

    q5tour = ["look", "take apple", "open chest", "north", "x rose", "south",
              "i", "drop apple", "look", "i"]
    equiv("q5-every-command", probe5, q5tour, list(range(1, len(q5tour))))
    equiv("q5-rng", probe5, ["roll"] * 6, [1, 2, 4], expect=["You roll"])
    equiv("q5-attribute", probe5, ["count"] * 5, [2, 4],
          expect=["Count is 5."])

    # The undo history, including after undos already taken.  (Quest 5 logs
    # every turn, so a LOOK between undos is itself one more to undo.)
    equiv("q5-undo", probe5,
          ["take apple", "open chest", "north", "south", "undo", "undo",
           "undo", "look", "undo", "undo", "i"],
          [3, 4, 6, 7, 9], expect=["Undo: open chest", "Undo: take apple"])

    # The recurring <timer> (3 s) and a one-shot SetTimeout (5 s); the Glk
    # frontend counts a second per timer event.
    equiv("q5-timer", probe5,
          ["look", "tick:3", "look", "tick:3", "look", "tick:3", "look"],
          [1, 2, 3], sa_delays=1, expect=["[pulse]"])
    equiv("q5-timer-midcycle", probe5,
          ["look", "tick:2", "look", "tick:1", "look"],
          [1], sa_delays=1, expect=["[pulse]"])
    equiv("q5-timeout", probe5,
          ["hold", "tick:2", "look", "tick:3", "look"],
          [1, 2], sa_delays=1, expect=["[timeout]"])

    # Closed under a `wait`, the first of the command and then the second:
    # the pause autosaves like the prompts below, and the keypress that
    # passed the first is part of the record.  Each roll must come out as in
    # the single session.
    pending("q5-wait", probe5, ["look", "roll", "nap"], ["roll", "look"],
            resume=True, expect=["You stir", "You wake up.", "You roll"])
    pending("q5-wait-second", probe5, ["look", "roll", "nap", "key:32"],
            ["roll", "look"], resume=True,
            expect=["You stir", "You wake up.", "You roll"])
    # The same under the request form of the pause.
    pending("q5-request-wait", probe53, ["look", "roll", "doze"],
            ["roll", "look"], resume=True, sa_delays=1,
            expect=["You come round", "You are awake.", "You roll"])
    pending("q5-request-wait-second", probe53,
            ["look", "roll", "doze", "key:32"], ["roll", "look"],
            resume=True, sa_delays=1,
            expect=["You come round", "You are awake.", "You roll"])

    # Closed under `get input`, `show menu`, `ask` or Core's "which ball?"
    # menu: each autosaves (engine state from the start of the turn plus a
    # replay record), and the relaunch replays the command back to it with
    # the output muted.  juggle draws a number and asks a question first.
    pending("q5-get-input", probe5, ["look", "type"], ["hello", "look"],
            resume=True, expect=["You typed hello."])
    pending("q5-show-menu", probe5, ["look", "pick"], ["2", "look"],
            resume=True, expect=["You picked green."])
    pending("q5-ask", probe5, ["look", "query"], ["yes", "look"],
            resume=True, expect=["Confirmed."])
    pending("q5-which", probe5, ["north", "take ball"], ["2", "i"],
            resume=True, expect=["blue ball"])
    pending("q5-which-juggle", probe5, ["north", "roll", "juggle", "yes"],
            ["1", "i", "roll"], resume=True, expect=["red ball", "You roll"])
    # The replayed turn is logged again on top of the history the autosave
    # carried from the start of the turn: it undoes as one turn, and the
    # turns before it are still there to undo.
    pending("q5-which-undo", probe5, ["take apple", "north", "take ball"],
            ["2", "i", "undo", "undo", "i", "undo", "undo", "look"],
            resume=True,
            expect=["blue ball", "Undo: take ball", "Undo: north",
                    "You are in a lounge."])
    # Undoing on past the answered question runs into the reference's own
    # quirk: one turn is rolled back twice, the second time against lists
    # that no longer match, which fails (the message goes to stderr, not the
    # page) and leaves the turns before it still undoable.
    pending("q5-which-undo-error", probe5,
            ["take apple", "north", "take ball"],
            ["2", "undo", "i", "undo", "undo", "undo", "undo", "look", "i"],
            resume=True,
            expect=["Undo: take apple", "You can see a chest and an apple."])

    # An inline object link: live after a relaunch (cut after "look"), and
    # the verb menu it pops reopens after a relaunch (cut after the click).
    # The apple link is the fourth link the room description registers (exits
    # and objects come before it); it is addressed by number because after a
    # relaunch the restored transcript is not re-sent as SETLINK/PRINT pairs,
    # so a `click:apple` lookup by text finds nothing while `link:0:4` still
    # hits the live link.  "1" is "Look at", "2" is "Take".
    equiv("q5-inline-link", probe5,
          ["look", "link:0:4", "1", "i"], [1],
          expect=["Choose", "Look at", "Take"])
    pending("q5-link-menu", probe5, ["look", "link:0:4"],
            ["2", "i"], resume=True, expect=["You are carrying", "apple"])

    # The same prompts in an ASL 600 game, where `ask` and `show menu` draw
    # an inline option list that is hidden again once answered.  A relaunch
    # after an answered prompt must not bring the hidden list back; closed
    # under one the relaunch waits under the list without drawing it again;
    # and the "which ball?" replay has to get through juggle's inline
    # question with the output muted.
    equiv("q5-v600-prompts", probe6,
          ["look", "pick", "2", "look", "query", "yes", "look", "roll"],
          [3, 4, 6, 7],
          expect=["2: green", "You picked green.", "1: Yes", "Confirmed."])
    pending("q5-v600-show-menu", probe6, ["look", "pick"], ["2", "look"],
            resume=True, expect=["You picked green."])
    pending("q5-v600-ask", probe6, ["look", "query"], ["yes", "look"],
            resume=True, expect=["Confirmed."])
    pending("q5-v600-which-juggle", probe6,
            ["north", "roll", "juggle", "yes"],
            ["1", "i", "roll"], resume=True, expect=["red ball", "You roll"])

    # Closed under the prompts the start script puts up, before any turn:
    # the autosave is the answers so far, and the relaunch boots with them.
    # The last is the expression form of `ask`, blocking inside the menu's
    # callback.  (The very first prompt needs no autosave: nothing has been
    # typed yet, and a relaunch is a fresh start.)
    who = "You are Ann, female, sure True, lucky number"
    pending("q5-intro-menu", probe5intro, ["Ann"], ["2", "yes", "who"],
            resume=True, expect=[who])
    pending("q5-intro-ask", probe5intro, ["Ann", "2"], ["yes", "who", "look"],
            resume=True, expect=[who])
    # The pause ahead of them, with nothing typed at all.
    pending("q5-intro-wait", probe5intro, [], ["Ann", "2", "yes", "who"],
            resume=True, expect=[who])

    for name, gamefile, sol in (
            ("q5-walk-exit-the-room", "Exit the Room.quest",
             "Exit the Room.cmd"),
            ("q5-walk-bears-quest", "Bear's Epic Quest.quest",
             "Bear's Epic Quest.cmd"),
            ("q5-walk-arc-ii", "ARC II.quest", "ARC II.cmd")):
        if os.path.exists(os.path.join(Q5GOLDENS, sol)):
            walk(name, os.path.join(Q5GAMES, gamefile), quest5_script(sol))

    cases.append(dict(kind=case_damaged_container, name="q5-corrupt",
                      game=probe5, damage="garbage",
                      garbage=b"not a saved game at all\n",
                      first=["take apple", "north"], fresh=["look"],
                      intro="A lounge."))
    return cases


# ---- driver ---------------------------------------------------------------

def source_mtime():
    newest = 0
    for d in (QUESTION, os.path.join(QUESTION, "quest5")):
        for f in os.listdir(d):
            if f.endswith((".cc", ".c", ".mm", ".inc", ".hh", ".h")):
                newest = max(newest, os.path.getmtime(os.path.join(d, f)))
    return newest


def stage_default_terp(tmp):
    build = os.path.join(REPO, "build", "Debug")
    terp = os.path.join(build, "question")
    lib = os.path.join(build, "libglkimp.dylib")
    for path in (terp, lib):
        if not os.path.exists(path):
            sys.exit("run_autosave_tests: %s missing; pass --build or --terp"
                     % path)
    if os.path.getmtime(terp) < source_mtime():
        sys.exit("run_autosave_tests: %s is older than the engine sources;"
                 " pass --build (or --terp)" % terp)
    os.makedirs(os.path.join(tmp, "MacOS"))
    os.makedirs(os.path.join(tmp, "Frameworks"))
    shutil.copy2(terp, os.path.join(tmp, "MacOS", "question"))
    shutil.copy2(lib, os.path.join(tmp, "Frameworks", "libglkimp.dylib"))
    return os.path.join(tmp, "MacOS", "question")


def xcodebuild():
    cmd = ["xcodebuild", "-project", os.path.join(REPO, "Spatterlight.xcodeproj"),
           "-target", "question", "-configuration", "Debug", "-arch", "arm64",
           "ONLY_ACTIVE_ARCH=YES", "CLANG_ENABLE_EXPLICIT_MODULES=NO",
           "SYMROOT=" + os.path.join(REPO, "build"), "build"]
    print("building Question (Debug)...", file=sys.stderr)
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
        for case in build_cases(stage):
            if pattern not in case["name"]:
                continue
            if not os.path.exists(case["game"]):
                print("%-24s SKIP     (%s)"
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
            print("%-24s %-8s %s" % (case["name"], status, note))
            if bad or verbose:
                for msg in res.problems[1:]:
                    print("%-24s          %s" % ("", msg))
                for d in res.detail:
                    print("\n".join("    " + l for l in d.splitlines()))
    print("%d failed" % failed if failed else "all passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
