#!/bin/sh
# Writes what c64diskimage.c extracts from every C64 disk image under the
# given directories (default: the scott, plus and taylor test corpora) as TSV
# on stdout. Diff two runs to see what a change to the reader did.
#
#   ./snapshot.sh > before.tsv; ...edit...; ./snapshot.sh | diff before.tsv -
#
# A run that crashes is recorded as a CRASH row rather than stopping.

here=$(cd "$(dirname "$0")" && pwd)
terps=$(cd "$here/../.." && pwd)
probe="$here/c64diskimage_probe"

cc -std=c11 -O1 -g -Wall -I"$here/.." \
    -o "$probe" "$here/c64diskimage_probe.c" "$here/../c64diskimage.c" || exit 1

[ $# -eq 0 ] && set -- "$terps/scott/test" "$terps/scott/saga/test" "$terps/plus/test" "$terps/taylor/test"

find "$@" -type f \( -iname '*.d64' -o -iname '*.d71' -o -iname '*.d81' \) | LC_ALL=C sort -u |
while IFS= read -r image; do
    case "$image" in "$terps"/*) shown=${image#"$terps"/} ;; *) shown=$(basename "$image") ;; esac
    out=$("$probe" "$image" 2>/dev/null)
    status=$?
    [ -n "$out" ] && printf '%s\n' "$out" | sed "s|^|$shown	|"
    [ $status -ne 0 ] && printf '%s\tCRASH\t%d\n' "$shown" "$status"
done
exit 0
