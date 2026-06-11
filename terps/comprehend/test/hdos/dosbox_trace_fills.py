#!/usr/bin/env python3
"""Capture VRAM before every op14 PAINT during the Talisman DOS title draw.

Drives DOSBox-X via the dosbox-x-remotedebug GDB stub + QMP directly.
Outputs /tmp/nativetrace/fill_<seq>_<imgoff>.vram (16 KB CGA dumps, state
BEFORE that fill executes) plus manifest.tsv with seed info per fill.
"""
import sys, os, time
sys.path.insert(0, "/Users/administrator/dosbox-x-remotedebug/tests/integration")
from dosbox_debug import DOSBoxInstance

OUT = "/tmp/nativetrace"
SIG = bytes.fromhex("e8dd07803e96a2017501c33a1e5a9d77")  # PicOp14Paint @ CS:2630

os.makedirs(OUT, exist_ok=True)

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
            lines = gdb.screen_dump()
            if any(needle in l for l in lines):
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
print("dosbox up", flush=True)

# The stub may auto-halt on connect: resume so DOS can boot.
try:
    gdb.continue_()
except Exception:
    pass

assert wait_text(gdb, "A:\\"), "no A:\\ prompt"
print("at A:\\ prompt", flush=True)

# Halt at the program's entry point so breakpoints can be armed before the
# title draws.
qmp.debug_break_on_exec(True)
qmp.type_text("novel1\r", delay=0.05)
stop = gdb.wait_for_stop(timeout=20.0)
print("break-on-exec stop:", stop, flush=True)
qmp.debug_break_on_exec(False)

regs = gdb.read_registers()
print(f"entry cs={regs.cs:#x} eip={regs.eip:#x}", flush=True)

# Find the interpreter code: scan around the load address for the op14 entry.
code_lin = None
scan_base = max(0x600, (regs.eip & ~0xFFF) - 0x2000)
for base in range(scan_base, scan_base + 0x40000, 0x1000):
    blob = read_big(gdb, base, 0x1000 + 16)
    i = blob.find(SIG)
    if i >= 0:
        code_lin = base + i - 0x2630
        break
assert code_lin is not None, "interpreter signature not found"
ds_lin = code_lin + 0x2E70
print(f"code base {code_lin:#x}, DS base {ds_lin:#x}", flush=True)

BP_ENTRY = code_lin + 0x2630   # op14 entry (coords not read yet)
BP_SEED  = code_lin + 0x2660   # after coord read + seed-is-white test
gdb.set_breakpoint(BP_ENTRY)
gdb.set_breakpoint(BP_SEED)
gdb.continue_()
print("tracing...", flush=True)

manifest = open(f"{OUT}/manifest.tsv", "w")
manifest.write("seq\timgoff\tkind\ty\tB\tP\tsub_odd\tsub_even\n")
seq = 0
stream_base = None     # learned at first hit: BP register maps to img offset
last_imgoff = -1

while True:
    try:
        r = gdb.wait_for_stop(timeout=25.0)
    except Exception:
        r = None
    if not r:
        print("no more stops; done", flush=True)
        break
    regs = gdb.read_registers()
    eip = regs.eip
    if eip == BP_ENTRY:
        bp = regs.ebp
        if stream_base is None:
            stream_base = bp - 1 - 0x21d   # first PAINT opcode in title is at 0x21d
        imgoff = bp - 1 - stream_base
        last_imgoff = imgoff
        seq += 1
        vram = read_big(gdb, 0xB8000, 0x4000)
        with open(f"{OUT}/fill_{seq:03d}_{imgoff:04x}.vram", "wb") as f:
            f.write(vram)
        manifest.write(f"{seq}\t{imgoff:#x}\tentry\t\t\t\t\t\n")
        manifest.flush()
        print(f"fill #{seq} imgoff {imgoff:#x}", flush=True)
    elif eip == BP_SEED:
        st = gdb.read_memory(ds_lin + 0x9D36, 2)
        y = st[0] | (st[1] << 8)
        bpx = gdb.read_memory(ds_lin + 0x9D52, 2)
        subs = gdb.read_memory(ds_lin + 0x9D55, 2)
        manifest.write(f"{seq}\t{last_imgoff:#x}\tseeded\t{y}\t{bpx[0]}\t{bpx[1]}\t{subs[0]}\t{subs[1]}\n")
        manifest.flush()
    else:
        print(f"unexpected stop at {eip:#x}", flush=True)
        gdb.step(); gdb.continue_()
        continue
    gdb.step()
    gdb.continue_()
    if eip == BP_ENTRY and last_imgoff > 0x1014:   # past the last PAINT in title
        print("past last title PAINT; finishing", flush=True)
        break

# final state (let the rest of the title finish drawing first)
time.sleep(8.0)
gdb.halt()
vram = read_big(gdb, 0xB8000, 0x4000)
with open(f"{OUT}/final.vram", "wb") as f:
    f.write(vram)
manifest.close()
gdb.continue_()
dbx.stop()
print("trace complete:", seq, "fills", flush=True)
