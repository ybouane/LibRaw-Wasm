#!/usr/bin/env python3
# Provenance for test/integration/lossy.dng — regenerate with: python3 make_lossy_dng.py
# (requires `pip install pillow numpy`). The committed fixture is the binary output of this
# script; CI does NOT run this, it just decodes the checked-in .dng.
#
# It builds a minimal Adobe-style *lossy* DNG: PhotometricInterpretation = LinearRaw (34892),
# Compression = 34892 (lossy / baseline-JPEG), 8-bit RGB, single tile. This is exactly the
# class of file from issue #27 — LibRaw must route it to lossy_dng_load_raw(), which decodes
# the embedded baseline JPEG via libjpeg. The pixels encode a gradient (R ramps left->right,
# G ramps top->bottom) so a correct decode is verifiable, not just "non-empty".
import io, struct
import numpy as np
from PIL import Image

W, H = 256, 168
OUT = 'lossy.dng'

xx, yy = np.meshgrid(np.linspace(0, 255, W), np.linspace(0, 255, H))
rgb = np.zeros((H, W, 3), np.uint8)
rgb[..., 0] = xx.astype(np.uint8)              # R ramps left->right
rgb[..., 1] = yy.astype(np.uint8)              # G ramps top->bottom
rgb[..., 2] = ((xx + yy) / 2).astype(np.uint8) # B diagonal

jb = io.BytesIO()
Image.fromarray(rgb, 'RGB').save(jb, format='JPEG', quality=92)
jpeg = jb.getvalue()

LE = '<'
def rational(n, d=10000):
    return (int(round(n * d)), d)

BYTE, ASCII, SHORT, LONG, RATIONAL, SRATIONAL = 1, 2, 3, 4, 5, 10
model = b'SyntheticLossyDNG\x00'
cm = [1,0,0, 0,1,0, 0,0,1]  # XYZ->cam, near identity

entries = [
    (254,  LONG,  [0]),                 # NewSubFileType = 0 (full-res raw)
    (256,  LONG,  [W]),                 # ImageWidth
    (257,  LONG,  [H]),                 # ImageLength
    (258,  SHORT, [8,8,8]),             # BitsPerSample
    (259,  SHORT, [34892]),             # Compression = lossy JPEG (Adobe)
    (262,  SHORT, [34892]),             # PhotometricInterpretation = LinearRaw
    (273,  LONG,  [0]),                 # StripOffsets   (patched below)
    (277,  SHORT, [3]),                 # SamplesPerPixel
    (278,  LONG,  [H]),                 # RowsPerStrip
    (279,  LONG,  [len(jpeg)]),         # StripByteCounts
    (322,  LONG,  [W]),                 # TileWidth
    (323,  LONG,  [H]),                 # TileLength
    (324,  LONG,  [0]),                 # TileOffsets    (patched below)
    (325,  LONG,  [len(jpeg)]),         # TileByteCounts
    (50706, BYTE, [1,4,0,0]),           # DNGVersion 1.4
    (50707, BYTE, [1,3,0,0]),           # DNGBackwardVersion
    (50708, ASCII, list(model)),        # UniqueCameraModel
    (50717, LONG, [255]),               # WhiteLevel
    (50714, SHORT, [0]),                # BlackLevel
    (50721, SRATIONAL, [rational(v) for v in cm]),  # ColorMatrix1
    (50778, SHORT, [21]),               # CalibrationIlluminant1 = D65
    (50728, RATIONAL, [rational(1), rational(1), rational(1)]),  # AsShotNeutral
]

def encode_values(typ, vals):
    b = bytearray()
    for v in vals:
        if typ in (BYTE, ASCII): b += struct.pack(LE+'B', v & 0xff)
        elif typ == SHORT:       b += struct.pack(LE+'H', v & 0xffff)
        elif typ == LONG:        b += struct.pack(LE+'I', v & 0xffffffff)
        elif typ == RATIONAL:    b += struct.pack(LE+'II', v[0] & 0xffffffff, v[1] & 0xffffffff)
        elif typ == SRATIONAL:   b += struct.pack(LE+'ii', v[0], v[1])
    return bytes(b)

n = len(entries)
ifd_offset = 8
data_offset = ifd_offset + 2 + n*12 + 4

encoded = [(tag, typ, len(vals), encode_values(typ, vals)) for (tag, typ, vals) in entries]

running = data_offset
placements = {}
for (tag, typ, cnt, payload) in encoded:
    if len(payload) > 4:
        if len(payload) % 2: payload += b'\x00'
        placements[tag] = running
        running += len(payload)
jpeg_offset = running

out = bytearray()
out += struct.pack(LE+'2sHI', b'II', 42, ifd_offset)
out += struct.pack(LE+'H', n)
for (tag, typ, cnt, payload) in encoded:
    if tag in (324, 273):
        payload = struct.pack(LE+'I', jpeg_offset)
    out += struct.pack(LE+'HHI', tag, typ, cnt)
    if len(payload) <= 4:
        out += payload + b'\x00'*(4-len(payload))
    else:
        out += struct.pack(LE+'I', placements[tag])
out += struct.pack(LE+'I', 0)
for (tag, typ, cnt, payload) in encoded:
    if len(payload) > 4:
        if len(payload) % 2: payload += b'\x00'
        out += payload
out += jpeg

open(OUT, 'wb').write(out)
print(f"wrote {OUT}  {len(out)} bytes  ({W}x{H}, jpeg={len(jpeg)}B)")
