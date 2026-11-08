#!/usr/bin/env bash
# install.sh — Instala los 4 formatos del Pedagogical Compressor en Linux.
#
# Uso:   ./install.sh          (después de compilar con ./build.sh)
#
# Instala:
#   ~/.vst3/Pedagogical Compressor.vst3
#   ~/.lv2/PedagogicalCompressor.lv2
#   ~/.clap/Pedagogical Compressor.clap
#   ~/.local/bin/PedagogicalCompressor   (standalone)
#
# Borra primero cualquier versión anterior para no dejar duplicados.

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

if [ -d "$ART/VST3/Pedagogical Compressor.vst3" ]; then
    cp -r "$ART/VST3/Pedagogical Compressor.vst3" "$HOME/.vst3/"
    echo "  ✔ VST3  → ~/.vst3/"
fi

if [ -d "$ART/LV2/Pedagogical Compressor.lv2" ]; then
    # Se instala sin espacios en el nombre (LV2 se identifica por URI,
    # el nombre de la carpeta es libre y así es más amable con la shell)
    cp -r "$ART/LV2/Pedagogical Compressor.lv2" "$HOME/.lv2/PedagogicalCompressor.lv2"
    echo "  ✔ LV2   → ~/.lv2/PedagogicalCompressor.lv2"
fi

if [ -d "$ART/CLAP/Pedagogical Compressor.clap" ]; then
    cp -r "$ART/CLAP/Pedagogical Compressor.clap" "$HOME/.clap/"
    echo "  ✔ CLAP  → ~/.clap/"
fi

if [ -f "$ART/Standalone/Pedagogical Compressor" ]; then
    cp "$ART/Standalone/Pedagogical Compressor" "$HOME/.local/bin/PedagogicalCompressor"
    chmod +x "$HOME/.local/bin/PedagogicalCompressor"
    echo "  ✔ Standalone → ~/.local/bin/PedagogicalCompressor"
fi

echo ""
echo "✔ Instalación completa."
echo "  Abre REAPER → Options → Preferences → Plug-ins → Re-scan."
echo "  El standalone se lanza con:  PedagogicalCompressor"
