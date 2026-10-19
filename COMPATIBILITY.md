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
| Filesystem | Implemented: FAT32 on USB mass storage (`fat32.c`) |
| Audio | **Silent stub** — no sound/music in this milestone |
| CI | Dockerized libXenon build workflow |

> **Audio is intentionally silent in the first milestone.** The game's audio
> call sites are guarded by `m_isValid == false`, so nothing plays but nothing
> crashes. The next step is a `xenon_sound` backend (48 kHz S16 stereo) with
> WAV decode + resampling.

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
| `DirectMusic8` (`dmusici.h`) | **silent stub** | `src/SoundSystem.cpp` |
| Win32 `FindFirstFile`/`CreateDirectory`/`CreateFile` | `fat32.c` FAT32 driver | `src/filesystem.h`, `xenon/source/platform_fs.cpp` |
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
data cache.

### 3. Input event numbers

`KeyPoll.cpp` checks **literal** event types `1536` (axis), `1538` (dpad),
`1539`/`1540` (button down/up) — the joystick-range numbers used by the Xbox
SDL2x build. The libXenon input backend must synthesise those exact numbers
(plus `SDL_CONTROLLERDEVICEADDED`) and fill `caxis`/`cbutton`/`cdevice`.

### 4. Filesystem

libXenon ships no FAT/FAT32 driver, so `xenon/source/fat32.c` (from dxs360) is
used. `platform_fs.cpp` mounts the first USB mass-storage LUN through
`USBStorage_Read_Sectors_0`/`Write_Sectors_0`. The game's `D:\...` paths are
translated by `MapPath()`.

Expected layout on the USB stick:

```
/data/graphics/*.png
/data/music/*.wav        (unused until audio is implemented)
/data/sounds/*.wav       (unused until audio is implemented)
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
