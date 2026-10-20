#!/usr/bin/env python3
"""package_release.py - build the VVVVVV (Xbox 360 / libXenon) release ZIP.

The ZIP layout is:

    vvvvvv.elf32
    README.txt
    NOTICE.txt
    SHA256SUMS.txt
    data/MANIFEST-required.txt
    data/README.txt              (always)
    data/graphics/*.png          (only with --data)
    data/sounds/*.wav            (only with --data)
    data/vvvvvvmusic.vvv         (only with --data)

VVVVVV's art, graphics and music are proprietary and may NOT be redistributed.
The public package (--no-assets) therefore contains no assets; the user must
supply them for personal use from the official Make and Play Edition.

Every mode validates and prints the full list of required assets. With --data
(or --check) the command FAILS if any required asset is missing, unreadable or
obviously corrupt, so an incomplete package can never be produced.

Usage:
    python3 tools/package_release.py --no-assets --out dist/vvvvvv-elf32.zip
    python3 tools/package_release.py --data /path/to/data --out dist/full.zip
    python3 tools/package_release.py --check /path/to/data

The data directory may be either the data root (contains graphics/, sounds/)
or its parent (contains data/graphics/, data/sounds/).
"""
import argparse
import hashlib
import os
import shutil
import struct
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RELEASE = ROOT / "release"
MANIFEST_REQUIRED = RELEASE / "required-assets.txt"
MANIFEST_OPTIONAL = RELEASE / "optional-assets.txt"

FIXED_DATE = (2020, 1, 1, 0, 0, 0)


class Failure(Exception):
    pass


def read_manifest(path):
    if not path.is_file():
        raise Failure("missing manifest: %s" % path)
    out = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        out.append(line)
    return out


def verify_elf(path):
    if not path.is_file():
        raise Failure("ELF not found: %s" % path)
    data = path.read_bytes()[:64]
    if len(data) < 52 or data[:4] != b"\x7fELF":
        raise Failure("not an ELF file: %s" % path)
    if data[4] != 1:                       # EI_CLASS
        raise Failure("not ELF32: %s" % path)
    if data[5] != 2:                       # EI_DATA
        raise Failure("not big-endian: %s" % path)
    machine = struct.unpack(">H", data[18:20])[0]
    if machine != 20:                      # EM_PPC
        raise Failure("not PowerPC (e_machine=%d): %s" % (machine, path))
    return path.stat().st_size


def normalize_data_dir(raw):
    d = Path(raw)
    if not d.is_dir():
        raise Failure("data directory not found: %s" % d)
    if (d / "graphics").is_dir():
        return d
    if (d / "data" / "graphics").is_dir():
        return d / "data"
    # No graphics/ at all: still return d so the per-file report can show what
    # is missing rather than bailing out with a vague error.
    return d


def detect_kind(rel):
    ext = rel.lower().rsplit(".", 1)[-1]
    return {"png": "png", "wav": "wav", "ogg": "ogg"}.get(ext)


def check_file(path, rel):
    """Return (ok, detail). Missing/empty/corrupt are all failures."""
    if not path.is_file():
        return False, "missing"
    size = path.stat().st_size
    if size == 0:
        return False, "empty"
    kind = detect_kind(rel)
    try:
        with path.open("rb") as fp:
            head = fp.read(16)
    except OSError as e:
        return False, "unreadable (%s)" % e
    if kind == "png" and not head.startswith(b"\x89PNG\r\n\x1a\n"):
        return False, "not a PNG (%d bytes)" % size
    if kind == "wav":
        if head[:4] != b"RIFF" or head[8:12] != b"WAVE":
            return False, "not a WAV (%d bytes)" % size
    if kind == "ogg" and not head.startswith(b"OggS"):
        return False, "not an Ogg file (%d bytes)" % size
    return True, "%d bytes" % size


def report(data_dir, required, optional):
    missing = []
    print("Required assets (data/):")
    for rel in required:
        ok, detail = check_file(data_dir / rel, rel)
        print("  %-7s %-34s %s" % ("OK" if ok else "MISSING", rel, detail))
        if not ok:
            missing.append(rel)
    print("Optional assets (data/):")
    for rel in optional:
        ok, detail = check_file(data_dir / rel, rel)
        print("  %-7s %-34s %s" % ("OK" if ok else "absent", rel, detail))
    print("Summary: %d required present, %d required missing, %d optional present"
          % (len(required) - len(missing), len(missing),
             sum(1 for rel in optional if check_file(data_dir / rel, rel)[0])))
    return missing


def add_zip_file(zf, arcname, src, executable=False):
    info = zipfile.ZipInfo(arcname, FIXED_DATE)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = ((0o755 if executable else 0o644) << 16)
    with open(src, "rb") as fp:
        zf.writestr(info, fp.read())


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fp:
        for chunk in iter(lambda: fp.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def build_zip(elf, out, data_dir, required, optional, include_assets):
    if data_dir is not None:
        missing = report(data_dir, required, optional)
        if missing:
            raise Failure("%d required asset(s) missing; refusing to package. "
                          "See the list above." % len(missing))
    else:
        print("Assets: none included (proprietary). %d required files must be "
              "supplied by the user." % len(required))

    staging = Path(tempfile.mkdtemp(prefix="vvvvvv-pkg-"))
    try:
        shutil.copy2(elf, staging / "vvvvvv.elf32")
        shutil.copy2(RELEASE / "README.txt", staging / "README.txt")
        shutil.copy2(RELEASE / "NOTICE.txt", staging / "NOTICE.txt")

        datadir = staging / "data"
        datadir.mkdir()
        (datadir / "MANIFEST-required.txt").write_text(
            (MANIFEST_REQUIRED.read_text(encoding="utf-8")
             + "\nOptional assets are listed in the repository at "
               "release/optional-assets.txt.\n"),
            encoding="utf-8")

        if include_assets:
            for rel in required:
                dst = datadir / rel
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(data_dir / rel, dst)
            (datadir / "README.txt").write_text(
                "VVVVVV GAME DATA (personal copy)\n"
                "================================\n\n"
                "This folder was populated from your own VVVVVV data. The art,\n"
                "graphics and music are proprietary and this ZIP must NOT be\n"
                "redistributed. See ../NOTICE.txt.\n",
                encoding="utf-8")
        else:
            (datadir / "README.txt").write_text(
                "VVVVVV GAME DATA GOES HERE\n"
                "==========================\n\n"
                "This public ZIP does not include VVVVVV's graphics, sounds or\n"
                "music: they are proprietary and may not be redistributed.\n\n"
                "For personal use, download the official free Make and Play\n"
                "Edition data.zip from:\n\n"
                "    https://thelettervsixtim.es/makeandplay/\n\n"
                "Then EXTRACT THE CONTENTS OF data.zip INTO THIS FOLDER, so you\n"
                "end up with:\n\n"
                "    data/graphics/*.png      (21 files)\n"
                "    data/sounds/*.wav        (28 files used by this port)\n"
                "    data/vvvvvvmusic.vvv\n\n"
                "Do not rename the folders and do not put graphics/ at the USB\n"
                "root. The game mounts the FAT32 volume and opens absolute\n"
                "paths like /data/graphics/tiles.png. The location of\n"
                "vvvvvv.elf32 is irrelevant.\n\n"
                "See MANIFEST-required.txt for the complete list (50 files).\n",
                encoding="utf-8")

        # Checksums for everything except the checksum file itself.
        sums = []
        for path in sorted(staging.rglob("*")):
            if path.is_file():
                rel = path.relative_to(staging).as_posix()
                sums.append("%s  %s" % (sha256(path), rel))
        (staging / "SHA256SUMS.txt").write_text("\n".join(sums) + "\n",
                                                encoding="utf-8")

        out.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as zf:
            for path in sorted(staging.rglob("*")):
                if path.is_file():
                    rel = path.relative_to(staging).as_posix()
                    add_zip_file(zf, rel, path,
                                 executable=(rel == "vvvvvv.elf32"))
    finally:
        shutil.rmtree(staging, ignore_errors=True)


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--elf", default=str(ROOT / "xenon" / "vvvvvv.elf32"))
    ap.add_argument("--out", default=str(ROOT / "dist" / "vvvvvv-elf32.zip"))
    ap.add_argument("--data", metavar="DIR",
                    help="bundle assets from DIR (must be complete)")
    ap.add_argument("--no-assets", action="store_true",
                    help="public package; assets are user-supplied")
    ap.add_argument("--check", metavar="DIR",
                    help="only validate DIR and exit (no ZIP)")
    args = ap.parse_args(argv)

    elf = Path(args.elf)
    try:
        required = read_manifest(MANIFEST_REQUIRED)
        optional = read_manifest(MANIFEST_OPTIONAL)
        if not required:
            raise Failure("required-assets.txt is empty")
        duplicates = {r for r in required if required.count(r) > 1}
        if duplicates:
            raise Failure("duplicate manifest entries: %s" % ", ".join(sorted(duplicates)))

        size = verify_elf(elf)
        print("ELF: %s (%d bytes, big-endian PowerPC ELF32)" % (elf, size))

        if args.check:
            missing = report(normalize_data_dir(args.check), required, optional)
            if missing:
                raise Failure("%d required asset(s) missing." % len(missing))
            print("All %d required assets are present." % len(required))
            return 0

        if args.data and args.no_assets:
            raise Failure("--data and --no-assets are mutually exclusive")
        if not args.data and not args.no_assets:
            raise Failure("choose --data DIR (bundle assets) or --no-assets "
                          "(public package)")

        data_dir = normalize_data_dir(args.data) if args.data else None
        build_zip(elf, Path(args.out), data_dir, required, optional,
                  include_assets=bool(args.data))
        print("Wrote %s" % args.out)
        if args.data:
            print("WARNING: this ZIP contains proprietary VVVVVV assets. "
                  "Do not redistribute it; it is for your personal use only.")
        return 0
    except Failure as e:
        print("ERROR: %s" % e, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
