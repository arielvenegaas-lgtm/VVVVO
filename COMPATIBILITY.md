# VVVVVV on Xbox 360 (libXenon) — compatibility & port notes

This project is a from-scratch port of **VVVVVV** to the Xbox 360 using
**libXenon** (bare metal, no XDK). It is based on the Xbox Original port
[XVVVVX](https://github.com/BttrDrgn/XVVVVX) by `BttrDrgn`, with the libXenon
build/infrastructure approach taken from
[dxs360](https://github.com/arielvenegaas-lgtm/dxs360).

The goal is a real, hardware-runnable `.elf32` (Aurora File Manager / XeLL)
that reuses XVVVVX's game logic while replacing every XDK / Win32 / SDL2
dependency with a libXenon one.

---

## Status

| Area | State |
|------|-------|
| Toolchain | GCC 16.2.0 `xenon-*` (free60/libxenon) — verified producing PPC ELF |
| Game logic | Compiles from XVVVVX sources (`src/`) |
| Video | Implemented: CPU 320×240 ARGB → Xenos scan-out (integer scale, centred) |
| Input | Wired to libXenon USB pad reports (SDL event synthesis) |
| Filesystem | Implemented: FAT32 on USB mass storage; `fopen`/`remove` routed through it |
| Frame rate | Authentic 30 Hz simulation (34/41/55/83 ms), renderer optimised |
| Audio | **Sound effects and music play** (WAV SFX + Ogg Vorbis music → 48 kHz S16 stereo via `xenon_sound`) |
| CI | Dockerized libXenon build workflow + host test suite |

> **Audio is implemented.** `CSound::Create(t,p)` maps the 28 effect indices to
> `data/sounds/*.wav`, decoded (PCM 8/16-bit, mono/stereo, any rate) and mixed
> in software with linear-interpolation resampling to the DAC's 48 kHz S16
> stereo format (`xenon/source/wav.c`, `xenon/source/xenon_audio.cpp`). Music is
> decoded from the game's embedded Ogg Vorbis blob by **stb_vorbis** (vendored,
> public domain) and streamed through the same mixer at the game's music volume
> (`xenon/source/stb_vorbis.c`, `xenon_stb_vorbis.c`). Ogg tracks loop when the
> stream ends. Music and effects share the DAC ring. *(Audio output still needs
> hardware confirmation; the decode/mix path is covered by host tests.)*

> **Boot ordering.** All hardware/filesystem work (DAC init, effect preload,
> asset reads) happens in `main()` *after* `FILESYSTEM_init()`. It is deliberately
> not done from global constructors such as `CSound sfx;` (file-scope in
> `src/Input.cpp`), whose constructors run during static initialization, before
> the framebuffer and FAT volume exist.
>
> **Loading screen.** `main()` draws a real `VvvvoGfx_LoadScreen(pct, stage)`
> with the port's built-in 5x7 font, so progress text is visible before any PNG
> has loaded. The percentage advances only when a stage actually completes
> (mount, audio, input, each decoded asset, sprite arrays, save data). The
> original fake `PRELOADER` animation is no longer entered. If a core asset is
> missing or corrupt, `VvvvoGfx_ErrorScreen()` reports it readably instead of
> leaving a black screen.
>
> **FAT32 mount is O(1).** `Fat32_MountVolume()` trusts the FSInfo free-cluster
> hint instead of scanning the whole FAT. The old scan issued one synchronous
> USB read per 512-byte FAT sector and took ~10 minutes on a multi-GB stick.
> `Fat32_RecountFree()` is available if an exact count is ever needed. File reads
> now batch one USB transfer per cluster.
>
> **`fs::*` targets the mounted volume.** The inline helpers in
> `src/filesystem.h` pass `FsVolume()` to the `Fat32V_*` API; passing `NULL` made
> every call a no-op (so `fs::exists()` was always false and logging silently
> did nothing).
>
> **Asset path is absolute, not relative to the ELF.** libXenon has no current
> directory. The game's `D:\\...` literals are mapped by `MapPath()` to
> volume-relative POSIX paths (`/data/graphics/font.png`) and opened directly on
> the mounted FAT32 volume. `xenon_mount_fat32()` probes all three libXenon USB
> storage devices and prefers the volume whose root contains
> `data/graphics/font.png`; if none does, it keeps the first mountable volume so
> the missing-data error can be shown. Consequently the USB stick **root** must
> contain `data/`, and the `.elf32` may live anywhere. Internal HDD is not
> mounted (libXenon has no HDD filesystem driver in this build).
>
> **Asset validation and packaging.** At startup all 21 required PNGs are
> checked; failures show a readable error naming the missing files. Release
> packaging (`tools/package_release.py`) validates the 50 required assets
> (21 PNGs, 28 WAVs, `vvvvvvmusic.vvv`), refuses incomplete input, and the
> public ZIP contains only the ELF plus documentation because those assets are
> proprietary. `tests/test_asset_manifest_host.py` and
> `tests/test_package_layout_host.py` guard the manifest and ZIP.
> *(All of the above still needs hardware confirmation; the ordering, mount
> cost, path mapping and packaging are covered by host/source tests.)*

---

## What was reused from XVVVVX

Everything that is pure game logic runs unmodified:

- `src/Entity.cpp`, `Ent.cpp`, `Logic.cpp`, `Game.cpp`, `Map.cpp`
- `src/Script.cpp`, `Scripts.cpp`, `TerminalScripts.cpp`, `Textbox.cpp`
- `src/titlerender.cpp`, `Editor.cpp`, `states/Preloader.cpp`
- `src/Graphics.cpp`, `GraphicsResources.cpp`, `GraphicsUtil.cpp` (SDL software
  surfaces; see endianness below)
- `lodepng/`, `tinyxml/`

## What had to be replaced

| XVVVVX (Xbox / XDK) | VVVVO (Xbox 360 / libXenon) | File(s) |
|---------------------|------------------------------|---------|
| `xapilib` `XInputGetState` | `usb_init` + `get_controller_data` | `xenon/source/input.cpp` |
| `d3d8`/`d3dx8`/shaders (D3D9 path in dxs360) | Xenos scan-out framebuffer | `xenon/source/gfx.cpp`, `game_gfx.cpp` |
| `DirectMusic8` (`dmusici.h`) | SFX via `xenon_sound` (software WAV mixer); music via stb_vorbis Ogg decoder | `src/SoundSystem.cpp`, `xenon/source/xenon_audio.cpp`, `wav.c`, `stb_vorbis.c` |
| Win32 `FindFirstFile`/`CreateDirectory`/`CreateFile` | `fat32.c` FAT32 driver | `src/filesystem.h`, `xenon/source/platform_fs.cpp` |
| libc `fopen`/`fread`/`fwrite`/`fseek` (lodepng, TinyXML, `ifstream`) | link-time `--wrap=fopen` → `funopen()` over FAT32 | `xenon/source/platform_file.cpp` |
| libc `remove` (save deletion) | link-time `--wrap=remove` → `Fat32V_Unlink` | `xenon/source/platform_file.cpp` |
| `D:\data`, `D:\user` hardcoded paths | mapped to `/data`, `/user` on mounted volume | `MapPath()` in `src/filesystem.h` |
| SDL2 window/renderer/texture | SDL2-compatible shim → Xenos | `xenon/source/sdlstub.cpp`, `SDL.h` |
| `XLaunchNewImage` (dashboard) | `xenon_smc_power_reboot` | `xenon/source/platform_misc.cpp` |
| `Sleep()` | removed / busy wait in `SDL_Delay` | `src/Input.cpp` |

---

## Key technical issues

### 1. Big-endian (this is the big one)

The Xenon is **big-endian**. XVVVVX already contains conditional endian code in
`GraphicsResources.cpp` and `GraphicsUtil.cpp` keyed on `SDL_BYTEORDER`. The
port defines:

```c
#define SDL_BYTEORDER SDL_BIG_ENDIAN
```

so lodepng's RGBA byte order is interpreted correctly (`Rmask = 0xff000000`).
The SDL shim derives channel shifts from the masks passed to
`SDL_CreateRGBSurface`, so `SDL_MapRGB`/`SDL_BlitSurface`/`ReadPixel` stay
consistent across surfaces. The final present step unpacks
`R<<24|G<<16|B<<8|A` into the Xenos shadow's `0xAARRGGBB`.

### 2. Rendering model

The game draws entirely into CPU `SDL_Surface`s (320×240, 32bpp) with
`SDL_BlitSurface`, `SDL_FillRect`, `SDL_MapRGB` and direct pixel access. No
GPU features are required. `Screen::FlipScreen()` hands the finished surface to
`VvvvoGfx_Present()`, which integer-scales it into the scan-out and flushes the
data cache. The present path clips the centred rect to the scan-out and never
computes a negative origin, so a scan-out smaller than the source cannot write
out of bounds. `SDL_BlitSurface` converts source→destination components by
their masks and only writes an alpha byte when the destination has an alpha
channel; `GetSubSurface` inherits the source surface's format so the blit is a
same-format copy.

### 3. Input event numbers

`KeyPoll.cpp` checks **literal** event types `1536` (axis), `1538` (dpad),
`1539`/`1540` (button down/up) — the joystick-range numbers used by the Xbox
SDL2x build. The libXenon input backend must synthesise those exact numbers
(plus `SDL_CONTROLLERDEVICEADDED`/`REMOVED`) and fill `caxis`/`cbutton`/`cdevice`.

Button indices are real `SDL_GameControllerButton` values (A=0, B=1, X=2, Y=3,
BACK=4, GUIDE=5, START=6, LS=7, RS=8, LB=9, RB=10, DPAD=11..14). The left stick
is delivered as `SDL_CONTROLLER_AXIS_LEFTX/LEFTY` (0/1) with **Y inverted** to
SDL's convention (positive = down), because libXenon reports stick-up as
positive. When no fresh USB report arrives for ~2 s the pad is treated as
disconnected: a `SDL_CONTROLLERDEVICEREMOVED` is emitted once and every held
button/stick is released, so an unplug cannot leave a button latched down.
*(Y polarity and disconnect timing need hardware confirmation.)*

### 4. Filesystem

libXenon ships no FAT/FAT32 driver, so `xenon/source/fat32.c` (from dxs360) is
used. `platform_fs.cpp` mounts the first USB mass-storage LUN through
`USBStorage_Read_Sectors_0`/`Write_Sectors_0`. The game's `D:\...` paths are
translated by `MapPath()`. All libc file access (used by lodepng, TinyXML and
`std::ifstream`) is routed through the same volume at link time via
`--wrap=fopen`, and `remove()` via `--wrap=remove`, so PNG loading, save files
and custom-level loading/saving all reach the USB stick.

Expected layout on the USB stick:

```
/data/graphics/*.png
/data/music/*.ogg        (decoded by stb_vorbis and streamed to the DAC)
/data/sounds/*.wav       (decoded and mixed to the DAC)
/user/saves/
/user/levels/
/user/logs/
```

### 5. Compiler flags

Keep `-O2` and **never** `-O3`: the Xenon's VMX128 core mis-executes standard
AltiVec emitted by GCC's auto-vectorizer. `-fno-tree-vectorize` is mandatory.

---

## Build

```sh
export DEVKITXENON=/usr/local/xenon
make -C xenon          # -> xenon/vvvvvv.elf32
```

In CI (free60/libxenon Docker image):

```sh
docker run --rm -v "$PWD":/app -w /app free60/libxenon:latest \
  bash -lc "make -C xenon"
```

Run `vvvvvv.elf32` from Aurora's File Manager or XeLL with a FAT32 USB stick
containing the `data/` tree.

---

## Licensing

VVVVVV's source is distributed under its custom **non-commercial** license
(see `LICENSE-VVVVVV.txt`); altered versions must be marked and the original
author acknowledged. **The art, graphics and music are proprietary and are NOT
included in this repository** — you must supply your own `data/` from a
legitimate copy of VVVVVV.

Vendored code: `lodepng` (zlib), `tinyxml` (zlib), `fat32.c` (from dxs360;
see its header). The Xbox XDK libraries used by XVVVVX are non-redistributable
and are not used here.
