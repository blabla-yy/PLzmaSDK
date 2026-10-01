#!/usr/bin/env python3
"""Writes the hand-built 7z fixtures next to this script.

7-Zip cannot be made to write a damaged, truncated or concatenated coder stream, an unknown method id,
or a folder whose declared size disagrees with its data, so these archives are assembled directly:
one file, one folder, one coder, plain (unencoded) header. Run from anywhere; needs only the Python
standard library, plus 7-Zip's `7zz` for the one encrypted fixture.
"""

import bz2
import lzma
import os
import random
import shutil
import struct
import subprocess
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))


def number(value):
    """7z variable-length NUMBER."""
    for extra in range(9):
        if extra == 8 or value < (1 << (7 * (extra + 1))):
            break
    if extra == 8:
        return b"\xff" + struct.pack("<Q", value)
    first_high = (0xFF << (8 - extra)) & 0xFF
    high = value >> (8 * extra)
    rest = value & ((1 << (8 * extra)) - 1)
    return bytes([first_high | high]) + rest.to_bytes(extra, "little")


def archive(packed, method_id, unpack_size, crc, name="payload.bin"):
    """`name=None` stores the file with no name property at all."""
    coder = bytes([len(method_id)]) + method_id
    header = b"".join([
        b"\x01",                                   # kHeader
        b"\x04",                                   # kMainStreamsInfo
        b"\x06", number(0), number(1),             # kPackInfo: pos 0, one stream
        b"\x09", number(len(packed)), b"\x00",
        b"\x07", b"\x0b", number(1), b"\x00",      # kUnPackInfo, kFolder, 1 folder, not external
        number(1), coder,                          # one simple coder
        b"\x0c", number(unpack_size),              # kCodersUnPackSize
        b"\x00",
        b"\x08",                                   # kSubStreamsInfo: one stream per folder,
        b"\x0a", b"\x01", struct.pack("<I", crc),  # its CRC (7-Zip checks this one, not the folder's)
        b"\x00",
        b"\x00",                                   # end of streams info
        b"\x05", number(1),                        # kFilesInfo, one file
        b"" if name is None else b"".join([
            b"\x11", number(len(name) * 2 + 3), b"\x00",
            name.encode("utf-16-le") + b"\x00\x00",
        ]),
        b"\x00",
        b"\x00",
    ])
    start = struct.pack("<QQI", len(packed), len(header), zlib.crc32(header))
    signature = b"7z\xbc\xaf\x27\x1c\x00\x04" + struct.pack("<I", zlib.crc32(start)) + start
    return signature + packed + header


def raw_deflate(data, level=9):
    c = zlib.compressobj(level, zlib.DEFLATED, -15)
    return c.compress(data) + c.flush()


def write(name, blob):
    with open(os.path.join(HERE, name), "wb") as f:
        f.write(blob)


COPY = b"\x00"
BZIP2 = b"\x04\x02\x02"
DEFLATE64 = b"\x04\x01\x09"
DEFLATE = b"\x04\x01\x08"
UNKNOWN = b"\x04\x02\xff"
ZSTD = b"\x04\xf7\x11\x01"  # 7-Zip-zstd / NanaZip; no decoder here

payload = b"".join(b"line %06d of the xzip codec payload\n" % i for i in range(20000))
crc = zlib.crc32(payload)
half = len(payload) // 2

bz = bz2.compress(payload, 9)
two_streams = bz2.compress(payload[:half], 9) + bz2.compress(payload[half:], 9)
corrupt_bz = bytearray(bz)
corrupt_bz[len(bz) // 2] ^= 0xFF
deflated = raw_deflate(payload)
corrupt_deflate = bytearray(deflated)
corrupt_deflate[len(deflated) // 2] ^= 0xFF

write("bzip2-concatenated.7z", archive(two_streams, BZIP2, len(payload), crc))
write("bzip2-truncated.7z", archive(bz[: len(bz) // 2], BZIP2, len(payload), crc))
write("bzip2-corrupt.7z", archive(bytes(corrupt_bz), BZIP2, len(payload), crc))
write("bzip2-declared-shorter.7z", archive(bz, BZIP2, len(payload) - 1, crc))
write("bzip2-declared-longer.7z", archive(bz, BZIP2, len(payload) + 1, crc))
write("bzip2-garbage-after-stream.7z", archive(bz + b"garbage!", BZIP2, len(payload), crc))
write("bzip2-garbage-instead-of-next-stream.7z", archive(bz + b"garbage!", BZIP2, len(payload) + 1, crc))
write("deflate-truncated.7z", archive(deflated[: len(deflated) // 2], DEFLATE, len(payload), crc))
write("deflate-corrupt.7z", archive(bytes(corrupt_deflate), DEFLATE, len(payload), crc))
write("deflate-declared-shorter.7z", archive(deflated, DEFLATE, len(payload) - 1, crc))
write("deflate-declared-longer.7z", archive(deflated, DEFLATE, len(payload) + 1, crc))
write("unknown-method.7z", archive(bz, UNKNOWN, len(payload), crc))
# Invalid from the first byte: a bzip2 stream without its magic, and a deflate block of type 3,
# which does not exist (BFINAL=1, BTYPE=11).
write("bzip2-bad-magic.7z", archive(b"NOTBZIP2" * 4, BZIP2, len(payload), crc))
write("deflate-invalid-block.7z", archive(b"\x07" + b"\x00" * 31, DEFLATE, len(payload), crc))
write("deflate64-invalid-block.7z", archive(b"\x07" + b"\x00" * 31, DEFLATE64, len(payload), crc))
write("zstd-method.7z", archive(bz, ZSTD, len(payload), crc))
write("copy-crc-mismatch.7z", archive(payload, COPY, len(payload), crc ^ 1))
write("nameless-single.7z", archive(bz, BZIP2, len(payload), crc, name=None))
write("payload.txt", payload)


def patched_method(source, old_id, new_id):
    """Rewrites a coder id inside a plain-header 7z and fixes both header CRCs."""
    blob = bytearray(open(source, "rb").read())
    next_offset, next_size, _ = struct.unpack("<QQI", blob[12:32])
    start = 32 + next_offset
    header = bytes(blob[start:start + next_size])
    assert header.count(old_id) == 1, "method id must appear exactly once"
    header = header.replace(old_id, new_id)
    blob[start:start + next_size] = header
    blob[28:32] = struct.pack("<I", zlib.crc32(header))
    blob[8:12] = struct.pack("<I", zlib.crc32(bytes(blob[12:32])))
    return bytes(blob)


def seven_zip(args, files, tmp):
    for name, data in files.items():
        with open(os.path.join(tmp, name), "wb") as f:
            f.write(data)
    out = os.path.join(tmp, "out.7z")
    if os.path.exists(out):
        os.remove(out)
    subprocess.run(["7zz", "a", "-bso0", "-mhc=off", "-mtm=off", "-mtc=off", "-mta=off"] + args
                   + ["out.7z"] + list(files), cwd=tmp, check=True)
    return out


def packed_stream(source):
    """The single pack stream of a one-folder archive: everything between the signature header
    and the next header."""
    blob = open(source, "rb").read()
    next_offset = struct.unpack("<Q", blob[12:20])[0]
    return blob[32:32 + next_offset]


# An encrypted item whose coder has no decoder: needs 7-Zip for the AES part (`brew install sevenzip`).
if shutil.which("7zz"):
    with tempfile.TemporaryDirectory() as tmp:
        with open(os.path.join(tmp, "a.txt"), "wb") as f:
            f.write(payload[:4096])
        subprocess.run(["7zz", "a", "-bso0", "-m0=BZip2", "-p1234", "-mhc=off", "-mtm=off", "-mtc=off",
                        "-mta=off", "out.7z", "a.txt"], cwd=tmp, check=True)
        write("aes-unknown-method.7z", patched_method(os.path.join(tmp, "out.7z"), BZIP2, UNKNOWN))

        # Deflate64: Python has no encoder, so take 7-Zip's stream and re-wrap it. The payload repeats
        # a 40 KiB block 48 KiB later, a distance plain Deflate's 32 KiB window cannot reach.
        rng = random.Random(64)
        block = bytes(rng.getrandbits(8) for _ in range(40 * 1024))
        far = block + bytes(rng.getrandbits(8) for _ in range(8 * 1024)) + block
        write("deflate64-long-distance.7z", open(seven_zip(["-m0=Deflate64"], {"far.bin": far}, tmp), "rb").read())
        write("far.bin", far)
        d64 = packed_stream(seven_zip(["-m0=Deflate64"], {"payload.bin": payload}, tmp))
        corrupt_d64 = bytearray(d64)
        corrupt_d64[len(d64) // 2] ^= 0xFF
        write("deflate64-rewrapped.7z", archive(d64, DEFLATE64, len(payload), crc))
        write("deflate64-truncated.7z", archive(d64[: len(d64) // 2], DEFLATE64, len(payload), crc))
        write("deflate64-corrupt.7z", archive(bytes(corrupt_d64), DEFLATE64, len(payload), crc))
        write("deflate64-declared-shorter.7z", archive(d64, DEFLATE64, len(payload) - 1, crc))
        write("deflate64-declared-longer.7z", archive(d64, DEFLATE64, len(payload) + 1, crc))

        # The other two coders made by 7-Zip itself, so the decoders are not only tested on streams
        # Python wrote. Several BZip2 blocks (100k each) and several Deflate blocks.
        write("bzip2-by-7zip.7z", open(seven_zip(["-m0=BZip2:d=100k"], {"payload.bin": payload}, tmp), "rb").read())
        write("deflate-by-7zip.7z", open(seven_zip(["-m0=Deflate"], {"payload.bin": payload}, tmp), "rb").read())
        write("aes-deflate64.7z", open(seven_zip(["-m0=Deflate64", "-p1234"], {"payload.bin": payload}, tmp), "rb").read())

# xz streams cut short, or followed by bytes that are not another stream: the operation results
# 7z archives never produce.
xz = lzma.compress(payload, format=lzma.FORMAT_XZ)
write("payload-truncated.xz", xz[: len(xz) // 2])
write("payload-trailing-garbage.xz", xz + b"not another xz stream")
