#!/usr/bin/env python3
"""Capture DOSBox ground-truth framebuffers for *every* Talisman DOS picture.

Instead of playing the game to each room, this drives the native picture
interpreter directly: at the input-wait loop it patches a tiny stub
(`MOV AL,G; CALL <draw>`) over the current IP and lets the engine load+draw
picture G, then dumps CGA VRAM.  The stub "marches" forward 5 bytes per
picture (we never return to the trampled wait-loop code), so no register
writes or save-states are needed -- only memory writes + breakpoints, which
the lokkju GDB stub supports.

We do NOT pre-clear VRAM: each room stream fully repaints its 280x160 window
(that's how in-game room transitions work), and pre-clearing to white instead
corrupts the white-region flood fills.  Drawing each picture over the previous
page reproduces the native result byte-for-byte (verified: redrawing the cell
over the natural cell == cell.fb at 0 diffs).

Picture numbering (from NOVEL.EXE 1cc5->1d25->1e10, see test/gmcga/TODO.md):
  G is 1-based.  file_index = ((G-1) & 0x7f) >> 4 ;  pic_index = (G-1) & 0xf
  rooms   -> CALL 0x1cc5, filename 'R'+('A'+file_index)   (RA..RG)
  objects -> CALL 0x1cf5, filename 'O'+('A'+file_index)   (OA,OB,OE,OF)
Stream bytes = file[off[pic] : off[pic+1]] where off[] is the 17-word table
in the file's first 0x22 bytes.

Outputs to OUT/: <name>.fb (320x200 palette-index bytes) + manifest.tsv.
"""
import sys, os, time
sys.path.insert(0, "/Users/administrator/dosbox-x-remotedebug/tests/integration")

GAME = "/Users/administrator/Downloads/comprehend games/talisman-challenging-the-sands-of-time"
OUT  = "/tmp/talcap"
SIG  = bytes.fromhex("e8dd07803e96a2017501c33a1e5a9d77")  # PicOp14Paint @ CS:2630
CONF = "/tmp/talisman_capture.conf"

ROOM_FILES = ["RA", "RB", "RC", "RD", "RE", "RF", "RG"]
OBJ_FILES  = ["OA", "OB", "OE", "OF"]


def enumerate_pictures():
    """Return [(kind, fileletter, fileidx, picidx, offset, length), ...]."""
    pics = []
    for kind, files in (("R", ROOM_FILES), ("O", OBJ_FILES)):
        for fn in files:
            fidx = ord(fn[1]) - ord('A')
            data = open(os.path.join(GAME, fn), "rb").read()
            tlen = data[0] | (data[1] << 8)            # = 0x22
            n = tlen // 2                              # 17 entries
            offs = [data[2*i] | (data[2*i+1] << 8) for i in range(n)]
            for pic in range(min(15, n - 1)):          # pic_index 0..14 reachable
                o, nxt = offs[pic], offs[pic+1]
                if not (0 < o < nxt <= len(data)) or nxt - o < 4:
                    continue                           # unused / sentinel slot
                pics.append((kind, fn[1], fidx, pic, o, nxt - o))
    return pics


def vram_to_fb(vram):
    """CGA mode-4 de-interleave: 16 KB VRAM -> 320x200 palette indices."""
    fb = bytearray(320 * 200)
    for y in range(200):
        base = (y & 1) * 0x2000 + (y >> 1) * 80
        o = y * 320
        for xb in range(80):
            b = vram[base + xb]
            fb[o]   = (b >> 6) & 3
            fb[o+1] = (b >> 4) & 3
            fb[o+2] = (b >> 2) & 3
            fb[o+3] = b & 3
            o += 4
    return bytes(fb)


def read_big(gdb, addr, size, chunk=1024):
    out = b""
    while size > 0:
        n = min(chunk, size)
        out += gdb.read_memory(addr, n)
        addr += n; size -= n
    return out


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
[autoexec]
MOUNT A "{GAME}" -t floppy
A:
""")
    dbx = DOSBoxInstance(
        executable="/Users/administrator/dosbox-x-remotedebug/src/dosbox-x",
        config=CONF, working_dir="/Users/administrator/dosbox-x-remotedebug")
    dbx.start()
    gdb, qmp = dbx.gdb, dbx.qmp
    print("dosbox up", flush=True)
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
    qmp.type_text(" ", delay=0.05)      # leave title -> prison cell
    time.sleep(4)                       # cell draws, engine reaches input wait

    gdb.halt()
    code_lin = None
    for base in range(0x600, 0xA0000, 0x1000):
        try:
            blob = read_big(gdb, base, 0x1000 + 16)
        except Exception:
            continue
        i = blob.find(SIG)
        if i >= 0:
            code_lin = base + i - 0x2630
            break
    assert code_lin is not None, "interpreter signature not found"
    ds_lin = code_lin + 0x2E70
    print(f"code base {code_lin:#x}, DS base {ds_lin:#x}, "
          f"gate={read_big(gdb,ds_lin+0x34ba,1).hex()}", flush=True)

    WAIT_IP = 0x207F                    # input poll site (found via Ctrl-C live)
    gdb.set_breakpoint(code_lin + WAIT_IP)
    gdb.continue_()
    gdb.wait_for_stop(timeout=20)
    gdb.remove_breakpoint(code_lin + WAIT_IP)
    regs = gdb.read_registers()
    print(f"halted at input wait, eip={regs.eip:#x} cs={regs.cs:#x}", flush=True)

    # "Marching" stub: each picture gets a fresh 5-byte stub
    #   B0 nn      MOV AL, G
    #   E8 rr rr   CALL 0x1cc5          (rel recomputed per location)
    # placed at `cur`; the CALL returns to cur+5 where we breakpoint.  We then
    # advance cur by 5 and write the next stub at the return site.  A self-loop
    # (JMP back) instead fails: this GDB stub won't reliably resume over a
    # breakpoint sitting on the loop's JMP, so only the first draw would run.
    # 105 room stubs span 0x207F..0x228C, safely below the first picture handler
    # (Circle @0x2330) -- so the march never tramples interpreter code.
    #
    # Each room is drawn over a BLACK page: full-screen rooms are authored that
    # way and reproduce the renderer (which resets to white) pixel-exactly
    # (verified: cell-over-black == cell.fb at 0 diffs).  Cutscene/overlay pics
    # want their real in-game predecessor instead and are captured via play.
    pics = [p for p in enumerate_pictures() if p[0] == "R"]
    print(f"{len(pics)} room pictures to capture", flush=True)
    manifest = open(f"{OUT}/manifest.tsv", "w")
    manifest.write("name\tkind\tfile\tpic\toffset\tlength\tG\n")

    cur = code_lin + WAIT_IP
    for (kind, letter, fidx, pic, off, length) in pics:
        G = fidx * 16 + pic + 1
        name = f"{kind}{letter}_{pic:02d}"
        cur_ip = cur - code_lin
        rel = (0x1cc5 - (cur_ip + 5)) & 0xFFFF
        stub = bytes([0xB0, G & 0xFF, 0xE8, rel & 0xFF, (rel >> 8) & 0xFF])
        for i in range(0, 0x4000, 1024):             # black page
            gdb.write_memory(0xB8000 + i, b"\x00" * 1024)
        gdb.write_memory(ds_lin + 0x3476, b"\xff")   # defeat 1cc5 "same as last"
        gdb.write_memory(cur, stub)
        gdb.set_breakpoint(cur + 5)
        gdb.continue_()
        gdb.wait_for_stop(timeout=20)
        gdb.remove_breakpoint(cur + 5)
        regs = gdb.read_registers()
        if regs.eip != cur + 5:
            print(f"  WARN {name}: eip {regs.eip:#x} != {cur+5:#x}", flush=True)
        fb = vram_to_fb(read_big(gdb, 0xB8000, 0x4000))
        open(f"{OUT}/{name}.fb", "wb").write(fb)
        manifest.write(f"{name}\t{kind}\t{kind}{letter}\t{pic}\t{off}\t{length}\t{G}\n")
        manifest.flush()
        print(f"  {name}  G={G} off={off:#x} len={length}", flush=True)
        cur += 5

    manifest.close()
    print("done", flush=True)
    try:
        dbx.stop()
    except Exception:
        pass


if __name__ == "__main__":
    main()
