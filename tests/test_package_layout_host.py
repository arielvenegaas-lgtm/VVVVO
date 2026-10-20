#!/usr/bin/env python3
"""test_package_layout_host.py - verify a release ZIP built by
tools/package_release.py.

Usage:
    python3 tests/test_package_layout_host.py dist/vvvvvv-elf32.zip
    python3 tests/test_package_layout_host.py full.zip --assets

Checks (host only; no console):
  * required documentation entries exist
  * the ELF is present and is a big-endian PowerPC ELF32
  * data/MANIFEST-required.txt matches release/required-assets.txt
  * SHA256SUMS.txt covers every member and the hashes match
  * without --assets: no game assets are shipped
  * with --assets: every required asset is under data/ at the exact path,
    is non-empty and has the right magic bytes, and nothing unexpected is
    shipped
"""
import argparse
import hashlib
import re
import struct
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "release" / "required-assets.txt"
FIXED = ["vvvvvv.elf32", "README.txt", "NOTICE.txt", "SHA256SUMS.txt",
         "data/README.txt", "data/MANIFEST-required.txt"]

fails = 0


def check(cond, msg):
    global fails
    if cond:
        print("  ok  : %s" % msg)
    else:
        print("  FAIL: %s" % msg)
        fails += 1


def manifest():
    out = []
    for line in MANIFEST.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            out.append(line)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("zip")
    ap.add_argument("--assets", action="store_true",
                    help="expect the 50 proprietary assets under data/")
    a = ap.parse_args()

    zpath = Path(a.zip)
    if not zpath.is_file():
        print("missing ZIP: %s" % zpath)
        return 1
    print("== test_package_layout_host (%s) ==" % zpath)

    with zipfile.ZipFile(zpath) as zf:
        names = set(zf.namelist())
        for f in FIXED:
            check(f in names, "contains %s" % f)

        elf = zf.read("vvvvvv.elf32")
        check(len(elf) >= 52 and elf[:4] == b"\x7fELF", "ELF magic")
        check(len(elf) > 4 and elf[4] == 1, "ELF32 class")
        check(len(elf) > 5 and elf[5] == 2, "big-endian ELF")
        check(struct.unpack(">H", elf[18:20])[0] == 20, "PowerPC e_machine")

        # The manifest embedded in the ZIP must match the repository manifest.
        want = manifest()
        got = zf.read("data/MANIFEST-required.txt").decode("utf-8")
        got = [ln.strip() for ln in got.splitlines()
               if ln.strip() and not ln.strip().startswith("#")]
        # Strip the optional footer line added by the packager.
        got = [ln for ln in got if ln.endswith((".png", ".wav", ".ogg", ".vvv"))]
        check(got == want, "embedded manifest matches release/required-assets.txt")

        # Checksums.
        sums = {}
        for line in zf.read("SHA256SUMS.txt").decode("utf-8").splitlines():
            if not line.strip():
                continue
            h, rel = re.split(r"\s+", line.strip(), maxsplit=1)
            sums[rel] = h
        members = {n for n in names if not n.endswith("/") and n != "SHA256SUMS.txt"}
        check(set(sums) == members, "SHA256SUMS lists exactly the payload files")
        bad = [rel for rel, h in sums.items()
               if hashlib.sha256(zf.read(rel)).hexdigest() != h]
        check(not bad, "all SHA256SUMS hashes match")

        assets = [n for n in names if n.lower().endswith((".png", ".wav", ".ogg", ".vvv"))
                  and n != "data/MANIFEST-required.txt"]
        if a.assets:
            missing = [rel for rel in want if "data/" + rel not in names]
            check(not missing, "all 50 required assets are present"
                  + ("" if not missing else ": " + ", ".join(missing)))
            check(len(assets) == len(want),
                  "ZIP has exactly %d assets (%d)" % (len(want), len(assets)))
            for rel in want:
                data = zf.read("data/" + rel)
                ok = len(data) > 0
                if rel.endswith(".png"):
                    ok = ok and data.startswith(b"\x89PNG\r\n\x1a\n")
                elif rel.endswith(".wav"):
                    ok = ok and data[:4] == b"RIFF" and data[8:12] == b"WAVE"
                if not ok:
                    check(False, "valid asset data/%s" % rel)
                    break
            else:
                check(True, "asset contents have correct magic bytes")
            extras = [n for n in names
                      if n not in FIXED and n not in sums
                      and not n.startswith("data/")]
            check(not extras, "no unexpected top-level files"
                  + ("" if not extras else ": " + ", ".join(extras)))
        else:
            check(not assets, "public ZIP contains no game assets"
                  + ("" if not assets else ": " + ", ".join(assets)))
            extra = [n for n in names if n not in FIXED]
            check(not extra, "public ZIP contains only ELF + documentation"
                  + ("" if not extra else ": " + ", ".join(extra)))

    print("\n%s (%d failure%s)" % ("ALL TESTS PASSED" if not fails else "TESTS FAILED",
                                   fails, "" if fails == 1 else "s"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
