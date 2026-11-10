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
