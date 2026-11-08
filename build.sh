#!/usr/bin/env bash
# build.sh — Compila el Pedagogical Compressor para Linux en 1 comando.
#
# Uso:
#   chmod +x build.sh
#   ./build.sh            # compila (o reusa el build anterior)
#   ./build.sh --clean    # borra build/ y compila desde CERO (recomendado
#                         # tras reemplazar archivos del proyecto)
#
# Qué hace:
#   1. Detecta tu distro e instala dependencias (con sudo)
#   2. Configura CMake (descarga JUCE 8 + clap-juce-extensions automáticamente)
#   3. Compila VST3, LV2, CLAP y Standalone
#   4. Te dice cómo instalar con ./install.sh
#
# No necesitas instalar JUCE a mano — FetchContent lo descarga solo.

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
BUILD_TYPE="${BUILD_TYPE:-Release}"
CLEAN=0
[ "${1:-}" = "--clean" ] && CLEAN=1

echo "──────────────────────────────────────────────────────────"
echo " Pedagogical Compressor — Build script (Linux x86_64)"
echo "──────────────────────────────────────────────────────────"

# ─── 1. Detectar distro e instalar dependencias ────────────────────────
install_deps() {
    if command -v apt-get >/dev/null 2>&1; then
        echo "→ Instalando dependencias (Debian/Ubuntu/Mint)…"
        sudo apt-get update
        # Mint 22 / Ubuntu 24.04 usan webkit2gtk-4.1; versiones viejas 4.0
        sudo apt-get install -y --no-install-recommends \
            cmake git build-essential pkg-config \
            libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev \
            libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
            libglu1-mesa-dev mesa-common-dev \
            libasound2-dev \
            libwebkit2gtk-4.1-dev libcurl4-openssl-dev \
        || sudo apt-get install -y --no-install-recommends \
            cmake git build-essential pkg-config \
            libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev \
            libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
            libglu1-mesa-dev mesa-common-dev \
            libasound2-dev \
            webkit2gtk-4.0-dev libcurl4-openssl-dev
    elif command -v dnf >/dev/null 2>&1; then
        echo "→ Instalando dependencias (Fedora/RHEL)…"
        sudo dnf install -y cmake git gcc-c++ pkg-config \
            freetype-devel libX11-devel libXcomposite-devel libXcursor-devel \
            libXext-devel libXinerama-devel libXrandr-devel libXrender-devel \
            mesa-libGLU-devel mesa-libGL-devel \
            alsa-lib-devel \
            webkit2gtk4.1-devel libcurl-devel \
        || sudo dnf install -y cmake git gcc-c++ pkg-config \
            freetype-devel libX11-devel libXcomposite-devel libXcursor-devel \
            libXext-devel libXinerama-devel libXrandr-devel libXrender-devel \
            mesa-libGLU-devel mesa-libGL-devel \
            alsa-lib-devel \
            webkit2gtk4.0-devel libcurl-devel
    elif command -v pacman >/dev/null 2>&1; then
        echo "→ Instalando dependencias (Arch)…"
        sudo pacman -S --needed --noconfirm cmake git base-devel pkgconf \
            freetype2 libx11 libxcomposite libxcursor libxext libxinerama \
            libxrandr libxrender glu mesa \
            alsa-lib \
            webkit2gtk curl
    else
        echo "No pude detectar tu gestor de paquetes." >&2
        echo "Instala manualmente: cmake, g++, pkg-config, freetype2, x11 libs, mesa, alsa." >&2
        exit 1
    fi
}

if ! command -v cmake >/dev/null 2>&1; then
    echo "cmake no encontrado. Instalando dependencias…"
    install_deps
fi

# ─── 2. Limpieza opcional ──────────────────────────────────────────────
if [ "$CLEAN" = "1" ] && [ -d "$BUILD_DIR" ]; then
    echo "→ Borrando build/ anterior (clean build)…"
    rm -rf "$BUILD_DIR"
fi

# ─── 3. Configurar CMake ───────────────────────────────────────────────
echo "→ Configurando CMake (Build type: ${BUILD_TYPE})…"
cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" "${PROJECT_DIR}"

# ─── 4. Compilar en paralelo ───────────────────────────────────────────
JOBS="$(nproc 2>/dev/null || echo 4)"
echo "→ Compilando con ${JOBS} hilos… (la 1ª vez tarda varios minutos)"
cmake --build "${BUILD_DIR}" -j"${JOBS}"

# ─── 5. Mostrar dónde están los binarios ───────────────────────────────
ARTEFACTS="${BUILD_DIR}/PedagogicalCompressor_artefacts/${BUILD_TYPE}"
echo ""
echo "──────────────────────────────────────────────────────────"
echo " ✔ Build completo. Binarios en:"
echo "   ${ARTEFACTS}/VST3/"
echo "   ${ARTEFACTS}/LV2/"
echo "   ${ARTEFACTS}/CLAP/"
echo "   ${ARTEFACTS}/Standalone/"
echo ""
echo " Para instalar en ~/.vst3 ~/.lv2 ~/.clap ejecuta:"
echo "   ./install.sh"
echo "──────────────────────────────────────────────────────────"
