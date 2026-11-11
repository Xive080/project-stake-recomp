#!/bin/sh
# Traduce el ELF principal con PS2Recomp (dependencia externa en third_party/, GPL-3.0).
# Uso: scripts/translate.sh RUTA/AL/SCES_504.11   (el ELF sale de tu propio disco; no se versiona)
set -e
cd "$(dirname "$0")/.."
ELF="$1"; [ -f "$ELF" ] || { echo "uso: $0 SCES_504.11"; exit 1; }
B=build/ps2recomp
[ -x $B/ps2xRecomp/ps2_recomp ] || {
  git clone --depth 1 https://github.com/ran-j/PS2Recomp third_party/PS2Recomp
  (cd third_party/PS2Recomp && git submodule update --init --depth 1)
  cmake -S third_party/PS2Recomp -B $B -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DPS2X_BUILD_RUNTIME=OFF -DPS2X_BUILD_STUDIO=OFF -DPS2X_BUILD_TEST=OFF -DPS2X_BUILD_ANALYZER=ON
  cmake --build $B
}
mkdir -p work; cp "$ELF" work/game.elf
$B/ps2xAnalyzer/ps2_analyzer work/game.elf work/game.toml
# El analizador marca como "codigo automodificable" todo `sw` seguido de j/jal cuyo destino cae dentro del
# segmento cargable, y lo sustituye por nop. En este ELF el segmento incluye .data, asi que los 51 casos son
# escrituras normales a globales de sceMpeg/sceCd/MPS/ADX (p.ej. 0x182520 guarda el puntero de trabajo de MPS,
# sin el cual MPS_Create devuelve 0 y Sofdec no arranca). El juego no se automodifica: se quitan esos parches.
sed -i.bak '/# Potential self-modifying code/d' work/game.toml && rm -f work/game.toml.bak
$B/ps2xRecomp/ps2_recomp work/game.toml > work/run.log 2>&1 || true
echo "primera pasada, errores: $(grep -c '\[error\]' work/run.log)"
# segunda pasada: las funciones con error son datos tomados por codigo; se saltan
grep '\[error\]' work/run.log | sed -E 's/.*function=([^ ]+).*/\1/' | sort -u > work/errfuncs.txt
python3 scripts/make_skip.py work/game.toml work/errfuncs.txt work/game_skip.toml
$B/ps2xRecomp/ps2_recomp work/game_skip.toml > work/run2.log 2>&1 || true
echo "segunda pasada, errores: $(grep -c '\[error\]' work/run2.log)"
