#!/bin/sh
# Build stream_test.c against glkimp's stream.c and compare its output with
# stream_test.expected. Pass --bless to rewrite the expected file.
set -e
here=$(cd "$(dirname "$0")" && pwd)
glkimp=$(dirname "$here")
out=$(mktemp -d /tmp/glkimp-stream-test-build-XXXXXX)
trap 'rm -rf "$out"' EXIT

cc -std=gnu11 -Wall -Wno-unused-function -g -fsanitize=address,undefined \
    -I"$glkimp" "$here/stream_test.c" "$glkimp/stream.c" "$glkimp/cgunicod.c" \
    -o "$out/stream_test"
"$out/stream_test" > "$out/actual.txt"

if [ "$1" = "--bless" ]; then
    cp "$out/actual.txt" "$here/stream_test.expected"
    echo "stream_test.expected rewritten"
elif diff -u "$here/stream_test.expected" "$out/actual.txt"; then
    echo "stream_test: ok"
else
    echo "stream_test: FAILED"
    exit 1
fi
