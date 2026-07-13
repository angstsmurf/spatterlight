#!/usr/bin/env python3
"""Capture the committed make-test fixture goldens for the v1 CGA renderer.

Re-runnable distillation of dosbox_dump_all_v1.py (see that script and memory
comprehend-v1-dumpall-harness for the RE of the entry points): boots NOVEL1.EXE
under DOSBox-X (CGA machine), dumps the title screen VRAM at boot, then drives
the high-level picture entry points (FUN_1000_0b9f room / 0bc3 object) with a
marching MOV AL,id; CALL stub to capture the text-cutscene and object-composite
fixtures.  Goldens are raw 16 KB CGA VRAM (0xB8000), the format
test/test_gmcgav1_pics.cpp unpacks.

  MODE=cc  python3 dosbox_capture_fixtures.py   -> cc_title.fb rc_05.fb oa_01.fb
  MODE=tr  python3 dosbox_capture_fixtures.py   -> tr_title.fb   (TR's own boot)
  MODE=trmerged python3 dosbox_capture_fixtures.py -> tr_rc09.fb (CC engine +
      TR .MS1 images on a merged mount; TR's own NOVEL1.EXE stalls on a
      name-entry prompt, and the two engines' drawing tables are byte-identical)

Output dir: V1OUT (default /tmp/v1fix).
"""
import sys, os, time, shutil, glob
sys.path.insert(0, "/Users/administrator/dosbox-x-remotedebug/tests/integration")

CC   = os.environ.get("V1CC", "/Users/administrator/Downloads/comprehend games/crimson-crown")
TR   = os.environ.get("V1TR", "/Users/administrator/Downloads/comprehend games/transylvania")
OUT  = os.environ.get("V1OUT", "/tmp/v1fix")
MODE = os.environ.get("MODE", "cc")
CONF = "/tmp/v1_fixture_capture.conf"

# FUN_1000_0c4c opening bytes (load routine) -- locates the code base at runtime.
SIG        = bytes.fromhex("5256578d1ef00d891eec900441a2362c")
DRAW_OFF   = 0x0c4c
CODE_TO_DS = 0x3900
ENTRY_ROOM = 0x0b9f
ENTRY_OBJ  = 0x0bc3
G_2439     = 0x2439           # picture-id byte (dedupe guard)


def read_big(gdb, addr, size, chunk=1024):
    out = b""
    while size > 0:
        n = min(chunk, size)
        out += gdb.read_memory(addr, n); addr += n; size -= n
    return out


def main():
    from dosbox_debug import DOSBoxInstance
    os.makedirs(OUT, exist_ok=True)

    if MODE == "cc":
        mount = CC
    elif MODE == "tr":
        mount = TR
    elif MODE == "trmerged":
        mount = "/tmp/v1_merged_mount"
        if os.path.isdir(mount):
            shutil.rmtree(mount)
        shutil.copytree(CC, mount)
        for p in glob.glob(os.path.join(TR, "*.MS1")) + \
                 glob.glob(os.path.join(TR, "*.ms1")):
            shutil.copy(p, mount)
    else:
        raise SystemExit(f"bad MODE {MODE}")

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
MOUNT A "{mount}" -t floppy
A:
""")
    dbx = DOSBoxInstance(
        executable="/Users/administrator/dosbox-x-remotedebug/src/dosbox-x",
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

    def dump_vram():
        gdb.halt()
        v = read_big(gdb, 0xB8000, 0x4000)
        gdb.continue_()
        return v

    def save(name, vram):
        open(os.path.join(OUT, name + ".fb"), "wb").write(vram)
        print(f"  saved {name}.fb", flush=True)

    assert wait_text("A:\\"), "no A: prompt"
    print("at A: prompt", flush=True)
    qmp.type_text("novel1\r", delay=0.05)

    # Title: wait for the draw to finish by polling VRAM until it stabilises
    # (the title paints progressively for several seconds).
    time.sleep(4)
    prev = dump_vram()
    for _ in range(20):
        time.sleep(1.5)
        cur = dump_vram()
        if cur == prev and any(cur):
            break
        prev = cur
    title_name = "cc_title" if MODE in ("cc", "trmerged") else "tr_title"
    save(title_name, cur)

    if MODE == "tr":                     # only the title needed from TR's boot
        try: dbx.stop()
        except Exception: pass
        print("done", flush=True)
        return

    qmp.type_text(" ", delay=0.05)       # leave title -> start room
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
    regs = gdb.read_registers()
    print(f"code base {code_lin:#x}, DS {ds_lin:#x}, eip {regs.eip:#x}", flush=True)

    def preclear():
        for i in range(0, 0x4000, 1024):
            gdb.write_memory(0xB8000 + i, b"\x00" * 1024)

    cur_march = [regs.eip]

    def draw_id(entry_off, pic_id):
        gdb.write_memory(ds_lin + G_2439, b"\x00")
        march = cur_march[0]
        rel = (entry_off - ((march - code_lin) + 5)) & 0xFFFF
        stub = bytes([0xB0, pic_id & 0xFF, 0xE8, rel & 0xFF, (rel >> 8) & 0xFF])
        gdb.write_memory(march, stub)
        gdb.set_breakpoint(march + 5)
        gdb.continue_()
        gdb.wait_for_stop(timeout=30)
        gdb.remove_breakpoint(march + 5)
        r = gdb.read_registers()
        assert r.eip == march + 5, f"stub ended at {r.eip:#x}, want {march+5:#x}"
        cur_march[0] = march + 5

    if MODE == "cc":
        # RC pic 5 (sage text cutscene): room id = file 2 ('C') * 16 + 5 + 1
        preclear()
        draw_id(ENTRY_ROOM, 2 * 16 + 5 + 1)
        save("rc_05", read_big(gdb, 0xB8000, 0x4000))
        # OA pic 1 (id 2) over its placed room RA pic 13 (id 14), no preclear
        # between: the object's flood fills are bounded by the room geometry.
        preclear()
        draw_id(ENTRY_ROOM, 14)
        draw_id(ENTRY_OBJ, 2)
        save("oa_01", read_big(gdb, 0xB8000, 0x4000))
    else:                                # trmerged
        # TR RC pic 9 (Zin text page): room id 42
        preclear()
        draw_id(ENTRY_ROOM, 2 * 16 + 9 + 1)
        save("tr_rc09", read_big(gdb, 0xB8000, 0x4000))

    try: dbx.stop()
    except Exception: pass
    print("done", flush=True)


if __name__ == "__main__":
    main()
