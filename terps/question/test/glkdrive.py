#!/usr/bin/env python3
"""Minimal fake Spatterlight app: speaks the glkimp message protocol on the
terp's stdin/stdout so an autosave/autorestore cycle can be exercised without
the GUI.  Feeds scripted line inputs, records PRINT output and AUTOSAVE
messages, and ends the session with EVTQUIT (the autosave-preserving 'window
closed' exit).

This file is shared, byte for byte, between terps/question/test/ and
terps/scarier/test/ (each test tree stays self-contained); `make check` in
question/test compares the two.  Edit one, copy to the other.

    glkdrive.py [options] TERP GAME HOME [COMMAND ...]

      --transcript PATH   answer the file dialog with PATH instead of
                          cancelling it, so a SCRIPT / GLK SCRIPT command
                          really opens a transcript file

      --no-determinism    clear the determinism (testing mode) setting

      --timers            set sa_delays, so the frontends arm their real-time
                          Glk timer and "tick:N" entries have something to
                          deliver to

Script entries that are not typed lines:

  link:N         click hyperlink value N in the last window that requested
                 links (link:PEER:N targets a window) -- the only way to reach
                 the Question frontends' link handling, since CheapGlk (the other
                 harnesses) reports no hyperlink support at all.  Scarier's
                 frontends set no links, so for it this entry is inert
  click:TEXT     click the most recent hyperlink whose text is TEXT (the
                 text between a SETLINK and its SETLINK 0), so a script can
                 name a pane entry or an inline object without knowing the
                 value the frontend gave it
  key:X          answer the next CHAR request with X (a single character, or
                 a decimal keycode) instead of the default space
  tick:N         deliver N timer events while the terp is waiting for input,
                 for the frontends' real-time timers (Question: Quest 4 ticks once
                 per event, Quest 5 counts each as a second; Scarier: the
                 ADRIFT 4 <wait x.x> pause and, with determinism off, the
                 ADRIFT 5 real-time clock); needs the sa_delays setting on, or
                 no timer is ever armed and the entry is dropped with a note
                 in the log

Ending the script while a prompt is up sends EVTQUIT, the autosave-preserving
"window closed" exit, so "did this prompt autosave, and does the next launch
resume usably?" is answerable: run once to quit mid-prompt, then again with
no script to see what autorestore brings back.

Determinism is on by default, matching the app's testing mode: it is what
makes the walkthrough regressions reproducible.  Turn it OFF to reach code
paths it deliberately suppresses -- notably Scarier's ADRIFT 5 real-time
mode, which is the only mode where the terp takes over line-input echo
(gsc_a5_start_real_time forces real_time FALSE under determinism).

Autosaves are keyed by game content and land in the REAL
~/Library/Application Support (NSApplicationSupportDirectory ignores $HOME),
so a second run of the same game restores the first run's state -- including
a still-open transcript stream pointing at the old path.  Clear the game's
autosave directory between runs that must start clean."""

import os
import struct
import subprocess
import sys
import threading

MSG = struct.Struct("<6iQ")   # int cmd, a1..a5; size_t len

# protocol.h enum, in order
(NOREPLY, OKAY, ERROR, HELLO, PROMPTOPEN, PROMPTSAVE, NEWWIN, DELWIN, SIZWIN,
 CLRWIN, MOVETO, PRINT, UNPRINT, MAKETRANSPARENT, STYLEHINT, CLEARHINT,
 STYLEMEASURE, SETBGND, REFRESH, SETTITLE, AUTOSAVE, RESET, BANNERCOLS,
 BANNERLINES, TIMER, INITCHAR, CANCELCHAR, INITLINE, CANCELLINE, SETECHO,
 TERMINATORS, INITMOUSE, CANCELMOUSE, FILLRECT, FINDIMAGE, LOADIMAGE,
 SIZEIMAGE, DRAWIMAGE, FLOWBREAK, NEWCHAN, DELCHAN, FINDSOUND, LOADSOUND,
 SETVOLUME, PLAYSOUND, STOPSOUND, PAUSE, UNPAUSE, BEEP, SETLINK, INITLINK,
 CANCELLINK, SETZCOLOR, SETREVERSE, QUOTEBOX, SHOWERROR, CANPRINT, PURGEIMG,
 MENUITEM, NEXTEVENT, EVTARRANGE, EVTREDRAW, EVTLINE, EVTKEY, EVTMOUSE,
 EVTTIMER, EVTHYPER, EVTSOUND, EVTVOLUME, EVTPREFS, EVTQUIT,
 EVTTEST) = range(72)

def make_settings(determinism=1, sa_delays=0):
    return struct.pack(
        "<6i4f21i",
        800, 600, 0, 0, 0, 0,           # screen w/h, buffer/grid margins
        8.0, 16.0, 8.0, 16.0,           # grid cell w/h, buffer cell w/h
        0x000000, 0xffffff,             # buffer fg/bg
        0x000000, 0xffffff,             # grid fg/bg
        1,                              # do_styles
        0,                              # quote_boxes
        sa_delays,                      # sa_delays: real-time timers on
        0, 0, 0, 0, 0,                  # sa_display_style, sa_inventory,
                                        # sa_palette, slowdraw, flicker
        0, 0, 0, 0, 0,                  # zmachine*, voiceover, z6*
        determinism,                    # determinism
        0, 0, 0)                        # error_handling, comprehend, force_arrange


SETTINGS = make_settings()


class Driver:
    def __init__(self, terp, game, home, script, tag="", savepath=None,
                 determinism=1, sa_delays=0):
        env = dict(os.environ)
        env["HOME"] = home
        self.savepath = savepath
        self.settings = make_settings(determinism, sa_delays)
        self.p = subprocess.Popen([terp, game], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, env=env)
        self.script = list(script)
        self.tag = tag
        self.arrange_pending = True
        self.line_peer = None
        self.char_peer = None
        self.link_peer = None
        self.link_peers = set()
        self.open_link = None           # (peer, val) of the link being printed
        self.link_texts = []            # [(peer, val, text)] in print order
        self.timer = 0
        self.next_chan = 1
        self.transcript = []            # decoded PRINT text, in order
        self.autosaves = []             # AUTOSAVE hashes seen
        self.log = []

    def reply(self, cmd, a1=0, a2=0, a3=0, a4=0, a5=0, payload=b""):
        self.p.stdin.write(MSG.pack(cmd, a1, a2, a3, a4, a5, len(payload)))
        if payload:
            self.p.stdin.write(payload)
        self.p.stdin.flush()

    def read_exact(self, n):
        buf = b""
        while len(buf) < n:
            got = self.p.stdout.read(n - len(buf))
            if not got:
                return None
            buf += got
        return buf

    def run(self):
        while True:
            hdr = self.read_exact(MSG.size)
            if hdr is None:
                break
            cmd, a1, a2, a3, a4, a5, ln = MSG.unpack(hdr)
            payload = self.read_exact(ln) if ln else b""
            if ln and payload is None:
                break
            self.dispatch(cmd, a1, a2, a3, a4, a5, payload)
        self.p.stdin.close()
        rc = self.p.wait()
        err = self.p.stderr.read().decode("utf-8", "replace")
        return rc, err

    def dispatch(self, cmd, a1, a2, a3, a4, a5, payload):
        if cmd == HELLO:
            self.reply(OKAY, 1, 1)      # graphics on, sound on
        elif cmd == NEXTEVENT:
            self.next_event(a1)
        elif cmd == PRINT:
            text = payload.decode("utf-16-le", "replace")
            self.transcript.append(text)
            if self.open_link and self.open_link[0] == a1:
                self.link_texts[-1][2] += text
        elif cmd == SETLINK:
            # a1 = window, a2 = link value (0 ends the linked run).
            if a2:
                self.open_link = (a1, a2)
                self.link_texts.append([a1, a2, ""])
            else:
                self.open_link = None
        elif cmd == CLRWIN:
            # The frontends rebuild a cleared pane's links from scratch.
            self.link_texts = [t for t in self.link_texts if t[0] != a1]
        elif cmd == INITLINE:
            self.line_peer = a1
        elif cmd == CANCELLINE:
            self.line_peer = None
            self.reply(OKAY)
        elif cmd == INITLINK:
            self.link_peer = a1
            self.link_peers.add(a1)
            self.log.append("INITLINK peer=%d" % a1)
        elif cmd == CANCELLINK:
            self.link_peer = None
        elif cmd == INITCHAR:
            self.char_peer = a1
        elif cmd == CANCELCHAR:
            self.char_peer = None
        elif cmd == TIMER:
            self.timer = a1
            if a1:
                self.log.append("TIMER armed %d ms" % a1)
        elif cmd == SETECHO:
            # val=0 means the terp has taken over line-input echo; the
            # library must then keep its own copy out of the echo stream.
            self.log.append("SETECHO peer=%d val=%d" % (a1, a2))
        elif cmd == NEWWIN:
            self.log.append("NEWWIN type=%d peer=%d" % (a1, a2))
            self.reply(OKAY, a2)        # echo the expected peer
        elif cmd == DELWIN:
            self.log.append("DELWIN peer=%d" % a1)
        elif cmd == STYLEMEASURE:
            self.reply(OKAY, 0, 0)
        elif cmd == UNPRINT:
            self.reply(OKAY, 0)
        elif cmd == FINDIMAGE:
            self.reply(OKAY, 0)
        elif cmd == SIZEIMAGE:
            self.reply(OKAY, 0, 0)
        elif cmd == FINDSOUND:
            self.reply(OKAY, 0)
        elif cmd == NEWCHAN:
            self.reply(OKAY, self.next_chan)
            self.next_chan += 1
        elif cmd == BANNERCOLS:
            self.reply(OKAY, 80)
        elif cmd == BANNERLINES:
            self.reply(OKAY, 24)
        elif cmd == CANPRINT:
            self.reply(OKAY, 1)
        elif cmd in (PROMPTOPEN, PROMPTSAVE):
            if self.savepath:
                self.log.append("PROMPT%s -> %s"
                                % ("SAVE" if cmd == PROMPTSAVE else "OPEN",
                                   self.savepath))
                self.reply(OKAY, payload=self.savepath.encode("utf-8") + b"\0")
            else:
                self.reply(OKAY)        # len 0 = cancelled dialog
        elif cmd == AUTOSAVE:
            self.autosaves.append(a2)
            self.log.append("AUTOSAVE hash=%d after %d transcript chunks"
                            % (a2, len(self.transcript)))
        # everything else is one-way; ignore

    def find_link(self, text):
        """(peer, val) of the most recently printed link whose text is
        `text` (trimmed), or None."""
        for peer, val, got in reversed(self.link_texts):
            if got.strip() == text:
                return peer, val
        return None

    def click(self, peer, val):
        self.log.append("[click link %d in peer %d]" % (val, peer))
        self.link_peer = None
        self.reply(EVTHYPER, peer, val)

    def waiting_for_input(self):
        return self.line_peer is not None or self.char_peer is not None

    def next_event(self, block):
        if self.arrange_pending:
            self.arrange_pending = False
            self.reply(EVTARRANGE, payload=self.settings)
        elif (self.script and self.script[0].startswith("tick:")
              and self.waiting_for_input()):
            # "tick:N": N timer events, one per NEXTEVENT, while input is
            # pending.  A tick with no timer armed is dropped (not held: the
            # game may have disarmed its timers on the previous one).
            spec = self.script[0][5:]
            n = int(spec)
            if not self.timer:
                self.script.pop(0)
                self.log.append("[tick:%s dropped: no timer armed]" % spec)
                self.next_event(block)
                return
            if n > 1:
                self.script[0] = "tick:%d" % (n - 1)
            else:
                self.script.pop(0)
            self.log.append("[tick]")
            self.reply(EVTTIMER)
        elif (self.script and self.script[0].startswith("link:")
              and self.link_peer is not None):
            # "link:VALUE" clicks in the last window to request links;
            # "link:PEER:VALUE" targets a specific window (the side pane).
            spec = self.script.pop(0)[5:]
            if ":" in spec:
                peer, val = (int(x) for x in spec.split(":", 1))
            else:
                peer, val = self.link_peer, int(spec)
            self.click(peer, val)
        elif (self.script and self.script[0].startswith("click:")
              and self.link_peer is not None):
            text = self.script.pop(0)[6:]
            hit = self.find_link(text)
            if hit is None:
                self.log.append("[click:%s: no such link on screen]" % text)
                self.next_event(block)
                return
            self.click(*hit)
        elif self.line_peer is not None and self.script:
            # A "key:" answer left over when a LINE is what is wanted means the
            # script and the terp have drifted apart; drop it noisily rather
            # than type "key:y" into the game.  A click:/link: entry left over
            # is likewise dropped: the link it names never appeared.
            while self.script and self.script[0].split(":", 1)[0] in (
                    "key", "click", "link", "tick"):
                self.log.append("[stray %s dropped: a line was requested]"
                                % self.script.pop(0))
            if not self.script:
                self.log.append("[script done: EVTQUIT]")
                self.reply(EVTQUIT)
                return
            text = self.script.pop(0)
            self.log.append("> " + text)
            data = text.encode("utf-16-le")
            peer = self.line_peer
            self.line_peer = None
            self.reply(EVTLINE, peer, len(text), 0, payload=data)
        elif self.line_peer is not None:
            self.log.append("[script done: EVTQUIT]")
            self.reply(EVTQUIT)
        elif self.char_peer is not None and not self.script:
            self.log.append("[char request after script: EVTQUIT]")
            self.reply(EVTQUIT)
        elif self.char_peer is not None:
            # A script entry "key:X" answers this char request with X; anything
            # else is left for the next line request and the keypress defaults
            # to space, which is what a "press any key" pause wants.  Y/N
            # confirms (Scarier's hint display, quit/restart) need the real
            # letter.
            peer = self.char_peer
            self.char_peer = None
            code = 32
            if self.script and self.script[0].startswith("key:"):
                spec = self.script.pop(0)[len("key:"):]
                code = ord(spec) if len(spec) == 1 else int(spec)
                self.log.append("key> " + spec)
            self.reply(EVTKEY, peer, code)
        elif self.timer:
            self.reply(EVTTIMER)
        elif block == 0:
            self.reply(OKAY)
        else:
            # blocking wait with nothing to deliver: tick the clock
            self.reply(EVTTIMER)


def main():
    argv = sys.argv[1:]
    savepath, determinism, sa_delays = None, 1, 0
    while argv and argv[0].startswith("--"):
        opt = argv.pop(0)
        if opt == "--transcript":
            savepath = argv.pop(0)
        elif opt == "--no-determinism":
            determinism = 0
        elif opt == "--timers":
            sa_delays = 1
        else:
            sys.exit("unknown option " + opt)

    terp, game, home = argv[0], argv[1], argv[2]
    script = argv[3:]
    os.makedirs(home, exist_ok=True)
    d = Driver(terp, game, home, script, savepath=savepath,
               determinism=determinism, sa_delays=sa_delays)
    timer = threading.Timer(60.0, d.p.kill)
    timer.start()
    rc, err = d.run()
    timer.cancel()
    print("=== exit %s, %d autosaves, script left: %s" %
          (rc, len(d.autosaves), d.script))
    for line in d.log:
        print("LOG " + line)
    print("=== TRANSCRIPT ===")
    print("".join(d.transcript))
    if err.strip():
        print("=== STDERR ===")
        print(err[-2000:])


if __name__ == "__main__":
    main()
