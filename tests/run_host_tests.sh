#!/usr/bin/env bash
#
# run_host_tests.sh: build the FAT32 driver host tests and run them against a
# freshly created, real FAT32 image.
#
# These are HOST tests (Linux/aarch64). They do NOT run on Xbox 360 hardware;
# they verify the portable FAT32 parsing/writing logic that the ELF reuses.
#
# Requires: gcc, dosfstools (mkfs.fat), mtools (mmd/mcopy/mdel).
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

img="$work/fat.img"
binary="$work/test_fat32_host"

echo "== creating 64 MiB FAT32 image =="
dd if=/dev/zero of="$img" bs=1M count=64 status=none
mkfs.fat -F 32 -n VVVVO "$img" >/dev/null

echo "== populating with mtools =="
export MTOOLS_SKIP_CHECK=1
mmd -i "$img" ::/data
mmd -i "$img" ::/data/graphics
mmd -i "$img" ::/user
mmd -i "$img" ::/user/saves
mmd -i "$img" ::/user/levels
mmd -i "$img" ::/user/logs

printf 'hello world' > "$work/hello.txt"
mcopy -i "$img" "$work/hello.txt" ::/data/hello.txt

# A >1-cluster file (32 KB cluster default at this size -> use ~200 KB).
python3 - "$work/big.bin" <<'PY'
import sys
with open(sys.argv[1], 'wb') as f:
    f.write(bytes((i & 0xFF) for i in range(200000)))
PY
mcopy -i "$img" "$work/big.bin" ::/data/big.bin

# A couple of PNG-like files so enumeration finds >= 2 entries.
printf 'PNG1' > "$work/font.png"
printf 'PNG2' > "$work/tiles.png"
mcopy -i "$img" "$work/font.png" ::/data/graphics/font.png
mcopy -i "$img" "$work/tiles.png" ::/data/graphics/tiles.png

echo "== building host tests =="
gcc -O2 -g -Wall -Wextra -o "$binary" "$here/test_fat32_host.c" "$root/xenon/source/fat32.c"
gcc -O2 -g -Wall -Wextra -o "$work/test_blit_host" "$here/test_blit_host.c"
gcc -O2 -g -Wall -Wextra -o "$work/test_wav_host" "$here/test_wav_host.c" "$root/xenon/source/wav.c"
g++ -O2 -g -Wall -Wextra -w -I"$root/xenon/source" -o "$work/test_mixer_host" "$here/test_mixer_host.c" \
    "$root/xenon/source/xenon_audio.cpp" "$root/xenon/source/wav.c" \
    "$root/xenon/source/xenon_stb_vorbis.c" -lm
g++ -O2 -g -Wall -Wextra -w -I"$root/xenon/source" -o "$work/test_music_host" "$here/test_music_host.c" \
    "$root/xenon/source/xenon_audio.cpp" "$root/xenon/source/wav.c" \
    "$root/xenon/source/xenon_stb_vorbis.c" -lm
gcc -O2 -g -Wall -Wextra -o "$work/gen_wav" "$here/gen_wav.c" -lm
mkdir -p "$work/wavs"
"$work/gen_wav" "$work/wavs"

echo
echo "== test_blit_host (colour / blend / endian math) =="
"$work/test_blit_host"

echo
echo "== test_wav_host (WAV decode / resample) =="
"$work/test_wav_host" "$work/wavs"

echo
echo "== test_mixer_host (voice lifetime / stealing / saturation) =="
"$work/test_mixer_host"

echo
echo "== test_music_host (Ogg Vorbis decode + resample) =="
"$work/test_music_host" "$here/fixtures_tone.ogg" "$here/fixtures_tone8k.ogg"

echo
echo "== test_fat32_host (FAT32 driver vs real image) =="
"$binary" "$img"

echo
echo "== test_mount_reads_host (mount must not scan the whole FAT) =="
bigimg="$work/fat_big.img"
# Sparse 2 GiB volume: a full-FAT scan here would be thousands of sector reads.
truncate -s 2G "$bigimg"
mkfs.fat -F 32 -n VVVVOB "$bigimg" >/dev/null
mmd -i "$bigimg" ::/data
mcopy -i "$bigimg" "$work/big.bin" ::/data/big.bin
gcc -O2 -g -Wall -Wextra -o "$work/test_mount_reads_host" \
    "$here/test_mount_reads_host.c" "$root/xenon/source/fat32.c"
"$work/test_mount_reads_host" "$bigimg"

echo
bash "$here/test_startup_order_host.sh"
