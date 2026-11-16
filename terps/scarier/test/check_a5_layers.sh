#!/bin/sh
# check_a5_layers.sh LAYERS OBJECT...
#
# The ADRIFT 5 layering check, the adrift5/ counterpart of check_layers.sh.
# LAYERS (adrift5/LAYERS) lists the engine's modules by layer, lowest first.
# Each object is one module: run/*.o is `run`, text/*.o is `text`, any other
# object is named for its file.  For every symbol an object leaves undefined
# that another module defines, the defining module must sit on a lower
# layer, or on the same layer when that layer's name ends in `*`.  Fails on
# any other call, on an object whose module LAYERS does not list, and on a
# listed module with no object.

layers=$1
shift
tmp=${TMPDIR:-/tmp}/check_a5_layers.$$
trap 'rm -f "$tmp".*' EXIT

[ -f "$layers" ] && [ $# -gt 0 ] || {
  echo "usage: check_a5_layers.sh LAYERS OBJECT..." >&2
  exit 2
}

for object in "$@"; do
  case $object in
    */run/*.o)  module=run ;;
    */text/*.o) module=text ;;
    *)          module=$(basename "$object" .o) ;;
  esac
  nm -g "$object" | awk -v m="$module" -v f="$(basename "$object")" '
    NF == 2 && $1 == "U" { print "U", m, f, $2 }
    NF == 3 && $2 != "U" { print "D", m, f, $3 }'
done > "$tmp.syms" || exit 1

# A symbol defined by more than one module is an inline or template copy, not
# a call between modules, and is skipped.
awk -v layersfile="$layers" '
  BEGIN {
    while ((getline line < layersfile) > 0) {
      sub(/#.*/, "", line)
      n = split(line, w, /[ \t]+/)
      first = 1
      for (i = 1; i <= n; i++) {
        if (w[i] == "") continue
        if (first) {
          depth++
          knot = (w[i] ~ /\*$/)
          first = 0
          continue
        }
        layer[w[i]] = depth
        open[w[i]] = knot
        listed[w[i]] = 1
      }
    }
  }
  $1 == "D" {
    if ($4 in owner && owner[$4] != $2) owner[$4] = ""
    else if (!($4 in owner)) owner[$4] = $2
    seen[$2] = 1
    next
  }
  $1 == "U" { seen[$2] = 1; use[++nuse] = $2 " " $3 " " $4 }
  END {
    status = 0
    for (m in seen)
      if (!(m in listed)) {
        print "check-a5-layers: module " m " is not in " layersfile
        status = 1
      }
    for (m in listed)
      if (!(m in seen)) {
        print "check-a5-layers: " layersfile " lists " m ", which has no object"
        status = 1
      }
    for (i = 1; i <= nuse; i++) {
      split(use[i], u, " ")
      caller = u[1]; sym = u[3]
      callee = (sym in owner) ? owner[sym] : ""
      if (callee == "" || callee == caller) continue
      if (!(caller in listed) || !(callee in listed)) continue
      if (layer[callee] < layer[caller]) continue
      if (layer[callee] == layer[caller] && open[caller]) continue
      print "check-a5-layers: " u[2] " (" caller ") calls " sym \
            " in " callee ", which is not below it in " layersfile
      status = 1
    }
    exit status
  }' "$tmp.syms" | c++filt > "$tmp.out"
# awk's exit status is lost in the pipe; any output line is a failure.
if [ -s "$tmp.out" ]; then
  cat "$tmp.out"
  exit 1
fi
echo "check-a5-layers: adrift5/ calls only downward" \
  "($(grep -c '^[^#[:space:]]' "$layers") layers, $# objects)"
exit 0
