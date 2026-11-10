#!/usr/bin/env bash
# every version's firmware kept on the site itself: the newest in fw/<board>/, each older one in
# fw/<version>/<board>/, fw/versions.json lists them all, the newest first. a run puts its own build
# in fw/ and takes the older ones from the site as it is now. nothing is built again.
# the list only grows: a version the site has and this run can't fetch stops the run before it
# publishes, and so does a lower number than one the site has
#   archive.sh <fw> <version> <version text> <site url>
# <fw> holds a folder a board, as the firmware jobs leave them, their manifests made. needs curl
set -euo pipefail
FW="$1"
VER="$2"
TEXT="$3"
URL="${4%/}"

newer() {   # 0 (true) if version $1 is above $2
  [ "$1" != "$2" ] && [ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | tail -n 1)" = "$1" ]
}

# Pages answers 503 now and then, and a run asks it for many files: a passing error is asked again
# (curl's --retry: 408, 429, 500, 502, 503, 504 and timeouts). an answer that stays bad still stops
# the run. ?run= gets past the CDN's ten minute cache
RETRY="--retry 4 --retry-delay 5"
get() {     # <url path> <file>: 0 if the site answered 200
  local code
  code=$(curl -sS $RETRY -o "$2" -w '%{http_code}' "$URL/$1?run=${GITHUB_RUN_ID:-0}")
  if [ "$code" != 200 ]; then
    echo "$URL/$1 answered $code" >&2
    return 1
  fi
}

# what the site keeps now, one line a version: version|text|dir|board board ...
# versions.json is this script's own, one version a line, read back the way it is written
live=$(mktemp)
code=$(curl -sS $RETRY -o "$live.json" -w '%{http_code}' "$URL/fw/versions.json?run=${GITHUB_RUN_ID:-0}")
case "$code" in
  200) sed -n 's/.*"version": "\([^"]*\)", "text": "\([^"]*\)", "dir": "\([^"]*\)", "boards": \[ \(.*\) \].*/\1|\2|\3|\4/p' \
         "$live.json" > "$live" ;;
  404) echo "the site keeps no versions yet"
       : > "$live" ;;
  *)   echo "the site's fw/versions.json answered $code: stopping, or the versions it keeps would be lost" >&2
       exit 1 ;;
esac

kept=$(mktemp)                                   # version|text of each older one, as they get here
while IFS='|' read -r v text dir boards <&3; do
  if newer "$v" "$VER"; then
    echo "src/core/options.h says $VER, the site has $v: a new version needs a higher FW_VERSION" >&2
    exit 1
  fi
  [ "$v" = "$VER" ] && continue                  # the same number: this build is that version now
  for b in $(echo "$boards" | tr -d '",'); do
    mkdir -p "$FW/$v/$b"
    # flash_args names the parts, then the one-file image
    get "$dir/$b/flash_args" "$FW/$v/$b/flash_args" || { echo "stopping, or $v would be lost" >&2; exit 1; }
    while read -r addr file; do
      case "$addr" in ""|--*) continue ;; esac
      get "$dir/$b/$file" "$FW/$v/$b/$file" || { echo "stopping, or $v would be lost" >&2; exit 1; }
    done < "$FW/$v/$b/flash_args"
    get "$dir/$b/fusionedge-$b.zip" "$FW/$v/$b/fusionedge-$b.zip" || { echo "stopping, or $v would be lost" >&2; exit 1; }
  done
  echo "$v|$text" >> "$kept"
  echo "$v: $(echo "$boards" | tr -d '",'), from the site"
done 3< "$live"

# one entry of versions.json: the version, its text, its folder from the page, the boards in it.
# all of them are ours (options.h, the git log, make-manifests.sh's names): nothing to escape
entry() {   # <version> <text> <dir as the page has it> <dir here>
  local boards="" m
  for m in "$4"/*/manifest.json; do
    if [ -f "$m" ]; then
      boards="$boards${boards:+, }\"$(basename "$(dirname "$m")")\""
    fi
  done
  printf '    { "version": "%s", "text": "%s", "dir": "%s", "boards": [ %s ] }' "$1" "$2" "$3" "$boards"
}

list=$(entry "$VER" "$TEXT" "fw" "$FW")
while IFS='|' read -r v text; do
  bash .github/make-manifests.sh "$FW/$v" "$text" > /dev/null
  list="$list,"$'\n'"$(entry "$v" "$text" "fw/$v" "$FW/$v")"
done < <(sort -t '|' -k 1,1 -V -r "$kept")
printf '{\n  "versions": [\n%s\n  ]\n}\n' "$list" > "$FW/versions.json"
cat "$FW/versions.json"
