import struct, sys

DLL = sys.argv[1]
d = open(DLL, "rb").read()
def u16(o): return struct.unpack_from("<H", d, o)[0]
def u32(o): return struct.unpack_from("<I", d, o)[0]

e = u32(0x3C)
machine = u16(e + 4)
chars = u16(e + 22)
optOff = e + 4 + 20
magic = u16(optOff)
ib = u32(optOff + 28)
ep = u32(optOff + 16)
numDirs = u32(optOff + 92)
dirOff = optOff + 96
numSec = u16(e + 4 + 2)
sizeOpt = u16(e + 4 + 16)
secTab = optOff + sizeOpt
secs = []
for i in range(numSec):
    s = secTab + i * 40
    name = d[s:s+8].split(b"\0")[0].decode("latin1")
    secs.append((name, u32(s+8), u32(s+12), u32(s+16), u32(s+20)))

def rva2off(rva):
    for name, vsize, va, rawSize, rawPtr in secs:
        if va <= rva < va + max(vsize, rawSize):
            return rawPtr + (rva - va)
    return -1

def cstr(o):
    j = d.index(b"\0", o)
    return d[o:j].decode("latin1")

print("=== %s ===" % DLL)
print("machine=%s  chars=0x%X  magic=0x%X  ImageBase=0x%X  EntryRVA=0x%X" % (hex(machine), chars, magic, ib, ep))
print("sections:", [(n, hex(va)) for n, vs, va, rs, rp in secs])

# Exports
exp_va, exp_sz = u32(dirOff), u32(dirOff + 4)
print("\n=== EXPORTS (%d) ===" % (exp_sz // 40 if exp_va else 0))
if exp_va:
    o = rva2off(exp_va)
    nnames = u32(o + 24)
    addrNames = u32(o + 32)
    addrOrds = u32(o + 36)
    addrFuncs = u32(o + 28)
    baseOrd = u32(o + 16)
    for i in range(nnames):
        nrva = u32(rva2off(addrNames) + i * 4)
        ordi = u16(rva2off(addrOrds) + i * 2)
        frva = u32(rva2off(addrFuncs) + ordi * 4)
        name = cstr(rva2off(nrva))
        print("  ord=%d rva=0x%X  %s" % (baseOrd + ordi, frva, name))

# Imports
imp_va, imp_sz = u32(dirOff + 8), u32(dirOff + 12)
print("\n=== IMPORTS ===")
if imp_va:
    o = rva2off(imp_va)
    while True:
        oft, ts, fc, nameRva, ft = u32(o), u32(o+4), u32(o+8), u32(o+12), u32(o+16)
        if not (oft or nameRva or ft):
            break
        dll = cstr(rva2off(nameRva))
        print("[%s]" % dll)
        t = rva2off(oft) if oft else rva2off(ft)
        k = 0
        while True:
            tv = u32(t + k * 4)
            if tv == 0: break
            if tv & 0x80000000:
                fn = "ord#%d" % (tv & 0xFFFF)
            else:
                fn = cstr(rva2off(tv & 0x7FFFFFFF) + 2)
            print("   %s" % fn)
            k += 1
        o += 20
