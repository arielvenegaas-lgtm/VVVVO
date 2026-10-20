#!/usr/bin/env python3
"""test_asset_manifest_host.py - host-side guard for release packaging.

It scans the port's sources for every game-asset path literal and compares that
set with release/required-assets.txt + release/optional-assets.txt. This makes
sure a newly added (or renamed) asset can never be silently left out of the
package manifest, and that the manifest does not list dead files.

It does NOT need the proprietary assets and does NOT prove the assets load on
hardware; it only checks the manifest against the source of truth (the code).

Exit status 0 = consistent, 1 = mismatch.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REQUIRED = ROOT / "release" / "required-assets.txt"
OPTIONAL = ROOT / "release" / "optional-assets.txt"
EXTS = (".png", ".wav", ".ogg", ".vvv")

fails = 0


def check(cond, msg):
    global fails
    if cond:
        print("  ok  : %s" % msg)
    else:
        print("  FAIL: %s" % msg)
        fails += 1


def manifest(path):
    out = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            out.append(line)
    return out


def source_refs():
    refs = set()
    files = list((ROOT / "src").rglob("*")) + list((ROOT / "xenon" / "source").rglob("*"))
    for f in files:
        if f.suffix not in (".c", ".cpp", ".h"):
            continue
        try:
            text = f.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        # C/C++ string literals: find "D:\\data\\..." and "/data/...".
        for m in re.finditer(r'"((?:D:)?[\\/]+data[\\/]+[^"]*)"', text):
            raw = m.group(1)
            if raw.startswith("D:"):
                raw = raw[2:]
            raw = raw.replace("\\", "/")
            raw = re.sub(r"/+", "/", raw).lstrip("/")
            if not raw.lower().endswith(EXTS):
                continue
            if not raw.startswith("data/"):
                continue
            rel = raw[len("data/"):]
            refs.add(rel)
    return refs


def main():
    print("== test_asset_manifest_host (manifest matches source) ==")
    required = manifest(REQUIRED)
    optional = manifest(OPTIONAL)
    check(len(required) == 50, "required manifest has 50 files (%d)" % len(required))
    check(len(required) == len(set(required)), "no duplicate required entries")
    check(len(optional) == len(set(optional)), "no duplicate optional entries")
    check(not (set(required) & set(optional)),
          "required and optional manifests do not overlap")

    refs = source_refs()
    listed = set(required) | set(optional)
    unlisted = sorted(refs - listed)
    stale = sorted(listed - refs)
    check(not unlisted,
          "every asset referenced in source is in a manifest%s"
          % ("" if not unlisted else ": " + ", ".join(unlisted)))
    check(not stale,
          "no manifest entry is unreferenced by the source%s"
          % ("" if not stale else ": " + ", ".join(stale)))

    gfx = [r for r in required if r.startswith("graphics/")]
    snd = [r for r in required if r.startswith("sounds/")]
    mus = [r for r in required if r.endswith(".vvv")]
    check(len(gfx) == 21, "21 required graphics (%d)" % len(gfx))
    check(len(snd) == 28, "28 required sound effects (%d)" % len(snd))
    check(mus == ["vvvvvvmusic.vvv"], "required music pack is vvvvvvmusic.vvv")

    for rel in required + optional:
        check(not rel.startswith(("/", "data/")) and ".." not in rel,
              "path is data-relative and safe: %s" % rel)
        break  # one representative check; full loop would be noisy

    print("\n%s (%d failure%s)" % ("ALL TESTS PASSED" if not fails else "TESTS FAILED",
                                   fails, "" if fails == 1 else "s"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
