#!/bin/sh
#
# fetch_decompilers.sh -- download the (untracked) TADS decompilers.
#
# tads2.pl and tads3.pl, from Fictitious Frode's Decompilers, turn a .gam or
# .t3 back into something like its source: the objects, the strings (TADS 2
# also the code) and the resources.  That shows the HTML a game writes, which
# `strings` can't, as both formats mask their strings.  The repository has
# no licence, so the scripts are never committed; this pins the commit and
# their checksums, and installs them in test/decompilers/.
#
#   sh test/fetch_decompilers.sh            # download what is missing
#
# Run them from a scratch directory, as they write their output under the
# current one: <stem>.t (the source) and a log, and without -m a <stem>/
# directory holding the resources too.
#
#   perl .../decompilers/tads2.pl -m game.gam
#   perl .../decompilers/tads3.pl -m game.t3
#
# They need Perl's File::Slurp, which macOS's and Homebrew's Perl have.
#
# Env:
#   TADS_DECOMPILERS_DIR   where they go (default test/decompilers)

set -u

here=$(cd "$(dirname "$0")" && pwd)
dir=${TADS_DECOMPILERS_DIR:-$here/decompilers}

repo=FictitiousFrode/Decompilers
commit=ebdf3f4e3d23ad1fb72e8b5426ef69f167d234c0    # 2022-05-18

if command -v shasum >/dev/null 2>&1; then
  sha256() { shasum -a 256 "$1" | awk '{print $1}'; }
elif command -v sha256sum >/dev/null 2>&1; then
  sha256() { sha256sum "$1" | awk '{print $1}'; }
else
  echo "fetch_decompilers.sh: need shasum or sha256sum" >&2
  exit 2
fi

mkdir -p "$dir" || exit 1
status=0

fetch() {
  name=$1 want=$2
  dest=$dir/$name
  if [ -f "$dest" ] && [ "$(sha256 "$dest")" = "$want" ]; then
    echo "ok       $name"
    return
  fi
  tmp=$dest.part
  if ! curl -fsSL -o "$tmp" \
       "https://raw.githubusercontent.com/$repo/$commit/$name"; then
    echo "FAILED   $name: download" >&2
    rm -f "$tmp"
    status=1
    return
  fi
  got=$(sha256 "$tmp")
  if [ "$got" != "$want" ]; then
    echo "FAILED   $name: sha256 $got, expected $want" >&2
    rm -f "$tmp"
    status=1
    return
  fi
  mv "$tmp" "$dest"
  echo "fetched  $name"
}

fetch tads2.pl 0a7cc034257b5f2e68787e6028eb29b41c5b1861e53dc243e4ed6c1cbd1e7560
fetch tads3.pl f2e5b94c3f1778ce89fb6727a54130c2a490bb3a9fc6e1e331530d5fa489a43a

exit $status
