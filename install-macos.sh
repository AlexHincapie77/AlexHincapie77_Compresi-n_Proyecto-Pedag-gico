#!/usr/bin/env bash
# install-macos.sh — Instala el Pedagogical Compressor en macOS
#
# Instala en las rutas estándar macOS (per-user, sin sudo):
#   - VST3       → ~/Library/Audio/Plug-Ins/VST3/
#   - AU         → ~/Library/Audio/Plug-Ins/Components/
#   - CLAP       → ~/Library/Audio/Plug-Ins/CLAP/
#   - Standalone → /Applications/  (requiere sudo)
#
# Después de instalar:
#   - REAPER:    Preferences → Plug-ins → Re-scan (VST, AU, CLAP)
#   - Logic Pro: se escanea automáticamente al abrir
#   - Bitwig:    Settings → Locations → Plug-ins → Rescan
#
# Validación AU (opcional pero recomendada):
#   auval -vt aumf Pdcm Peda
# Esto valida el componente AU contra el suite oficial de Apple.

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

# ─── VST3 ──────────────────────────────────────────────────────────────
if [ -d "$ART/VST3/Pedagogical Compressor.vst3" ]; then
    cp -R "$ART/VST3/Pedagogical Compressor.vst3" "$VST3_DIR/"
    # Eliminar atributo quarantine para que Gatekeeper no bloquee
    xattr -dr com.apple.quarantine "$VST3_DIR/Pedagogical Compressor.vst3" 2>/dev/null || true
    echo "  ✔ VST3  → ~/Library/Audio/Plug-Ins/VST3/"
else
    echo "  ✗ VST3 no encontrado en $ART/VST3/"
fi

# ─── AU (Audio Unit para Logic Pro) ────────────────────────────────────
if [ -d "$ART/AU/Pedagogical Compressor.component" ]; then
    cp -R "$ART/AU/Pedagogical Compressor.component" "$AU_DIR/"
    xattr -dr com.apple.quarantine "$AU_DIR/Pedagogical Compressor.component" 2>/dev/null || true
    echo "  ✔ AU    → ~/Library/Audio/Plug-Ins/Components/"
else
    echo "  ✗ AU no encontrado en $ART/AU/"
fi

# ─── CLAP ──────────────────────────────────────────────────────────────
if [ -d "$ART/CLAP/Pedagogical Compressor.clap" ]; then
    cp -R "$ART/CLAP/Pedagogical Compressor.clap" "$CLAP_DIR/"
    xattr -dr com.apple.quarantine "$CLAP_DIR/Pedagogical Compressor.clap" 2>/dev/null || true
    echo "  ✔ CLAP  → ~/Library/Audio/Plug-Ins/CLAP/"
else
    echo "  ✗ CLAP no encontrado en $ART/CLAP/"
fi

# ─── Standalone (.app) ─────────────────────────────────────────────────
if [ -d "$ART/Standalone/Pedagogical Compressor.app" ]; then
    # /Applications requiere sudo si no eres owner
    if [ -w "$APP_DIR" ]; then
        cp -R "$ART/Standalone/Pedagogical Compressor.app" "$APP_DIR/"
        xattr -dr com.apple.quarantine "$APP_DIR/Pedagogical Compressor.app" 2>/dev/null || true
        echo "  ✔ App   → /Applications/"
    else
        echo "  ⚠ No se pudo copiar a /Applications/ (sin permisos)"
        echo "    Ejecuta con sudo:  sudo ./install-macos.sh"
        echo "    O copia manualmente el .app a /Applications/"
        # Alternativa: copiar a ~/Applications
        mkdir -p "$HOME/Applications"
        cp -R "$ART/Standalone/Pedagogical Compressor.app" "$HOME/Applications/"
        xattr -dr com.apple.quarantine "$HOME/Applications/Pedagogical Compressor.app" 2>/dev/null || true
        echo "  ✔ App   → ~/Applications/ (alternativa sin sudo)"
    fi
else
    echo "  ✗ Standalone no encontrado en $ART/Standalone/"
fi

# ─── Validación AU con auval ──────────────────────────────────────────
echo ""
echo "→ Validación AU recomendada (para Logic Pro):"
echo "    auval -vt aumf Pdcm Peda"
echo "  Esto ejecuta el suite de validación oficial de Apple."
echo "  Si pasa, Logic Pro lo listará entre los plugins disponibles."
echo "  Si falla, abre la consola y revisa el log del componente."

echo ""
echo "✔ Instalación completa."
echo "  REAPER:    Preferences → Plug-ins → Re-scan (VST3, AU, CLAP)"
echo "  Logic Pro: se escanea automáticamente al abrir (si auval pasa)"
echo "  Bitwig:    Settings → Locations → Plug-ins → Rescan"
echo "  Standalone: abre desde /Applications/Pedagogical Compressor.app"
