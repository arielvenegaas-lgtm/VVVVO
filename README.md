# VVVVVV — Xbox 360 (libXenon)

A bare-metal port of **VVVVVV** to the Xbox 360, built with
[libXenon](https://github.com/Free60Project/libxenon). Based on the Xbox
Original port [XVVVVX](https://github.com/BttrDrgn/XVVVVX), reusing its game
logic and replacing every XDK/Win32/SDL2 dependency with a libXenon one.

> **Status:** graphics, input, FAT32 filesystem and **sound effects** are in
> place; music is silent (the soundtrack is Ogg Vorbis). See
> [COMPATIBILITY.md](COMPATIBILITY.md).

## Requirements

- The free60 libXenon toolchain (`DEVKITXENON`), e.g. the
  `free60/libxenon` Docker image.
- A FAT32 USB stick containing the VVVVVV `data/` tree (graphics, music,
  sounds) and empty `user/` folders.

## Build

```sh
export DEVKITXENON=/usr/local/xenon
make -C xenon
```

Output: `xenon/vvvvvv.elf32` (PowerPC big-endian ELF). Launch it from Aurora's
File Manager or XeLL.

Or with Docker:

```sh
docker run --rm -v "$PWD":/app -w /app free60/libxenon:latest \
  bash -lc "make -C xenon"
```

## Runtime layout on the USB stick

```
/data/graphics/*.png
/data/music/*.wav      (not played — soundtrack is Ogg Vorbis)
/data/sounds/*.wav     (played: WAV effects mixed to the DAC)
/user/saves/
/user/levels/
/user/logs/
```

The game's `D:\...` paths are mapped onto the mounted FAT32 volume at runtime.

## Repository layout

```
src/            XVVVVX game sources (ported platform files live here too)
lodepng/        PNG decoder (zlib license)
tinyxml/        XML parser (zlib license)
xenon/          libXenon platform layer + SDL2 shim + build
  source/       gfx (Xenos), input (USB pad), fat32, sdlstub, game_gfx,
                platform_file (fopen/remove over FAT32),
                wav (PCM decoder), xenon_audio (xenon_sound mixer),
                sdlblit (shared pixel/blend math)
  include/      (generated headers)
display_test/   standalone Xenos colour-bar diagnostic
tests/          host-side tests (FAT32 vs real image, colour/blend, WAV)
reference/      original XVVVVX files, kept for diffing the port
```

## Controls

Xbox 360 controller (via libXenon USB):

| Action | Button |
|--------|--------|
| Move | Left stick / D-pad |
| Accept / flip gravity | A |
| Back / cancel | B |
| Map / inventory | Y |
| Pause / menu | Start |

## License

VVVVVV's source is under its custom non-commercial license; altered versions
must be marked. The **game assets are proprietary and are NOT included** — you
must supply your own `data/` from a legitimate copy. Vendored libraries:
lodepng and tinyxml (zlib).

This port is not affiliated with Terry Cavanagh or the VVVVVV team.
