#!/bin/sh
# Writes what ciderpress.c extracts from every Apple II disk image under the
# given directories (default: the scott, plus and comprehend test corpora) as
# TSV on stdout. Diff two runs to see what a change to the reader did.
#
#   ./snapshot.sh > before.tsv; ...edit...; ./snapshot.sh | diff before.tsv -
#
# An op that crashes is recorded as a CRASH row rather than stopping the run.

here=$(cd "$(dirname "$0")" && pwd)
terps=$(cd "$here/../.." && pwd)
probe="$here/ciderpress_probe"

cc -std=c11 -O1 -g -Wall -I"$here/.." -I"$terps/common_utils" \
    -o "$probe" "$here/ciderpress_probe.c" "$here/../ciderpress.c" "$here/../woz2nib.c" || exit 1

[ $# -eq 0 ] && set -- "$terps/scott/test" "$terps/plus/test" "$terps/comprehend/test"

find "$@" -type f \( -iname '*.dsk' -o -iname '*.do' -o -iname '*.po' -o -iname '*.nib' -o -iname '*.woz' \) | LC_ALL=C sort |
while IFS= read -r image; do
    case "$image" in "$terps"/*) shown=${image#"$terps"/} ;; *) shown=$(basename "$image") ;; esac
    for op in dos twice saga prodos full range v6 nib; do
        out=$("$probe" "$op" "$image" 2>/dev/null)
        status=$?
        [ -n "$out" ] && printf '%s\n' "$out" | sed "s|^|$shown	|"
        [ $status -ne 0 ] && printf '%s\t%s\tCRASH\t%d\n' "$shown" "$op" "$status"
    done
done
exit 0
