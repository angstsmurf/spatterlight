#!/bin/bash
#
# Reset a locally built (Xcode) Spatterlight to a first-launch state: quits it,
# clears its NSUserDefaults, deletes its Core Data store, folder bookmarks and
# caches, and discards saved window state. A build signed with a team ID, such
# as the sandboxed release build, keeps its data in its own container and
# team-prefixed app group, which this script does not touch.
#
# Interpreter save/autosave folders (e.g. "Bocfel Files") and anything else in
# the Application Support folder are left alone unless --all is passed.
#
# Usage: scripts/reset-spatterlight.sh [--dry-run] [--all]

set -u

BUNDLE_ID="net.ccxvii.spatterlight"
PREFS="$HOME/Library/Preferences/$BUNDLE_ID"
SUPPORT_DIR="$HOME/Library/Application Support/Spatterlight"
GROUP_DIR="$HOME/Library/Group Containers/group.$BUNDLE_ID"
FAILED=0

DRY_RUN=0
ALL=0
for arg in "$@"; do
    case "$arg" in
        -n|--dry-run) DRY_RUN=1 ;;
        -a|--all) ALL=1 ;;
        -h|--help)
            sed -n '3,12p' "$0" | sed 's/^# \{0,1\}//'
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
        FAILED=1
    fi
}

# 1. Quit local builds so they can't write defaults or the store back out.
# Every build shares a bundle ID, so tell them apart by signature: a build
# signed with a team ID keeps its data elsewhere and is left running.
spatterlight_procs() {
    # comm is the whole executable path, which may contain spaces.
    ps -axo pid=,comm= | sed -n \
        's|^ *\([0-9][0-9]*\) \(/.*/Spatterlight\.app/Contents/MacOS/Spatterlight\)$|\1 \2|p'
}
team_of() {
    codesign -dv "$1" 2>&1 | sed -n 's/^TeamIdentifier=//p' | grep -v '^not set$'
}
local_pids() {
    local pid exe
    spatterlight_procs | while read -r pid exe; do
        [ -n "$(team_of "$exe")" ] || echo "$pid"
    done
}
spatterlight_procs | while read -r pid exe; do
    team=$(team_of "$exe")
    [ -n "$team" ] && echo "Leaving $exe (pid $pid) alone: signed by team $team, so its data is not reset here."
done
pids=$(local_pids)
if [ -n "$pids" ]; then
    echo "Quitting local Spatterlight build (pid $(echo $pids))..."
    run kill $pids
    if [ "$DRY_RUN" -eq 0 ]; then
        for _ in $(seq 1 20); do
            [ -z "$(local_pids)" ] && break
            sleep 0.5
        done
        pids=$(local_pids)
        [ -n "$pids" ] && kill -9 $pids
        sleep 0.5
        if [ -n "$(local_pids)" ]; then
            echo "FAILED to quit Spatterlight (pid $(echo $(local_pids)))" >&2
            FAILED=1
        fi
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

# 4. Security-scoped folder bookmarks. FolderAccess keeps these in the app
# group container even when the store is not there. Without a team ID the
# group has no prefix, so this is not the release build's container.
remove "$GROUP_DIR/Bookmarks.dict"

# 5. Caches and cookies of the unsandboxed build.
remove "$HOME/Library/Caches/$BUNDLE_ID"
remove "$HOME/Library/HTTPStorages/$BUNDLE_ID"
remove "$HOME/Library/HTTPStorages/$BUNDLE_ID.binarycookies"

# 6. Window restoration state, so no old windows reopen on launch.
remove "$HOME/Library/Saved Application State/$BUNDLE_ID.savedState"

if [ "$FAILED" -ne 0 ]; then
    echo "Reset incomplete, see the errors above." >&2
    exit 1
fi
echo "Done. The local Spatterlight build will start as if for the first time."
echo "Recent macOS versions keep saved window state where this script can't reach it."
echo "To skip restoring windows once, launch with: open <Spatterlight.app> --args -ApplePersistenceIgnoreState YES"
