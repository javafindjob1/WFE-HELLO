import struct, sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

DLL = r"C:\3rd\war5\Game.dll"
IB = 0x6F000000
d = open(DLL, "rb").read()
def u16(o): return struct.unpack_from("<H", d, o)[0]
def u32(o): return struct.unpack_from("<I", d, o)[0]

e = u32(0x3C); optOff = e + 4 + 20
numSec = u16(e + 4 + 2); sizeOpt = u16(e + 4 + 16); secTab = optOff + sizeOpt
secs = []
for i in range(numSec):
    s = secTab + i * 40
    secs.append((d[s:s+8].split(b"\0")[0].decode("latin1"), u32(s+8), u32(s+12), u32(s+16), u32(s+20)))

def rva2off(rva):
    for name, vs, va, rs, rp in secs:
        if va <= rva < va + max(vs, rs):
            return rp + (rva - va)
    return -1

md = Cs(CS_ARCH_X86, CS_MODE_32)
for rva in [int(x, 16) for x in sys.argv[1:]]:
    off = rva2off(rva)
    if off < 0:
        print("bad rva %X" % rva)
        continue
    print("===== Game.dll + 0x%X (VA 0x%X) =====" % (rva, IB + rva))
    for ins in md.disasm(d[off:off+0x120], IB + rva):
        print("0x%08X: %-8s %s" % (ins.address, ins.mnemonic, ins.op_str))
