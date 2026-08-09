#!/usr/bin/env python3
import sys,os,re
# Annotate a replay ($A5WORK/out.txt) with the location each command ran in, by
# matching room-name headers against an a5dump map scrape ($A5WORK/map.tsv).
# usage: A5WORK=<dir> annotate.py [n_last_cmds]
# Blind-walkthrough derivation tooling.  Its write-up was pruned once done
# (TODO_a5_walkthrough_wiring.md); see the pruned-docs note at the top of
# ../notes/A5_WALKTHROUGH_FINDINGS.md for how to recover it.
P=os.environ.get("A5WORK", ".").rstrip("/")+"/"
names={}
for line in open(P+"map.tsv"):
    parts=line.rstrip("\n").split("\t")
    if len(parts)>1 and parts[1].strip():
        nm=re.sub(r'<[^>]*>','',parts[1]).strip()
        names.setdefault(nm, parts[0])
out=open(P+"out.txt").read().splitlines()
cur="?"
res=[]
for ln in out:
    if ln.startswith("> "):
        res.append([ln[2:], cur, ""])
    else:
        t=ln.strip()
        t2=re.sub(r'<[^>]*>','',t).strip()
        if t2 and t2 in names:
            cur=f"{names[t2]}:{t2}"
            if res: res[-1][2]=cur
n=int(sys.argv[1]) if len(sys.argv)>1 else 40
for cmd,before,after in res[-n:]:
    print(f"{before:45s} | {cmd:35s} | {'-> '+after if after and after!=before else ''}")
