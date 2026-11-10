#!/usr/bin/env bash
# puts one board's build in <out>/<board>/: the files, a flash_args that says at what address each
# one goes, and the one-file image of them all, zipped. esptool checks every image on the way
#   collect.sh <board> <chip> <pio env> <out>
# run at the repository's root after `pio run -e <env>`, `pio run -e <env> -t buildfs` and
# `pio project metadata -e <env> --json-output --json-output-path meta.json`
set -euo pipefail
BOARD="$1"
CHIP="$2"
ENV="$3"
OUT="$4/$1"
BUILD=".pio/build/$ENV"
VER=$(sed -n 's/^#define FW_VERSION "\([0-9][0-9.]*\)".*/\1/p' src/core/options.h | head -n 1)
command -v esptool > /dev/null || { echo "no esptool on PATH" >&2; exit 1; }
mkdir -p "$OUT"
: > "$OUT/flash_args"

# the bootloader, the partition table and boot_app0, at the addresses PlatformIO itself writes them.
# boot_app0 sets the OTA data to start app0, the slot the firmware goes to: a radio that updated
# itself over WiFi into app1 would otherwise go on starting the old firmware there
images=$(grep -o '"flash_images": \[[^]]*\]' meta.json | grep -o '{[^}]*}' || true)
[ -n "$images" ] || { echo "no flash_images in meta.json" >&2; exit 1; }
while read -r addr path; do
  path=${path//\\\\//}                       # a Windows path in JSON: C:\\x -> C:/x
  cp "$path" "$OUT/"
  echo "$addr $(basename "$path")" >> "$OUT/flash_args"
done < <(echo "$images" | sed -n 's/.*"offset": "\([^"]*\)", "path": "\([^"]*\)".*/\1 \2/p')

# the firmware and the LittleFS image go where upload and uploadfs put them: the first app partition
# (app0, the one boot_app0 starts), and the data partition of subtype spiffs or littlefs, in the
# partition table this build used
table=$(sed -n 's/^board_build\.partitions *= *\([^ ;]*\).*/\1/p' platformio.ini | head -n 1)
app=$(awk -F, '$2 ~ /^ *app *$/ { gsub(/ /, "", $4); print $4; exit }' "$table")
[ -n "$app" ] || { echo "no app partition in $table" >&2; exit 1; }
cp "$BUILD/firmware.bin" "$OUT/"
echo "$app firmware.bin" >> "$OUT/flash_args"

fs=$(awk -F, '$2 ~ /^ *data *$/ && $3 ~ /^ *(spiffs|littlefs) *$/ { gsub(/ /, "", $4); print $4; exit }' "$table")
[ -n "$fs" ] || { echo "no LittleFS partition in $table" >&2; exit 1; }
cp "$BUILD/littlefs.bin" "$OUT/"
echo "$fs littlefs.bin" >> "$OUT/flash_args"

# every image the chip boots from, read back by esptool: a wrong chip or a broken file stops here
for f in $(awk '{ print $2 }' "$OUT/flash_args"); do
  case "$f" in
    bootloader.bin|firmware.bin)
      esptool --chip "$CHIP" image-info "$OUT/$f" ;;
  esac
done

# one file for tools that take one, written at 0x0. it covers NVS too, so it is for a first install.
# on the classic ESP32 it starts with 0x1000 bytes of nothing: the ROM looks for the bootloader there
one="fusionedge-$BOARD-$VER.bin"
(cd "$OUT" && esptool --chip "$CHIP" merge-bin -o "../$one" $(tr '\n' ' ' < flash_args))
boot=$(awk '$2 == "bootloader.bin" { print $1 }' "$OUT/flash_args")
tail -c +$((boot + 1)) "$4/$one" > "$4/from-boot.bin"     # the image as the ROM reads it
esptool --chip "$CHIP" image-info "$4/from-boot.bin"
rm "$4/from-boot.bin"
while read -r addr file; do                  # and every part sits in it at its own address
  size=$(stat -c %s "$OUT/$file")
  cmp -n "$size" -i "$((addr)):0" "$4/$one" "$OUT/$file" || { echo "$file is not at $addr in $one" >&2; exit 1; }
done < "$OUT/flash_args"
echo "$one: every part at its address"
(cd "$4" && zip -9 -q "$BOARD/fusionedge-$BOARD.zip" "$one" && rm "$one")

cat "$OUT/flash_args"
ls -l "$OUT"
