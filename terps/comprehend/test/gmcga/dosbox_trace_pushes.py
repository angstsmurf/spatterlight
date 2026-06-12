#!/usr/bin/env python3
"""Trace every FUN_2da0 push attempt of title fill #15 in the native interpreter."""
import sys, os, time
sys.path.insert(0, "/Users/administrator/dosbox-x-remotedebug/tests/integration")
from dosbox_debug import DOSBoxInstance

SIG = bytes.fromhex("e8dd07803e96a2017501c33a1e5a9d77")
TARGET_FILL = int(os.environ.get("FILL", "15"))

def read_big(gdb, addr, size, chunk=1024):
    out = b""
    while size > 0:
        n = min(chunk, size)
        out += gdb.read_memory(addr, n)
        addr += n; size -= n
    return out

def wait_text(gdb, needle, timeout=30):
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            if any(needle in l for l in gdb.screen_dump()):
                return True
        except Exception:
            pass
        time.sleep(0.5)
    return False

dbx = DOSBoxInstance(
    executable="/Users/administrator/dosbox-x-remotedebug/src/dosbox-x",
    config="/tmp/talisman_trace.conf",
    working_dir="/Users/administrator/dosbox-x-remotedebug",
)
dbx.start()
gdb, qmp = dbx.gdb, dbx.qmp
try:
    gdb.continue_()
except Exception:
    pass
assert wait_text(gdb, "A:\\"), "no prompt"
qmp.debug_break_on_exec(True)
qmp.type_text("novel1\r", delay=0.05)
gdb.wait_for_stop(timeout=20.0)
qmp.debug_break_on_exec(False)
regs = gdb.read_registers()

code_lin = None
scan_base = max(0x600, (regs.eip & ~0xFFF) - 0x2000)
for base in range(scan_base, scan_base + 0x40000, 0x1000):
    blob = read_big(gdb, base, 0x1000 + 16)
    i = blob.find(SIG)
    if i >= 0:
        code_lin = base + i - 0x2630
        break
assert code_lin
ds = code_lin + 0x2E70
print(f"code {code_lin:#x} ds {ds:#x}", flush=True)

BP_ENTRY = code_lin + 0x2630
BP_PUSH  = code_lin + 0x2da0
gdb.set_breakpoint(BP_ENTRY)
gdb.continue_()

fills = 0
out = open("/tmp/native_pushes.tsv", "w")
while True:
    r = gdb.wait_for_stop(timeout=25.0)
    if not r:
        print("timed out waiting", flush=True); break
    regs = gdb.read_registers()
    if regs.eip == BP_ENTRY:
        fills += 1
        print("fill", fills, flush=True)
        if fills == TARGET_FILL:
            gdb.set_breakpoint(BP_PUSH)
        elif fills == TARGET_FILL + 1:
            print("target fill done", flush=True)
            break
    elif regs.eip == BP_PUSH:
        st = gdb.read_memory(ds + 0x9D36, 2)
        y = st[0]
        b0d0 = gdb.read_memory(ds + 0xB0D0, 3)   # b0d0, head, tail
        b1d4 = gdb.read_memory(ds + 0xB1D4, 1)[0]
        cur = gdb.read_memory(ds + 0x9D52, 2)
        out.write(f"PUSH y={y} l={b0d0[0]}.{b1d4} r={cur[0]}.{cur[1]} d={regs.esi & 0xff} h={b0d0[1]} t={b0d0[2]}\n")
    gdb.step()
    gdb.continue_()
out.close()
dbx.stop()
print("done", flush=True)
