import sys
from bot import *
def load(fn):
    b=Bot(); b.v=False
    for c in open(fn).read().split("\n"):
        if c: b.L.send(c)
    b.L.cmds=[c for c in open(fn).read().split("\n") if c]
    return b
