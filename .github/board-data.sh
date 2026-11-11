#!/usr/bin/env bash
# fills build-data/, the LittleFS image of a board whose platformio.ini says data_dir = build-data
#   board-data.sh <options folder>
# its data.txt lists what to take, one path under data/ a line ('#' starts a comment). A 4 MB board
# holds less than data/ has: the fonts go into its firmware, the rest is chosen here.
set -euo pipefail
LIST="$1/data.txt"
[ -f "$LIST" ] || { echo "no $LIST" >&2; exit 1; }
rm -rf build-data
mkdir -p build-data
while IFS= read -r line; do
  path="${line%%#*}"
  path="$(echo "$path" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')"
  [ -n "$path" ] || continue
  [ -e "data/$path" ] || { echo "data/$path is missing" >&2; exit 1; }
  mkdir -p "build-data/$(dirname "$path")"
  cp -r "data/$path" "build-data/$path"
done < "$LIST"
du -sh build-data
