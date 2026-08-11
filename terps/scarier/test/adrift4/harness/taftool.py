#!/usr/bin/env python3
"""Unpack / repack ADRIFT 4.0 .taf files so run400.exe still accepts them.

A v4.0 .taf is:

    [14-byte signature][8 ASCII digits][zlib stream][15-byte trailer]

  * the digits are  len(file) - 14  in decimal;
  * the trailer is  0x00 + 12 bytes + CR LF.

The 12 bytes are the author's 8-character password stored as
``pw[0:4] + "Wild" + pw[4:8]`` (ADRIFT's author is Campbell Wild), obfuscated
with the same Visual Basic PRNG the v3.9/3.8 files use, indexed by ABSOLUTE
FILE OFFSET: the stream is advanced len(file)-14 times, then applied to the
twelve bytes.  run400.exe decodes it at load time and refuses the file with
"Error - Not an Adventure file." unless characters 5..8 spell "Wild".
(P-code: the Randomize 1976 / skip loop / Mid(s,5,4)<>"Wild" test at 0008EA62
in run400.txt.)  SCARE ignores both fields, so a file SCARE loads happily can
still be rejected by the Runner -- always repack with this.

Usage:
    python3 taftool.py unpack game.taf plain.bin      # write the inflated text
    python3 taftool.py pack plain.bin game.taf out.taf  # reuse out's password
"""
import sys, zlib

SIG = bytes.fromhex('3c423fc96a87c2cf93453e6139fa')


def _stream(n):
    """First n values of the VB6 PRNG after Rnd(-1); Randomize 1976."""
    s = 0x00a09e86
    out = bytearray()
    for _ in range(n):
        s = (s * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
        out.append((255 * s) // 0x1000000)
    return out


def unpack(path):
    """-> (inflated bytes, trailer lead byte, 12-byte plaintext password block)"""
    d = open(path, 'rb').read()
    assert d[:14] == SIG, 'not a v4.0 taf (3.9/3.8 are PRNG-obfuscated instead)'
    o = zlib.decompressobj()
    plain = o.decompress(d[22:])
    L = len(d)
    r = _stream(L)
    blk = bytes(d[L - 14 + i] ^ r[L - 14 + i] for i in range(12))
    return plain, o.unused_data[0], blk


def pack(plain, lead, blk, out):
    c = zlib.compress(plain, 6)          # level 6: what ZlibTool.ocx emits
    L = 22 + len(c) + 15
    r = _stream(L)
    ob = bytes(blk[i] ^ r[L - 14 + i] for i in range(12))
    d = SIG + ('%08d' % (L - 14)).encode() + c + bytes([lead]) + ob + b'\r\n'
    assert len(d) == L
    open(out, 'wb').write(d)
    return L


if __name__ == '__main__':
    if sys.argv[1] == 'unpack':
        plain, lead, blk = unpack(sys.argv[2])
        open(sys.argv[3], 'wb').write(plain)
        print('password block %r, lead %02x, %d bytes' % (blk, lead, len(plain)))
    elif sys.argv[1] == 'pack':
        _, lead, blk = unpack(sys.argv[3])
        print(pack(open(sys.argv[2], 'rb').read(), lead, blk, sys.argv[4]))
    else:
        sys.exit(__doc__)
