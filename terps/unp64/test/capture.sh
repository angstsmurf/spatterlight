#!/bin/sh
# Take the samples for the regression tests from the game images of the Scott
# and Taylor test suites: build their probes with capture_unp64.cpp in the
# place of unp64.cpp, run both suites, and keep every program unp64() was
# handed in samples/. See README.md.
#
#   sh capture.sh
#
# The probes of the two suites are rebuilt afterwards, without the capture.

set -e
HERE=$(cd "$(dirname "$0")" && pwd)
TERPS=$(cd "$HERE/../.." && pwd)
BUILD="$HERE/build"
SAMPLES="$HERE/samples"
mkdir -p "$SAMPLES"

# $1 = interpreter directory, $2 = probe, $3... = the command that runs the suite
capture() {
    dir="$TERPS/$1"; probe="$2"; shift 2
    obj="$BUILD/capture_$probe"
    mkdir -p "$obj"
    # An unp64.o newer than unp64.cpp, so that make leaves it alone.
    make -s -C "$dir" -f Makefile.headless OBJDIR="$obj" "$obj/6502_emu.o"
    c++ -O0 -g -std=c++17 -w -I"$HERE/.." -c "$HERE/capture_unp64.cpp" -o "$obj/unp64.o"
    rm -f "$dir/$probe"
    make -s -C "$dir" -f Makefile.headless OBJDIR="$obj" "$probe"
    (cd "$dir" && UNP64_CAPTURE_DIR="$SAMPLES" "$@") || true
    rm -f "$dir/$probe"
    make -s -C "$dir" -f Makefile.headless "$probe"
}

capture scott scott_format_probe python3 test/formats/run_format_tests.py
capture taylor taylor_image_probe python3 ../common_imagetest/run_image_tests.py test/images ./taylor_image_probe

# One line per sample: the images it was seen in.
sort -u "$SAMPLES/log.tsv" > "$SAMPLES/log.sorted" && mv "$SAMPLES/log.sorted" "$SAMPLES/log.tsv"
echo "$(ls "$SAMPLES"/*.prg | wc -l | tr -d ' ') samples in $SAMPLES"
