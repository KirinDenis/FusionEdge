#!/usr/bin/env bash
# writes two ESP Web Tools manifests next to every board's binaries
#   make-manifests.sh <fw folder> "<version text>"
# <fw folder>/<board>/ holds the files .github/collect.sh left there, and its flash_args, which says
# at what address each one goes. manifest.json writes all of them, the LittleFS image too.
# manifest-app.json leaves the LittleFS image out, so the station list and the WiFi list stay
set -euo pipefail
FW="$1"
VERSION="$2"

for dir in "$FW"/*/; do
  board=$(basename "$dir")
  case "$board" in
    s3-ili9488)   name='ESP32-S3 DevKitC-1 N16R8 with a 3.5" ILI9488'; family='ESP32-S3' ;;
    jc3248w535)   name='Guition JC3248W535 (AXS15231B)';               family='ESP32-S3' ;;
    gen4-35ct)    name='4D Systems gen4-ESP32-35CT';                    family='ESP32-S3' ;;
    wrover-n16r8) name='ESP32-WROVER-E N16R8 (experimental)';           family='ESP32' ;;
    *) echo "skipping unknown folder $board"; continue ;;
  esac
  all=""
  app=""
  while read -r addr file; do
    case "$addr" in ""|--*) continue ;; esac
    [ -f "$dir/$file" ] || { echo "$board: $file is missing" >&2; exit 1; }
    part="{ \"path\": \"$file\", \"offset\": $((addr)) }"
    all="$all${all:+, }$part"
    if [ "$file" != littlefs.bin ]; then
      app="$app${app:+, }$part"
    fi
  done < "$dir/flash_args"
  for kind in all app; do
    if [ "$kind" = all ]; then
      out="$dir/manifest.json"
      parts=$all
      what=""
    else
      out="$dir/manifest-app.json"
      parts=$app
      what=", firmware only"
    fi
    cat > "$out" <<JSON
{
  "name": "FusionEdge - ${name//\"/\\\"}$what",
  "version": "$VERSION",
  "new_install_prompt_erase": true,
  "builds": [ { "chipFamily": "$family", "parts": [ $parts ] } ]
}
JSON
  done
  echo "$board: $(tr -d '\n' < "$dir/manifest.json")"
done
