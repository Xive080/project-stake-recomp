#!/bin/sh
# Traduce los 13 overlays. Uso: scripts/translate_overlays.sh DIR_DISCO DIR_DECOMP
# DIR_DISCO contiene TBL/*MOD.BIN (tu disco extraido); DIR_DECOMP es el repo project-stake-decomp con asm/ generado.
set -e
cd "$(dirname "$0")/.."
DISC="$1"; DEC="$2"; B=build/ps2recomp
for bin in "$DISC"/TBL/*MOD.BIN; do
  m=$(basename "$bin" .BIN | tr 'A-Z' 'a-z')
  asm="$DEC/asm/$m/${m}_text.s"; [ -f "$asm" ] || { echo "$m: sin asm"; continue; }
  d=work/ov/$m; mkdir -p $d
  python3 scripts/make_overlay_elf.py "$bin" "$asm" $d/$m.elf $m "$DEC/config/ov_${m}_syms.txt"
  $B/ps2xAnalyzer/ps2_analyzer $d/$m.elf $d/$m.toml >/dev/null 2>&1
  sed -i.bak "s#^output = .*#output = \"$d/output/\"#" $d/$m.toml
  $B/ps2xRecomp/ps2_recomp $d/$m.toml > $d/run.log 2>&1 || true
  # segunda pasada: funciones con error = datos tomados por codigo (p. ej. cadenas ASCII); se saltan
  if grep -q '\[error\]' $d/run.log; then
    grep '\[error\]' $d/run.log | sed -E 's/.*function=([^ ]+).*/\1/' | sort -u > $d/errfuncs.txt
    python3 scripts/make_skip.py $d/$m.toml $d/errfuncs.txt $d/${m}_skip.toml
    sed -i.bak "s#^output = .*#output = \"$d/output/\"#" $d/${m}_skip.toml
    $B/ps2xRecomp/ps2_recomp $d/${m}_skip.toml > $d/run.log 2>&1 || true
  fi
  echo "$m: errores=$(grep -c '\[error\]' $d/run.log) ficheros=$(ls $d/output | wc -l)"
done
