#!/usr/bin/env python3
import sys, os, collections
# BFS shortest-path over an a5dump map scrape ($A5WORK/map.tsv: key<TAB>name<TAB>
# "Dir->dest; ...").  usage: A5WORK=<dir> route.py <fromLocId> <toLocId>
# Blind-walkthrough derivation tooling -- see TODO_a5_walkthrough_wiring.md.
P=os.environ.get("A5WORK", ".").rstrip("/")+"/"
G={}
NAME={}
DIR={'North':'n','South':'s','East':'e','West':'w','NorthEast':'ne','NorthWest':'nw','SouthEast':'se','SouthWest':'sw','Up':'u','Down':'d','In':'in','Out':'out'}
for line in open(P+"map.tsv"):
    parts=line.rstrip("\n").split("\t")
    key=parts[0]; NAME[key]=parts[1] if len(parts)>1 else ''
    G[key]=[]
    if len(parts)>2 and parts[2]:
        for mv in parts[2].split("; "):
            mv=mv.replace(" [R]","")
            d,dest=mv.split("->")
            G[key].append((DIR.get(d,d),dest))
def path(a,b):
    a,b=f"Location{a}",f"Location{b}"
    q=collections.deque([(a,[])]); seen={a}
    while q:
        cur,p=q.popleft()
        if cur==b: return p
        for d,nxt in G.get(cur,[]):
            if nxt not in seen:
                seen.add(nxt); q.append((nxt,p+[(d,nxt)]))
    return None
a,b=sys.argv[1],sys.argv[2] if len(sys.argv)>2 else ("1","1")
p=path(a,b)
if p is None: print("NO PATH")
else:
    print(" ".join(d for d,_ in p))
    for d,n in p: print(f"  {d} -> {n} {NAME[n]}")
