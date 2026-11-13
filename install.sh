#!/usr/bin/env bash
# install.sh — Instala los 4 formatos del Pedagogical Compressor en Linux.
#
# IMPORTANTE: en Linux el layout es DISTINTO según el formato:
#   VST3        -> bundle/directorio  (.vst3 es una carpeta)
#   LV2         -> bundle/directorio  (.lv2 es una carpeta con .so + manifest.ttl)
#   CLAP        -> ARCHIVO ÚNICO ELF  (.clap es un .so renombrado, NO es carpeta)
#   Standalone  -> binario ejecutable
#
# Bug previo: usábamos `[ -d ]` para CLAP, pero en Linux es archivo, no carpeta,
# así que la condición fallaba y el CLAP nunca se copiaba a ~/.clap/.  Ahora se
# prueba `-f` para archivos y `-d` para directorios según corresponda.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ART="${SCRIPT_DIR}/build/PedagogicalCompressor_artefacts/Release"

if [ ! -d "$ART" ]; then
    echo "ERROR: no existe $ART"
    echo "Compila primero:  ./build.sh"
    exit 1
fi

echo "→ Eliminando versiones anteriores…"
rm -rf "$HOME/.vst3/Pedagogical Compressor.vst3" \
       "$HOME/.lv2/Pedagogical Compressor.lv2" \
       "$HOME/.lv2/PedagogicalCompressor.lv2" \
       "$HOME/.clap/Pedagogical Compressor.clap" \
       "$HOME/.local/bin/PedagogicalCompressor"

echo "→ Copiando plugins…"
mkdir -p "$HOME/.vst3" "$HOME/.lv2" "$HOME/.clap" "$HOME/.local/bin"

# ─── VST3 (bundle/directorio) ────────────────────────────────────────────
if [ -d "$ART/VST3/Pedagogical Compressor.vst3" ]; then
    cp -r "$ART/VST3/Pedagogical Compressor.vst3" "$HOME/.vst3/"
    chmod -R +rX "$HOME/.vst3/Pedagogical Compressor.vst3"
    echo "  ✔ VST3       → ~/.vst3/"
else
    echo "  ✗ VST3 no encontrado en $ART/VST3/"
fi

# ─── LV2 (bundle/directorio con .so + manifest.ttl) ─────────────────────
# JUCE lo genera como "Pedagogical Compressor.lv2"; lo instalamos con ese
# mismo nombre.  REAPER/LV2 hosts escanean ~/.lv2/ automáticamente.
if [ -d "$ART/LV2/Pedagogical Compressor.lv2" ]; then
    cp -r "$ART/LV2/Pedagogical Compressor.lv2" "$HOME/.lv2/"
    chmod -R +rX "$HOME/.lv2/Pedagogical Compressor.lv2"
    echo "  ✔ LV2        → ~/.lv2/Pedagogical Compressor.lv2"
else
    echo "  ✗ LV2 no encontrado en $ART/LV2/"
fi

# ─── CLAP (ARCHIVO ÚNICO ELF en Linux, no es bundle) ────────────────────
# En Linux, el wrapper CLAP de clap-juce-extensions produce un .so renombrado
# a .clap.  No es una carpeta.  Debe ir a ~/.clap/ (REAPER y Bitwig escanean
# ahí por defecto; CLAP spec también lo recomienda).
if [ -f "$ART/CLAP/Pedagogical Compressor.clap" ]; then
    cp "$ART/CLAP/Pedagogical Compressor.clap" "$HOME/.clap/"
    chmod +x "$HOME/.clap/Pedagogical Compressor.clap"
    echo "  ✔ CLAP       → ~/.clap/Pedagogical Compressor.clap"
else
    echo "  ✗ CLAP no encontrado en $ART/CLAP/"
fi

# ─── Standalone ──────────────────────────────────────────────────────────
if [ -f "$ART/Standalone/Pedagogical Compressor" ]; then
    cp "$ART/Standalone/Pedagogical Compressor" "$HOME/.local/bin/PedagogicalCompressor"
    chmod +x "$HOME/.local/bin/PedagogicalCompressor"
    echo "  ✔ Standalone → ~/.local/bin/PedagogicalCompressor"
else
    echo "  ✗ Standalone no encontrado en $ART/Standalone/"
fi

echo ""
echo "──────────────────────────────────────────────────────────"
echo " ✔ Instalación completa. Resumen de rutas escaneadas:"
echo "    ~/.vst3/                                (VST3)"
echo "    ~/.lv2/                                 (LV2)"
echo "    ~/.clap/                                (CLAP)"
echo "    ~/.local/bin/PedagogicalCompressor       (Standalone)"
echo "──────────────────────────────────────────────────────────"
echo ""
echo " REAPER:  Options → Preferences → Plug-ins →"
echo "          VST → Añade '~/.vst3' si no está → Re-scan"
echo "          LV2 → Re-scan"
echo "          CLAP → Re-scan  (REAPER ≥ 6.71 escanea ~/.clap por defecto)"
echo ""
echo " Bitwig Studio escanea ~/.clap y ~/.vst3 automáticamente."
echo " El standalone se lanza con:  PedagogicalCompressor"
