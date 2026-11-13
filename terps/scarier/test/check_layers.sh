#!/bin/sh
# check_layers.sh OBJDIR HOOKS_HEADER
#
# The layering check: core/ is the bottom layer, and the only calls it may
# make upward, into library/ (lib_) and runner/ (run_), are the hooks that
# core/schooks.h declares.  Lists the lib_ and run_ symbols the core objects
# in OBJDIR leave undefined -- the ones no core object defines itself -- and
# fails on any that the hooks header does not name.

objdir=$1
hooks=$2
tmp=${TMPDIR:-/tmp}/check_layers.$$
trap 'rm -f "$tmp".*' EXIT

names='(lib|run)_[a-z0-9_]+'

ls "$objdir"/*.o >/dev/null 2>&1 || {
  echo "check-layers: no objects in $objdir" >&2
  exit 1
}

nm -g "$objdir"/*.o | c++filt | grep -v ' U ' | grep -E ' [A-TV-Za-tv-z] ' \
  | grep -oE "\\b$names" | sort -u > "$tmp.defined"
grep -oE "\\b$names \\(" "$hooks" | sed 's/ ($//' | sort -u > "$tmp.hooks"

status=0
for object in "$objdir"/*.o; do
  nm -u "$object" | c++filt | grep -oE "\\b$names" | sort -u \
    | comm -23 - "$tmp.defined" | comm -23 - "$tmp.hooks" > "$tmp.bad"
  if [ -s "$tmp.bad" ]; then
    status=1
    for symbol in $(cat "$tmp.bad"); do
      echo "check-layers: $(basename "$object" .o) calls $symbol," \
           "which is not a hook in $hooks"
    done
  fi
done

# A hook nothing in core/ calls any more should leave the header.
nm -u "$objdir"/*.o | c++filt | grep -oE "\\b$names" | sort -u > "$tmp.used"
for symbol in $(comm -23 "$tmp.hooks" "$tmp.used"); do
  status=1
  echo "check-layers: $symbol is in $hooks but core/ does not call it"
done

[ $status -eq 0 ] && echo "check-layers: core/ calls upward only through" \
  "$(wc -l < "$tmp.hooks" | tr -d ' ') hooks"
exit $status
