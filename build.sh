#!/usr/bin/env bash
#
# build.sh - reproducible build/verify for the VVVVVV Xbox 360 (libXenon) port.
#
# Steps:
#   1. Run the host test suite (pure C/C++, no console needed).
#   2. Cross-compile the bare-metal ELF32 (needs DEVKITXENON; skip with SKIP_CROSS=1).
#   3. Verify the output is a big-endian PowerPC ELF32.
#
# Usage:
#   ./build.sh              # tests + cross-build + verify
#   SKIP_CROSS=1 ./build.sh # tests + verify prebuilt ELF only
#
# The cross-build expects the free60/libxenon toolchain:
#   export DEVKITXENON=/usr/local/xenon
# or run inside the Docker image (see README).
set -euo pipefail

root="$(cd "$(dirname "$0")" && pwd)"
cd "$root"

echo "==> host tests"
bash tests/run_host_tests.sh

elf="xenon/vvvvvv.elf32"

if [ "${SKIP_CROSS:-0}" = "1" ]; then
    echo "==> cross-build skipped (SKIP_CROSS=1)"
    if [ ! -f "$elf" ]; then
        echo "no $elf to verify; nothing more to do" >&2
        exit 0
    fi
else
    if [ -z "${DEVKITXENON:-}" ]; then
        echo "DEVKITXENON is not set. Export it or use SKIP_CROSS=1." >&2
        exit 1
    fi
    echo "==> cross-build (DEVKITXENON=$DEVKITXENON)"
    make -C xenon -j"$(nproc)"
fi

echo "==> verify ELF"
python3 ci/verify_elf.py "$elf"

# Package the release. VVVVVV's assets are proprietary, so the default public
# ZIP contains only the ELF + documentation. Point VVVVVV_DATA at a complete
# data/ tree to build a personal package that also validates and bundles it.
if [ "${SKIP_PACKAGE:-0}" != "1" ]; then
    echo "==> package release"
    if [ -n "${VVVVVV_DATA:-}" ]; then
        python3 tools/package_release.py --elf "$elf" --data "$VVVVVV_DATA" \
            --out dist/vvvvvv-elf32-with-data.zip
    else
        python3 tools/package_release.py --elf "$elf" --no-assets \
            --out dist/vvvvvv-elf32.zip
    fi
fi

echo "==> done"
