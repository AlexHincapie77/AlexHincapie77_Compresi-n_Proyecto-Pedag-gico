#!/usr/bin/env bash
set -euo pipefail
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
BUILD_TYPE="${BUILD_TYPE:-Release}"
CLEAN=0
[ "${1:-}" = "--clean" ] && CLEAN=1
echo "Pedagogical Compressor - Build (macOS Universal)"
if ! command -v cmake >/dev/null 2>&1; then
    if command -v brew >/dev/null 2>&1; then brew install cmake; else exit 1; fi
fi
if [ -d /Applications/Xcode.app ]; then sudo xcode-select -s /Applications/Xcode.app 2>/dev/null || true; fi
if [ "$CLEAN" = "1" ] && [ -d "$BUILD_DIR" ]; then rm -rf "$BUILD_DIR"; fi
cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0" \
      -S "${PROJECT_DIR}"
cmake --build "${BUILD_DIR}" -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)" --config "${BUILD_TYPE}"
echo "Build OK. Instala con: ./install-macos.sh"
