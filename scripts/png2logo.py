import sys
import struct
import zlib


def read_png(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    pos = 8
    w = h = depth = ctype = 0
    idat = bytearray()
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        tag = data[pos + 4:pos + 8]
        payload = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if tag == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", payload[:10])
        elif tag == b"IDAT":
            idat += payload
        elif tag == b"IEND":
            break
    if depth != 8 or ctype != 6:
        raise ValueError("expected 8-bit RGBA")
    raw = zlib.decompress(bytes(idat))
    stride = w * 4
    out = bytearray(w * h * 4)
    prev = bytearray(stride)
    p = 0
    for y in range(h):
        ftype = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        if ftype == 0:
            pass
        elif ftype == 1:
            for i in range(4, stride):
                line[i] = (line[i] + line[i - 4]) & 0xFF
        elif ftype == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ftype == 3:
            for i in range(stride):
                a = line[i - 4] if i >= 4 else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif ftype == 4:
            for i in range(stride):
                a = line[i - 4] if i >= 4 else 0
                b = prev[i]
                c = prev[i - 4] if i >= 4 else 0
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                pr = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        else:
            raise ValueError("bad filter %d" % ftype)
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return w, h, out


def write_header(path, w, h, rgba, name):
    words = []
    for i in range(0, len(rgba), 4):
        r, g, b, a = rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]
        words.append((a << 24) | (r << 16) | (g << 8) | b)
    with open(path, "w") as f:
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("#define %s_W %d\n" % (name.upper(), w))
        f.write("#define %s_H %d\n\n" % (name.upper(), h))
        f.write("static const uint32_t %s_data[%d] = {\n" % (name, w * h))
        for i in range(0, len(words), 8):
            row = ", ".join("0x%08X" % v for v in words[i:i + 8])
            f.write("    " + row + ",\n")
        f.write("};\n")
    print("wrote", path, w, "x", h)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "assets/orbit_logo.png"
    dst = sys.argv[2] if len(sys.argv) > 2 else "src/gui/logo_data.h"
    name = sys.argv[3] if len(sys.argv) > 3 else "orbit_logo"
    w, h, rgba = read_png(src)
    write_header(dst, w, h, rgba, name)


if __name__ == "__main__":
    main()
