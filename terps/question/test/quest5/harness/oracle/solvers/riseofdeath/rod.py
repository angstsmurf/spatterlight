"""Rise of Death bot on the shared live driver (../goldenegg/live.py).

    GE_DRIVER=native python3 rserve.py PORT prefix.cmd
    python3 ../goldenegg/ask.py PORT 'b.status()'
"""
import os, re, sys
os.environ.setdefault("GE_GAME", "Rise of Death")
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "../goldenegg"))
from live import Session  # noqa: E402

Y = "Yogg_Saron."
STATS = ("Health", "MaxHealth", "Mana", "MaxMana", "Strength", "Magic", "Armor",
         "SpellArmor", "Souls", "MaxSouls", "UnspentMass", "Stage")


# Elemental zones (from the game source): room -> "dir:room ...".
MAP = {
    "Water1": "east:Water2 south:Water4",
    "Water2": "west:Water1 east:Water3 south:Water5",
    "Water3": "west:Water2 south:Water6",
    "Water4": "north:Water1 south:Water7 east:Water5",
    "Water5": "west:Water4 north:Water2 east:Water6 south:Water8",
    "Water6": "west:Water5 north:Water3 south:Water9",
    "Water7": "north:Water4 east:Water8 up:Earth3",
    "Water8": "north:Water5 east:Water9 west:Water7 down:PreNept",
    "Water9": "north:Water6 west:Water8",
    "PreNept": "south:Neptulon up:Water8",
    "Earth3": "down:Water7 west:Earth2 south:Earth6",
    "Earth1": "east:Earth2 south:Earth4 up:Air6",
    "Earth2": "west:Earth1 south:Earth5 east:Earth3 up:PreThera",
    "Earth4": "north:Earth1 east:Earth5 south:Earth7",
    "Earth5": "north:Earth2 east:Earth6 south:Earth8 west:Earth4",
    "Earth6": "north:Earth3 south:Earth9 west:Earth5",
    "Earth7": "north:Earth4 east:Earth8",
    "Earth8": "north:Earth5 west:Earth7 east:Earth9 down:Fire2",
    "Earth9": "north:Earth6 west:Earth8",
    "PreThera": "down:Earth2 north:Therazane",
    "Fire1": "east:Fire2 south:Fire4",
    "Fire2": "west:Fire1 south:Fire5 east:Fire3 up:Earth8",
    "Fire3": "west:Fire2 south:Fire6",
    "Fire4": "north:Fire1 east:Fire5 south:Fire7",
    "Fire5": "north:Fire2 east:Fire6 south:Fire8 west:Fire4",
    "Fire6": "north:Fire3 south:Fire9 west:Fire5",
    "Fire7": "north:Fire4 east:Fire8",
    "Fire8": "north:Fire5 west:Fire7 east:Fire9 down:PreRag",
    "Fire9": "north:Fire6 west:Fire8",
    "PreRag": "up:Fire8 down:Rag",
    "Air1": "east:Air2 south:Air4",
    "Air2": "west:Air1 south:Air5 east:Air3 up:PreAl",
    "Air3": "west:Air2 south:Air6",
    "Air4": "north:Air1 east:Air5 south:Air7",
    "Air5": "north:Air2 east:Air6 west:Air4 south:Air8",
    "Air6": "north:Air3 west:Air5 south:Air9 down:Earth1",
    "Air7": "north:Air4 east:Air8",
    "Air8": "north:Air5 west:Air7 east:Air9",
    "Air9": "north:Air6 west:Air8",
    "PreAl": "down:Air2 up:Al",
}


class Died(Exception):
    pass


class Bot:
    def __init__(self, cmds=()):
        self.s = Session(cmds)
        self.v = True
        self.last = ""
        self.haunt_to = 1.6     # Haunt until the foe takes this much physical damage

    @classmethod
    def load(cls, fn):
        return cls([l for l in open(fn).read().split("\n") if l.strip() and not l.startswith("#")])

    def ev(self, e):
        return self.s.ev(e)

    def num(self, e):
        try:
            return float(self.ev(e))
        except ValueError:
            return None

    def where(self):
        return self.ev(Y + "parent.name")

    def st(self, *a):
        return [self.num(Y + x) for x in a]

    def send(self, c):
        self.last = self.s.send(c)
        if self.s.finished:
            raise Died(f"finished after {c!r}: " + self.last.strip()[-400:])
        if self.v:
            print(f"[{len(self.s.cmds)}] {c} -> {self.where()}")
        return self.last

    def status(self):
        d = dict(zip(STATS, self.st(*STATS)))
        print(len(self.s.cmds), self.where(), {k: (round(v) if isinstance(v, float) else v) for k, v in d.items()})

    def save(self, fn):
        self.s.save(fn)

    # --- fights -----------------------------------------------------------

    def foe(self):
        return dict(type=self.num("Enemy.FoeType"), hp=self.num("Enemy.FoeHealth"),
                    max=self.num("Enemy.FoeMaxHealth"), souls=self.num("Enemy.FoeSouls"))

    def move(self):
        """Default per-turn choice: rip the soul out when allowed, else shadow ball."""
        if self.ev(Y + "Grappled") == "True" or self.ev(Y + "Stoned") == "True":
            return "g"
        mana, = self.st("Mana")
        f = self.foe()
        if self.ev(Y + "RWords") == "Drain Soul" and mana >= 1000 \
                and f["hp"] < f["max"] * self.num(Y + "DrainLimit"):
            return "r"
        q = self.num("Enemy.FoeArmor")
        w = self.num("Enemy.FoeSpellArmor") if mana >= 5 else 0
        mh, mm, s, m = self.st("MaxHealth", "MaxMana", "Strength", "Magic")
        lash = 2 if self.ev(Y + "QWords") == "Tendril Lash" else 0.65
        ball = 15 if self.ev(Y + "WWords") == "Shadow Ball" else 8
        dq, dw = (mh / 50 + s) * lash * q, (mm / 50 + m) * ball * w
        if dw > dq:
            return "w"
        # air elementals and storm revenants heal every other turn: lash
        # has to outrun that, so haunt their armor much higher first
        haunt_to = 4.0 if f["type"] in (19, 23) else self.haunt_to
        if self.ev(Y + "EWords") == "Haunt" and q < haunt_to and mana >= 1004 \
                and self.num("Enemy.FoeMagic") > 1:
            return "E"
        return "q" if dq > 0 else ("E" if mana >= 4 else "o")

    def fight(self, move=None, limit=200):
        """Play out the current fight; returns the number of foe turns."""
        move = move or self.move
        for t in range(limit):
            loc = self.where()
            if loc == "Fight Room":
                self.send(move())
            elif loc == "Fight Happens":
                if (self.num("Enemy.FoeHealth") or 0) <= 0 and self.num(Y + "CanDrain") == 1:
                    self.send("a")
                self.send("c")
            else:
                return t
        raise RuntimeError("fight did not end")

    def heal(self):
        while self.num(Y + "Health") < self.num(Y + "MaxHealth"):
            self.send("d")
            if self.where() in ("Fight Room", "Fight Happens"):
                self.fight()

    # --- walking ----------------------------------------------------------

    def path(self, to):
        from collections import deque
        here = self.where()
        prev = {here: None}
        q = deque([here])
        while q:
            r = q.popleft()
            if r == to:
                break
            for e in MAP.get(r, "").split():
                d, n = e.split(":")
                if n not in prev:
                    prev[n] = (r, d)
                    q.append(n)
        steps = []
        while prev[to]:
            to, d = prev[to]
            steps.append(d)
        return steps[::-1]

    def walk(self, d):
        """One step; fight whatever ambushes us (we end up in the room entered)."""
        self.send(d)
        if self.where() in ("Fight Room", "Fight Happens"):
            self.fight()
            return True
        return False

    def goto(self, to):
        while self.where() != to:
            self.walk(self.path(to)[0])

    # --- growth -----------------------------------------------------------

    def _enter_upgrade(self, room):
        """`u`, then east until `room`; raise if a step gets nowhere."""
        self.send("u")
        while self.where() != room:
            here = self.where()
            self.send("east")
            if self.where() == here:
                raise RuntimeError(f"cannot reach {room} from {here}: "
                                   + self.last.strip()[-200:])

    def buy(self, tier, key):
        """Buy upgrade `key` (1-3) of `tier`; returns True if a stat changed."""
        room = "UpgradeRoom" if tier == 0 else f"UpgradeRoomTier{tier}"
        before = self.st(*STATS)
        self._enter_upgrade(room)
        self.send(key)
        out = self.last
        if self.st(*STATS) == before:
            raise RuntimeError(f"buy {tier}/{key} did nothing: " + out.strip()[-300:])
        self._leave_upgrade()
        return out

    def buy_max(self, tier, key, most=999):
        """Buy a repeatable upgrade until it stops changing anything."""
        self._enter_upgrade(f"UpgradeRoomTier{tier}")
        n = 0
        while n < most:
            before = self.st(*STATS)
            self.send(key)
            if self.st(*STATS) == before:
                self.s.cmds.pop()   # no-op press: drop it from the script
                self.s.log.pop()
                break
            n += 1
        self._leave_upgrade()
        return n

    def _leave_upgrade(self):
        """Heal up (MaxHealth upgrades leave Health behind), then `b`.

        Leaving can land in a zone fight, so it goes through walk(). `d`
        works in the tier rooms but not in the base UpgradeRoom."""
        if self.where() != "UpgradeRoom":
            while self.num(Y + "Health") < self.num(Y + "MaxHealth"):
                self.send("d")
        self.walk("b")

    def farm(self, a, b2, until, heal=0.6):
        """Pace between rooms a and b2 until until() holds, healing when low."""
        while not until():
            h, mh = self.st("Health", "MaxHealth")
            if h < mh * heal:
                self.heal()
            self.walk(self.path(b2 if self.where() == a else a)[0])

    def farm2(self, until, homes=("Air6", "Earth1"), heal=0.95, log=print):
        """Alternate fights between the two zones of `homes` (their own
        multiplier stacks per same-element fight and resets otherwise).
        Each home sets the zone; a room next to it keeps that zone."""
        side = {"Air6": "Air3", "Earth1": "Earth2", "Water7": "Water4", "Fire2": "Fire1",
                "Earth8": "Earth7", "Earth3": "Earth6"}
        last = None
        while not until():
            if self.num(Y + "EnemyCountdown") == 0:
                h, mh = self.st("Health", "MaxHealth")
                if h < mh * heal:
                    self.heal()
                here = self.where()
                cur = homes[0] if here in (homes[0], side[homes[0]]) else homes[1]
                elem = {h: self.num(Y + re.match(r"[A-Z][a-z]+", h).group()) for h in homes}
                want = min(homes, key=lambda h: (elem[h], h == last))
                if here == want:
                    nxt = side[want]
                elif here == side[want]:
                    nxt = want
                else:
                    nxt = want
                self.send(self.path(nxt)[0])
                if self.where() in ("Fight Room", "Fight Happens"):
                    f = self.foe()
                    self.fight()
                    last = want
                    log("fight", int(f["type"]), "at", want, "hp left", round(self.num(Y + "Health")),
                        "souls", self.num(Y + "Souls"), "n", len(self.s.cmds))
            else:
                here = self.where()
                nxt = homes[1] if here == homes[0] else homes[0]
                self.send(self.path(nxt)[0])
