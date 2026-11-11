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
- Cobertura: los 4 592 ficheros estan todos por debajo de 0x92B000, es decir, solo el ejecutable principal (el decomp tiene 4 573 funciones ahi). Los 13 overlays (`TBL/*MOD.BIN`, ventanas que se solapan en 0x92B000, 0x9C4800, 0x9DF000 y 0xA0F800) se traducen aparte con `scripts/translate_overlays.sh`: cada `MWo3` se empaqueta como ELF minimo (`scripts/make_overlay_elf.py`, simbolos y tamanos del desensamblado del decomp, prefijo del modulo en cada nombre). Resultado: 2 850 funciones; 1 error de datos tomados por codigo (cadena ASCII en `optmod`, `sub_0093D000`) que una segunda pasada con `skip` elimina: 0 errores en los 13 modulos. Falta el cargador en el runtime que mapee el overlay en su ventana y despache por modulo.
- Siguiente: construir el runtime, enlazar y llegar a `main` con memoria y syscalls minimos.

Reproducir: `scripts/translate.sh RUTA/AL/SCES_504.11`.

## Estado de la recompilacion (2026-11-10)
- Traducido: 4 592 funciones del ejecutable principal + 2 850 de los 13 overlays, 0 errores tras saltar las areas de datos.
- Hecho: tablas por modulo (`scripts/prefix_tables.py`), parche de lookup de overlays para el runtime (`patches/`).
- Falta: compilar el runtime con el parche, enlazar y llegar a `main`; despues GS/VU1/DMA, audio y entrada. Detalle en `docs/OVERLAYS.md`.

## Build del runner (estado)

`runner/CMakeLists.txt` compila el runtime de PS2Recomp (dependencia externa en `third_party/`) junto con el codigo generado
del ejecutable principal y de los 13 overlays (un objeto por modulo) y `work/overlay_registry.cpp`.
Resultado: `build/stake/stake_recomp` enlaza en macOS arm64 (sse2neon, sin FFmpeg). Si se regeneran los overlays hay que repetir
`DISC=<disc> python3 scripts/prefix_tables.py work/ov work/overlay_registry.cpp` antes de compilar.
Primer arranque (`stake_recomp <elf>`): carga el ELF, abre ventana y audio y ejecuta `PowerOnInit`. El juego se quedaba en `nlDmaSendToMFifo`
esperando a que VIF1 consumiera el anillo MFIFO; `patches/ps2recomp-mfifo-vif1.patch` lo implementa (SPR->anillo con RBSR/RBOR y VIF1 lee los tags hasta MADR).
Con el parche el juego avanza y sube frames. Pendiente: ver que dibuja (GS/VU1), CDVD/AFS, audio, pad.

### Hallazgos al arrancar (2026-11-10)
- El disco hay que extraerlo **completo** junto al ELF (`bsdtar -xf <iso>`): el juego pide `0CFLIST.DIR`, `FS/SYS/*.IRX`, `FS/SND/*` ademas de `MOD/` y `TBL/`. El runtime toma la raiz del CD de la carpeta del ELF, asi que se lanza con `stake_recomp <disco>/SCES_504.11`.
- Los overlays tienen codigo dentro de su seccion de datos (`__sinit_*.cc`, thunks `@..__dt__`): `make_overlay_elf.py` ahora los toma de `config/ov_<mod>_syms.txt` y amplia `.text` para cubrirlos.
- gamemod y optmod comparten la ventana 0x92B000: el modulo activo se distingue por el idx de la cabecera MWo3 (+4).
- `patches/ps2recomp-dma-recvn.patch`: implementa `sceDmaRecvN` (el juego salia con "Unimplemented PS2 stub").
- Estado del primer arranque: tras la espera de `guncon2_init` (~70 s en este build; es un delay por cuenta) el juego llega a `mwPlySwitchToIdle` (Sofdec, 0x18e3d8) y espera al decodificador de video. Ventana aun negra. Siguiente: IPU/MPEG (FFmpeg esta desactivado en este build) o saltar la pelicula de arranque.
- `runner/stake_overrides.cpp` (mecanismo `PS2_REGISTER_GAME_OVERRIDE`, como en otros proyectos de PS2Recomp): `ADXM_WaitVsync` (0x16D690) se sustituye por una espera de 16 ms porque el runtime no entrega aun la interrupcion de vsync al hilo de ADX.
  Con eso el juego sale de la pelicula de arranque y corre su bucle principal (`ps2_main`, `Synch__Fv` una vez por fotograma), pero la ventana sigue negra.
- Diagnostico de la pantalla negra: `gif` se queda en 130 mientras `dma` sube; en el flujo VIF1 aparecen `MSCAL` con pc imposibles (>0x4000) y ningun `MPG`. Hipotesis: los microprogramas de VU1 no se cargan, o el flujo VIF1 (UNPACK/tags con TTE) se desalinea. Siguiente: volcar el flujo VIF1 de un fotograma y compararlo con PCSX2.

## Licencia

Este repositorio se distribuye bajo **GPL-3.0** (ver `LICENSE`). Los parches incluyen codigo derivado de PS2Recomp y de la IPU de
PCSX2/fixer-ipu (ambos GPL-3.0). No contiene el juego, la ISO, la BIOS ni assets: hace falta una copia propia del disco.
