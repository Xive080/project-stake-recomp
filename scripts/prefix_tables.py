#!/usr/bin/env python3
"""Renombra las tablas de funciones de cada overlay (todas se llaman g_ps2RecompiledFunctionTable*) para que puedan enlazarse juntas,
y genera overlay_registry.cpp con una tabla {idx de modulo -> base, fin, tabla}.
Uso: prefix_tables.py DIR_OV SALIDA_overlay_registry.cpp   (DIR_OV = work/ov, con <mod>/output/register_functions.cpp)"""
import re, struct, sys, os, glob
ov, out = sys.argv[1:3]
mods = sorted(d for d in os.listdir(ov) if os.path.exists(os.path.join(ov, d, 'output', 'register_functions.cpp')))
rows = []
for m in mods:
    rf = os.path.join(ov, m, 'output', 'register_functions.cpp')
    s = open(rf).read()
    if 'g_ps2RecompiledFunctionTable' in s:
        s = s.replace('g_ps2RecompiledFunctionTable', f'g_{m}_FuncTable')
        s = s.replace('GeneratedFunctionTableInitializer', f'Init_{m}')
        s = re.sub(r'g_%s_FuncTableSlotCount' % m, f'g_{m}_FuncTableSlotCount', s)
        open(rf, 'w').write(s)
    base = int(re.search(r'g_%s_FuncTableBase = (0x[0-9a-f]+)u' % m, s).group(1), 16)
    end = int(re.search(r'g_%s_FuncTableEnd = (0x[0-9a-f]+)u' % m, s).group(1), 16)
    cnt = int(re.search(r'g_%s_FuncTableSlotCount = (\d+)u' % m, s).group(1))
    binp = os.path.join(os.environ.get('DISC', ''), 'TBL', m.upper() + '.BIN')
    idx = struct.unpack_from('<I', open(binp, 'rb').read(8), 4)[0] if os.path.exists(binp) else -1
    rows.append((idx, m, base, end, cnt))
rows.sort()
with open(out, 'w') as f:
    f.write('// Generado por scripts/prefix_tables.py: no editar.\n#include "ps2_runtime.h"\n#include <cstdint>\n')
    for _, m, _, _, cnt in rows:
        f.write(f'extern const uint32_t g_{m}_FuncTableBase, g_{m}_FuncTableEnd;\nextern PS2Runtime::RecompiledFunction g_{m}_FuncTable[{cnt}u];\n')
    f.write('struct OverlayTable { int idx; const char* name; uint32_t base, end; PS2Runtime::RecompiledFunction* table; };\n')
    f.write('extern const OverlayTable g_overlayTables[] = {\n')
    for idx, m, _, _, _ in rows:
        f.write(f'  {{{idx}, "{m}", g_{m}_FuncTableBase, g_{m}_FuncTableEnd, g_{m}_FuncTable}},\n')
    f.write('};\nextern const int g_overlayTableCount = %d;\n' % len(rows))
print(len(rows), 'modulos')
