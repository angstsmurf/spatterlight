#!/bin/sh
# Run the regression suite's stages side by side (Makefile.headless `test`).
#
#   sh test/run_stages.sh "<make command>" stage1 stage2 ...
#
# Each stage is `<make command> <stage>`, started in the background with its
# output going to a log.  The logs are then printed whole, in the order the
# stages were named, each as soon as it (and every stage before it) has
# finished -- so the output reads exactly as it did when the stages ran one
# after another, just sooner.  Exits non-zero if any stage failed, after every
# stage has been printed, and lists the ones that did.
#
# The stages must not write to a shared path.  Today they don't: each uses its
# own /tmp names or a mktemp directory, and the binaries they run are built
# before this is called.
set -u

MAKECMD="$1"; shift
LOGDIR=$(mktemp -d); trap 'rm -rf "$LOGDIR"' EXIT

i=0
for stage in "$@"; do
  i=$((i + 1))
  # The status lands under a temporary name and is renamed into place, so
  # the poll below never reads a half-written file.
  ( $MAKECMD "$stage" > "$LOGDIR/$i.log" 2>&1
    echo $? > "$LOGDIR/$i.tmp" && mv "$LOGDIR/$i.tmp" "$LOGDIR/$i.rc" ) &
done

failed=""
i=0
for stage in "$@"; do
  i=$((i + 1))
  while [ ! -f "$LOGDIR/$i.rc" ]; do sleep 0.2; done
  cat "$LOGDIR/$i.log"
  rc=$(cat "$LOGDIR/$i.rc")
  [ "$rc" = 0 ] || failed="$failed $stage"
done
wait

if [ -n "$failed" ]; then
  echo "suite: FAILED stage(s):$failed" >&2
  exit 1
fi
