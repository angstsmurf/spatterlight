#!/usr/bin/env python3
# Decode a VICE screenshot of a C64 / Atari 8-bit *full* bitmap SAGA room (the
# SagaPlus titles — Spider-Man, Buckaroo Banzai, …) into a golden grid for
# c64a8test. Same as `c64_decode_png.py --align edge`: the wider, edge-weighted
# offset search described there.
#
#   c64a8_decode_png.py grid    <png> <spec> <bin> <out.c64> [dx dy]
#   c64a8_decode_png.py cmp     <png> <spec> <bin>           [dx dy]  align+score
#   c64a8_decode_png.py capture <png> <spec> <bin> <outdir>  [dx dy]  copy spec+dats+golden

import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from c64_decode_png import main  # noqa: E402

if __name__ == '__main__':
    main(align='edge')
