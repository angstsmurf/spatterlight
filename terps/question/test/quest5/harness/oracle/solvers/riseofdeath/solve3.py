from probe import *
import sys,random,re,pickle
random.seed(7)
start=sys.argv[1]; limit=int(sys.argv[2]); out=sys.argv[3]
import bot
if len(sys.argv)>4: bot.POL['ne']=int(sys.argv[4]); bot.POL['atk']=sys.argv[5]
b=load(start); b.v=False
PLAN=[(1,"1",3,1),(1,"3",5,1),(1,"2",8,1),(2,"3",40,1),(2,"2",45,1),(2,"1",50,1),(3,"2",50,20),(3,"1",50,8),
      (5,"1",650,1),(4,"1",350,1),(4,"3",350,1),(4,"2",350,1),(6,"1",250,29),(6,"2",500,15),(6,"3",500,10),(5,"2",700,1),(5,"3",750,1),(4,"3",350,1)]
DONE0={(1,'1'):1,(1,'3'):1,(1,'2'):1,(2,'3'):1,(2,'2'):1,(2,'1'):1,(3,'2'):12,(3,'1'):0,(5,'1'):1,(6,'1'):1,(6,'2'):1}
import os
ST=out+'.state'
if os.path.exists(start+'.state'): done,blocked0,denkeys0=pickle.load(open(start+'.state','rb'))
else: done,blocked0,denkeys0=dict(DONE0),{},[]
def capok(t,k):
    mh=b.num(Y+"MaxHealth") or 0; st=b.num(Y+"Strength") or 0
    if (t,k)==(3,"2"): return mh<230
    if (t,k)==(6,"1"): return st<180 and (b.num(Y+'Stage') or 0)>=3
    if (t,k)==(6,"2"): return mh<1000
    if (t,k)==(6,"3"): return (b.num(Y+'Stage') or 0)>=3
    return True
def want():
    m=b.num(Y+"UnspentMass") or 0
    for t,k,c,mx in PLAN:
        if done.get((t,k),0)<mx and capok(t,k):
            return (t,k,c) if m>=c else None
    return None
seen=set(); blocked=dict(blocked0); denkeys=list(denkeys0)
def sigf(): return tuple(b.num(x) for x in ("CThun.CThunTalks","NZoth.NZothTalks","Yogg_Saron.NZothCountdown","Yogg_Saron.Stage"))
def farm_gate():
    return b.L.ev("Genocide1.GenoAvailable")=="True" and ((b.num(Y+"MaxHealth") or 0)<1000 or (b.num(Y+"UnspentMass") or 0)<7000)
def story_ready():
    if farm_gate(): return False
    sig=sigf()
    return any(blocked.get(k)!=sig for k in denkeys)
def story(loc):
    txt=b.last
    keys=re.findall(r"Enter '([A-Za-z])' to interact",txt)
    if loc not in seen:
        seen.add(loc); print("== new room",loc,"| exits:",re.findall(r"You can go ([^\n]*)",txt)); 
def hook(b,loc):
    if loc=="Fight Room" and b.__dict__.get("lastfight")!=b.n-1:
        print("FIGHT type",b.num("Enemy.FoeType"),"foe hp",b.num("Enemy.FoeHealth"),"mine",b.st("Health","MaxHealth","Strength","Armor","SpellArmor"))
    if loc=="Fight Room": b.lastfight=b.n
    n=cave_n(loc)
    if n:
        hp,mhp=b.st("Health","MaxHealth")
        if hp<mhp*0.9: b.send("d"); return True
        nb=want()
        if nb:
            t,k,c=nb
            if buy_item(b,t,k): done[(t,k)]=done.get((t,k),0)+1; print("bought",t,k,b.st("UnspentMass","MaxHealth","Strength","Magic","MaxMana"))
            else: done[(t,k)]=99; print("buy failed",t,k,b.where(),[ (c,o[:80].replace("\n","/")) for c,o in b.L.trace[-9:]])
            return True
        geno=(b.num("Genocide1.GenoInProgress") or 0)>0 and b.num("Yogg_Saron.Stage")==3
        if story_ready() and not geno:
            if n!=10: b.send(step_toward(n,10)); return True
            b.send("go east"); return True
        opts={"go east":n%5!=0 and n!=10,"go west":(n-1)%5!=0,"go south":n<=20,"go north":n>5}
        b.send(random.choice([k for k,v in opts.items() if v])); return True
    if loc=="TutorialCave3": b.send("go south"); return True
    if loc.startswith("Upgrade") or loc.startswith("Status"): b.send("b"); return True
    if loc=="Stage1EndEnemy":
        hp,mhp=b.st("Health","MaxHealth")
        if hp<mhp*0.9: b.send("d"); return True
        keys=re.findall(r"Enter '([A-Za-z])' to interact",b.L.send("look"))
        sig=sigf()
        denkeys[:]=[k for k in "MNYX" if k in [x.upper() for x in keys]]
        late=(b.num("CThun.CThunTalks") or 0)>=6
        if (b.num("Genocide1.GenoInProgress") or 0)>0 and b.num("Yogg_Saron.Stage")==3 or farm_gate(): b.send("go west"); return True
        for k in ("NXY" if late else "MNYX"):
            if late and k=="N" and (b.num("NZoth.NZothTalks") or 0)>=6: continue
            if late and k in "NXY" and k.upper() in [x.upper() for x in keys]:
                b.send(k.lower()); return True
            if k.upper() in [x.upper() for x in keys] and blocked.get(k)!=sig:
                b.send(k.lower()); 
                if b.where()=="Fight Room": blocked[k]=sig
                else: blocked.pop(k,None)
                return True
        b.send("go west"); return True
    SCRIPT={"CThunExperimentMain":"c","CThunExperimentPowder1":"m","Genocide1":"x","Genocide2":"x"}
    if loc in SCRIPT: b.send(SCRIPT[loc]); return True
    txt=b.last
    story(loc)
    ex=re.findall(r"You can go ([^\n]*)",b.L.send("look"))
    print("[story]",loc,ex, b.n)
    late=(b.num("CThun.CThunTalks") or 0)>=6
    if ex and re.search(r"\bout\b",ex[0]):
        sig=tuple(b.num(x) for x in ("CThun.CThunTalks","NZoth.NZothTalks","Yogg_Saron.NZothCountdown","Yogg_Saron.Stage"))
        b.send("out")
        if b.where()=="Fight Room" and not late:
            for k in "MNYX": blocked[k]=sig
        return True
    if ex:
        b.send("go "+re.split(r"[ ,.]+",ex[0])[0]); return True
    return False
run(b,hook=hook,limit=limit)
open(out,'w').write('\n'.join(b.L.cmds)+'\n')
if not b.L.finished: print(b.where(), b.st('UnspentMass','Strength','Magic','MaxMana','Health','MaxHealth','Stage'))
print(b.last[-1500:])
pickle.dump((done,blocked,denkeys),open(ST,"wb"))
open(out,"w").write("\n".join(b.L.cmds)+"\n")
