#!/usr/bin/env bash
# build-macos.sh — Compila el Pedagogical Compressor en macOS (Universal Binary)
#
# Requisitos:
#   - Xcode 14+ (con Command Line Tools)
#   - CMake 3.22+  (brew install cmake)
#
# Genera binario universal (Apple Silicon arm64 + Intel x86_64)
# Formatos: VST3, AU, CLAP, Standalone
#
# Uso:
#   chmod +x build-macos.sh
#   ./build-macos.sh            # compila
#   ./build-macos.sh --clean    # borra build/ y compila desde cero
#   ./install-macos.sh          # instala en ~/Library/Audio/Plug-Ins

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
BUILD_TYPE="${BUILD_TYPE:-Release}"
CLEAN=0
[ "${1:-}" = "--clean" ] && CLEAN=1

echo "──────────────────────────────────────────────────────────"
echo " Pedagogical Compressor — Build script (macOS Universal)"
echo "──────────────────────────────────────────────────────────"

# Instalar CMake si falta
if ! command -v cmake >/dev/null 2>&1; then
    if command -v brew >/dev/null 2>&1; then
        echo "→ Instalando CMake vía Homebrew…"
        brew install cmake
    else
        echo "ERROR: CMake no encontrado y Homebrew no instalado." >&2
        echo "Instala Homebrew desde https://brew.sh o CMake desde https://cmake.org" >&2
        exit 1
    fi
fi

# Seleccionar Xcode si hay varios
if [ -d /Applications/Xcode.app ]; then
    sudo xcode-select -s /Applications/Xcode.app 2>/dev/null || true
fi

if [ "$CLEAN" = "1" ] && [ -d "$BUILD_DIR" ]; then
    echo "→ Borrando build/ anterior…"
    rm -rf "$BUILD_DIR"
fi

# Configurar CMake (Universal Binary: arm64 + x86_64)
echo "→ Configurando CMake (Universal arm64+x86_64)…"
cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0" \
      -S "${PROJECT_DIR}"

# Compilar
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
echo "→ Compilando con ${JOBS} hilos… (la 1ª vez tarda varios minutos)"
cmake --build "${BUILD_DIR}" -j"${JOBS}" --config "${BUILD_TYPE}"

echo ""
echo "──────────────────────────────────────────────────────────"
echo " ✔ Build completo. Binarios en:"
echo "   ${BUILD_DIR}/PedagogicalCompressor_artefacts/${BUILD_TYPE}/VST3/"
echo "   ${BUILD_DIR}/PedagogicalCompressor_artefacts/${BUILD_TYPE}/AU/"
echo "   ${BUILD_DIR}/PedagogicalCompressor_artefacts/${BUILD_TYPE}/CLAP/"
echo "   ${BUILD_DIR}/PedagogicalCompressor_artefacts/${BUILD_TYPE}/Standalone/"
echo ""
echo " Para instalar:  ./install-macos.sh"
echo "──────────────────────────────────────────────────────────"
