import struct, sys

DLL = sys.argv[1]
subs = sys.argv[2:]  # substrings to locate and find xrefs for
d = open(DLL, "rb").read()
def u16(o): return struct.unpack_from("<H", d, o)[0]
def u32(o): return struct.unpack_from("<I", d, o)[0]

e = u32(0x3C)
optOff = e + 4 + 20
ib = u32(optOff + 28)
numSec = u16(e + 4 + 2)
sizeOpt = u16(e + 4 + 16)
secTab = optOff + sizeOpt
secs = []
for i in range(numSec):
    s = secTab + i * 40
    name = d[s:s+8].split(b"\0")[0].decode("latin1")
    secs.append((name, u32(s+8), u32(s+12), u32(s+16), u32(s+20)))  # name,vsize,va,rawSize,rawPtr

def off2rva(off):
    for name, vsize, va, rawSize, rawPtr in secs:
        if rawPtr <= off < rawPtr + rawSize:
            return va + (off - rawPtr)
    return -1

text = next(s for s in secs if s[0] == ".text")
_, tvsize, tva, trawSize, trawPtr = text

for sub in subs:
    b = sub.encode("latin1")
    pos = 0
    found_any = False
    while True:
        idx = d.find(b, pos)
        if idx < 0:
            break
        rva = off2rva(idx)
        va = ib + rva if rva >= 0 else -1
        found_any = True
        if va >= 0:
            # scan .text for 4-byte references to va
            le = struct.pack("<I", va)
            t = d[trawPtr: trawPtr + trawSize]
            hits = []
            start = 0
            while True:
                h = t.find(le, start)
                if h < 0:
                    break
                hits.append(ib + tva + h)
                start = h + 1
            print("%-28s fileOff=0x%X VA=0x%X  xrefs=%s" % (sub, idx, va, [hex(x) for x in hits]))
        pos = idx + 1
    if not found_any:
        print("%-28s NOT FOUND" % sub)
