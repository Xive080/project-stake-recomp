# Overlays en el runtime (diseno, sin implementar)

Hallazgos leyendo PS2Recomp (third_party/, no versionado):
- `PS2Runtime::lookupFunction(addr)` busca en una tabla plana generada, `g_ps2RecompiledFunctionTable`, indexada por direccion (`generatedFunctionTableSlot`, con base y fin). Cubre un unico rango. El ELF principal ocupa 0x140000..0x92B000.
- Los overlays caen en ventanas que se solapan por encima de esa base: 0x92B000 (gamemod/optmod), 0x9C4800 (hnt, bs2-6, shp), 0x9DF000 (ao/trn) y 0xA0F800 (ac/org). Con una tabla plana dos modulos de la misma ventana chocarian.
- El runtime ya trae un backend de GS en CPU (`gs/`), un interprete de VIF1 y de VU1 (`vu/`, `ps2_vif1_interpreter.cpp`), audio y pad. No hay que escribirlos desde cero; si hay que adaptarlos.

Diseno propuesto:
1. Cada modulo genera su propia tabla `{direccion -> funcion}` con prefijo de modulo (ya lo hace `make_overlay_elf.py`).
2. El runtime guarda, por ventana, el modulo cargado ahora. `lookupFunction` mira primero la tabla del modulo activo de esa ventana y despues la tabla del ejecutable principal.
3. Se intercepta la rutina del juego que lee `TBL/*MOD.BIN` en la ventana (clase `OverlayMgrClass`) para fijar el modulo activo.
4. Si un modulo se descarga o se sustituye, la tabla de esa ventana se vacia: una direccion sin modulo cargado es un error explicito, no una llamada a ciegas.

Pendiente de comprobar: como detecta el juego el cambio de overlay (llamadas a `updateEntryStat__15OverlayMgrClassFii` y a la lectura de disco) y si algun overlay mantiene punteros a funciones de otro de la misma ventana.

## Como carga el juego un overlay (leido del desensamblado del decomp)
`OverlayMgrClass::loadModule(this, idx)` (0x201960):
1. Si el estado del modulo `idx` ya es 2, no hace nada.
2. Abre el fichero del modulo (`getFileName`, tabla en 0x26D630 + (idx+11)*16) con `ADXF_Open`.
3. Estado 1. La direccion de carga sale de la tabla de u32 en 0x2D8070 indexada por `idx`.
4. Lee el fichero entero con `ADXF_ReadNw32` en esa direccion y espera con `apADXF_WaitReadFinish`.
5. Llama a `mwOverlayInit(direccion, sectores << 11)`, `FlushCache(0)` y pone el estado en 2.

Conclusion: el punto de enganche del runtime es `mwOverlayInit(addr, size)`. El `idx` del modulo esta en la cabecera MWo3 (u32 en +4), asi que basta leer `addr+4` de la memoria guest y activar el modulo cuya tabla corresponde.

Indices (cabecera MWo3, coinciden con la tabla 0x2D8070): 1 gamemod 0x92B000; 2 hntmod 0x9C4800; 3 optmod 0x92B000; 4..8 bs2mod..bs6mod 0x9C4800; 9 shpmod 0x9C4800; 10 aomod 0x9DF000; 11 acmod 0xA0F800; 12 orgmod 0xA0F800; 13 trnmod 0x9DF000.

## Tablas por modulo (hecho)
`scripts/prefix_tables.py` renombra en cada `work/ov/<mod>/output/register_functions.cpp` la tabla `g_ps2RecompiledFunctionTable*` a `g_<mod>_FuncTable*` (si no, 13 modulos definen el mismo simbolo) y genera `overlay_registry.cpp` con `g_overlayTables[]` = {idx, nombre, base, fin, tabla}, el idx leido de la cabecera MWo3 (variable de entorno `DISC`). Los 13 modulos salen con su idx (1..13). Falta: que `lookupFunction` use esa tabla para direcciones >= 0x92B000 segun el modulo activo de la ventana.
