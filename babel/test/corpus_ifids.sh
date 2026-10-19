#!/bin/sh
# Corpus IFID regression for the babel modules.
#
#   test/corpus_ifids.sh [-b babel] DIR... > list.tsv
#
# Runs "babel -format", "-ifid" and "-meta" over every file below the given
# directories whose extension one of the modules (or containers) claims, and
# prints one sorted line per distinct (content, extension) pair:
#
#   sha256 <TAB> ext <TAB> format <TAB> ifids <TAB> sha256 of -meta output
#
# Keying on the content hash makes two lists from different machines or
# checkouts diffable. Make a list before a change and diff one made after it:
# IFIDs are stored in the Spatterlight library and in iFiction files, so any
# difference in the ifids column is a regression unless intended.

BABEL="$(dirname "$0")/../babel"
if [ "$1" = "-b" ]; then BABEL="$2"; shift 2; fi
if [ $# -eq 0 ]; then
    echo "usage: $0 [-b babel] DIR..." >&2
    exit 2
fi

EXTS='z[1-8] dat data ulx blb blorb zblorb zlb gblorb glb gam hex agx acd a3c
acx t3 taf mag l9 sna tap tzx z80 d64 t64 s64 dsk woz atr st msa plus plu
saga sag fiad cart rpk zip tay taylor quill exe cas asl quest aslx jacl j2
html htm'

pattern=""
for e in $EXTS; do
    pattern="$pattern -o -iname *.$e"
done
pattern="${pattern# -o }"

# shellcheck disable=SC2086
find "$@" -type f -size -64M \( $pattern \) -print0 2>/dev/null |
while IFS= read -r -d '' f; do
    sha=$(shasum -a 256 "$f" | cut -c1-64)
    ext=$(printf '%s' "${f##*.}" | tr 'A-Z' 'a-z')
    fmt=$("$BABEL" -format "$f" 2>&1 | sed -n 's/^Format: //p' | tr '\n' ' ')
    ifid=$("$BABEL" -ifid "$f" 2>&1 | sed -n 's/^IFID: //p' | tr '\n' ' ')
    meta=$("$BABEL" -meta "$f" 2>/dev/null | shasum -a 256 | cut -c1-16)
    printf '%s\t%s\t%s\t%s\t%s\n' "$sha" "$ext" "${fmt% }" "${ifid% }" "$meta"
done | sort -u
