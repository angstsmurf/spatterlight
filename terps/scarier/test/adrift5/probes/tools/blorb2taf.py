#!/usr/bin/env python3
"""Extract an ADRIFT v5 .taf from a .blorb package.

Matches FrankenDrift Blorb.vb / FileIO.vb Blorb load: find the Exec resource
(chunk type ADRI), and if babel lives in IFmd with a "0000" placeholder in
Exec, re-embed babel into a normal .taf layout.

Pre-5.0.20 Exec payloads (no babel size field) are obfuscated inside a blorb
but a bare .taf in that shape is read as plaintext (FileIO.vb:816), so the
obfuscation is stripped from the output.

--layout also extracts the Runner window layout the Generator embeds
(Blorb.vb DataChunk, the pane arrangement the game opens with).
"""

from __future__ import annotations

import argparse
import struct
import sys
import zlib
from pathlib import Path

from taf2xml import OBFUSCATION_KEY


class BlorbError(Exception):
    pass


def _u32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def _iter_chunks(blorb: bytes) -> list[tuple[bytes, int, bytes]]:
    """Parse IFF chunks inside an IFRS FORM.

    Ignores the top-level FORM length (ADRIFT often writes it wrong) and walks
    until EOF, matching FrankenDrift clsBlorb.
    """
    if len(blorb) < 12:
        raise BlorbError("too short to be a Blorb file")
    if blorb[0:4] != b"FORM":
        raise BlorbError("not an IFF FORM file")
    if blorb[8:12] != b"IFRS":
        raise BlorbError("not an IFRS (Blorb) file")

    chunks: list[tuple[bytes, int, bytes]] = []
    pos = 12
    while pos + 8 <= len(blorb):
        chunk_id = blorb[pos : pos + 4]
        length = _u32(blorb, pos + 4)
        data_start = pos + 8
        data_end = data_start + length
        if data_end > len(blorb):
            raise BlorbError(
                f"chunk {chunk_id!r} at {pos} overruns file "
                f"(len={length}, file={len(blorb)})"
            )
        chunks.append((chunk_id, pos, blorb[data_start:data_end]))
        pos = data_end + (length % 2)
    return chunks


def _load_ridx(ridx_data: bytes) -> list[tuple[bytes, int, int]]:
    if len(ridx_data) < 4:
        raise BlorbError("truncated RIdx chunk")
    num = _u32(ridx_data, 0)
    need = 4 + 12 * num
    if len(ridx_data) < need:
        raise BlorbError("RIdx chunk shorter than declared resource count")
    entries: list[tuple[bytes, int, int]] = []
    for i in range(num):
        base = 4 + i * 12
        usage = ridx_data[base : base + 4]
        number = _u32(ridx_data, base + 4)
        start = _u32(ridx_data, base + 8)
        entries.append((usage, number, start))
    return entries


def _chunk_at(blorb: bytes, start: int) -> tuple[bytes, bytes]:
    if start + 8 > len(blorb):
        raise BlorbError(f"resource start {start} past end of file")
    chunk_id = blorb[start : start + 4]
    length = _u32(blorb, start + 4)
    data_start = start + 8
    data_end = data_start + length
    if data_end > len(blorb):
        raise BlorbError(f"resource at {start} overruns file")
    return chunk_id, blorb[data_start:data_end]


def _exec_and_ifmd(blorb: bytes) -> tuple[bytes, bytes | None]:
    chunks = _iter_chunks(blorb)
    if not chunks or chunks[0][0] != b"RIdx":
        raise BlorbError("first chunk must be RIdx")

    entries = _load_ridx(chunks[0][2])
    exec_entry = next(
        (e for e in entries if e[0] == b"Exec" and e[1] == 0),
        None,
    )
    if exec_entry is None:
        # Fall back to first Exec, or first ADRI chunk.
        exec_entry = next((e for e in entries if e[0] == b"Exec"), None)

    exec_data: bytes | None = None
    if exec_entry is not None:
        chunk_id, exec_data = _chunk_at(blorb, exec_entry[2])
        if chunk_id != b"ADRI":
            raise BlorbError(
                f"Exec resource is {chunk_id!r}, expected ADRI (ADRIFT)"
            )
    else:
        for chunk_id, _pos, data in chunks:
            if chunk_id == b"ADRI":
                exec_data = data
                break

    if exec_data is None:
        raise BlorbError("no ADRI Exec resource found")

    ifmd = next((data for cid, _pos, data in chunks if cid == b"IFmd"), None)
    return exec_data, ifmd


def _layout_xml(blorb: bytes) -> bytes | None:
    """The embedded Runner window layout: a TEXT (or BINA) data chunk whose
    body begins "RLAY", not listed in RIdx (Blorb.vb, DataChunk).  Returns the
    SOAP-serialised Infragistics XML past the "RLAY" tag, or None."""
    for chunk_id, _pos, data in _iter_chunks(blorb):
        if chunk_id in (b"TEXT", b"BINA") and data[:4] == b"RLAY":
            return data[4:]
    return None


def _inflates(data: bytes) -> bool:
    try:
        zlib.decompressobj().decompress(data)
    except zlib.error:
        return False
    return True


def _deobfuscate(data: bytes) -> bytes:
    return bytes(
        b ^ OBFUSCATION_KEY[i % len(OBFUSCATION_KEY)] for i, b in enumerate(data)
    )


def _babel_size_field(babel_len: int) -> bytes:
    """Four-character uppercase hex length, zero-padded (ADRIFT style)."""
    field = f"{babel_len:X}".upper()
    if len(field) > 4:
        raise BlorbError(f"babel metadata too large ({babel_len} bytes)")
    return field.zfill(4).encode("ascii")


def _babel_from_ifmd(ifmd: bytes) -> bytes:
    """Normalize IFmd to ADRIFT TAF babel (starts with ``<ifindex``, no XML decl)."""
    text = ifmd.lstrip()
    if text.startswith(b"<?xml"):
        end = text.find(b"?>")
        if end != -1:
            text = text[end + 2 :].lstrip()
    return text


def blorb_to_taf(blorb: bytes) -> bytes:
    """Extract TAF bytes from an ADRIFT blorb."""
    exec_data, ifmd = _exec_and_ifmd(blorb)

    if len(exec_data) < 16:
        raise BlorbError("ADRI Exec resource too short")

    # ADRIFT blorb Exec uses a "0000" placeholder; restore babel from IFmd.
    if exec_data[12:16] == b"0000" and ifmd is not None:
        version = exec_data[0:12]
        rest = exec_data[16:]
        babel = _babel_from_ifmd(ifmd)
        size_field = _babel_size_field(len(babel))
        return version + size_field + babel + rest

    if exec_data[12:16] == b"0000" or exec_data[16:24] == b"<ifindex":
        # 5.0.20+ layout already: the size field tells every consumer the
        # payload is obfuscated, so it can pass through untouched.
        return exec_data

    # Pre-5.0.20 layout: the deflate stream follows the 12-byte version header
    # directly, with a 14-byte trailer.  Inside a blorb this payload is
    # obfuscated (bDeObfuscate, true for every Developer-built blorb), but a
    # bare .taf in this shape is read as plaintext (FileIO.vb:816) -- so strip
    # the obfuscation or the output is unloadable.  RtC.blorb is this shape.
    if len(exec_data) < 27:
        raise BlorbError("pre-5.0.20 Exec payload too short")
    region = exec_data[12:-14]
    if not _inflates(region):
        plain = _deobfuscate(region)
        if not _inflates(plain):
            raise BlorbError(
                "pre-5.0.20 Exec payload does not inflate, "
                "obfuscated or otherwise"
            )
        exec_data = exec_data[:12] + plain + exec_data[-14:]
    return exec_data


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Extract an ADRIFT v5 .taf from a .blorb package.",
    )
    parser.add_argument("input_file", help="path to the .blorb file")
    parser.add_argument(
        "--output-file",
        "-o",
        help="write .taf to this file (default: input with .taf suffix)",
    )
    parser.add_argument(
        "--layout",
        nargs="?",
        const="",
        metavar="FILE",
        help="also extract the embedded Runner window layout to FILE "
        "(default: input with .layout.xml suffix)",
    )
    args = parser.parse_args(argv)

    input_path = Path(args.input_file)
    try:
        blorb = input_path.read_bytes()
    except OSError as exc:
        print(f"error: cannot read {input_path}: {exc}", file=sys.stderr)
        return 1

    try:
        taf = blorb_to_taf(blorb)
        layout = _layout_xml(blorb) if args.layout is not None else None
    except BlorbError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if args.layout is not None and layout is None:
        print("error: no Runner layout chunk in blorb", file=sys.stderr)
        return 1

    output_path = (
        Path(args.output_file) if args.output_file else input_path.with_suffix(".taf")
    )
    try:
        output_path.write_bytes(taf)
    except OSError as exc:
        print(f"error: cannot write {output_path}: {exc}", file=sys.stderr)
        return 1

    if layout is not None:
        layout_path = (
            Path(args.layout)
            if args.layout
            else input_path.with_suffix(".layout.xml")
        )
        try:
            layout_path.write_bytes(layout)
        except OSError as exc:
            print(f"error: cannot write {layout_path}: {exc}", file=sys.stderr)
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
