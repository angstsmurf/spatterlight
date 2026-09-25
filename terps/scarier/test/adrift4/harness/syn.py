#!/usr/bin/env python3
"""List solution lines where run390's synonym pass and the OLD Scarier rewrite disagree.

Usage: harness/syn.py <game.taf> <solution file in goldens/>
e.g.   harness/syn.py CS2.taf cs2_solution.txt

Synonyms come from SCR_DUMP_TASKS stderr ("SYNONYM [x] -> [y]"), in file order.

Runner model (run390 45F18C, gate c() at 4334B0; ported into pf_filter_input() in
scprintf.cpp on 2026-09-25, so it is Scarier's model now too):
  * the typed line is lower-cased first;
  * the gate LCase()s both sides and looks only at the FIRST InStr hit that starts
    the line or follows a space; it passes only if that hit ends the line or is
    followed by a space, comma or full stop, else FALSE with no further search;
  * behind the gate, Replace(line, orig, LCase(repl), 1, -1, 0): a BINARY substring
    replace of EVERY occurrence, letters inside other words included -- and an
    Original with an upper-case letter in it (CS2's [СВ] -> [northeast]) passes the
    gate and then replaces nothing (Wine run390, runner_transcripts/cs2.txt).
Old Scarier model (before 2026-09-25): every whole-word hit is replaced, any case.

Measured lines: Dolg `войти в дом` and `позвонить в звонок` (Adrift_dolg.txt),
CS2 `св` -> "Да?", shablon `выбросить пачку в урну` (runner_transcripts/*.txt).
See notes/Dolg_walkthrough.md and notes/SYNONYM_GATE_TODO.md.
"""
import os
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import sys,re,subprocess
game,sol=sys.argv[1],sys.argv[2]
err=subprocess.run(['./harness/scare','games/'+game],input=b'\n\n\nquit\ny\n',capture_output=True,env={'SCR_DUMP_TASKS':'1','PATH':'/usr/bin:/bin'}).stderr.decode('cp1251')
syn=re.findall(r'^SYNONYM \[(.*?)\] -> \[(.*?)\]',err,re.M)
def gate(lo,ol):
    i=0
    while True:
        i=lo.find(ol,i)
        if i<0: return False
        if i==0 or lo[i-1]==' ':
            j=i+len(ol); return j>=len(lo) or lo[j] in ' ,.'
        i+=1
def runner(l):
    l=l.lower()
    for o,r in syn:
        if gate(l,o.lower()): l=l.replace(o,r.lower())
    return l
def old(l):
    l=l.lower()
    for o,r in syn:
        l=re.sub(r'(?<![^\s])'+re.escape(o)+r'(?![^\s])',r,l,flags=re.I)
    return l
bad=0
for n,l in enumerate(open('goldens/'+sol,encoding='cp1251').read().split('\n'),1):
    if l.startswith('#') or not l.strip(): continue
    a,b=runner(l),old(l)
    if a!=b: bad+=1; print(f'  {n}: {l!r} runner={a!r} old={b!r}')
print(game,len(syn),'synonyms,',bad,'differing lines')
