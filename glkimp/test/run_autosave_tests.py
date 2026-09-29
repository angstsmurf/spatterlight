#!/usr/bin/env python3
"""Spatterlight autosave/autorestore regression for Bocfel and Glulxe.

Each session runs the Spatterlight build of the terp under Scarier's
test/glkdrive.py fake app, which feeds scripted input and ends the session
with EVTQUIT -- the window-closed exit that keeps the autosave -- so the next
session with the same game autorestores.  (Scarier has its own suite in
terps/scarier/test/adrift4/harness/run_autosave_tests.py; this one follows it.)

The main check is equivalence: a script played in ONE session must print,
command for command, exactly what it prints when the process is killed and
relaunched after chosen commands.  Determinism is on (the app's testing
mode), so no goldens are needed -- the single-session run is the reference.
On top of that every relaunch must

  - emit zero NEWWIN/DELWIN before its first input (the windows come back
    from the archived library, not from the game reopening them), and print
    nothing into a text-buffer window before it -- except that Bocfel opens
    its windows before looking for an autosave, so for Bocfel every window
    opened before the first input must be closed again before it,

and every session must leave a complete pair behind: autosave.glksave and
autosave.plist, with no autosave-tmp.* left over (the
write-both-then-rename-as-a-pair order of spatterlight-autosave.mm and
spatterstart.m).  Each case starts with no autosave directory at all, so the
first session also proves the directory is created on demand.

A relaunched Glulxe restores inside glk_select and does not re-request line
input: in the real app the line request travels in the archived window.  The
fake app has no archive, so Session reads the request back out of
autosave.plist (see archived_input_requests) before the relaunch.

Cases marked xfail pin down state the autosave is known not to carry; an
XPASS fails the run so the list gets updated when one is fixed.

Autosaves are keyed by SPATTERLIGHT_AUTOSAVE_SIGNATURE (glkimp's
getautosavedir), set per case to a name no real game can have, and removed
before and after each case.  They still land in the REAL
~/Library/Application Support (NSApplicationSupportDirectory ignores $HOME).

Games: the tracked babel/test copies of Bronze and Sensory always run; the
local-only UITests/Supporting Files/Games ones are skipped when missing.

Usage:
  python3 run_autosave_tests.py [-v] [--build] [substring]

  --build  xcodebuild the Debug bocfel and glulxe targets first.
  -v       print the first differing command's output on a mismatch.
"""

import os
import plistlib
import pwd
import re
import shutil
import subprocess
import sys
import tempfile
import threading

HERE = os.path.dirname(os.path.abspath(__file__))
GLKIMP = os.path.dirname(HERE)
REPO = os.path.dirname(GLKIMP)
BUILD = os.path.join(REPO, "build", "Debug")
UIGAMES = os.path.join(REPO, "UITests", "Supporting Files", "Games")

sys.path.insert(0, os.path.join(REPO, "terps", "scarier", "test"))
import glkdrive  # noqa: E402

APP_SUPPORT = os.path.join(
    pwd.getpwuid(os.getuid()).pw_dir,
    "Library", "Application Support", "Spatterlight")
SESSION_TIMEOUT = 120.0
TEXTBUFFER = 3                      # glk wintype_TextBuffer

# terp name -> (autosave folder, source dirs whose files must not be newer
# than the binary, extra dylibs it links)
TERPS = {
    "bocfel": ("Bocfel Files",
               ["terps/bocfel", "terps/bocfel/bocfel-spatterlight", "glkimp"],
               ["libreadwoz.dylib"]),
    "glulxe": ("Glulxe Files", ["terps/glulxe", "glkimp"], []),
}


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
        """(pre, [chunk per input]): pre is [(window, text)] printed before
        the first input; each chunk is the text printed from one input up
        to the next, window switches marked.  Nothing after EVTQUIT is
        kept."""
        pre, chunks, cur, win = [], [], None, None
        for ev in self.events:
            if ev[0] == "quit":
                break
            if ev[0] in ("in", "key"):
                cur = ["> %s\n" % ev[1] if ev[0] == "in"
                       else "[key %d]\n" % ev[1]]
                chunks.append(cur)
                win = None
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
        opened = closed = 0
        for ev in self.events:
            if ev[0] in ("in", "key"):
                break
            opened += ev[0] == "newwin"
            closed += ev[0] == "delwin"
        return opened, closed


def archived_input_requests(plist_path):
    """(line_peer, char_peer) of the windows autosave.plist archived with a
    pending line or char request, or (None, None).  The TempLibrary archive
    is an NSKeyedArchiver plist; each window is a dict in $objects carrying
    its own peer and request flags."""
    try:
        with open(plist_path, "rb") as f:
            archive = plistlib.load(f)
    except (OSError, plistlib.InvalidFileException):
        return None, None
    line = char = None
    for obj in archive.get("$objects", []):
        if not isinstance(obj, dict) or "peer" not in obj:
            continue
        if obj.get("line_request") and line is None:
            line = obj["peer"]
        if obj.get("char_request") and char is None:
            char = obj["peer"]
    return line, char


def run_session(terp, game, script, signature, workdir, seed_input=False):
    os.environ["SPATTERLIGHT_AUTOSAVE_SIGNATURE"] = signature
    s = Session(terp, game, workdir, script)
    if seed_input:
        s.line_peer, s.char_peer = archived_input_requests(
            os.path.join(autosave_dir(terp, signature), "autosave.plist"))
    killed = []
    timer = threading.Timer(SESSION_TIMEOUT,
                            lambda: (killed.append(True), s.p.kill()))
    timer.start()
    try:
        s.rc, s.err = s.run()
    except (BrokenPipeError, ConnectionResetError, EOFError) as e:
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


def terp_name(terp):
    return os.path.basename(terp)


def autosave_dir(terp, signature):
    return os.path.join(APP_SUPPORT, TERPS[terp_name(terp)][0], "Autosaves",
                        signature)


def clean_autosave(terp, signature):
    shutil.rmtree(autosave_dir(terp, signature), ignore_errors=True)


def split_script(script, cuts):
    """Cut a script into sessions after the given counts of entries."""
    sessions, cur = [], []
    cuts = sorted(set(c for c in cuts if 0 < c < len(script)))
    for n, item in enumerate(script):
        if cuts and n == cuts[0]:
            sessions.append(cur)
            cur = []
            cuts.pop(0)
        cur.append(item)
    sessions.append(cur)
    return sessions


# ---- checks ---------------------------------------------------------------

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


def check_process(res, s, label):
    if s.rc != 0:
        res.fail("%s: exit status %s" % (label, s.rc), s.err.strip()[-800:])
    if s.leftover:
        res.fail("%s: script not consumed, %d entries left (%r...)"
                 % (label, len(s.leftover), s.leftover[:3]))


def check_pair(res, terp, sig, label):
    """A complete autosave pair and no half-written -tmp files.  (The -bak
    pair, the previous autosave, is kept on purpose.)"""
    d = autosave_dir(terp, sig)
    names = set(os.listdir(d)) if os.path.isdir(d) else set()
    for want in ("autosave.glksave", "autosave.plist"):
        if want not in names:
            res.fail("%s: %s missing (have %s)"
                     % (label, want, sorted(names) or "no directory"))
    stray = sorted(n for n in names if "-tmp." in n)
    if stray:
        res.fail("%s: temporaries left behind: %s" % (label, stray))


def case_equivalence(terp, case, res, verbose):
    """The control run plays the whole script in one session; the split run
    relaunches after each cut.  Their per-command output must agree."""
    game, script, cuts = case["game"], case["script"], case["cuts"]
    sig = "autosave-test-%s-%s" % (terp_name(terp), case["name"])
    seed = case.get("seed_input", False)

    with tempfile.TemporaryDirectory() as work:
        clean_autosave(terp, sig)
        control = run_session(terp, game, script, sig, work)
        check_process(res, control, "control")
        check_pair(res, terp, sig, "control")
        clean_autosave(terp, sig)
        _, want = control.chunks()
        for text in case.get("expect", []):
            if text not in "".join(want):
                res.fail("control: never printed %r, so the case does not"
                         " test what it says" % text)

        bufwins = {w for w, t in control.wintypes.items() if t == TEXTBUFFER}
        got = []
        for index, part in enumerate(split_script(script, cuts)):
            s = run_session(terp, game, part, sig, work, seed and index > 0)
            label = "session %d" % (index + 1)
            check_process(res, s, label)
            check_pair(res, terp, sig, label)
            pre, chunks = s.chunks()
            if index > 0:
                opened, closed = s.windows_before_input()
                if case.get("startup_windows"):
                    # Bocfel opens its windows before it looks for an
                    # autosave, restores into them (replaying its own
                    # history), then swaps in the archived ones; the app
                    # adopts those at the first input request.  All that is
                    # checked is that none of the startup ones survive.
                    if opened != closed:
                        res.fail("%s: autorestore opened %d windows but"
                                 " closed %d" % (label, opened, closed))
                else:
                    if opened or closed:
                        res.fail("%s: autorestore opened %d and closed %d"
                                 " windows" % (label, opened, closed))
                    printed = "".join(t for w, t in pre
                                      if w in bufwins).strip()
                    if printed:
                        res.fail("%s: printed into a buffer window before"
                                 " input" % label, printed[:400])
            got.extend(chunks)
        clean_autosave(terp, sig)

    for pattern in case.get("mask", []):
        want = [re.sub(pattern, "X", c) for c in want]
        got = [re.sub(pattern, "X", c) for c in got]
    for n, (w, g) in enumerate(zip(want, got)):
        if w != g:
            cmd = w.split("\n", 1)[0]
            res.fail("output differs at command %d (%s)" % (n + 1, cmd),
                     "--- one session ---\n%s\n--- relaunched ---\n%s"
                     % (w.rstrip(), g.rstrip()) if verbose else None)
            break
    else:
        if len(want) != len(got):
            res.fail("command count differs: %d in one session, %d relaunched"
                     % (len(want), len(got)))


def case_half_pair(terp, case, res, verbose):
    """One half of the pair missing at launch -- what a crash between the
    two renames used to leave.  The terp must not restore from the other
    half: it must boot fresh (opening its windows and printing its intro)
    and write a complete pair of its own."""
    sig = "autosave-test-%s-%s" % (terp_name(terp), case["name"])
    game = case["game"]
    with tempfile.TemporaryDirectory() as work:
        clean_autosave(terp, sig)
        s1 = run_session(terp, game, case["first"], sig, work)
        check_process(res, s1, "session 1")
        check_pair(res, terp, sig, "session 1")
        os.remove(os.path.join(autosave_dir(terp, sig), case["remove"]))
        s2 = run_session(terp, game, case["second"], sig, work,
                         case.get("seed_input", False))
        check_process(res, s2, "session 2")
        if not s2.newwin:
            res.fail("session 2: opened no windows, so it did not boot fresh")
        text = "".join(s2.transcript)
        if case["intro"] not in text:
            res.fail("session 2: intro text %r missing" % case["intro"],
                     text[-600:] if verbose else None)
        check_pair(res, terp, sig, "session 2")
        clean_autosave(terp, sig)


# ---- cases ----------------------------------------------------------------

def spread(script, parts):
    n = len(script)
    return [n * i // parts for i in range(1, parts)]


BRONZE = os.path.join(REPO, "babel", "test", "bronze", "Bronze.zblorb")
SENSORY = os.path.join(REPO, "babel", "test", "sensory", "sensory.ulx")

BRONZE_SCRIPT = [
    "yes", "look", "inventory", "x me", "n", "look", "x fountain", "w", "e", "s",
    "undo", "score", "n", "take all", "i", "wait",
]
# Not "s": leaving the museum ends Sensory.
SENSORY_SCRIPT = [
    "look", "inventory", "listen", "smell", "x me", "e", "look",
    "x photograph", "x painting", "w", "undo", "x button", "push button", "i",
]
# autosavetest.gblorb's "run all" stops at every "Kill and hit a key" and
# "Kill and enter a line" prompt: 19 keys, 7 lines, 22 keys, then its room.
AUTOSAVETEST_SCRIPT = (["run all"] + ["key:32"] * 19 + [""] * 7
                       + ["key:32"] * 22 + ["look", "i"])
# What varies between runs by design (the same masks as UITests'
# testAutosave): the random test's nondeterministic array, and an object
# address.
AUTOSAVETEST_MASK = [r"(?<=Array: 0!=).*", r"(?<=Mainwin parent: )\d+"]

CURSES_SCRIPT = [
    "look", "inventory", "x me", "e", "look", "w", "s", "n", "take all",
    "i", "undo", "score", "wait",
]


def build_cases():
    cases = []

    def equiv(terp, name, gamefile, script, cuts, **kw):
        cases.append(dict(kind=case_equivalence, terp=terp, name=name,
                          game=gamefile, script=script, cuts=cuts, **kw))

    def every(script):
        return list(range(1, len(script)))

    # Bocfel re-requests its line input after restoring; seeding it from
    # the plist would answer a request the terp has not made yet.
    zkw = dict(startup_windows=True)
    equiv("bocfel", "bronze", BRONZE, BRONZE_SCRIPT, spread(BRONZE_SCRIPT, 4),
          **zkw)
    equiv("bocfel", "bronze-every-command", BRONZE, BRONZE_SCRIPT[:8],
          every(BRONZE_SCRIPT[:8]), **zkw)
    equiv("bocfel", "curses", os.path.join(UIGAMES, "curses.z5"),
          CURSES_SCRIPT, spread(CURSES_SCRIPT, 3), **zkw)

    # The cuts keep "w" and the "undo" after it in one session; undo across
    # a relaunch is its own case below.
    equiv("glulxe", "sensory", SENSORY, SENSORY_SCRIPT, [3, 7, 11],
          seed_input=True)
    # Cut at key prompts and line prompts alike; the archived char request
    # is what the relaunch answers.
    equiv("glulxe", "autosavetest", os.path.join(UIGAMES,
                                                 "autosavetest.gblorb"),
          AUTOSAVETEST_SCRIPT, [1, 2, 7, 20, 23, 27, 30, 45, 49],
          seed_input=True, mask=AUTOSAVETEST_MASK)
    equiv("glulxe", "sensory-every-command", SENSORY, SENSORY_SCRIPT[:8],
          every(SENSORY_SCRIPT[:8]), seed_input=True)

    # Undo into turns played before the relaunch.  Bocfel's autosave
    # carries its undo chain (the Undo chunk of a SaveType::Autosave);
    # Glulxe's undo chain lives only in memory.
    equiv("bocfel", "undo-across-relaunch", BRONZE,
          ["yes", "n", "s", "undo", "undo", "look"], [3, 4, 5],
          expect=["Previous turn undone"], **zkw)
    equiv("glulxe", "undo-across-relaunch", SENSORY,
          ["look", "e", "w", "undo", "undo", "look"], [3, 4, 5],
          seed_input=True,
          xfail="Glulxe's undo chain is not autosaved")

    for terp, game, intro, seed in (
            ("bocfel", BRONZE, "Bronze", False),
            ("glulxe", SENSORY, "Sensory", True)):
        for half in ("autosave.plist", "autosave.glksave"):
            cases.append(dict(kind=case_half_pair, terp=terp,
                              name="no-" + half.split(".")[1], game=game,
                              first=["look", "inventory"], second=["look"],
                              remove=half, intro=intro, seed_input=seed))
    return cases


# ---- terps ----------------------------------------------------------------

def stage_terps(tmp):
    """Copy build/Debug's terps and dylibs into a MacOS/ + Frameworks/
    layout, which their @executable_path/../Frameworks load paths want."""
    os.makedirs(os.path.join(tmp, "MacOS"))
    os.makedirs(os.path.join(tmp, "Frameworks"))
    staged = {}
    for name, (_, srcdirs, libs) in TERPS.items():
        terp = os.path.join(BUILD, name)
        for path in [terp] + [os.path.join(BUILD, l)
                              for l in ["libglkimp.dylib"] + libs]:
            if not os.path.exists(path):
                sys.exit("run_autosave_tests: %s missing; pass --build" % path)
        newest = max(os.path.getmtime(os.path.join(REPO, d, f))
                     for d in srcdirs
                     for f in os.listdir(os.path.join(REPO, d))
                     if f.endswith((".c", ".cpp", ".h", ".m", ".mm")))
        if os.path.getmtime(terp) < newest:
            sys.exit("run_autosave_tests: %s is older than its sources;"
                     " pass --build" % terp)
        shutil.copy2(terp, os.path.join(tmp, "MacOS", name))
        for lib in ["libglkimp.dylib"] + libs:
            dest = os.path.join(tmp, "Frameworks", lib)
            if not os.path.exists(dest):
                shutil.copy2(os.path.join(BUILD, lib), dest)
        staged[name] = os.path.join(tmp, "MacOS", name)
    return staged


def xcodebuild():
    for target in TERPS:
        cmd = ["xcodebuild", "-project",
               os.path.join(REPO, "Spatterlight.xcodeproj"),
               "-target", target, "-configuration", "Debug",
               "CLANG_ENABLE_EXPLICIT_MODULES=NO", "build"]
        print("building %s (Debug)..." % target, file=sys.stderr)
        r = subprocess.run(cmd, cwd=REPO, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT)
        if r.returncode != 0:
            sys.stderr.write(r.stdout.decode("utf-8", "replace")[-3000:])
            sys.exit("run_autosave_tests: %s build failed" % target)


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

    if build:
        xcodebuild()

    failed = 0
    with tempfile.TemporaryDirectory() as stage:
        terps = stage_terps(stage)
        for case in build_cases():
            label = "%s/%s" % (case["terp"], case["name"])
            if pattern not in label:
                continue
            if not os.path.exists(case["game"]):
                print("%-32s SKIP     (%s)"
                      % (label, os.path.basename(case["game"])))
                continue
            res = Result()
            case["kind"](terps[case["terp"]], case, res, verbose)
            if case.get("xfail"):
                status = "XFAIL" if res.problems else "XPASS"
                bad = not res.problems
            else:
                status = "FAIL" if res.problems else "ok"
                bad = bool(res.problems)
            failed += bad
            note = res.problems[0] if res.problems else case.get("xfail", "")
            print("%-32s %-8s %s" % (label, status, note), flush=True)
            if bad or verbose:
                for msg in res.problems[1:]:
                    print("%-32s          %s" % ("", msg))
                for d in res.detail:
                    print("\n".join("    " + l for l in d.splitlines()))
    print("%d failed" % failed if failed else "all passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
