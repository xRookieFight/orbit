import sys
import struct
import zlib


def read_ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    parts = data.split(b"\n", 3)
    magic = parts[0].strip()
    dims = parts[1].split()
    w, h = int(dims[0]), int(dims[1])
    raw = parts[3]
    if magic != b"P6":
        raise ValueError("not P6")
    return w, h, raw[: w * h * 3]


def write_png(path, w, h, rgb):
    rows = b""
    stride = w * 3
    for y in range(h):
        rows += b"\x00" + rgb[y * stride:(y + 1) * stride]
    compressed = zlib.compress(rows, 6)

    def chunk(tag, payload):
        out = struct.pack(">I", len(payload)) + tag + payload
        out += struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
        return out

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", compressed) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


w, h, rgb = read_ppm(sys.argv[1])
write_png(sys.argv[2], w, h, rgb)
print("ok %dx%d" % (w, h))
