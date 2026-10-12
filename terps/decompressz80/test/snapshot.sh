#!/bin/sh
# Writes what the decompressz80 library makes of every ZX Spectrum snapshot and
# tape under the given directories (default: the scott and taylor test corpora)
# as TSV on stdout. Diff two runs to see what a change to the library did.
#
#   ./snapshot.sh > before.tsv; ...edit...; ./snapshot.sh | diff before.tsv -
#
# An op that crashes is recorded as a CRASH row rather than stopping the run.

here=$(cd "$(dirname "$0")" && pwd)
terps=$(cd "$here/../.." && pwd)
probe="$here/z80_probe"

cc -std=c11 -O1 -g -Wall -I"$here/.." -o "$probe" "$here/z80_probe.c" "$here"/../*.c || exit 1

[ $# -eq 0 ] && set -- "$terps/scott/test" "$terps/scott/saga/test" "$terps/taylor/test"

find "$@" -type f \( -iname '*.z80' -o -iname '*.slt' -o -iname '*.sna' -o -iname '*.tzx' -o -iname '*.tap' \) | LC_ALL=C sort |
while IFS= read -r image; do
    case "$image" in "$terps"/*) shown=${image#"$terps"/} ;; *) shown=$(basename "$image") ;; esac
    for op in z80 pages tzx tap payloads; do
        out=$("$probe" "$op" "$image" 2>/dev/null)
        status=$?
        [ -n "$out" ] && printf '%s\n' "$out" | sed "s|^|$shown	|"
        [ $status -ne 0 ] && printf '%s\t%s\tCRASH\t%d\n' "$shown" "$op" "$status"
    done
done
exit 0
