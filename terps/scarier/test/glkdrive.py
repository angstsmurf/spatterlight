#!/usr/bin/env python3
"""Minimal fake Spatterlight app: speaks the glkimp message protocol on the
terp's stdin/stdout so an autosave/autorestore cycle can be exercised without
the GUI.  Feeds scripted line inputs, records PRINT output and AUTOSAVE
messages, and ends the session with EVTQUIT (the autosave-preserving 'window
closed' exit)."""

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

SETTINGS = struct.pack(
    "<6i4f21i",
    800, 600, 0, 0, 0, 0,               # screen w/h, buffer/grid margins
    8.0, 16.0, 8.0, 16.0,               # grid cell w/h, buffer cell w/h
    0x000000, 0xffffff,                 # buffer fg/bg
    0x000000, 0xffffff,                 # grid fg/bg
    1,                                  # do_styles
    0, 0, 0, 0, 0, 0, 0,                # quotebox, sa_*, slowdraw, flicker
    0, 0, 0, 0, 0,                      # zmachine*, voiceover, z6*
    1,                                  # determinism
    0, 0, 0)                            # error_handling, comprehend, force_arrange


class Driver:
    def __init__(self, terp, game, home, script, tag=""):
        env = dict(os.environ)
        env["HOME"] = home
        self.p = subprocess.Popen([terp, game], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, env=env)
        self.script = list(script)
        self.tag = tag
        self.arrange_pending = True
        self.line_peer = None
        self.char_peer = None
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
            self.transcript.append(payload.decode("utf-16-le", "replace"))
        elif cmd == INITLINE:
            self.line_peer = a1
        elif cmd == CANCELLINE:
            self.line_peer = None
            self.reply(OKAY)
        elif cmd == INITCHAR:
            self.char_peer = a1
        elif cmd == CANCELCHAR:
            self.char_peer = None
        elif cmd == TIMER:
            self.timer = a1
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
            self.reply(OKAY)            # len 0 = cancelled dialog
        elif cmd == AUTOSAVE:
            self.autosaves.append(a2)
            self.log.append("AUTOSAVE hash=%d after %d transcript chunks"
                            % (a2, len(self.transcript)))
        # everything else is one-way; ignore

    def next_event(self, block):
        if self.arrange_pending:
            self.arrange_pending = False
            self.reply(EVTARRANGE, payload=SETTINGS)
        elif self.line_peer is not None and self.script:
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
            peer = self.char_peer
            self.char_peer = None
            self.reply(EVTKEY, peer, 32)   # space
        elif self.timer:
            self.reply(EVTTIMER)
        elif block == 0:
            self.reply(OKAY)
        else:
            # blocking wait with nothing to deliver: tick the clock
            self.reply(EVTTIMER)


def main():
    terp, game, home = sys.argv[1], sys.argv[2], sys.argv[3]
    script = sys.argv[4:]
    os.makedirs(home, exist_ok=True)
    d = Driver(terp, game, home, script)
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
