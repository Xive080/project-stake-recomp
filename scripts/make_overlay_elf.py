#!/usr/bin/env python3
"""Empaqueta un overlay MWo3 (TBL/XXXMOD.BIN) como ELF32 MIPS minimo para PS2Recomp.
Uso: make_overlay_elf.py MOD.BIN ASM_TEXT.s SALIDA.elf PREFIJO
- MWo3: u32 +8 = direccion de carga, +0xC = tamano de .text; el fichero entero se carga en `load`; el codigo empieza en load+0x80.
- Simbolos y tamanos salen del desensamblado del decomp (`glabel` + `nonmatching nombre, tam`)."""
import re, struct, sys
binp, asmp, out, pre = sys.argv[1:5]
d = open(binp, 'rb').read()
assert d[:4] == b'MWo3'
load, tsize, dsize, bss = struct.unpack_from('<4I', d, 8)
funcs = []; sizes = {}
lines = open(asmp).read().splitlines()
for i, l in enumerate(lines):
    m = re.match(r'nonmatching (\S+), (0x[0-9A-Fa-f]+)', l)
    if m: sizes[m.group(1)] = int(m.group(2), 16)
    m = re.match(r'glabel (\S+)', l)
    if m:
        for l2 in lines[i+1:i+8]:
            mm = re.match(r'\s*/\* [0-9A-Fa-f]+ ([0-9A-Fa-f]{8}) ', l2)
            if mm: funcs.append((m.group(1), int(mm.group(1), 16))); break
strtab = b'\0'; syms = [b'\0' * 16]
for n, a in funcs:
    sz = sizes.get(n, 4)
    syms.append(struct.pack('<IIIBBH', len(strtab), a, sz, 0x12, 0, 1))
    strtab += (pre + '__' + n).encode() + b'\0'
shstr = b'\0.text\0.data\0.symtab\0.strtab\0.shstrtab\0'
nm = lambda s: shstr.index(s.encode())
DOFF = 0x100
text_a = load + 0x80
data_a = text_a + tsize
body = d
symtab = b''.join(syms)
off_sym = DOFF + len(body); off_str = off_sym + len(symtab); off_shs = off_str + len(strtab)
off_sh = (off_shs + len(shstr) + 3) & ~3
def sh(name, typ, flags, addr, off, size, link=0, info=0, align=4, ent=0):
    return struct.pack('<10I', name, typ, flags, addr, off, size, link, info, align, ent)
shdrs = (sh(0,0,0,0,0,0,0,0,0) +
         sh(nm('.text'),1,6,text_a,DOFF+0x80,tsize,align=16) +
         sh(nm('.data'),1,3,data_a,DOFF+0x80+tsize,dsize,align=16) +
         sh(nm('.symtab'),2,0,0,off_sym,len(symtab),link=4,info=1,ent=16) +
         sh(nm('.strtab'),3,0,0,off_str,len(strtab),align=1) +
         sh(nm('.shstrtab'),3,0,0,off_shs,len(shstr),align=1))
entry = funcs[0][1] if funcs else text_a
eh = b'\x7fELF\x01\x01\x01' + b'\0'*9 + struct.pack('<HHIIIIIHHHHHH', 2, 8, 1, entry, 52, off_sh, 0x20924001, 52, 32, 1, 40, 6, 5)
ph = struct.pack('<8I', 1, DOFF, load, load, len(body), len(body) + bss, 7, 16)
blob = bytearray(eh + ph)
blob += b'\0' * (DOFF - len(blob)); blob += body + symtab + strtab + shstr
blob += b'\0' * (off_sh - len(blob)); blob += shdrs
open(out, 'wb').write(blob)
print(f'{pre}: load=0x{load:X} text=0x{tsize:X} data=0x{dsize:X} funcs={len(funcs)}')
