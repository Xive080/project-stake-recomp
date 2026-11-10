# Vampire Night - recompilacion estatica (PS2 PAL SCES-50411)

Traduce el codigo MIPS R5900 del ELF y de los overlays a C++ y lo enlaza con un runtime propio (memoria, syscalls, VU0/VU1, GIF/VIF/DMA, GS sobre OpenGL/GLES, AFS/ADX). Objetivo: Mac, Windows, Linux y Android.

Estado: sin empezar. Primer experimento: traducir el ELF principal con un recompilador existente y llegar a `main` con un runtime minimo, sin GS.

Simbolos, overlays, formatos de datos y notas: repo hermano `vampirenight-decomp` (docs/EMULADOR.md, docs/AFS.md, docs/ENGINE.md).

## Reglas
- No se versiona el ELF, el disco ni el codigo generado: se produce en local desde el disco del propio usuario.
- Sin rutas absolutas.

## Resultado del primer experimento (2026-11-10)
Con PS2Recomp (analizador + recompilador, sin runtime) sobre el ejecutable principal PAL:
- Se detectan 4 592 funciones y se generan 4 592 ficheros C++ (103 MB) en `work/output/`.
- 41 funciones dan error de instruccion no soportada; ninguna tiene nombre en la tabla de simbolos del ELF (todas son `sub_XXXXXXXX`, direcciones entre 0x1C000C y 0x2C0048). Comprobado contra el desensamblado del decomp: son datos (`D_002AB580`, `D_0026E138`, `.word` con 'invalid instruction'), no funciones. Se excluyen con `skip` (scripts/make_skip.py) y la segunda pasada da 0 errores.
- 4 551 funciones se traducen sin errores. Esto es solo traduccion: sin runtime, nada se ejecuta todavia.
- Cobertura: los 4 592 ficheros estan todos por debajo de 0x92B000, es decir, solo el ejecutable principal (el decomp tiene 4 573 funciones ahi). Los 13 overlays (2 861 funciones, `TBL/*MOD.BIN`, ventanas que se solapan en 0x92B000, 0x9C4800, 0x9DF000 y 0xA0F800) NO se traducen todavia: necesitan una pasada por overlay, nombres separados por modulo y un cargador en el runtime que los mapee y despache.
- Siguiente: construir el runtime, enlazar y llegar a `main` con memoria y syscalls minimos.

Reproducir: `scripts/translate.sh RUTA/AL/SCES_504.11`.
