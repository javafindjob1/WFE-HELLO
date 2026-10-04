import struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

DLL = r"C:\3rd\wc3gamesearcher\WFE\Application\Libraries\build_tools\hello_teammates.dll"
d = open(DLL, "rb").read()
def u16(o): return struct.unpack_from("<H", d, o)[0]
def u32(o): return struct.unpack_from("<I", d, o)[0]

e = u32(0x3C)
optOff = e + 4 + 20
numSec = u16(e + 4 + 2)
sizeOpt = u16(e + 4 + 16)
secTab = optOff + sizeOpt
secs = []
for i in range(numSec):
    s = secTab + i*40
    name = d[s:s+8].split(b"\0")[0].decode("latin1")
    secs.append((name, u32(s+8), u32(s+12), u32(s+16), u32(s+20)))

text = next(x for x in secs if x[0] == '.text')
_, vsize, va, rawSize, rawPtr = text
code = d[rawPtr: rawPtr + rawSize]
base = 0x10000000 + va

md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = True

# find instructions referencing interesting immediates: 0x1388 (5000), 0xBB8 (3000), 0x10 (VK_SHIFT), 0xD (VK_RETURN)
interesting = {0x1388: "5000", 0xBB8: "3000", 0x10: "VK_SHIFT", 0x0D: "VK_RETURN", 0x68: "'h'", 0x65: "'e'", 0x6C: "'l'", 0x6F: "'o'"}
seen = set()
for ins in md.disasm(code, base):
    for op in ins.operands:
        if op.type == 2 and (op.imm & 0xFFFF) in interesting:
            key = (ins.address, op.imm & 0xFFFF)
            if key in seen: continue
            seen.add(key)
            print(f"0x{ins.address:08X}: {ins.mnemonic} {ins.op_str}  ; imm hint {interesting[op.imm & 0xFFFF]}")

print("--- done, %d hits ---" % len(seen))
