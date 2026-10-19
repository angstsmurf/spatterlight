#!/usr/bin/env python3
"""Protocol trace of a Spatterlight Bocfel session, for the V6 games.

Runs the real Spatterlight build of bocfel under the glkdrive.py fake app
(terps/scarier/test) and writes every message the interpreter sends -- window
creation and sizing, style hints, colours, prints, rectangle fills, image
draws -- one per line.  The z6/ code is almost all window choreography and
image compositing, so the trace is what a refactor of it must leave alone.

Pictures reach the app as TIFF files named in a PURGEIMG message; the trace
records the SHA-1 of the file instead of its (temporary) name, so a changed
pixel anywhere in a composited border shows up as a changed line.

    v6trace.py [options] TERP STORY [ENTRY ...]

      --graphics N    z6_graphics setting: 0 Amiga, 1 Mac B/W, 2 Apple II,
                      3 VGA, 4 EGA, 5 CGA, 6 Blorb, 7 none
      --interp N      zmachine_terp setting (interpreter number)
      --voiceover     set voiceover_on
      --size WxH      window size in pixels (default 800x600)
      --nographics    start with graphics switched off
      --script FILE   a command script in the app's format, run before the
                      ENTRY arguments
      --signature S   autosave signature; the autosave directory is NOT
                      cleaned here (run_v6_traces.py does that)
      --pictures DIR  keep every picture the interpreter sends in DIR, under
                      the name the trace has for it (see picture_name), for
                      v6screens.py to draw
      --save FILE     the file to name when the game asks for one, so that
                      "restore" in the script loads FILE and "save" writes it

Script entries, besides the ones glkdrive.py knows (a typed line, key:X,
tick:N):

  arrange:WxH     resize the window
  mouse:X,Y[,P]   click at X,Y in window P (default: the window that last
                  asked for mouse input)
  prefs:G[:I[:V]] change the z6_graphics (and interpreter, voiceover) setting
                  and send the arrange event that carries it
  gfx:0 / gfx:1   switch graphics off / on (the EVTPREFS message), followed by
                  the forced arrange event the app sends with it
  fkey:N          end the pending line input with function key N

An entry with a prefix is delivered at the next request for an event.  A plain
line is typed the way the app types a command script: into a line request as
a line; into a character request as the single key if the line is one
character long, else key by key with Return at the end (so an empty line is
Return).  Should a line request turn up while a line is half typed, the whole
line is sent again as a line.  The arrow keys are the NSEvent function-key
characters U+F700..U+F703.  Unlike the app, a "press a key" pause does not
swallow the next command (see Tracer.feed_key); --impatient switches that
off, for a game played with keys alone (Journey).  --affirm y (or n) is the
answer when a game offers to solve or simplify a visual puzzle.
"""

import hashlib
import os
import struct
import sys
import threading
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, os.path.join(REPO, "terps", "scarier", "test"))
import glkdrive  # noqa: E402

NAMES = ("NOREPLY OKAY ERROR HELLO PROMPTOPEN PROMPTSAVE NEWWIN DELWIN SIZWIN"
         " CLRWIN MOVETO PRINT UNPRINT MAKETRANSPARENT STYLEHINT CLEARHINT"
         " STYLEMEASURE SETBGND REFRESH SETTITLE AUTOSAVE RESET BANNERCOLS"
         " BANNERLINES TIMER INITCHAR CANCELCHAR INITLINE CANCELLINE SETECHO"
         " TERMINATORS INITMOUSE CANCELMOUSE FILLRECT FINDIMAGE LOADIMAGE"
         " SIZEIMAGE DRAWIMAGE FLOWBREAK NEWCHAN DELCHAN FINDSOUND LOADSOUND"
         " SETVOLUME PLAYSOUND STOPSOUND PAUSE UNPAUSE BEEP SETLINK INITLINK"
         " CANCELLINK SETZCOLOR SETREVERSE QUOTEBOX SHOWERROR CANPRINT"
         " PURGEIMG MENUITEM NEXTEVENT EVTARRANGE EVTREDRAW EVTLINE EVTKEY"
         " EVTMOUSE EVTTIMER EVTHYPER EVTSOUND EVTVOLUME EVTPREFS EVTQUIT"
         " EVTTEST").split()

# Messages that say nothing about what the game drew.
SILENT = {glkdrive.NEXTEVENT, glkdrive.HELLO, glkdrive.AUTOSAVE}


def make_settings(width, height, graphics, interp, voiceover, force=0):
    return struct.pack(
        "<6i4f21i",
        width, height, 0, 0, 0, 0,      # screen w/h, buffer/grid margins
        8.0, 16.0, 8.0, 16.0,           # grid cell w/h, buffer cell w/h
        0x000000, 0xffffff,             # buffer fg/bg
        0x000000, 0xffffff,             # grid fg/bg
        1, 0, 0,                        # do_styles, quote_boxes, sa_delays
        0, 0, 0, 0, 0,                  # sa_*, slowdraw, flicker
        interp, 0, voiceover,           # zmachine_terp, no_err_win, voiceover
        graphics, 0,                    # z6_graphics, z6_colorize
        1,                              # determinism
        0, 0, force)                    # error_handling, comprehend, force


SPAWN = threading.Lock()
PATIENCE = 12
AFFIRM_PROMPT = "(Y is affirmative): >"

KEYCODES = {"\n": -6, "\r": -6, "\t": -9, "\x08": -7, "\x7f": -7,
            "\uf728": -7, "\uf700": -4, "\uf701": -5, "\uf702": -2,
            "\uf703": -3, "\uf72d": -11, "\uf72c": -10, "\uf729": -12,
            "\uf72b": -13}
PREFIXES = ("key", "tick", "link", "click", "arrange", "prefs", "gfx", "mouse",
            "fkey")


def is_plain(entry):
    head = entry.split(":", 1)
    return len(head) == 1 or head[0] not in PREFIXES


def file_digest(path):
    """(what the trace says of the file, its contents or None)."""
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError as e:
        return "unreadable(%s)" % e.strerror, None
    return "%s/%d" % (hashlib.sha1(data).hexdigest()[:16], len(data)), data


def picture_name(digest, offset=0, length=0):
    """File name, in a --pictures directory, of the picture a trace line
    calls "file DIGEST"; OFFSET and LENGTH for a LOADIMAGE, which names a
    piece of a file."""
    name = digest.replace("/", "-")
    if length:
        name += "@%d+%d" % (offset, length)
    return name + ".z"


def keep_picture(directory, name, data):
    """Store DATA deflated as NAME, unless a run got there first."""
    path = os.path.join(directory, name)
    if os.path.exists(path):
        return
    scratch = "%s.%d.%d" % (path, os.getpid(), threading.get_ident())
    with open(scratch, "wb") as f:
        f.write(zlib.compress(data, 1))
    os.replace(scratch, path)


class Tracer(glkdrive.Driver):
    def __init__(self, terp, story, home, script, width=800, height=600,
                 graphics=6, interp=0, voiceover=0, graphics_on=1, env=None,
                 patient=True, affirm=None, pictures=None, savepath=None):
        self.trace = []
        self.pictures = pictures
        self.affirm = affirm
        self.last_print = ""
        self.mouse_peer = None
        self.mouse_peers = set()
        # Resource numbers a picture has been sent under and not purged
        # since. The app answers FINDIMAGE from the same record
        # (ImageHandler imageIsLoaded:), and Bocfel draws a picture it has
        # already sent without sending it again when the answer is yes.
        self.held_images = set()
        self.untyped = None
        self.queue = []
        self.returns = 0
        self.patient = patient
        self.dims = (width, height)
        self.opts = [graphics, interp, voiceover]
        with SPAWN:                     # glkdrive copies os.environ
            saved = dict(os.environ)
            os.environ.update(env or {})
            # The app starts Bocfel with -n <interpreter number>; this is
            # the same option by another road.
            os.environ["BOCFEL_INT_NUMBER"] = str(interp or 6)
            try:
                super().__init__(terp, story, home, script,
                                 savepath=savepath)
            finally:
                os.environ.clear()
                os.environ.update(saved)
        self.graphics_on = graphics_on
        self.settings = make_settings(width, height, *self.opts)

    # ---- what the interpreter sends -------------------------------------

    def dispatch(self, cmd, a1, a2, a3, a4, a5, payload):
        if cmd not in SILENT:
            self.trace.append(self.describe(cmd, a1, a2, a3, a4, a5, payload))
        if cmd == glkdrive.PRINT:
            self.last_print = payload.decode("utf-16-le", "replace")
        if cmd == glkdrive.INITMOUSE:
            self.mouse_peer = a1
            self.mouse_peers.add(a1)
        elif cmd in (glkdrive.CANCELMOUSE, glkdrive.DELWIN):
            self.mouse_peers.discard(a1)
            if self.mouse_peer == a1:
                self.mouse_peer = None
        if cmd == glkdrive.HELLO:
            self.reply(glkdrive.OKAY, self.graphics_on, 1)
            return
        if cmd == glkdrive.PURGEIMG:
            if payload.rstrip(b"\0"):
                self.held_images.add(a1)
            else:
                self.held_images.discard(a1)
        elif cmd == glkdrive.LOADIMAGE:
            self.held_images.add(a1)
        elif cmd == glkdrive.FINDIMAGE:
            self.reply(glkdrive.OKAY, int(a1 in self.held_images))
            return
        super().dispatch(cmd, a1, a2, a3, a4, a5, payload)

    def describe(self, cmd, a1, a2, a3, a4, a5, payload):
        args = "%d %d %d %d %d" % (a1, a2, a3, a4, a5)
        if cmd == glkdrive.PRINT:
            rest = repr(payload.decode("utf-16-le", "replace"))
        elif cmd in (glkdrive.PURGEIMG, glkdrive.LOADIMAGE):
            path = payload.decode("utf-8", "replace").rstrip("\0")
            # The name of a PURGEIMG file is temporary.
            rest = ""
            if path:
                digest, data = file_digest(path)
                rest = "file " + digest
                if self.pictures and data is not None:
                    if cmd == glkdrive.LOADIMAGE and a3:
                        keep_picture(self.pictures,
                                     picture_name(digest, a2, a3),
                                     data[a2:a2 + a3])
                    else:
                        keep_picture(self.pictures, picture_name(digest),
                                     data)
            if cmd == glkdrive.PURGEIMG and path:
                try:
                    os.unlink(path)
                except OSError:
                    pass
            if cmd == glkdrive.PURGEIMG:
                # Only a1 carries anything.
                args = "%d" % a1
            if cmd == glkdrive.LOADIMAGE:
                # a3 is a resource length; the payload length rides in a
                # field the path length would otherwise leak into.
                args = "%d %d %d" % (a1, a2, a3)
        elif cmd == glkdrive.SIZWIN:
            rest = "rect %d,%d-%d,%d of %dx%d" % struct.unpack("<6i", payload)
        elif cmd == glkdrive.DRAWIMAGE:
            rest = "at %d,%d size %ux%u style %u rule %u max %u" \
                % struct.unpack("<2i5I", payload)
        elif cmd == glkdrive.FILLRECT:
            rects = []
            for off in range(0, len(payload) - 11, 12):
                color, x, y, w, h = struct.unpack_from("<I4h", payload, off)
                rects.append("%06x@%d,%d+%dx%d" % (color, x, y, w, h))
            rest = " ".join(rects)
            args = "%d" % a1
        elif cmd in (glkdrive.MENUITEM, glkdrive.SETTITLE,
                     glkdrive.SHOWERROR, glkdrive.QUOTEBOX):
            rest = repr(payload.decode("utf-8", "replace"))
        elif payload:
            rest = "payload %s/%d" % (hashlib.sha1(payload).hexdigest()[:12],
                                      len(payload))
        else:
            rest = ""
        return ("%s %s %s" % (NAMES[cmd], args, rest)).rstrip()

    # ---- what the app sends back ----------------------------------------

    def note(self, text):
        self.trace.append("--- " + text)

    def reply(self, cmd, a1=0, a2=0, a3=0, a4=0, a5=0, payload=b""):
        if cmd == glkdrive.EVTLINE:
            self.note("line %r" % payload.decode("utf-16-le"))
        elif cmd == glkdrive.EVTKEY:
            self.note("key %d" % a2)
        elif cmd == glkdrive.EVTQUIT:
            self.note("quit")
        super().reply(cmd, a1, a2, a3, a4, a5, payload)

    # Scripted input follows the app's CommandScriptHandler, with one
    # difference.  The V6 games pause for a key in many places (title
    # screens, Arthur's and Shogun's interludes, most of them on a timer), and
    # the command scripts of SpatterlightTests do not have a blank line for
    # every one of them.  The app would type the next command into such a
    # pause, losing it if it is a single letter.  Here a pause is answered
    # with Return and the command kept, when the pause is timed or the
    # command is longer than one letter.  (PATIENCE such answers in a row
    # mean it was no pause but a menu, and the line is typed after all.)

    def feed_line(self, peer):
        if self.untyped is not None and len(self.untyped[1]) > 1:
            text = self.untyped[0]      # half typed as keys: send it whole
        elif self.script and is_plain(self.script[0]):
            text = self.script.pop(0)
        else:
            return False
        self.untyped = None
        self.line_peer = None
        self.queue.append((glkdrive.EVTLINE, peer, text))
        return True

    def feed_key(self, peer):
        # With graphics off or VoiceOver on, the games offer to solve or
        # simplify their visual puzzles.  No command script answers that.
        if self.affirm and self.last_print.endswith(AFFIRM_PROMPT):
            self.last_print = ""
            self.char_peer = None
            self.queue.append((glkdrive.EVTKEY, peer, ord(self.affirm)))
            return True
        if self.untyped is None:
            if not self.script or not is_plain(self.script[0]):
                return False
            line = self.script[0]
            if self.patient and line and (len(line) > 1 or self.timer) \
                    and self.returns < PATIENCE:
                self.returns += 1
                self.char_peer = None
                self.queue.append((glkdrive.EVTKEY, peer, -6))
                return True
            self.returns = 0
            self.script.pop(0)
            self.untyped = [line, line if len(line) == 1 else line + "\n"]
        ch, rest = self.untyped[1][0], self.untyped[1][1:]
        self.untyped = [self.untyped[0], rest] if rest else None
        self.char_peer = None
        self.queue.append((glkdrive.EVTKEY, peer, KEYCODES.get(ch, ord(ch))))
        return True

    def next_event(self, block):
        if self.arrange_pending:
            self.arrange_pending = False
            self.reply(glkdrive.EVTARRANGE, payload=self.settings)
            return
        if not self.queue:
            if self.special():
                return
            if self.line_peer is not None:
                self.feed_line(self.line_peer)
            elif self.char_peer is not None:
                self.feed_key(self.char_peer)
        if self.queue:
            cmd, peer, what = self.queue.pop(0)
            if cmd == glkdrive.EVTLINE:
                self.reply(cmd, peer, len(what), 0,
                           payload=what.encode("utf-16-le"))
            else:
                self.reply(cmd, peer, what)
        elif not self.script and (self.line_peer is not None
                                  or self.char_peer is not None):
            self.reply(glkdrive.EVTQUIT)
        elif self.timer or block:
            self.reply(glkdrive.EVTTIMER)
        else:
            self.reply(glkdrive.OKAY)

    def special(self):
        """Deliver the script entry at the head if it is not a plain line;
        true if that answered the request for an event."""
        while self.script and not is_plain(self.script[0]):
            head = self.script[0]
            kind, spec = head.split(":", 1)
            if kind == "tick":
                if not self.timer:
                    self.script.pop(0)
                    self.note("%s dropped: no timer" % head)
                    continue
                if int(spec) > 1:
                    self.script[0] = "tick:%d" % (int(spec) - 1)
                else:
                    self.script.pop(0)
                self.note("tick")
                self.reply(glkdrive.EVTTIMER)
                return True
            self.script.pop(0)
            if kind == "arrange":
                self.dims = tuple(int(n) for n in spec.split("x"))
                self.send_arrange("arrange %dx%d" % self.dims)
                return True
            if kind == "prefs":
                for index, value in enumerate(spec.split(":")):
                    self.opts[index] = int(value)
                self.send_arrange("prefs graphics=%d interp=%d voiceover=%d"
                                  % tuple(self.opts))
                return True
            if kind == "gfx":
                self.graphics_on = int(spec)
                self.note("graphics %s" % ("on" if self.graphics_on
                                           else "off"))
                # The library asks for the next event again by itself after
                # this one; that request gets the forced arrange.
                self.reply(glkdrive.EVTPREFS, 0, self.graphics_on, 1, 1, 0)
                self.note("forced arrange")
                self.settings = make_settings(self.dims[0], self.dims[1],
                                              *self.opts, force=1)
                self.arrange_pending = True
                return True
            if kind == "mouse":
                spec = [int(n) for n in spec.split(",")]
                peer = spec[2] if len(spec) > 2 else self.mouse_peer
                if peer is None or peer not in self.mouse_peers:
                    self.note("%s dropped: no mouse request" % head)
                    continue
                self.mouse_peers.discard(peer)
                if self.mouse_peer == peer:
                    self.mouse_peer = None
                self.note("mouse %d,%d in %d" % (spec[0], spec[1], peer))
                self.reply(glkdrive.EVTMOUSE, peer, spec[0], spec[1])
                return True
            if kind == "key" and self.char_peer is not None:
                code = ord(spec) if len(spec) == 1 else int(spec)
                peer, self.char_peer = self.char_peer, None
                self.reply(glkdrive.EVTKEY, peer, code)
                return True
            if kind == "fkey" and self.line_peer is not None:
                peer, self.line_peer = self.line_peer, None
                self.note("function key %s ends the line" % spec)
                self.reply(glkdrive.EVTLINE, peer, 0, -16 - int(spec))
                return True
            self.note("%s dropped: nothing asked for it" % head)
        return False

    def send_arrange(self, why, force=0):
        self.note(why)
        self.settings = make_settings(self.dims[0], self.dims[1], *self.opts,
                                      force=force)
        self.reply(glkdrive.EVTARRANGE, payload=self.settings)


def load_script(text):
    """Lines of a command script file; '# ' lines are comments, as in the
    app."""
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    return [l.rstrip("\r") for l in lines if not l.startswith("# ")]


def run(terp, story, home, script, timeout=120.0, **kw):
    """(trace lines, exit status, stderr, script entries left)."""
    t = Tracer(terp, story, home, script, **kw)
    killed = []
    timer = threading.Timer(timeout, lambda: (killed.append(1), t.p.kill()))
    timer.start()
    try:
        rc, err = t.run()
    except (BrokenPipeError, ConnectionResetError, EOFError) as e:
        t.p.kill()
        t.p.wait()
        rc = "%s (exit %s)" % (type(e).__name__, t.p.returncode)
        err = t.p.stderr.read().decode("utf-8", "replace")
    finally:
        timer.cancel()
    if killed:
        rc = "killed after %ds" % timeout
    return t.trace, rc, err, list(t.script)


def main():
    argv = sys.argv[1:]
    kw = {}
    while argv and argv[0].startswith("--"):
        opt = argv.pop(0)
        if opt == "--graphics":
            kw["graphics"] = int(argv.pop(0))
        elif opt == "--interp":
            kw["interp"] = int(argv.pop(0))
        elif opt == "--voiceover":
            kw["voiceover"] = 1
        elif opt == "--size":
            kw["width"], kw["height"] = (int(n) for n in
                                         argv.pop(0).split("x"))
        elif opt == "--signature":
            os.environ["SPATTERLIGHT_AUTOSAVE_SIGNATURE"] = argv.pop(0)
        elif opt == "--impatient":
            kw["patient"] = False
        elif opt == "--affirm":
            kw["affirm"] = argv.pop(0)
        elif opt == "--pictures":
            kw["pictures"] = argv.pop(0)
            os.makedirs(kw["pictures"], exist_ok=True)
        elif opt == "--save":
            kw["savepath"] = argv.pop(0)
        elif opt == "--nographics":
            kw["graphics_on"] = 0
        elif opt == "--script":
            with open(argv.pop(0), encoding="utf-8") as f:
                kw["_script"] = load_script(f.read())
        else:
            sys.exit("unknown option " + opt)
    if len(argv) < 2:
        sys.exit(__doc__)
    home = os.path.join(os.environ.get("TMPDIR", "/tmp"), "v6trace-home")
    os.makedirs(home, exist_ok=True)
    script = kw.pop("_script", []) + argv[2:]
    trace, rc, err, left = run(argv[0], argv[1], home, script, **kw)
    print("\n".join(trace))
    print("=== exit %s, script left: %s" % (rc, left))
    if err.strip():
        sys.stderr.write(err[-4000:])


if __name__ == "__main__":
    main()
