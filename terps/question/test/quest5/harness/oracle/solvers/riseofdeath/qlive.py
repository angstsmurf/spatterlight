import subprocess, os
Q="/Users/administrator/spatterlight/terps/question/test/quest5"
class Live:
    def __init__(self, game, seed=1234):
        env=dict(os.environ, QVH_SEED=str(seed))
        self.p=subprocess.Popen(["dotnet",f"{Q}/harness/oracle/bin/Release/net10.0/qvh.dll",f"{Q}/games/{game}.quest","-"],
            stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True,bufsize=1,env=env)
        self.cmds=[]; self.finished=False
    def send(self,c,record=True):
        if record and not c.startswith("eval:"): self.cmds.append(c)
        self.trace=getattr(self,'trace',[])
        self.p.stdin.write(c+"\n"); self.p.stdin.flush()
        out=[]
        while True:
            l=self.p.stdout.readline()
            if l=="" : self.finished=True; break
            if l=="\x01\n": break
            out.append(l)
        t="".join(out)
        if record and not c.startswith('eval:'): self.trace.append((c,t))
        if "[state=Finished]" in t: self.finished=True
        return t
    def ev(self,e):
        t=self.send("eval:"+e,False)
        return t.split(" = ",1)[1].strip() if " = " in t else t
