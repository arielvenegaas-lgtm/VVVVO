# Provenance and licensing

- Game sources under `src/` are derived from XVVVVX
  (https://github.com/BttrDrgn/XVVVVX), itself a port of VVVVVV.
  VVVVVV and its source are (c) Terry Cavanagh and contributors and are
  distributed under VVVVVV's custom non-commercial license. This port is an
  altered version and is marked as such here. See the upstream VVVVVV
  repository for the full license text.
- `lodepng/` is (c) Lode Vandevenne, zlib license.
- `tinyxml/` is (c) Lee Thomason, zlib license.
- `xenon/source/fat32.c` is derived from the dxs360 project
  (https://github.com/arielvenegaas-lgtm/dxs360).
- `xenon/source/stb_vorbis.c` is stb_vorbis by Sean Barrett
  (https://github.com/nothings/stb), released into the public domain (or MIT,
  at your option). Used unmodified; `xenon_stb_vorbis.c` / `stb_vorbis.h` are
  thin build wrappers.
- The VVVVVV art, graphics and music are proprietary and are NOT included in
  this repository or in the public release ZIP. You must supply your own
  `data/` tree for personal use from the official Make and Play Edition
  (https://thelettervsixtim.es/makeandplay/). `release/required-assets.txt`
  lists the exact files; `tools/package_release.py` validates them and refuses
  to build an incomplete package.
- No Microsoft XDK code or libraries are included or required.
