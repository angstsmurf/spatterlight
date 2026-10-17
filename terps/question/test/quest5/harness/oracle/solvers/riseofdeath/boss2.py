import sys
from probe import *
from bot import *
b=load(sys.argv[1]); b.v=False
def save():
    open(sys.argv[2],"w").write("\n".join(b.L.cmds)+"\n")
TARGET={0:21,1:21,2:1,3:5,4:25}
def N(e): return b.num(e) or 0
for it in range(900):
    if b.L.finished: print("FINISHED"); print(b.last[-1500:]); break
    loc=b.where(); st=int(N("GenocideFinaleIntro.CaveBossStage"))
    if loc=="Fight Room":
        ft=int(N("Enemy.FoeType")); an=int(N("Enemy.FoeAttackNum")); mana=N(Y+"Mana"); fm=N("Enemy.FoeMagic")
        if ft in (13,14): m="e" if mana>=4 else "o"
        elif ft==15: m="o" if an==3 else ("e" if mana>=4 else "o")
        elif ft==16: m="e" if (fm>9 and mana>=4) else "o"
        else: m="q"
        b.send(m)
        print(it,loc,ft,an,m,"hp",N(Y+"Health"),"fmana",N("Enemy.FoeMana"),"fm",round(fm,1))
    elif loc=="Fight Happens": b.send("c")
    elif loc=="CaveBossFinale":
        b.send("r"); print("R pressed"); print(b.last[-1500:]); save(); break
    elif loc.startswith("CaveBoss"):
        if N(Y+"Health")<N(Y+"MaxHealth")*0.95 and False: pass
        b.send("out"); print(it,"out from",loc)
    elif cave_n(loc):
        n=cave_n(loc)
        if N(Y+"Health")<N(Y+"MaxHealth"): b.send("d"); print(it,"d hp",N(Y+"Health"))
        else:
            b.send(step_toward(n,TARGET[st])); print(it,"go",loc,"->",TARGET[st])
    else:
        print("unknown loc",loc); print(b.last[-800:]); break
    if it%20==0: save()
save()
print("END",b.where(),b.st("Health","MaxHealth","Stage","Armor","SpellArmor","UnspentMass") if not b.L.finished else "")
