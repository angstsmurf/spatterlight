# Adaptive Spectre-banish solver for The Spectre of Castle Coris (WORK IN PROGRESS).
#
# The game's built-in WLKTHRGH does not win under a faithful engine: its
# navigation is written for an earlier map revision, and the Spectre is a
# periodic turn-based killer that must be banished with the prayer book HELD.
# This driver replays a command list through test/a5run_dump, finds the first
# Spectre encounter that is not banished before it kills, and injects a banish
# ("read prayer", wrapped in "get book"/"drop book" when the book is down for a
# two-handed action), then re-runs -- iterating until it wins, dies on a
# non-Spectre cause, or gets stuck on a navigation desync it cannot fix.
#
# It is NOT a finished solution: it converges the Spectre timing but cannot
# repair the map-navigation desyncs (the portcullis/courtyard cascade), which
# need a manual region-by-region re-derivation against this build's map.
#
# Usage:  SP=<scratchdir> python3 test/spectre_prayer_solver.py <command-list.txt>
#   (run from terps/scarier; writes the converged list to $SP/scoc_solved.txt
#    on a win, else $SP/scoc_partial.txt.)
#
import subprocess,sys,re,os,time
GAME="test/adrift5-games/TheSpectreOfCastleCoris.blorb"; BIN="test/a5run_dump"; SP=os.environ['SP']
def run(cmds):
    open(f"{SP}/cur.txt","w").write('o\nb\n'+'\n'.join(cmds)+'\n')
    return subprocess.run([BIN,GAME,f"{SP}/cur.txt"],capture_output=True,text=True).stdout
def blocks(out):
    parts=re.split(r'(?m)^> (.*)$',out); res=[]
    for i in range(1,len(parts),2):
        res.append((parts[i].strip(), parts[i+1] if i+1<len(parts) else ''))
    return res
PRESENT=re.compile(r'materialising in front of you|The spectre is here|rooted to the spot|starts to coalesce')
BANISH=re.compile(r'You say the words|You say out loud|You recite|apparition wails|mist fades from view')
NOBOOK=re.compile(r'not carrying the prayer book')
DEATH=re.compile(r'you die to the sound of sibilant laughter')
WON=re.compile(r'YOU HAVE WON|You have won')
def held_state(cmds,upto):
    held=True
    for c in cmds[:upto+1]:
        if c in ('drop book','put book in bag'): held=False
        elif c=='get book': held=True
    return held
cmds=[l.rstrip('\n') for l in open(sys.argv[1])]
cmds=[c for c in cmds if c.strip()!='' and not c.startswith('#')]
if cmds[:2]==['o','b']: cmds=cmds[2:]
t0=time.time(); hist={}
for it in range(400):
    if time.time()-t0>95: print("TIME BUDGET",len(cmds)); break
    out=run(cmds)
    if WON.search(out):
        print(f"*** WON *** after {it} edits, {len(cmds)} cmds"); open(f"{SP}/scoc_solved.txt","w").write('\n'.join(cmds)+'\n'); sys.exit(0)
    bl=blocks(out)
    # find first block where spectre becomes present and is NOT banished before death/end
    present=False; fixed=False
    for bi in range(2,len(bl)):
        cmd,txt=bl[bi]
        if not present and PRESENT.search(txt):
            present=True; start=bi
        if present:
            if BANISH.search(txt):
                present=False; continue
            if DEATH.search(txt) or bi==len(bl)-1:
                # unhandled encounter starting at 'start'
                ci=start-2
                key=('P',ci); hist[key]=hist.get(key,0)+1
                if hist[key]>6:
                    print(f"STUCK at cmd#{ci} '{cmds[ci] if ci<len(cmds) else '?'}'"); print("ctx:",cmds[max(0,ci-5):ci+4]); print(bl[start][1][:200]); sys.exit(4)
                ins=['read prayer'] if held_state(cmds,ci) else ['get book','read prayer','drop book']
                cmds[ci+1:ci+1]=ins
                fixed=True; break
    if fixed: continue
    if DEATH.search(out):
        for bi in range(len(bl)-1,1,-1):
            if DEATH.search(bl[bi][1]):
                ci=bi-2; print(f"NON-SPECTRE DEATH cmd#{ci} '{cmds[ci] if ci<len(cmds) else '?'}'"); print("ctx:",cmds[max(0,ci-6):ci+2]); print("blk:",bl[bi][1][:250]); break
        open(f"{SP}/scoc_partial.txt","w").write('\n'.join(cmds)+'\n'); sys.exit(1)
    m=re.findall(r'scored \d+ out of a possible \d+.*|possible \d+',out)
    print(f"END no-win {len(cmds)} cmds; {m[-1:]}"); print(out[-350:]); open(f"{SP}/scoc_partial.txt","w").write('\n'.join(cmds)+'\n'); sys.exit(2)
open(f"{SP}/scoc_partial.txt","w").write('\n'.join(cmds)+'\n'); print("done-loop",len(cmds))
