#!/usr/bin/env python3
"""Find which native op paints crypt's (188,25) magenta (TODO residual #1).

Fill-by-fill BEFORE/AFTER proved every op14 flood fill is pixel-exact, and the
state right AFTER the last fill (fill 13) has (188,25)=cyan in BOTH native and
renderer.  The committed/native FINAL has it magenta -- so a later op (#154..)
paints it.  This breakpoints the brush blitter (OVR 0x88c) and the line plotter
and, after fill 13, reports each hit's effect on the target VRAM byte so we can
name the op family (brush op12 vs line op10) and its coordinates.
"""
import sys, os, time
sys.path.insert(0, "/Users/administrator/dosbox-x-remotedebug/tests/integration")

GAME = "/Users/administrator/Downloads/comprehend games/crimson-crown"
CONF = "/tmp/v1_magenta.conf"
SIG  = bytes.fromhex("525657 8d1ef00d 891eec90 0441 a2362c".replace(" ", ""))
DRAW_OFF = 0x0c4c
CODE_TO_DS = 0x3900
ENTRY_ROOM = 0x0b9f
PIC_ID = 0 * 16 + 3 + 1            # crypt = RA pic 3 -> linear id 4
OUT_PAT = "/tmp/live_pat.bin"
OUT_SUB = "/tmp/live_sub.bin"
OVR_LOAD_OFF = 0x2de7
BRUSH_OVR = 0x88c                  # PicBlitBrushColumn (per residual #2 RE)
# target byte: row 25 (odd) byte 52  -> CGA 0x2000 + (25>>1)*80 + 52
TGT = 0xB8000 + 0x2000 + (25 >> 1) * 80 + 52


def read_big(gdb, addr, size, chunk=1024):
    out = b""
    while size > 0:
        n = min(chunk, size); out += gdb.read_memory(addr, n); addr += n; size -= n
    return out


def main():
    from dosbox_debug import DOSBoxInstance
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
    dbx = DOSBoxInstance(executable="/Users/administrator/dosbox-x-remotedebug/src/dosbox-x",
                         config=CONF, working_dir="/Users/administrator/dosbox-x-remotedebug")
    dbx.start()
    gdb, qmp = dbx.gdb, dbx.qmp
    try: gdb.continue_()
    except Exception: pass

    def wait_text(needle, timeout=30):
        t0 = time.time()
        while time.time() - t0 < timeout:
            try:
                if any(needle in l for l in gdb.screen_dump()): return True
            except Exception: pass
            time.sleep(0.4)
        return False

    assert wait_text("A:\\"), "no A: prompt"
    qmp.type_text("novel1\r", delay=0.05); time.sleep(5)
    qmp.type_text(" ", delay=0.05); time.sleep(4)
    gdb.halt()

    code_lin = None
    for base in range(0x600, 0xA0000, 0x1000):
        try: blob = read_big(gdb, base, 0x1000 + 16)
        except Exception: continue
        i = blob.find(SIG)
        if i >= 0: code_lin = base + i - DRAW_OFF; break
    assert code_lin is not None, "code sig not found"
    ds_lin = code_lin + CODE_TO_DS
    brush_lin = code_lin + OVR_LOAD_OFF + BRUSH_OVR
    print(f"code {code_lin:#x} ds {ds_lin:#x} brush {brush_lin:#x} tgt {TGT:#x}", flush=True)

    def gword(off):
        b = gdb.read_memory(ds_lin + off, 2); return b[0] | (b[1] << 8)
    def gbyte(off):
        return gdb.read_memory(ds_lin + off, 1)[0]
    def tgtpix():
        b = gdb.read_memory(TGT, 1)[0]
        return (b >> 6) & 3, b           # MSB pair = x188 ; raw byte

    # Dump the LIVE pattern table (DGROUP 0x9111, 30*4 bytes) + subindex
    # (0x9189, 109 words) so we can diff against the committed OVR slice.
    patlive = read_big(gdb, ds_lin + 0x9111, 30 * 4)
    sublive = read_big(gdb, ds_lin + 0x9189, 109 * 2)
    print("LIVE pat[13] =", [hex(b) for b in patlive[13 * 4:13 * 4 + 4]], flush=True)
    print("LIVE pat[5]  =", [hex(b) for b in patlive[5 * 4:5 * 4 + 4]], flush=True)
    print("LIVE sub[71] =", sublive[71 * 2], sublive[71 * 2 + 1], flush=True)
    open(OUT_PAT, "wb").write(patlive); open(OUT_SUB, "wb").write(sublive)

    regs = gdb.read_registers(); march = regs.eip
    gdb.write_memory(ds_lin + 0x2439, b"\x00")
    cur_off = march - code_lin
    rel = (ENTRY_ROOM - (cur_off + 5)) & 0xFFFF
    gdb.write_memory(march, bytes([0xB0, PIC_ID & 0xFF, 0xE8, rel & 0xFF, (rel >> 8) & 0xFF]))
    # Store instructions inside the blitter: 0x90b = 1st byte, 0x96a = spill
    # bytes (2nd/3rd) in FUN_094c. Arm these to catch the exact write to TGT.
    store1_lin = code_lin + OVR_LOAD_OFF + 0x90b
    store2_lin = code_lin + OVR_LOAD_OFF + 0x96a
    stores_armed = [False]

    def regs_all():
        r = gdb.read_registers()
        return r

    gdb.set_breakpoint(march + 5)
    gdb.set_breakpoint(brush_lin)
    gdb.continue_()

    nm = "kcmw"
    last = None
    hit = 0
    prev = None          # state recorded at the PREVIOUS brush entry
    while True:
        r = gdb.wait_for_stop(timeout=30)
        assert r, "stalled"
        eip = gdb.read_registers().eip
        if eip == march + 5:
            break
        if eip in (store1_lin, store2_lin):
            # a blitter store: is it writing row 25 bytes 50/51/52?
            r = gdb.read_registers()
            lin = r.es * 16 + (r.ebx & 0xFFFF)
            ROWBYTES = {0xB8000 + 0x23C0 + 50, 0xB8000 + 0x23C0 + 51, TGT}
            if lin in ROWBYTES and stores_armed[0]:
                di = r.edi & 0xFFFF
                patbyte = gdb.read_memory(ds_lin + 0x9111 + di, 1)[0]
                bcol = lin - (0xB8000 + 0x23C0)
                print(f"  row25 byte{bcol} {'1st' if eip==store1_lin else 'spill'} "
                      f"DI={di} pat={patbyte:#04x} DX={r.edx&0xFFFF:#06x} "
                      f"gb={gbyte(0x9382)}", flush=True)
                gdb.step(); gdb.continue_(); continue
            if lin == TGT:
                before = gdb.read_memory(TGT, 1)[0]
                di = r.edi & 0xFFFF
                dx = r.edx & 0xFFFF
                patbyte = gbyte(di & 0xFFFF) if False else \
                    gdb.read_memory(ds_lin + 0x9111 + di, 1)[0]
                gdb.step()
                after = gdb.read_memory(TGT, 1)[0]
                which = "1st" if eip == store1_lin else "spill"
                print(f"  STORE@{which} TGT {before:#04x}->{after:#04x} "
                      f"DI={di} pat[DI]={patbyte:#04x} DX={dx:#06x} "
                      f"idx4={gbyte(0x9380)} gb={gbyte(0x9382)} row={gbyte(0x9366)}",
                      flush=True)
                gdb.continue_()
            else:
                gdb.step(); gdb.continue_()
            continue
        if eip == brush_lin:
            hit += 1
            v, raw = tgtpix()
            if hit == 256 and not stores_armed[0]:
                gdb.set_breakpoint(store1_lin); gdb.set_breakpoint(store2_lin)
                stores_armed[0] = True
            # state of the brush ABOUT to run (entry of 0x88c)
            st = dict(hit=hit, gb=gbyte(0x9382), sub=gbyte(0x9383),
                      idx4=gbyte(0x9380), row=gbyte(0x9366), xor=gbyte(0x9381),
                      sel=gbyte(0x9376), oddi=gbyte(0x9385), eveni=gbyte(0x9386),
                      x1=gword(0x9360), y1=gword(0x9362), pix=nm[v])
            if v != last:
                # the flip happened during the PREVIOUS stamp; print both
                print(f">>> (188,25) {nm[last] if last else '?'}->{nm[v]} "
                      f"raw={raw:#04x}", flush=True)
                if prev:
                    print(f"    PAINTER stamp #{prev['hit']}: gb={prev['gb']} "
                          f"sub={prev['sub']} idx4={prev['idx4']} row={prev['row']} "
                          f"XOR={prev['xor']} sel={prev['sel']} odd={prev['oddi']} "
                          f"even={prev['eveni']} x1={prev['x1']} y1={prev['y1']} "
                          f"(pix@entry={prev['pix']})", flush=True)
                print(f"    next    stamp #{st['hit']}: gb={st['gb']} sub={st['sub']} "
                      f"idx4={st['idx4']} row={st['row']} x1={st['x1']}", flush=True)
                last = v
            prev = st
            gdb.step(); gdb.continue_()
        else:
            gdb.step(); gdb.continue_()

    v, raw = tgtpix()
    print(f"FINAL (188,25)={nm[v]} raw={raw:#04x} after {hit} brush hits", flush=True)
    try: dbx.stop()
    except Exception: pass


if __name__ == "__main__":
    main()
