#!/bin/bash
#
# Reset a locally built (Xcode) Spatterlight to a first-launch state: quits it,
# clears its NSUserDefaults, deletes its Core Data store, and discards saved
# window state. The sandboxed release build in /Applications keeps its data in
# its own container and app group, which this script does not touch.
#
# Interpreter save/autosave folders (e.g. "Bocfel Files") and the UI test
# store (UITests.storedata) are left alone unless --all is passed.
#
# Usage: scripts/reset-spatterlight.sh [--dry-run] [--all]

set -u

BUNDLE_ID="net.ccxvii.spatterlight"
PREFS="$HOME/Library/Preferences/$BUNDLE_ID"
SUPPORT_DIR="$HOME/Library/Application Support/Spatterlight"

DRY_RUN=0
ALL=0
for arg in "$@"; do
    case "$arg" in
        -n|--dry-run) DRY_RUN=1 ;;
        -a|--all) ALL=1 ;;
        -h|--help)
            sed -n '3,11p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *) echo "Unknown option: $arg" >&2; exit 2 ;;
    esac
done

run() {
    if [ "$DRY_RUN" -eq 1 ]; then
        echo "would run: $*"
    else
        "$@"
    fi
}

remove() {
    local path="$1"
    [ -e "$path" ] || return 0
    if [ "$DRY_RUN" -eq 1 ]; then
        echo "would delete: $path"
    elif rm -rf "$path"; then
        echo "deleted: $path"
    else
        echo "FAILED to delete: $path" >&2
    fi
}

# 1. Quit local builds so they can't write defaults or the store back out.
# Both builds share a bundle ID, so pick processes by path and spare the
# release build in /Applications.
debug_pids() {
    ps -axo pid=,comm= | awk '
        $2 ~ /\/Spatterlight\.app\/Contents\/MacOS\/Spatterlight$/ &&
        $2 !~ /^\/Applications\// { print $1 }'
}
pids=$(debug_pids)
if [ -n "$pids" ]; then
    echo "Quitting local Spatterlight build (pid $(echo $pids))..."
    run kill $pids
    if [ "$DRY_RUN" -eq 0 ]; then
        for _ in $(seq 1 20); do
            [ -z "$(debug_pids)" ] && break
            sleep 0.5
        done
        pids=$(debug_pids)
        [ -n "$pids" ] && kill -9 $pids
    fi
fi

# 2. NSUserDefaults. Address the plist by path: a bare `defaults delete
# $BUNDLE_ID` is redirected to the release build's sandbox container once
# that exists. Going through `defaults` also updates cfprefsd's cache.
if [ -e "$PREFS.plist" ]; then
    run defaults delete "$PREFS" 2>/dev/null
    remove "$PREFS.plist"
fi

# 3. Core Data store. Unsigned builds have no TeamPrefix, so CoreDataManager
# keeps the store in Application Support rather than the app group container.
shopt -s nullglob
if [ "$ALL" -eq 1 ]; then
    remove "$SUPPORT_DIR"
elif [ -d "$SUPPORT_DIR" ]; then
    for f in "$SUPPORT_DIR"/Spatterlight.storedata* \
             "$SUPPORT_DIR"/migration-*.storedata* \
             "$SUPPORT_DIR"/.Spatterlight_SUPPORT; do
        remove "$f"
    done
fi
shopt -u nullglob

# 4. Window restoration state, so no old windows reopen on launch.
remove "$HOME/Library/Saved Application State/$BUNDLE_ID.savedState"

echo "Done. The local Spatterlight build will start as if for the first time."
echo "Recent macOS versions keep saved window state where this script can't reach it."
echo "To skip restoring windows once, launch with: open <Spatterlight.app> --args -ApplePersistenceIgnoreState YES"
