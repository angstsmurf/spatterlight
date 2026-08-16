#!/bin/sh
#
# fetch_games.sh -- populate and verify the (untracked) Quest game corpora.
#
# The .asl/.cas/.quest files the regression suites replay are third-party and
# are never committed; only their checksums are, in quest4/games.manifest.tsv
# and quest5/games.manifest.tsv.  This script reads those manifests and either
# checks what is on disk against them (verify, the default) or downloads the
# rows that have a recorded online source (fetch).
#
#   sh test/fetch_games.sh                   # verify both corpora
#   sh test/fetch_games.sh verify quest5     # verify one
#   sh test/fetch_games.sh fetch             # download what is missing
#   sh test/fetch_games.sh manual            # list rows that need a manual download
#
# Why the checksums matter: the Quest 5 goldens are byte-exact QuestViva
# transcripts of one specific build of one specific file.  Several games in
# these corpora were republished in place under the same name -- the corpus
# deliberately pins the older build in three cases -- so blindly re-downloading
# by filename would swap the game under a frozen transcript and read as an
# engine regression.  See test/GAMES.md.  fetch never installs a file whose
# sha256 does not match the manifest, and verify fails on a mismatch (but not
# on a missing file -- an absent corpus is a legitimate state that makes the
# corpus harnesses skip).
#
# Env:
#   Q4_GAMES_DIR / Q5_GAMES_DIR   corpus locations (default <corpus>/games)
#   GEAS_GAMES_CACHE              archive cache   (default test/.game-cache)

set -u

here=$(cd "$(dirname "$0")" && pwd)
cache=${GEAS_GAMES_CACHE:-$here/.game-cache}

if command -v shasum >/dev/null 2>&1; then
  sha256() { shasum -a 256 "$1" | awk '{print $1}'; }
elif command -v sha256sum >/dev/null 2>&1; then
  sha256() { sha256sum "$1" | awk '{print $1}'; }
else
  echo "fetch_games.sh: need shasum or sha256sum" >&2
  exit 2
fi

short() { echo "$1" | cut -c1-8; }

games_dir() {
  case "$1" in
    quest4) echo "${Q4_GAMES_DIR:-$here/quest4/games}" ;;
    quest5) echo "${Q5_GAMES_DIR:-$here/quest5/games}" ;;
  esac
}

# cache/<host>/<basename> -- the IF Archive and the Wayback Machine both serve
# files that only differ by which host they came from.
cache_path() {
  _host=$(echo "$1" | sed -e 's|^[a-z]*://||' -e 's|/.*||')
  _base=$(basename "$1" | sed 's/%20/ /g')
  echo "$cache/$_host/$_base"
}

# The Wayback Machine starts refusing connections outright after ~20 downloads
# in quick succession -- curl fails in milliseconds with "Couldn't connect", not
# with an HTTP status -- and stays that way for a while.  curl's own --retry
# waits 1s then 2s, nowhere near long enough, so a straight run gives up on most
# of the web.archive.org rows.  Space those requests out and retry the
# connection-level failures with a much longer backoff.  Other hosts are
# untouched: a 404 there should fail immediately, not after two minutes.
throttle=0
is_wayback() { case "$1" in *web.archive.org/*) return 0 ;; *) return 1 ;; esac; }

download() {                            # download URL -> cache, echo the path
  _url=$1
  _dst=$(cache_path "$_url")
  [ -s "$_dst" ] && { echo "$_dst"; return 0; }
  mkdir -p "$(dirname "$_dst")" || return 1

  if is_wayback "$_url"; then
    _tries=6
    [ "$throttle" -eq 1 ] && sleep 2
    throttle=1
  else
    _tries=1
  fi

  _n=1
  while :; do
    # -L is not optional for ifarchive.org: it 302s every file URL to
    # ukrestrict.ifarchive.org, and without it curl saves the 110-byte
    # "Redirecting to: ..." stub instead of the game.
    if curl -fsSL --compressed --retry 2 --max-time 900 -o "$_dst.part" "$_url" < /dev/null; then
      mv "$_dst.part" "$_dst"
      echo "$_dst"
      return 0
    else
      # Must be read in the else branch: after `fi`, $? is the status of the
      # `if` itself, which is 0 when the condition failed and there is no else.
      _rc=$?
    fi
    rm -f "$_dst.part"
    # 6 host lookup, 7 connect (the one the Wayback throttle actually returns),
    # 28 timeout, 35/52 TLS or empty reply.  Nothing else: with --compressed a
    # plain 404 comes back as 56, so retrying that would sleep out the full
    # backoff on every genuinely dead URL.
    case "$_rc" in 6|7|28|35|52) ;; *) return 1 ;; esac
    [ "$_n" -ge "$_tries" ] && return 1
    sleep $((_n * 10))
    _n=$((_n + 1))
  done
}

extract() {                             # extract ARCHIVE MEMBER -> DEST
  if unzip -p "$1" "$2" > "$3" 2>/dev/null < /dev/null && [ -s "$3" ]; then
    return 0
  fi
  rm -f "$3"
  return 1
}

mode=verify
corpora=""
for arg in "$@"; do
  case "$arg" in
    verify|fetch|manual) mode=$arg ;;
    quest4|quest5)       corpora="$corpora $arg" ;;
    -h|--help)           sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "fetch_games.sh: unknown argument '$arg'" >&2; exit 2 ;;
  esac
done
[ -n "$corpora" ] || corpora="quest4 quest5"

rc=0
tab=$(printf '\t')

for corpus in $corpora; do
  manifest="$here/$corpus/games.manifest.tsv"
  dir=$(games_dir "$corpus")
  if [ ! -f "$manifest" ]; then
    echo "$corpus: no manifest at $manifest" >&2
    rc=2
    continue
  fi
  # Only fetch creates the corpus directory: an absent one is how the harnesses
  # decide to skip outright, and verify must not change that.
  [ "$mode" = fetch ] && mkdir -p "$dir"

  n_ok=0 n_missing=0 n_bad=0 n_manual=0 n_got=0 n_fail=0

  # shellcheck disable=SC2162  (we want read's backslash-literal behaviour off)
  while IFS="$tab" read -r file size sha source member title note; do
    case "$file" in ''|'#'*) continue ;; esac
    path="$dir/$file"

    if [ "$mode" = manual ]; then
      if [ "$source" = "-" ]; then
        n_manual=$((n_manual + 1))
        printf 'MANUAL   %-46s %s\n' "$file" "$note"
      fi
      continue
    fi

    if [ -f "$path" ]; then
      got=$(sha256 "$path")
      if [ "$got" = "$sha" ]; then
        n_ok=$((n_ok + 1))
        continue
      fi
      n_bad=$((n_bad + 1))
      printf 'MISMATCH %-46s have %s.. want %s..\n' "$file" "$(short "$got")" "$(short "$sha")"
      continue
    fi

    if [ "$source" = "-" ]; then
      n_manual=$((n_manual + 1))
      [ "$mode" = fetch ] && printf 'MANUAL   %-46s %s\n' "$file" "$note"
      continue
    fi

    if [ "$mode" != fetch ]; then
      n_missing=$((n_missing + 1))
      continue
    fi

    if ! archive=$(download "$source"); then
      n_fail=$((n_fail + 1))
      printf 'FAILED   %-46s download failed: %s\n' "$file" "$source"
      continue
    fi

    # A couple of quest4 rows live in a subdirectory of the corpus, because the
    # game's own .asl expects to be there.
    mkdir -p "$(dirname "$path")" || { n_fail=$((n_fail + 1)); continue; }

    tmp="$path.fetching"
    if [ "$member" = "-" ]; then
      cp "$archive" "$tmp" || { n_fail=$((n_fail + 1)); continue; }
    elif ! extract "$archive" "$member" "$tmp"; then
      n_fail=$((n_fail + 1))
      printf 'FAILED   %-46s cannot extract %s from %s\n' "$file" "$member" "$(basename "$source")"
      continue
    fi

    got=$(sha256 "$tmp")
    if [ "$got" = "$sha" ]; then
      mv "$tmp" "$path"
      n_got=$((n_got + 1))
      printf 'FETCHED  %-46s %s\n' "$file" "$(basename "$source")"
    else
      rm -f "$tmp"
      n_fail=$((n_fail + 1))
      printf 'FAILED   %-46s upstream no longer matches (got %s..) %s\n' \
             "$file" "$(short "$got")" "$source"
    fi
  done < "$manifest"

  # A Certain Oscar (Oscar.asl) !includes movecont.lib, a QDK helper library
  # that was never distributed with the game and has not survived anywhere we
  # can find.  Quest refuses to load a game whose library is missing, so the
  # QuestViva oracle (quest4/harness/oracle/) cannot open the game without one;
  # geas skips a missing include and plays on either way.  Synthesize a
  # comment-only stand-in: it defines nothing, so both engines behave exactly
  # as if the include had been skipped, and the oracle can open the game.
  # Not a manifest row: the manifest pins third-party files, and this is ours.
  if [ "$mode" = fetch ] && [ "$corpus" = quest4 ] && [ ! -f "$dir/movecont.lib" ]; then
    printf "%s\r\n%s\r\n%s\r\n" \
      "' movecont.lib stub: the real QDK library was never distributed with the game." \
      "' Both geas and QuestViva read this adjacent file; it defines nothing, matching" \
      "' geas's historical behaviour of including nothing when the file was missing." \
      > "$dir/movecont.lib"
    printf 'WROTE    %-46s synthesized stub (see comment in script)\n' movecont.lib
  fi

  if [ "$mode" = manual ]; then
    printf '%s: %d row(s) to download by hand\n' "$corpus" "$n_manual"
    continue
  fi

  printf '%s: %d ok' "$corpus" "$n_ok"
  [ "$n_got"     -gt 0 ] && printf ', %d fetched'      "$n_got"
  [ "$n_missing" -gt 0 ] && printf ', %d missing'      "$n_missing"
  [ "$n_manual"  -gt 0 ] && printf ', %d manual'       "$n_manual"
  [ "$n_bad"     -gt 0 ] && printf ', %d MISMATCHED'   "$n_bad"
  [ "$n_fail"    -gt 0 ] && printf ', %d failed'       "$n_fail"
  printf ' (%s)\n' "$dir"

  [ "$n_bad"  -gt 0 ] && rc=1
  [ "$n_fail" -gt 0 ] && rc=1
done

exit $rc
