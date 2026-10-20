# VVVVVV — Xbox 360 (libXenon)

A bare-metal port of **VVVVVV** to the Xbox 360, built with
[libXenon](https://github.com/Free60Project/libxenon). Based on the Xbox
Original port [XVVVVX](https://github.com/BttrDrgn/XVVVVX), reusing its game
logic and replacing every XDK/Win32/SDL2 dependency with a libXenon one.

> **Status:** graphics, input, FAT32 filesystem, **sound effects and music** are
> in place (music via the vendored stb_vorbis Ogg decoder). See
> [COMPATIBILITY.md](COMPATIBILITY.md).

## Requirements

- The free60 libXenon toolchain (`DEVKITXENON`), e.g. the
  `free60/libxenon` Docker image.
- A FAT32 USB stick whose **root** contains a `data/` tree (graphics, sounds,
  `vvvvvvmusic.vvv`). The game creates `user/` on first boot. The location of
  the `.elf32` file does not matter; libXenon has no working directory and the
  game opens absolute paths on the mounted volume (`/data/...`).

## Build

```sh
export DEVKITXENON=/usr/local/xenon
./build.sh            # host tests + cross-build + ELF verify
```

`build.sh` runs the host test suite first, then builds, then checks the output
is a big-endian PowerPC ELF32. Use `SKIP_CROSS=1 ./build.sh` to run only the
tests, or `make -C xenon` for the cross-build alone. Output:
`xenon/vvvvvv.elf32`. Launch it from Aurora's File Manager or XeLL.

Or with Docker:

```sh
docker run --rm -v "$PWD":/app -w /app free60/libxenon:latest \
  bash -lc "./build.sh"
```

## Tests

```sh
bash tests/run_host_tests.sh
```

Pure Linux/host tests (no console needed): the FAT32 driver against a real
image (read/write, atomic replace, rename), the colour/blend and pixel-format
math, the WAV decoder (malformed/truncated/unusual inputs) and the audio mixer
(voice lifetime, stealing, saturation). These cannot prove the GPU/audio path
works on hardware — that still needs a console.

## Runtime layout on the USB stick

The game mounts the first FAT32 USB volume (probing all three USB storage
ports) and treats its root as the old `D:\` drive. Paths are mapped as:

```
D:\data\graphics\tiles.png  ->  /data/graphics/tiles.png
D:\user\saves\qsave.vvv      ->  /user/saves/qsave.vvv
```

So the stick must look like this:

```
<USB ROOT>/
  data/
    graphics/*.png         21 files
    sounds/*.wav           28 files
    vvvvvvmusic.vvv
  user/                    created on first boot
    saves/  levels/  logs/
  vvvvvv.elf32             anywhere (root or a folder)
```

The 50 required data files are listed in `release/required-assets.txt`.
They are **not** included: VVVVVV's art, sounds and music are proprietary.
For personal use, get the official free Make and Play data from
<https://thelettervsixtim.es/makeandplay/> and extract its `data.zip`
**into** the `data/` folder on the stick.

`data/music/*.ogg`, `data/BinaryMusic.vvv` and `data/mmmmmm.vvv` are optional;
this port plays the packed `vvvvvvmusic.vvv` via stb_vorbis.

## Release packaging

`./build.sh` runs the tests, cross-builds, verifies the ELF and writes
`dist/vvvvvv-elf32.zip` (ELF + README/NOTICE + the required-asset manifest; no
proprietary assets). To validate and bundle your own data into a personal ZIP:

```sh
python3 tools/package_release.py --check /path/to/data        # report only
VVVVVV_DATA=/path/to/data ./build.sh                          # personal ZIP
python3 tools/package_release.py --data /path/to/data \
    --out dist/vvvvvv-elf32-with-data.zip
```

The tool lists every required file, fails if any is missing/corrupt, and
refuses to build an incomplete package. `tests/test_asset_manifest_host.py`
keeps the manifest in sync with the asset paths in the source, and
`tests/test_package_layout_host.py` checks a produced ZIP.

## Repository layout

```
src/            XVVVVX game sources (ported platform files live here too)
lodepng/        PNG decoder (zlib license)
tinyxml/        XML parser (zlib license)
xenon/          libXenon platform layer + SDL2 shim + build
  source/       gfx (Xenos), input (USB pad), fat32, sdlstub, game_gfx,
                platform_file (fopen/remove over FAT32),
                wav (PCM decoder), stb_vorbis (Ogg Vorbis, vendored),
                xenon_audio (xenon_sound mixer + music), sdlblit (shared
                pixel/blend math)
  include/      (generated headers)
display_test/   standalone Xenos colour-bar diagnostic
tests/          host-side tests (FAT32 vs real image, colour/blend, WAV, mixer,
                Ogg Vorbis decode, asset manifest, release ZIP layout)
release/        required/optional asset manifest + README/NOTICE templates
tools/          package_release.py (validate assets, build release ZIP)
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
lodepng and tinyxml (zlib), stb_vorbis (public domain).

This port is not affiliated with Terry Cavanagh or the VVVVVV team.
