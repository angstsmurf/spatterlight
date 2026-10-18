"""qvh live-mode session with cheap rewind.

A Session owns the playing process plus a pool of *standby* processes parked at
the last checkpoint. checkpoint() catches the standbys up; rewind() swaps one in
(instant) and rebuilds a replacement by replay in a background thread. Replay is
the only faithful rewind: QuestViva saves don't carry the RNG state.

GE_DRIVER=native plays on the native aslx_replay live mode instead (same
protocol, ~6x faster replays); verify the result on qvh before freezing it.
"""
import os, re, subprocess, threading

Q = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../.."))
DLL = f"{Q}/harness/oracle/bin/Release/net10.0/qvh.dll"
GAME = f"{Q}/games/{os.environ.get('GE_GAME', 'The Quest for the Golden Egg')}.quest"
NATIVE = f"{Q}/harness/aslx_replay"


class Proc:
    def __init__(self, seed=None):
        env = dict(os.environ)
        if seed is not None:
            env["QVH_SEED"] = env["ASLX_SEED"] = str(seed)
        argv = [NATIVE, GAME, "-"] if os.environ.get("GE_DRIVER") == "native" \
            else ["dotnet", DLL, GAME, "-"]
        self.p = subprocess.Popen(argv, stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                  text=True, bufsize=1, env=env)
        self.finished = False
        self.n = 0                      # commands sent (excluding eval:)

    def send(self, c):
        self.p.stdin.write(c + "\n")
        self.p.stdin.flush()
        out = []
        while True:
            l = self.p.stdout.readline()
            if l == "":
                self.finished = True
                break
            if l == "\x01\n":
                break
            out.append(l)
        t = "".join(out)
        if "[state=Finished]" in t:
            self.finished = True
        if not c.startswith("eval:"):
            self.n += 1
        return t

    def ev(self, e):
        t = self.send("eval:" + e)
        m = re.search(r"\[eval\] .*? = (.*)", t, re.S)
        if m and t.startswith("[eval] " + e + " = "):
            return t[len("[eval] " + e + " = "):].strip()
        if "[eval-error]" in t:
            raise RuntimeError(t.strip())
        return m.group(1).strip() if m else t.strip()

    def kill(self):
        try:
            self.p.kill()
            self.p.wait()
        except Exception:
            pass


class Session:
    """Playing process + a pool of standby processes parked at the checkpoint."""

    def __init__(self, cmds=(), seed=None, pool=3):
        self.seed = seed
        self.pool = pool
        self.cmds = []
        self.log = []                   # (cmd, output) for the current line of play
        self.cur = Proc(seed)
        for c in cmds:
            self.send(c)
        self.ck = len(self.cmds)
        self.ready = []                 # standbys at self.ck
        self.lock = threading.Lock()
        self.cv = threading.Condition(self.lock)
        self.gen = 0                    # bumps when the checkpoint moves
        self.building = {}             # gen -> builds in flight
        self._fill()

    # -- standby management -------------------------------------------------
    def _fill(self):
        while len(self.ready) + self.building.get(self.gen, 0) < self.pool:
            self.building[self.gen] = self.building.get(self.gen, 0) + 1
            threading.Thread(target=self._build, args=(self.gen, list(self.cmds[:self.ck])),
                             daemon=True).start()

    def _build(self, gen, cmds):
        p = Proc(self.seed)
        for c in cmds:
            p.send(c)
        with self.cv:
            self.building[gen] -= 1
            if gen == self.gen:
                self.ready.append(p)
            else:
                p.kill()
            self.cv.notify_all()

    def _take(self):
        with self.cv:
            while not self.ready:
                self.cv.wait()
            p = self.ready.pop()
            self._fill()
            return p

    def checkpoint(self):
        """Make the current position the rewind target (standbys catch up)."""
        with self.cv:
            new = self.cmds[self.ck:]
            for p in self.ready:
                for c in new:
                    p.send(c)
            self.ck = len(self.cmds)
            self.gen += 1               # in-flight builds are for the old point
            self._fill()

    def rewind(self):
        """Return to the last checkpoint."""
        p = self._take()
        self.cur.kill()
        self.cur = p
        del self.cmds[self.ck:]
        del self.log[self.ck:]

    def rewind_to(self, n):
        """Return to an arbitrary earlier command count (slow: full replay)."""
        with self.cv:
            for p in self.ready:
                p.kill()
            self.ready = []
            self.gen += 1
        self.cur.kill()
        cmds, log = self.cmds[:n], self.log[:n]
        self.cur = Proc(self.seed)
        for c in cmds:
            self.cur.send(c)
        self.cmds, self.log = cmds, log
        with self.cv:
            self.ck = n
            self._fill()

    # -- play -----------------------------------------------------------------
    @property
    def finished(self):
        return self.cur.finished

    def send(self, c):
        t = self.cur.send(c)
        self.cmds.append(c)
        self.log.append((c, t))
        return t

    def ev(self, e):
        return self.cur.ev(e)

    def save(self, fn):
        with open(fn, "w") as f:
            f.write("\n".join(self.cmds) + "\n")

    def close(self):
        with self.cv:
            self.gen += 1
            for p in self.ready:
                p.kill()
            self.ready = []
        self.cur.kill()
