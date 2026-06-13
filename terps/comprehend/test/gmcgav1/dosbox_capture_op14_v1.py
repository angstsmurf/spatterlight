#!/usr/bin/env python3
"""Live per-op14 VRAM capture for the v1 Comprehend DOS (CGA) flood fill.

Goal: pin down the flood-fill span-boundary residual (TODO_v1_cga_graphics.md
"OPEN: flood-fill span-boundary residual").  The native interpreter and our
graphics_magician_cga.cpp port agree on every primitive *except* a few-pixel
sliver at one dither boundary on odd rows.  Static analysis of PC_GRAPH.OVR's
706-byte op14 overlap-merge block ruled out phase/parity, leaving a span-queue
*traversal-order* corner.  This harness records the native's VRAM state BEFORE
every op14 PAINT so we can diff fill-by-fill against the renderer's
GMCGA_TRACE_DIR snapshots (see compare_op14.py) and find the exact fill (and
boundary word) where the two diverge.

Method mirrors dosbox_dump_all_v1.py: boot CC's NOVEL1.EXE, locate the code
base by the FUN_1000_0c4c signature, then drive a high-level draw entry point
(FUN_1000_0b9f room / 0bc3 object) for ONE target picture via an injected stub.
The difference: before kicking the draw we arm a breakpoint at op14_paint inside
the loaded PC_GRAPH.OVR overlay (it shares NOVEL.EXE's CS at load offset
0x2de7; op14_paint is OVR offset 0x4d3 -> CS 0x32ba), and on every hit we dump
0xB800 and the seed/fill DGROUP state.

  TARGET=RB:3   (file letter : pic index)   -- default; the clean RB_03 case
  BASE=RA:0     optional room base for an object target (drawn untraced first)
  V1OUT=/tmp/op14cap

Outputs <OUT>/fill_NNN.vram (16 KB raw CGA, state BEFORE fill N) + manifest.tsv.
"""
import sys, os, time
sys.path.insert(0, "/Users/administrator/dosbox-x-remotedebug/tests/integration")

GAME   = os.environ.get("V1GAME", "/Users/administrator/Downloads/comprehend games/crimson-crown")
OUT    = os.environ.get("V1OUT", "/tmp/op14cap")
TARGET = os.environ.get("TARGET", "RB:3")     # file-letter:pic  (room picture)
BASE   = os.environ.get("BASE", "")           # optional room base for an object
CONF   = "/tmp/v1_op14.conf"

# FUN_1000_0c4c signature (load+draw); same as dosbox_dump_all_v1.py.
SIG        = bytes.fromhex("525657 8d1ef00d 891eec90 0441 a2362c".replace(" ", ""))
DRAW_OFF   = 0x0c4c
CODE_TO_DS = 0x3900                 # DGROUP(0x1390) - CODE(0x1000) = 0x3900 lin
G_2439     = 0x2439                 # picture-id dedupe guard
ENTRY_ROOM = 0x0b9f
ENTRY_OBJ  = 0x0bc3
IMAGES_PER_FILE = 16

# op14_paint: PC_GRAPH.OVR loads into NOVEL.EXE's CS at offset 0x2de7; the
# handler is at OVR offset 0x4d3 -> CS offset 0x32ba.  Position-independent
# opening bytes (the e8 calls are CS-relative, so stable) as a sanity check.
OVR_LOAD_OFF = 0x2de7
OP14_OVR_OFF = 0x04d3
OP14_CS_OFF  = OVR_LOAD_OFF + OP14_OVR_OFF      # 0x32ba
OP14_SIG     = bytes.fromhex("e8b9fd81fba0007205754abb9f00891e6693e85cfbe884fb")

# DGROUP fill state (shared NOVEL.EXE/OVR), from comprehend-v1-cga-graphics:
G_X1, G_Y1 = 0x9360, 0x9362        # cur x / y (seed after coord read)
G_X2, G_Y2 = 0x9364, 0x9366        # x2 / y2 (span endpoints)
G_PEN      = 0x9368                 # pen value (2bpp)
G_FILLSEL  = 0x9376                 # fill selector
G_PATEVEN  = 0x9386                 # even-row pattern idx
G_PATODD   = 0x9385                 # odd-row pattern idx


def ms1_npics(path):
    import struct
    d = open(path, "rb").read()
    if len(d) < 38:
        return 0
    w = struct.unpack("<17H", d[4:4 + 34])
    n = 0
    for i in range(16):
        if w[i + 1] > w[i] and w[i + 1] <= len(d):
            n += 1
        else:
            break
    return n


def read_big(gdb, addr, size, chunk=1024):
    out = b""
    while size > 0:
        n = min(chunk, size)
        out += gdb.read_memory(addr, n)
        addr += n
        size -= n
    return out


def parse_target(spec):
    """'RB:3' -> (entry, pic_id, name).  Letter prefix R=room O=object."""
    file_tag, pic = spec.split(":")
    pic = int(pic)
    kind = file_tag[0].upper()          # 'R' or 'O'
    letter = file_tag[1].upper()
    fi = ord(letter) - ord('A')
    pic_id = fi * IMAGES_PER_FILE + pic + 1
    entry = ENTRY_OBJ if kind == 'O' else ENTRY_ROOM
    name = f"{kind}{letter}_{pic:02d}"
    return entry, pic_id, name


def main():
    from dosbox_debug import DOSBoxInstance
    os.makedirs(OUT, exist_ok=True)

    open(CONF, "w").write(f"""[sdl]
autolock=false
[dosbox]
quit warning=false
gdbserver=true
gdbserver port=2159
qmpserver=true
qmpserver port=4444
[machine]
machine=cga
[autoexec]
MOUNT A "{GAME}" -t floppy
A:
""")
    dbx = DOSBoxInstance(
        executable="/Users/administrator/dosbox-x-remotedebug/src/dosbox-x",
        config=CONF, working_dir="/Users/administrator/dosbox-x-remotedebug")
    dbx.start()
    gdb, qmp = dbx.gdb, dbx.qmp
    try:
        gdb.continue_()
    except Exception:
        pass

    def wait_text(needle, timeout=30):
        t0 = time.time()
        while time.time() - t0 < timeout:
            try:
                if any(needle in l for l in gdb.screen_dump()):
                    return True
            except Exception:
                pass
            time.sleep(0.4)
        return False

    assert wait_text("A:\\"), "no A: prompt"
    print("at A: prompt", flush=True)
    qmp.type_text("novel1\r", delay=0.05)
    time.sleep(5)                       # title draws (loads PC_GRAPH.OVR)
    qmp.type_text(" ", delay=0.05)      # leave title -> start room
    time.sleep(4)

    gdb.halt()
    code_lin = None
    for base in range(0x600, 0xA0000, 0x1000):
        try:
            blob = read_big(gdb, base, 0x1000 + 16)
        except Exception:
            continue
        i = blob.find(SIG)
        if i >= 0:
            code_lin = base + i - DRAW_OFF
            break
    assert code_lin is not None, "FUN_1000_0c4c signature not found"
    ds_lin = code_lin + CODE_TO_DS
    print(f"code base {code_lin:#x}, DS base {ds_lin:#x}", flush=True)

    # Resolve op14_paint and verify the overlay is where we expect.
    op14_lin = code_lin + OP14_CS_OFF
    got = gdb.read_memory(op14_lin, len(OP14_SIG))
    if got != OP14_SIG:
        print(f"op14 @ {op14_lin:#x} signature MISMATCH: {got.hex()}", flush=True)
        print("scanning for op14_paint signature...", flush=True)
        op14_lin = None
        for base in range(code_lin, code_lin + 0x40000, 0x1000):
            blob = read_big(gdb, base, 0x1000 + len(OP14_SIG))
            j = blob.find(OP14_SIG)
            if j >= 0:
                op14_lin = base + j
                break
        assert op14_lin is not None, "op14_paint signature not found anywhere"
        print(f"  found op14_paint at {op14_lin:#x} "
              f"(CS off {op14_lin - code_lin:#x})", flush=True)
    else:
        print(f"op14_paint at {op14_lin:#x} (CS off {OP14_CS_OFF:#x}) verified",
              flush=True)

    def gread(off, n):
        return read_big(gdb, ds_lin + off, n)

    def gword(off):
        b = gread(off, 2)
        return b[0] | (b[1] << 8)

    def gword_abs(g, lin):
        b = g.read_memory(lin, 2)
        return b[0] | (b[1] << 8)

    regs = gdb.read_registers()
    march = regs.eip                   # flat linear stub site (input-wait EIP)
    print(f"march/stub base = {march:#x}", flush=True)

    def draw_blocking(entry_off, pic_id, trace):
        """Plant `MOV AL,id; CALL entry` at `march`, run to march+5.

        If `trace`, stop on every op14_paint hit in between: dump VRAM + state.
        Returns the number of fills traced.
        """
        gdb.write_memory(ds_lin + G_2439, b"\x00")     # force redraw
        cur_off = march - code_lin
        rel = (entry_off - (cur_off + 5)) & 0xFFFF
        stub = bytes([0xB0, pic_id & 0xFF, 0xE8, rel & 0xFF, (rel >> 8) & 0xFF])
        gdb.write_memory(march, stub)
        gdb.set_breakpoint(march + 5)
        if trace:
            gdb.set_breakpoint(op14_lin)
        gdb.continue_()

        nfills = 0
        while True:
            r = gdb.wait_for_stop(timeout=30)
            assert r, "draw stalled (no stop)"
            eip = gdb.read_registers().eip
            if eip == march + 5:
                break
            if trace and eip == op14_lin:
                nfills += 1
                # BEFORE-state: the canvas op14_paint is about to fill.
                vram = read_big(gdb, 0xB8000, 0x4000)
                open(f"{OUT}/fill_{nfills:03d}.vram", "wb").write(vram)
                rec = (f"{nfills}\t{gword(G_X1)}\t{gword(G_Y1)}\t"
                       f"{gword(G_X2)}\t{gword(G_Y2)}\t"
                       f"{gread(G_PEN,1)[0]}\t{gread(G_FILLSEL,1)[0]}\t"
                       f"{gread(G_PATEVEN,1)[0]}\t{gread(G_PATODD,1)[0]}\n")
                manifest.write(rec)
                manifest.flush()
                print(f"  fill #{nfills}  seed=({gword(G_X1)},{gword(G_Y1)}) "
                      f"sel={gread(G_FILLSEL,1)[0]} "
                      f"pat e/o={gread(G_PATEVEN,1)[0]}/{gread(G_PATODD,1)[0]}",
                      flush=True)
                # AFTER-state: run to op14_paint's return (near-call, so [SS:SP]
                # holds the return CS-offset at function entry), dump again.
                r0 = gdb.read_registers()
                ret_off = gword_abs(gdb, r0.ss * 16 + (r0.esp & 0xFFFF))
                ret_lin = code_lin + ret_off
                gdb.step()                         # off the op14 breakpoint
                gdb.set_breakpoint(ret_lin)
                gdb.continue_()
                rr = gdb.wait_for_stop(timeout=30)
                assert rr, "fill did not return"
                gdb.remove_breakpoint(ret_lin)
                after = read_big(gdb, 0xB8000, 0x4000)
                open(f"{OUT}/fill_{nfills:03d}_after.vram", "wb").write(after)
                gdb.step()                         # off the return breakpoint
                gdb.continue_()
            else:
                print(f"  unexpected stop {eip:#x}", flush=True)
                gdb.step()
                gdb.continue_()

        gdb.remove_breakpoint(march + 5)
        if trace:
            gdb.remove_breakpoint(op14_lin)
        return nfills

    entry, pic_id, name = parse_target(TARGET)
    print(f"target {name} (entry {entry:#x} id {pic_id})", flush=True)

    manifest = open(f"{OUT}/manifest.tsv", "w")
    manifest.write("fill\tx1\ty1\tx2\ty2\tpen\tsel\tpat_even\tpat_odd\n")

    # Optional room base for an object target -- drawn first, untraced, so its
    # geometry bounds the object's fills (matches GMCGA_BASE_MS1 in the test).
    if BASE:
        bentry, bid, bname = parse_target("R" + BASE if not BASE[0].isalpha()
                                          else BASE)
        draw_blocking(ENTRY_ROOM, bid, trace=False)
        print(f"drew base {BASE}", flush=True)

    n = draw_blocking(entry, pic_id, trace=True)
    manifest.close()

    # Final fully-drawn frame (state AFTER the last fill + any later primitives).
    final = read_big(gdb, 0xB8000, 0x4000)
    open(f"{OUT}/final.vram", "wb").write(final)
    print(f"captured {n} fills + final.vram for {name} -> {OUT}", flush=True)
    try:
        dbx.stop()
    except Exception:
        pass


if __name__ == "__main__":
    main()
