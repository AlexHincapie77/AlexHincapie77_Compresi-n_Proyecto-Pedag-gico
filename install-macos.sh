#!/usr/bin/env bash
# install-macos.sh — Instala el Pedagogical Compressor en macOS
#
# Uso:   ./install-macos.sh   (después de compilar con ./build-macos.sh)
#
# Instala:
#   ~/Library/Audio/Plug-Ins/VST3/Pedagogical Compressor.vst3
#   ~/Library/Audio/Plug-Ins/Components/Pedagogical Compressor.component   (AU)
#   ~/Library/Audio/Plug-Ins/CLAP/Pedagogical Compressor.clap
#   /Applications/Pedagogical Compressor.app                              (Standalone)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ART="${SCRIPT_DIR}/build/PedagogicalCompressor_artefacts/Release"

if [ ! -d "$ART" ]; then
    echo "ERROR: no existe $ART"
    echo "Compila primero:  ./build-macos.sh"
    exit 1
fi

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
CLAP_DIR="$HOME/Library/Audio/Plug-Ins/CLAP"
APP_DIR="/Applications"

echo "→ Eliminando versiones anteriores…"
rm -rf "$VST3_DIR/Pedagogical Compressor.vst3" \
       "$AU_DIR/Pedagogical Compressor.component" \
       "$CLAP_DIR/Pedagogical Compressor.clap" \
       "$APP_DIR/Pedagogical Compressor.app"

echo "→ Copiando plugins…"
mkdir -p "$VST3_DIR" "$AU_DIR" "$CLAP_DIR"

if [ -d "$ART/VST3/Pedagogical Compressor.vst3" ]; then
    cp -R "$ART/VST3/Pedagogical Compressor.vst3" "$VST3_DIR/"
    echo "  ✔ VST3  → ~/Library/Audio/Plug-Ins/VST3/"
fi

if [ -d "$ART/AU/Pedagogical Compressor.component" ]; then
    cp -R "$ART/AU/Pedagogical Compressor.component" "$AU_DIR/"
    echo "  ✔ AU    → ~/Library/Audio/Plug-Ins/Components/"
fi

if [ -d "$ART/CLAP/Pedagogical Compressor.clap" ]; then
    cp -R "$ART/CLAP/Pedagogical Compressor.clap" "$CLAP_DIR/"
    echo "  ✔ CLAP  → ~/Library/Audio/Plug-Ins/CLAP/"
fi

if [ -d "$ART/Standalone/Pedagogical Compressor.app" ]; then
    cp -R "$ART/Standalone/Pedagogical Compressor.app" "$APP_DIR/"
    echo "  ✔ App   → /Applications/"
fi

echo ""
echo "✔ Instalación completa."
echo "  En REAPER/Logic: re-escanea plug-ins si no aparecen."
echo "  Standalone: abre desde /Applications/Pedagogical Compressor.app"
