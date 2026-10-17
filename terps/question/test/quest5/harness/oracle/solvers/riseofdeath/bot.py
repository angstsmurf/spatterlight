import sys,re
from qlive import *
Y="Yogg_Saron."
class Bot:
    def __init__(s,seed=1234,verbose=True):
        s.L=Live("Rise of Death",seed); s.v=verbose; s.last=""; s.n=0
    def num(s,e):
        t=s.L.ev(e)
        try: return float(t)
        except: return None
    def where(s): return s.L.ev("Yogg_Saron.parent.name")
    def send(s,c):
        s.n+=1
        s.tr=getattr(s,'tr',[])
        s.last=s.L.send(c)
        if s.L.finished: return s.last
        if s.v: print(f"[{s.n}] {c!r} -> {s.where() if not s.L.finished else 'FIN'}  hp={s.L.ev(Y+'Health') if not s.L.finished else ''}")
        return s.last
    def st(s,*a): return [s.num(Y+x) for x in a]

def cave_n(name):
    m=re.fullmatch(r"Cave(\d+)",name); return int(m.group(1)) if m else None
def step_toward(cur,tgt):
    r,c=divmod(cur-1,5); R,C=divmod(tgt-1,5)
    if r<R: return "go south"
    if r>R: return "go north"
    if c<C: return "go east"
    if c>C: return "go west"
POL={"ne":3,"atk":"w"}
def fightmove(b):
    if b.L.ev(Y+"Grappled")=="True": return "g"
    mana=b.num(Y+"Mana") or 0
    W=b.L.ev(Y+"WWords"); E=b.L.ev(Y+"EWords")
    key=(b.num("Enemy.FoeType"),b.num("Enemy.FoeMaxHealth"),b.num(Y+"Kills"))
    if b.__dict__.get("fid") is None: b.fid=key; b.ecount=0
    if E=="Cripple" and b.num("Enemy.FoeType")==4 and b.ecount<POL["ne"]:
        b.ecount+=1; return "e"
    if E=="Haunt" and b.num("Enemy.FoeType")==4 and b.ecount<POL["ne"] and mana>=9:
        b.ecount+=1; return "e"
    if POL["atk"]=="w" and W=="Shadow Ball" and mana>=3: return "w"
    if POL["atk"]=="w" and W=="Drain Life" and mana>=5: return "w"
    return "q"
def run(b,target=None,limit=400,hook=None):
    for _ in range(limit):
        if b.L.finished: print("FINISHED");print(b.last[-1500:]);return
        loc=b.where()
        if loc not in ("Fight Room","Fight Happens"): b.fid=None
        if hook:
            r=hook(b,loc)
            if r: continue
        if loc=="Fight Room": b.send(fightmove(b))
        elif loc=="Fight Happens":
            if (b.num("Enemy.FoeHealth") or 0)<=0 and b.num(Y+"CanDrain")==1: b.send("a")
            b.send("c")
        else:
            print("unknown loc",loc); return

def snap(b): return tuple(b.st("UnspentMass","Souls","MaxHealth","MaxMana","Strength","Magic"))
def buy(b):
    """walk the upgrade rooms east, press 1/2/3 where it changes state; prune no-ops."""
    n0=len(b.L.cmds)
    if not b.where().startswith("Upgrade"):
        b.send("u")
    # rewind to tier0 room
    while b.where()!="UpgradeRoom":
        b.send("go west")
    bought=0
    while True:
        for k in "123":
            before=snap(b)
            b.send(k)
            if snap(b)==before: b.L.cmds.pop()
            else: bought+=1
        w=b.where()
        b.send("go east")
        if b.where()==w:
            b.L.cmds.pop(); break
    while b.where().startswith("Upgrade"):
        b.send("b")
        if b.where().startswith("Upgrade"): b.send("go west")
    if bought==0: del b.L.cmds[n0:]
    return bought

# (tier, key, cost, currency, max count)
PLAN=[(1,"1",3,"UnspentMass",1),(1,"3",5,"UnspentMass",1),(1,"2",8,"UnspentMass",1),
      (2,"3",40,"UnspentMass",1),(2,"2",45,"UnspentMass",1),(2,"1",50,"UnspentMass",1),(3,"2",50,"UnspentMass",20)]
def buy_item(b,tier,key):
    n0=len(b.L.cmds)
    before=snap(b)
    b.send("u")
    ok=True
    for _ in range(tier):
        w=b.where(); b.send("go east")
        if b.where()==w: ok=False;break
    if ok: b.send(key)
    ok2= ok and snap(b)!=before
    b.send("b")
    while b.where().startswith("Upgrade"): b.send("go west")
    return ok2
