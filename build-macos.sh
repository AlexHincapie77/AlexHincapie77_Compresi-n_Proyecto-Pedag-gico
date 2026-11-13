#!/usr/bin/env bash
# build-macos.sh — Compila el Pedagogical Compressor en macOS (Universal Binary)
#
# Genera un binario universal (arm64 + x86_64) compatible con:
#   - Apple Silicon (M1/M2/M3/M4)
#   - Intel Macs (desde macOS 11 Big Sur en adelante)
#
# Formatos producidos:
#   - VST3       → ~/Library/Audio/Plug-Ins/VST3/
#   - AU         → ~/Library/Audio/Plug-Ins/Components/  (para Logic Pro, GarageBand)
#   - CLAP       → ~/Library/Audio/Plug-Ins/CLAP/
#   - Standalone → /Applications/  (.app bundle)

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
BUILD_TYPE="${BUILD_TYPE:-Release}"
CLEAN=0
[ "${1:-}" = "--clean" ] && CLEAN=1

echo "──────────────────────────────────────────────────────────"
echo " Pedagogical Compressor — Build script (macOS Universal)"
echo "──────────────────────────────────────────────────────────"

# ─── 1. Verificar Xcode ────────────────────────────────────────────────
if ! command -v xcode-select >/dev/null 2>&1; then
    echo "ERROR: Xcode Command Line Tools no instalado." >&2
    echo "  Instala con:  xcode-select --install" >&2
    echo "  O descarga Xcode desde https://developer.apple.com/xcode/" >&2
    exit 1
fi

XCODE_PATH="$(xcode-select -p 2>/dev/null || echo "")"
if [ -z "$XCODE_PATH" ]; then
    echo "ERROR: no se pudo determinar la ruta de Xcode." >&2
    echo "  Ejecuta:  sudo xcode-select -s /Applications/Xcode.app/Contents/Developer" >&2
    exit 1
fi
echo "→ Usando Xcode en: $XCODE_PATH"

if [ -d /Applications/Xcode.app ]; then
    sudo xcode-select -s /Applications/Xcode.app 2>/dev/null || true
fi

# ─── 2. Verificar CMake ───────────────────────────────────────────────
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

# ─── 3. Limpieza opcional ─────────────────────────────────────────────
if [ "$CLEAN" = "1" ] && [ -d "$BUILD_DIR" ]; then
    echo "→ Borrando build/ anterior…"
    rm -rf "$BUILD_DIR"
fi

# ─── 4. Configurar CMake (Universal arm64+x86_64) ────────────────────
echo "→ Configurando CMake (Universal arm64+x86_64, macOS 11.0+)…"
cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0" \
      -S "${PROJECT_DIR}"

# ─── 5. Compilar en paralelo ──────────────────────────────────────────
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
echo "→ Compilando con ${JOBS} hilos… (la 1ª vez tarda varios minutos)"
cmake --build "${BUILD_DIR}" -j"${JOBS}" --config "${BUILD_TYPE}"

# ─── 6. Verificar que los binarios se generaron ───────────────────────
ART="${BUILD_DIR}/PedagogicalCompressor_artefacts/${BUILD_TYPE}"
echo ""
echo "→ Verificando artefactos generados…"
for fmt in VST3 AU CLAP Standalone; do
    if [ -d "${ART}/${fmt}" ]; then
        echo "  ✔ ${fmt} generado"
    else
        echo "  ✗ ${fmt} NO generado — revisa el log de compilación arriba"
    fi
done

# ─── 7. Verificar arquitectura de los binarios ────────────────────────
echo ""
echo "→ Verificando arquitectura del VST3…"
SO_VST3="${ART}/VST3/Pedagogical Compressor.vst3/Contents/MacOS/PedagogicalCompressor"
if [ -f "$SO_VST3" ]; then
    ARCHS="$(file "$SO_VST3" 2>/dev/null | grep -oE 'arm64|x86_64' | sort -u | tr '\n' ' ')"
    if echo "$ARCHS" | grep -q 'arm64' && echo "$ARCHS" | grep -q 'x86_64'; then
        echo "  ✔ Binario universal: $ARCHS"
    else
        echo "  ⚠ Solo una arquitectura: $ARCHS"
        echo "    El binario NO es universal — revisa CMAKE_OSX_ARCHITECTURES"
    fi
else
    echo "  ⚠ No se pudo verificar el .so interno del VST3"
fi

echo ""
echo "──────────────────────────────────────────────────────────"
echo " ✔ Build completo. Binarios en:"
echo "   ${ART}/VST3/"
echo "   ${ART}/AU/"
echo "   ${ART}/CLAP/"
echo "   ${ART}/Standalone/"
echo ""
echo " Para instalar:  ./install-macos.sh"
echo "──────────────────────────────────────────────────────────"
