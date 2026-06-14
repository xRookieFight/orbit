import gzip
import struct
import sys


def load_psf(path):
    data = gzip.open(path, "rb").read()
    if data[:2] == b"\x36\x04":
        mode = data[2]
        height = data[3]
        count = 512 if (mode & 1) else 256
        width = 8
        glyph_size = height
        glyphs = [data[4 + i * glyph_size:4 + (i + 1) * glyph_size] for i in range(count)]
        table_off = 4 + count * glyph_size
        unicode_table = parse_psf1_table(data[table_off:], count) if (mode & 2) else None
        return glyphs, width, height, unicode_table
    if data[:4] == b"\x72\xb5\x4a\x86":
        (version, headersize, flags, count, charsize, height, width) = struct.unpack("<7I", data[4:32])
        glyphs = [data[headersize + i * charsize:headersize + (i + 1) * charsize] for i in range(count)]
        table_off = headersize + count * charsize
        unicode_table = parse_psf2_table(data[table_off:], count) if (flags & 1) else None
        return glyphs, width, height, unicode_table
    raise ValueError("not a PSF font")


def parse_psf1_table(data, count):
    table = []
    cps = []
    i = 0
    while i + 1 < len(data) and len(table) < count:
        v = struct.unpack("<H", data[i:i + 2])[0]
        i += 2
        if v == 0xFFFF:
            table.append(cps)
            cps = []
        elif v != 0xFFFE:
            cps.append(v)
    return table


def parse_psf2_table(data, count):
    table = []
    cps = []
    i = 0
    while i < len(data) and len(table) < count:
        b = data[i]
        if b == 0xFF:
            table.append(cps)
            cps = []
            i += 1
        elif b == 0xFE:
            while i < len(data) and data[i] != 0xFF:
                i += 1
        else:
            n = 1
            if b >= 0xF0:
                n = 4
            elif b >= 0xE0:
                n = 3
            elif b >= 0xC0:
                n = 2
            cp = data[i:i + n].decode("utf-8", "replace")
            cps.append(ord(cp[0]))
            i += n
    return table


def build_map(glyphs, table):
    out = [glyphs[0]] * 256
    if table is None:
        for i in range(min(256, len(glyphs))):
            out[i] = glyphs[i]
        return out
    for gi, cps in enumerate(table):
        for cp in cps:
            if 0 <= cp < 256:
                out[cp] = glyphs[gi]
    return out


def emit(name, path, out):
    glyphs, width, height, table = load_psf(path)
    mapped = build_map(glyphs, table)
    rowbytes = (width + 7) // 8
    out.write("const unsigned char %s[256][%d] = {\n" % (name, height * rowbytes))
    for c in range(256):
        g = mapped[c]
        vals = ", ".join("0x%02x" % b for b in g[:height * rowbytes])
        out.write("    { %s },\n" % vals)
    out.write("};\n\n")
    return width, height


with open(sys.argv[1], "w") as out:
    out.write("#include \"font.h\"\n\n")
    emit("font_regular", "/usr/share/consolefonts/Lat15-Terminus16.psf.gz", out)
    emit("font_bold", "/usr/share/consolefonts/Lat15-TerminusBold16.psf.gz", out)
print("ok")
