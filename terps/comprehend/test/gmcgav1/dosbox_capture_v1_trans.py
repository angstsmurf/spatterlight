#!/usr/bin/env python3
"""Capture clean CGA VRAM ground truth for the v1 Crimson Crown rooms.

Boots NOVEL1.EXE in DOSBox-X (CGA), walks to a couple of rooms by typed
commands, and dumps the raw 16 KB CGA VRAM (0xB8000) to <name>.fb -- written
directly to host disk (no copy/paste round-trip).  Pair with gmcgav1test, which
de-interleaves the .fb to 320x200 indices and compares the 280x160 window.
"""
import sys, os, time
sys.path.insert(0, "/Users/administrator/dosbox-x-remotedebug/tests/integration")

GAME = "/Users/administrator/Downloads/comprehend games/transylvania"
OUT  = os.path.dirname(os.path.abspath(__file__))
CONF = "/tmp/cc_v1_capture.conf"

# (name, [commands to reach it from the start room], settle seconds)
ROOMS = [
    ("trans1", [],            4),   # start room (after title)
    ("trans2",     ["Petter\rSanna\r"], 4),
]


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
    time.sleep(4)                       # title draws
    qmp.type_text(" ", delay=0.05)      # leave title -> start room (lakeshore)

    def dump(name):
        gdb.halt()
        vram = b""
        addr = 0xB8000
        while len(vram) < 0x4000:
            vram += gdb.read_memory(addr + len(vram), min(1024, 0x4000 - len(vram)))
        open(os.path.join(OUT, name + ".fb"), "wb").write(vram)
        print(f"  dumped {name}.fb ({len(vram)} bytes)", flush=True)
        gdb.continue_()

    for name, cmds, settle in ROOMS:
        for c in cmds:
            qmp.type_text(c, delay=0.05)
        time.sleep(settle)
        dump(name)

    try:
        dbx.stop()
    except Exception:
        pass
    print("done", flush=True)


if __name__ == "__main__":
    main()
