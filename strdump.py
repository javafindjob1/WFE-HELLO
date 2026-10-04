import struct, sys

DLL = sys.argv[1]
minlen = int(sys.argv[2]) if len(sys.argv) > 2 else 4
d = open(DLL, "rb").read()

out = []
# ASCII
i = 0
while i < len(d):
    b = d[i]
    if 0x20 <= b < 0x7F:
        j = i
        while j < len(d) and 0x20 <= d[j] < 0x7F:
            j += 1
        if j - i >= minlen:
            out.append((i, "A", d[i:j].decode("latin1")))
        i = j
    else:
        i += 1

# UTF-16LE (non-ascii content)
i = 0
while i + 1 < len(d):
    c = d[i] | (d[i+1] << 8)
    if 0x20 <= c < 0xFFFE and (c >= 0x80):
        j = i
        chars = []
        while j + 1 < len(d):
            cc = d[j] | (d[j+1] << 8)
            if 0x20 <= cc < 0xFFFE:
                chars.append(cc); j += 2
            else:
                break
        if len(chars) >= 3:
            s = ''.join(chr(x) for x in chars)
            out.append((i, "W", s))
        i = j + 2
    else:
        i += 2

out.sort(key=lambda x: x[0])
outfile = sys.argv[3] if len(sys.argv) > 3 else None
lines = ["0x%06X %s %s" % (off, kind, s) for off, kind, s in out]
if outfile:
    open(outfile, "w", encoding="utf-8", errors="replace").write("\n".join(lines))
    print("wrote %d strings to %s" % (len(lines), outfile))
else:
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
    print("\n".join(lines))
