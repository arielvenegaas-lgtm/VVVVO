"""Verify a libXenon output is a big-endian PowerPC ELF32.

Usage: python3 ci/verify_elf.py <path-to.elf32>
"""
import sys

if len(sys.argv) != 2:
    print("usage: verify_elf.py <path-to.elf32>", file=sys.stderr)
    sys.exit(2)

data = open(sys.argv[1], "rb").read()

if data[:4] != b"\x7fELF":
    print("error: not an ELF file", file=sys.stderr)
    sys.exit(1)
if data[4] != 1:
    print("error: not ELF32 (class=%d)" % data[4], file=sys.stderr)
    sys.exit(1)
if data[5] != 2:
    print("error: not big-endian (data=%d)" % data[5], file=sys.stderr)
    sys.exit(1)

# e_machine (2 bytes, big-endian) at offset 18; EM_PPC = 20.
machine = int.from_bytes(data[18:20], "big")
if machine != 20:
    print("error: not PowerPC (e_machine=%d)" % machine, file=sys.stderr)
    sys.exit(1)

print("ELF32 big-endian PowerPC OK; size=%d bytes" % len(data))
