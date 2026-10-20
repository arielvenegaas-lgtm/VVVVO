#!/usr/bin/env bash
#
# test_startup_order_host.sh: regression guard for the Xbox 360 boot stall.
#
# The bug: `CSound sfx;` is a file-scope object in src/Input.cpp, so its
# constructor (and SoundSystem's) runs during C++ static initialization --
# before main(), before VvvvoGfx_Init(), and before FILESYSTEM_init(). An
# earlier revision called XenonAudio_Preload()/MapPath()/xenon_sound_init()
# from those constructors, so boot performed real FAT/USB I/O in an
# uninitialized state and froze right after the video-init message.
#
# This is a SOURCE-level guard (no hardware needed): it fails if the audio
# constructors ever start touching the DAC or filesystem again, or if
# SoundSystem_Startup() is not called after FILESYSTEM_init() in main().
#
# This does NOT prove the game runs on an Xbox 360; it only prevents the
# specific, previously-diagnosed ordering regression from coming back.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"

fail=0
note() { printf '  %s\n' "$*"; }
bad()  { printf '  FAIL: %s\n' "$*"; fail=$((fail + 1)); }
ok()   { printf '  ok  : %s\n' "$*"; }

sound="$root/src/SoundSystem.cpp"
main="$root/src/main.cpp"

echo "== test_startup_order_host (static-init must not touch FS/DAC) =="

[ -f "$sound" ] || { echo "missing $sound"; exit 1; }
[ -f "$main" ]  || { echo "missing $main";  exit 1; }

# Extract each constructor body: from the signature to the first closing brace.
extract_ctor() {
    # $1 = file, $2 = "Class::Class"
    awk -v sig="$2" '
        index($0, sig "(") {
            started = 1
        }
        started {
            print
            # Count braces to find the end of the function.
            for (i = 1; i <= length($0); i++) {
                c = substr($0, i, 1)
                if (c == "{") depth++
                else if (c == "}") { depth--; if (depth == 0 && seen_open) { exit } }
            }
            if (index($0, "{")) seen_open = 1
        }
    ' "$1"
}

check_ctor_clean() {
    local label="$1" body="$2"
    if [ -z "$body" ]; then
        bad "$label constructor not found"
        return
    fi
    # Strip // comments so explanatory comments mentioning these names are OK.
    local stripped
    stripped="$(printf '%s\n' "$body" | sed 's://.*::')"
    local banned=0
    for sym in XenonAudio_Init XenonAudio_Preload XenonAudio_SetSfxList \
               RegisterSfxOnce MapPath FsReadWhole xenon_sound_init \
               usb_init FILESYSTEM_init; do
        if printf '%s\n' "$stripped" | grep -q "\b$sym\b"; then
            bad "$label constructor calls $sym (must be deferred to SoundSystem_Startup)"
            banned=1
        fi
    done
    [ "$banned" -eq 0 ] && ok "$label constructor does no FS/DAC work"
}

check_ctor_clean "SoundSystem::SoundSystem" "$(extract_ctor "$sound" 'SoundSystem::SoundSystem')"
check_ctor_clean "CSound::CSound"           "$(extract_ctor "$sound" 'CSound::CSound')"

# SoundSystem_Startup must exist and do the deferred work.
if grep -q 'void SoundSystem_Startup' "$sound"; then
    ok "SoundSystem_Startup is defined"
else
    bad "SoundSystem_Startup is not defined"
fi

startup_body="$(extract_ctor "$sound" 'void SoundSystem_Startup')"
if printf '%s\n' "$startup_body" | grep -q 'XenonAudio_Init'; then
    ok "SoundSystem_Startup brings up the DAC"
else
    bad "SoundSystem_Startup does not call XenonAudio_Init"
fi
if printf '%s\n' "$startup_body" | grep -q 'XenonAudio_Preload'; then
    ok "SoundSystem_Startup preloads effects"
else
    bad "SoundSystem_Startup does not call XenonAudio_Preload"
fi

# In main(), SoundSystem_Startup() must appear after FILESYSTEM_init().
fs_line="$(grep -n 'FILESYSTEM_init()' "$main" | head -1 | cut -d: -f1)"
st_line="$(grep -n 'SoundSystem_Startup()' "$main" | head -1 | cut -d: -f1)"
if [ -n "$fs_line" ] && [ -n "$st_line" ] && [ "$st_line" -gt "$fs_line" ]; then
    ok "main() calls SoundSystem_Startup after FILESYSTEM_init (line $st_line > $fs_line)"
else
    bad "main() must call SoundSystem_Startup after FILESYSTEM_init (fs=$fs_line startup=$st_line)"
fi

# Boot must show a real loading screen drawn with the built-in font, not rely on
# game assets (which is what made the earlier boot progress invisible).
if grep -q 'VvvvoGfx_LoadScreen' "$main"; then
    ok "main() draws the real loading screen"
else
    bad "main() does not call VvvvoGfx_LoadScreen"
fi
if grep -q 'VvvvoGfx_LoadScreen' "$root/src/GraphicsResources.cpp"; then
    ok "asset loading advances per-file progress"
else
    bad "GraphicsResources.cpp does not advance loading progress"
fi
# The fake preloader must not be used as the post-load state any more.
if grep -q 'game.gamestate = PRELOADER' "$main"; then
    bad "main() still enters the fake PRELOADER state"
else
    ok "main() skips the fake preloader"
fi

# Mounting must not scan the whole FAT: on a large stick that is one USB
# read per 512-byte sector and took minutes.
fat="$root/xenon/source/fat32.c"
if [ -f "$fat" ]; then
    mount_body="$(extract_ctor "$fat" 'int Fat32_MountVolume')"
    if printf '%s\n' "$mount_body" | grep -q 'CountFreeClusters'; then
        bad "Fat32_MountVolume() scans the whole FAT (very slow)"
    else
        ok "Fat32_MountVolume() does not scan the whole FAT"
    fi
else
    bad "missing $fat"
fi

# The fs:: wrappers must target the mounted volume, not NULL (passing NULL made
# every call a silent no-op, so fs::exists() was always false).
fsh="$root/xenon/source/filesystem.h"
if [ -f "$fsh" ]; then
    if grep -Eq 'Fat32V_[A-Za-z]+[[:space:]]*\([[:space:]]*NULL' "$fsh"; then
        bad "filesystem.h passes NULL to Fat32V_* (fs::* becomes a no-op)"
    else
        ok "filesystem.h passes the mounted volume to Fat32V_*"
    fi
    if grep -q 'FsVolume()' "$fsh"; then
        ok "filesystem.h uses FsVolume()"
    else
        bad "filesystem.h does not use FsVolume()"
    fi
else
    bad "missing $fsh"
fi

echo
if [ "$fail" -eq 0 ]; then
    echo "ALL TESTS PASSED (0 failures)"
    exit 0
else
    echo "FAILED ($fail failure(s))"
    exit 1
fi
