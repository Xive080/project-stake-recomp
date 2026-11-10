# Vampire Night - recompilacion estatica (PS2 PAL SCES-50411)

Traduce el codigo MIPS R5900 del ELF y de los overlays a C++ y lo enlaza con un runtime propio (memoria, syscalls, VU0/VU1, GIF/VIF/DMA, GS sobre OpenGL/GLES, AFS/ADX). Objetivo: Mac, Windows, Linux y Android.

Estado: sin empezar. Primer experimento: traducir el ELF principal con un recompilador existente y llegar a `main` con un runtime minimo, sin GS.

Simbolos, overlays, formatos de datos y notas: repo hermano `vampirenight-decomp` (docs/EMULADOR.md, docs/AFS.md, docs/ENGINE.md).

## Reglas
- No se versiona el ELF, el disco ni el codigo generado: se produce en local desde el disco del propio usuario.
- Sin rutas absolutas.
