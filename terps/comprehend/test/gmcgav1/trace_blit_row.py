#!/usr/bin/env python3
"""Single-step trace of the crypt brush blitter (OVR 0x88c) on row 25.

Resolves TODO blocker #1: the static disasm of 0x88c/0x94c says the spill byte
gb+2 (=byte52) gets pattern phase (gb+2)&3 == byte&3 == 0 (cyan), yet the live
store recorded DI=55 (phase 3, magenta).  This dumps, for every store that lands
on row-25 bytes 50..53, the ordered (store, BX→byte, DI, pattern[DI], before->after,
gb=[0x9382], idx4=[0x9380]) so the BX/DI walk is pinned exactly.

Mirrors find_magenta_op.py's boot (NOVEL1.EXE -> crypt RA pic 3, id 4); arms the
two store breakpoints only at brush hit 256 to keep GDB round-trips cheap.
"""
import sys, os, time
sys.path.insert(0, "/Users/administrator/dosbox-x-remotedebug/tests/integration")

GAME = "/Users/administrator/Downloads/comprehend games/crimson-crown"
CONF = "/tmp/v1_blitrow.conf"
SIG  = bytes.fromhex("525657 8d1ef00d 891eec90 0441 a2362c".replace(" ", ""))
DRAW_OFF = 0x0c4c
CODE_TO_DS = 0x3900
ENTRY_ROOM = 0x0b9f
PIC_ID = 0 * 16 + 3 + 1            # crypt = RA pic 3 -> linear id 4
OVR_LOAD_OFF = 0x2de7

ROW25_BASE = 0xB8000 + 0x2000 + (25 >> 1) * 80   # odd-row bank + (25>>1)*80
WATCH = {ROW25_BASE + c: c for c in (49, 50, 51, 52, 53)}


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
    brush_lin = code_lin + OVR_LOAD_OFF + 0x88c
    store1_lin = code_lin + OVR_LOAD_OFF + 0x90b   # first byte store
    storeS_lin = code_lin + OVR_LOAD_OFF + 0x96a   # COLWRITE spill store
    addbx_lin = code_lin + OVR_LOAD_OFF + 0x8e7
    print(f"code {code_lin:#x} ds {ds_lin:#x} brush {brush_lin:#x} "
          f"st1 {store1_lin:#x} stS {storeS_lin:#x}", flush=True)

    def gbyte(off):
        return gdb.read_memory(ds_lin + off, 1)[0]
    def patbyte(di):
        return gdb.read_memory(ds_lin + 0x9111 + (di & 0xFFFF), 1)[0]

    regs = gdb.read_registers(); march = regs.eip
    gdb.write_memory(ds_lin + 0x2439, b"\x00")
    rel = (ENTRY_ROOM - ((march - code_lin) + 5)) & 0xFFFF
    gdb.write_memory(march, bytes([0xB0, PIC_ID & 0xFF, 0xE8, rel & 0xFF, (rel >> 8) & 0xFF]))

    gdb.set_breakpoint(march + 5)
    gdb.set_breakpoint(brush_lin)
    gdb.continue_()

    hit = 0
    armed = False
    while True:
        r = gdb.wait_for_stop(timeout=30)
        assert r, "stalled"
        eip = gdb.read_registers().eip
        if eip == march + 5:
            break
        if eip == brush_lin:
            hit += 1
            if hit == 256 and not armed:
                gdb.set_breakpoint(store1_lin)
                gdb.set_breakpoint(storeS_lin)
                gdb.set_breakpoint(addbx_lin)
                armed = True
            gdb.step(); gdb.continue_(); continue
        if eip == addbx_lin and armed:
            r = gdb.read_registers()
            bx = r.ebx & 0xFFFF
            lin = r.es * 16 + bx
            # row-base for row25 in VRAM is ROW25_BASE; ES:BX is the absolute VRAM byte
            y = gbyte(0x9366)
            if y == 25:
                print(f"  [8e7 row25] BX={bx:#06x} ES:BX={lin:#x} "
                      f"gb={gbyte(0x9382)} sub={gbyte(0x9383)} idx4={gbyte(0x9380)} "
                      f"col={lin-ROW25_BASE}", flush=True)
            gdb.step(); gdb.continue_(); continue
        if eip in (store1_lin, storeS_lin) and armed:
            r = gdb.read_registers()
            lin = r.es * 16 + (r.ebx & 0xFFFF)
            if lin in WATCH:
                di = r.edi & 0xFFFF
                before = gdb.read_memory(lin, 1)[0]
                pat = patbyte(di)
                which = "1st " if eip == store1_lin else "spill"
                gdb.step()
                after = gdb.read_memory(lin, 1)[0]
                print(f"  STORE {which} col={WATCH[lin]:2d} BX={r.ebx&0xFFFF:#06x} "
                      f"DI={di} pat[DI]={pat:#04x} {before:#04x}->{after:#04x} "
                      f"DX={r.edx&0xFFFF:#06x} gb={gbyte(0x9382)} idx4={gbyte(0x9380)}",
                      flush=True)
                gdb.continue_(); continue
            gdb.step(); gdb.continue_(); continue
        gdb.step(); gdb.continue_()

    print("done", flush=True)
    try: dbx.stop()
    except Exception: pass


if __name__ == "__main__":
    main()
