#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ART="${SCRIPT_DIR}/build/PedagogicalCompressor_artefacts/Release"
if [ ! -d "$ART" ]; then echo "ERROR: no existe $ART - compila primero con ./build-macos.sh"; exit 1; fi
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
CLAP_DIR="$HOME/Library/Audio/Plug-Ins/CLAP"
APP_DIR="/Applications"
rm -rf "$VST3_DIR/Pedagogical Compressor.vst3" \
       "$AU_DIR/Pedagogical Compressor.component" \
       "$CLAP_DIR/Pedagogical Compressor.clap" \
       "$APP_DIR/Pedagogical Compressor.app"
mkdir -p "$VST3_DIR" "$AU_DIR" "$CLAP_DIR"
[ -d "$ART/VST3/Pedagogical Compressor.vst3" ] && cp -R "$ART/VST3/Pedagogical Compressor.vst3" "$VST3_DIR/" && echo "  VST3 -> $VST3_DIR/"
[ -d "$ART/AU/Pedagogical Compressor.component" ] && cp -R "$ART/AU/Pedagogical Compressor.component" "$AU_DIR/" && echo "  AU -> $AU_DIR/"
[ -d "$ART/CLAP/Pedagogical Compressor.clap" ] && cp -R "$ART/CLAP/Pedagogical Compressor.clap" "$CLAP_DIR/" && echo "  CLAP -> $CLAP_DIR/"
[ -d "$ART/Standalone/Pedagogical Compressor.app" ] && cp -R "$ART/Standalone/Pedagogical Compressor.app" "$APP_DIR/" && echo "  App -> /Applications/"
echo "Instalacion completa."
