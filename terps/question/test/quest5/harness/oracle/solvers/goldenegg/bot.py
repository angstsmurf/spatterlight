"""Golden Egg solver helpers on top of live.Session.

    from bot import *
    b = Bot.load("../../overrides/The Quest for the Golden Egg.cmd")
    b.goto("dungeon_two_room15"); b.fight(); b.status()

Everything that can die goes through attempt(): it checkpoints, runs the step,
and on death (or ending below a health floor) rewinds and retries with a
perturbed fight. Plain filler ("look", "wait", unknown verbs) draws no random
numbers, so it cannot reshuffle anything; changing which enemy we hit, or
spending one turn not attacking, shifts the RNG stream for the rest of the
fight.
"""
import re
from collections import deque
from live import Session

FILLER = "look"
DIRS = {"north", "south", "east", "west", "northeast", "northwest", "southeast",
        "southwest", "up", "down", "in", "out"}


def objs(s):
    return re.findall(r"Object: (\S+?);", s)


class Failed(Exception):
    """A dice outcome went wrong: attempt() rerolls."""


class Stuck(Exception):
    """The plan itself is wrong (no path, a move that didn't move): don't reroll."""


class Bot:
    def __init__(self, cmds=(), seed=None, verbose=True):
        self.s = Session(cmds, seed)
        self.v = verbose
        self._exits = None
        self.last = ""
        self.react = []                 # (regex, command) answered right after a send
        self.perturb = 0                # attempt() reroll number; fight() varies on it
        self.first = ("giant electric eel", "electric eel")  # alias suffixes killed first
        self.fightopts = {}             # defaults for every fight(), also the ones goto()/do() start

    @classmethod
    def load(cls, fn, **kw):
        cmds = [l.strip() for l in open(fn) if l.strip() and not l.startswith("#")]
        return cls(cmds, **kw)

    # -- queries ----------------------------------------------------------
    def ev(self, e):
        return self.s.ev(e)

    def num(self, e):
        try:
            return int(self.ev(e))
        except ValueError:
            return None

    def hp(self):
        return self.num("player.health")

    def room(self):
        return self.ev("player.parent.name")

    def time(self):
        return self.num("game.time")

    def kids(self, of="player.parent"):
        return objs(self.ev(f"GetDirectChildren({of})"))

    def inv(self):
        return objs(self.ev("GetDirectChildren(player)"))

    def enemies(self):
        """Live enemies in the room: (name, alias, hitpoints)."""
        r = []
        for k in self.kids():
            v = self.ev(f'HasBoolean({k},"dead") and not {k}.dead')
            if v == "True":
                r.append((k, self.ev(f"{k}.alias"), self.num(f"{k}.hitpoints")))
        return r

    def status(self):
        st = dict(n=len(self.s.cmds), room=self.room(), hp=self.hp(),
                  max=self.num("player.healthmax"), time=self.time(),
                  money=self.num("player.money"), eq=self.ev("player.equipped"))
        print(st)
        return st

    # -- raw play ---------------------------------------------------------
    def send(self, c, show=None):
        t = self.s.send(c)
        self.last = t
        if show if show is not None else self.v:
            print(f"[{len(self.s.cmds)}] > {c}\n" + t.replace(f"> {c}\n", "", 2).strip()[-1500:])
        if self.s.finished and "You are dead" in t:
            raise Failed("dead")
        for pat, cmd in self.react:
            if re.search(pat, t):
                self.send(cmd(self) if callable(cmd) else cmd, show)
                break
        return t

    def do(self, *cmds):
        for c in cmds:
            self.send(c)
            if self.enemies():
                self.fight()
        return self.last

    # -- map --------------------------------------------------------------
    def exits(self):
        if self._exits is None:
            ex = re.findall(r"Exit: ([^;]+);", self.ev("AllExits()"))
            self._exits = {}
            for e in ex:
                v = self.ev(f'{e}.parent.name+"|"+{e}.to.name+"|"+{e}.alias+"|"+GetDisplayAlias({e}.to)')
                p = v.split("|")
                if len(p) == 4:
                    if p[2] not in DIRS:        # unnamed shop doors: "go to <room>"
                        p[2] = "go to " + p[1]
                    self._exits[e] = tuple(p[:3])
        return self._exits

    def path(self, target, avoid=()):
        """BFS over currently visible, unlocked exits. Returns direction list."""
        g = {}
        for e, (a, b, d) in self.exits().items():
            g.setdefault(a, []).append((e, b, d))
        start = self.room()
        prev = {start: None}
        q = deque([start])
        while q:
            r = q.popleft()
            if r == target:
                break
            for e, b, d in g.get(r, []):
                if b in prev or b in avoid:
                    continue
                if self.ev(f"{e}.visible and not {e}.locked and not {e}.lookonly") != "True":
                    continue
                prev[b] = (r, d)
                q.append(b)
        if target not in prev:
            raise Stuck(f"no path {start} -> {target}")
        dirs = []
        r = target
        while prev[r]:
            r, d = prev[r]
            dirs.append(d)
        return dirs[::-1]

    def goto(self, target, avoid=(), minhp=35):
        for d in self.path(target, avoid):
            if self.hp() < minhp:
                self.heal()
            before = self.room()
            self.send(d)
            if self.enemies():
                self.fight()
            if self.room() == before:
                raise Stuck(f"stuck going {d} from {before}: {self.last.strip()[-200:]}")
        return self.room()

    # -- combat -----------------------------------------------------------
    def attack(self, name, alias):
        t = self.send("attack " + alias)
        if "Please choose" in t:
            # Menus only answer to option numbers ("menu:<name>" is "Er, what..?").
            # Options list every same-alias object in room order, corpses
            # included; guess our slot, then walk the rest past "already dead".
            opts = re.findall(r"^(\d+): (.*)$", t, re.M)
            twins = [k for k in self.kids() if self.ev(f"{k}.alias") == alias]
            first = twins.index(name) + 1 if name in twins and len(twins) == len(opts) else 1
            for n in [first] + [i for i in range(1, len(opts) + 1) if i != first]:
                t = self.send(str(n))
                if "already dead" not in t:
                    break
                t = self.send("attack " + alias)
                if "Please choose" not in t:
                    break
        return t

    def fight(self, limit=80, minhp=None, eatbelow=None):
        """Kill everything in the room: self.first kinds, then weakest first.

        On reroll k (self.perturb) the fight varies: even k targets the
        strongest first, and turn (k-1)//2 is spent waiting instead of
        attacking, which shifts every later roll. Below eatbelow health a
        turn goes to eating one carried food instead (while any is left)."""
        if minhp is None:
            minhp = self.fightopts.get("minhp", 0)
        if eatbelow is None:
            eatbelow = self.fightopts.get("eatbelow", 0)
        k = self.perturb
        for turn in range(limit):
            es = self.enemies()
            if not es:
                return True
            if eatbelow and self.hp() < eatbelow and self.foods():
                self.heal(self.hp() + 1)
                continue
            if minhp and self.hp() < minhp:
                raise Failed(f"low hp {self.hp()} vs {es}")
            es.sort(key=lambda e: (not e[1].endswith(self.first),
                                   e[2] if e[2] is not None else 999))
            if k and k % 2 == 0:
                es.reverse()
            if k and turn == (k - 1) // 2:
                self.send("wait")
                continue
            self.attack(es[0][0], es[0][1])
        raise Failed("fight did not end")

    # -- recovery ---------------------------------------------------------
    def foods(self):
        """Carried edibles as (eathealth, object, alias), smallest first."""
        r = []
        for o in self.inv():
            if self.ev(f'HasAttribute({o},"eathealth")') == "True":
                r.append((self.num(f"{o}.eathealth"), o, self.ev(f"{o}.alias")))
        return sorted(r)

    def heal(self, target=None):
        """Eat carried food until health reaches target (default max-8),
        picking the smallest item that closes the gap, else the biggest."""
        mx = self.num("player.healthmax")
        target = target or mx - 8
        for _ in range(40):
            gap = target - self.hp()
            fs = self.foods()
            if gap <= 0 or not fs:
                break
            h, o, alias = next((f for f in fs if f[0] >= gap), fs[-1])
            t = self.send("eat " + alias)
            if "Please choose" in t:
                opts = re.findall(r"^(\d+): (.*)$", t, re.M)
                twins = [k for k in self.kids("player") if self.ev(f"{k}.alias") == alias]
                n = twins.index(o) + 1 if o in twins and len(twins) == len(opts) else 1
                self.send(str(n))
            if o in self.inv():         # not eaten: don't loop on it
                break
        return self.hp()

    def drink(self, target=None, refill=True):
        """Sip from the bottle until health reaches target, refilling when it runs
        dry (only works where `fill bottle` does: Fountain Meadow, the chapel...)."""
        target = target or self.num("player.healthmax")
        for _ in range(80):
            if self.hp() >= target:
                break
            bottles = self.bottles()
            full = [i for i, (o, n) in enumerate(bottles) if n > 0]
            if not full:
                if not refill or "You fill it" not in self.bottle_cmd("fill bottle", 0):
                    break
                full = [0]
            before = self.hp()
            self.bottle_cmd("drink from bottle", full[0])
            if self.hp() <= before:     # underwater it is a breath of air, not a heal
                break
        if refill:                      # top every bottle up again where we can
            for i, (o, n) in enumerate(self.bottles()):
                if n < 10 and "You fill it" not in self.bottle_cmd("fill bottle", i):
                    break
        return self.hp()

    def bottles(self):
        """Carried bottles as (object, sips left), in menu order."""
        return [(o, self.num(f"{o}.full")) for o in self.inv() if re.fullmatch(r"bottle\d+", o)]

    def bottle_cmd(self, cmd, i):
        """Send a bottle command, answering the which-bottle menu with bottle i."""
        t = self.send(cmd)
        if "Please choose" in t:
            t = self.send(str(i + 1))
        return t

    VENOMS = ("adder", "asp", "viper", "cavedragon")

    def cure(self):
        """Drink a carried antivenom for each active snake poisoning. The menu
        also lists shop stock in sight, so try matching options last-first
        (carried clones sort after the stock) until one is drunk."""
        for v in self.VENOMS:
            if self.ev(f'GetBoolean(player,"poisonedby{v}")') != "True":
                continue
            t = self.send("use antivenom")
            if "Please choose" not in t:
                continue
            opts = [n for n, a in re.findall(r"^(\d+): (.*)$", t, re.M) if v in a.lower()]
            for i, n in enumerate(reversed(opts)):
                if i:
                    self.send("use antivenom")
                if "You drink" in self.send(n):
                    break
        return [v for v in self.VENOMS if self.ev(f'GetBoolean(player,"poisonedby{v}")') == "True"]

    # -- search -----------------------------------------------------------
    def attempt(self, step, tries=30, floor=1, label="step"):
        """Run step(self) from a checkpoint; on failure rewind and rerun it
        with self.perturb = k so fight() plays the dice differently."""
        self.s.checkpoint()
        for k in range(tries):
            if k:
                self.s.rewind()
            self.perturb = k
            try:
                v0 = self.v
                self.v = False
                step(self)
                self.v = v0
                if not self.s.finished and self.hp() >= floor:
                    if self.v:
                        print(f"{label}: ok after {k} rerolls, hp={self.hp()} n={len(self.s.cmds)}")
                    self.s.checkpoint()
                    return k
            except Failed as e:
                self.v = v0
                if self.v:
                    print(f"{label}: try {k} failed: {e}")
            finally:
                self.perturb = 0
        raise Failed(f"{label}: no success in {tries} tries")

    def save(self, fn):
        self.s.save(fn)
