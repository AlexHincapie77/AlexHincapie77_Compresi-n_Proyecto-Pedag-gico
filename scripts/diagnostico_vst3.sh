#!/usr/bin/env bash
# ============================================================================
# diagnostico_vst3.sh — Diagnóstico completo del VST3 del Pedagogical Compressor
#
# Uso:  bash diagnostico_vst3.sh
#
# Verifica:
#   1. Si el VST3 existe en build/  (compilación)
#   2. Si está instalado en ~/.vst3/  (instalación)
#   3. La estructura del bundle  (Contents/x86_64-linux/...so)
#   4. Permisos de ejecución del .so interno
#   5. Dependencias dinámicas del .so  (ldd — busca "not found")
#   6. Rutas escaneadas por REAPER / Bitwig
#   7. Caché de plugins de REAPER  (reaper-vstplugins64.ini)
#   8. Si el DAW tiene listada la ruta ~/.vst3
#
# No modifica nada, solo informa.  Al final imprime un resumen
# con la causa más probable.
# ============================================================================
set -uo pipefail

# ── colores ────────────────────────────────────────────────────────────────
C_R="\033[31m"; C_G="\033[32m"; C_Y="\033[33m"; C_C="\033[36m"; C_N="\033[0m"
ok()   { echo -e "  ${C_G}✔${C_N} $*"; }
warn() { echo -e "  ${C_Y}⚠${C_N} $*"; }
err()  { echo -e "  ${C_R}✗${C_N} $*"; }
hdr()  { echo -e "\n${C_C}──[ $* ]${C_C}──────────────────────────────────${C_N}"; }

# ── rutas base ─────────────────────────────────────────────────────────────
# El script se ejecuta normalmente desde el directorio del proyecto,
# pero también funciona si se copia a otro lado.  Detecta el directorio
# donde reside el script.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Si está en scripts/, sube un nivel para encontrar build/
[ "$(basename "$SCRIPT_DIR")" = "scripts" ] && PROJECT_DIR="$(dirname "$SCRIPT_DIR")" || PROJECT_DIR="$SCRIPT_DIR"
BUILD_ART="${PROJECT_DIR}/build/PedagogicalCompressor_artefacts/Release/VST3"
INSTALLED_VST3="$HOME/.vst3/Pedagogical Compressor.vst3"
SO_NAME="PedagogicalCompressor.so"   # nombre interno generado por JUCE

echo -e "${C_C}════════════════════════════════════════════════════════════════${C_N}"
echo -e "${C_C} Diagnóstico VST3 — Pedagogical Compressor${C_N}"
echo -e "${C_C}════════════════════════════════════════════════════════════════${C_N}"
echo "  Proyecto:  $PROJECT_DIR"
echo "  Build:     $BUILD_ART"
echo "  Instalado: $INSTALLED_VST3"
echo "  Fecha:     $(date)"

# ============================================================================
# 1) ¿Existe el build?
# ============================================================================
hdr "1) Compilación (build/)"

FOUND_BUILT=""
for cand in \
    "$BUILD_ART/Pedagogical Compressor.vst3" \
    "$PROJECT_DIR/build/PedagogicalCompressor_artefacts/Debug/VST3/Pedagogical Compressor.vst3"; do
    if [ -d "$cand" ]; then
        ok "VST3 encontrado en build: $cand"
        FOUND_BUILT="$cand"
        break
    fi
done
if [ -z "$FOUND_BUILT" ]; then
    err "No se encontró el VST3 en build/.  ¿Compilaste con ./build.sh?"
    echo "    Esperado en: $BUILD_ART"
fi

# ============================================================================
# 2) ¿Está instalado en ~/.vst3/?
# ============================================================================
hdr "2) Instalación en ~/.vst3/"

FOUND_INSTALLED=""
if [ -d "$INSTALLED_VST3" ]; then
    ok "Bundle VST3 instalado en: $INSTALLED_VST3"
    FOUND_INSTALLED="$INSTALLED_VST3"
elif [ -d "$HOME/.vst3/PedagogicalCompressor.vst3" ]; then
    warn "Existe un bundle con nombre distinto: ~/.vst3/PedagogicalCompressor.vst3"
    FOUND_INSTALLED="$HOME/.vst3/PedagogicalCompressor.vst3"
else
    err "NO está instalado en ~/.vst3/"
    echo ""
    echo "    Para instalar, ejecuta desde el directorio del proyecto:"
    echo "        ./install.sh"
    echo "    O copia manualmente:"
    echo "        cp -r \"$BUILD_ART/Pedagogical Compressor.vst3\" ~/.vst3/"
    echo "        chmod -R +rX ~/.vst3/Pedagogical\\ Compressor.vst3"
fi

# Listar también cualquier otro VST3 con "edagog" en el nombre (por si quedó uno antiguo)
echo ""
echo "  Otros bundles VST3 relacionados encontrados en ~/.vst3/:"
shopt -s nullglob
count=0
for d in "$HOME/.vst3"/*.vst3; do
    name="$(basename "$d")"
    case "$name" in
        *edagog*|*ompres*) echo "    - $name"; count=$((count+1)) ;;
    esac
done
[ "$count" = "0" ] && echo "    (ninguno)"
shopt -u nullglob

# ============================================================================
# 3) Estructura del bundle
# ============================================================================
hdr "3) Estructura del bundle VST3"

check_bundle() {
    local bundle="$1"
    [ -z "$bundle" ] && { err "Bundle vacío"; return 1; }
    [ -d "$bundle" ] || { err "No es directorio: $bundle"; return 1; }

    # En Linux, el bundle debe ser:
    #   MiPlugin.vst3/
    #   └── Contents/
    #       └── x86_64-linux/
    #           └── MiPlugin.so
    echo "  Contenido de $bundle:"
    find "$bundle" -maxdepth 4 -printf "    %P\n" 2>/dev/null | head -20

    local so_path="$bundle/Contents/x86_64-linux/$SO_NAME"
    if [ -f "$so_path" ]; then
        ok ".so interno encontrado: $so_path"
        echo "$so_path"
        return 0
    fi

    # A veces JUCE usa otro nombre.  Buscar cualquier .so dentro.
    local found_any
    found_any="$(find "$bundle/Contents" -name "*.so" -type f 2>/dev/null | head -1)"
    if [ -n "$found_any" ]; then
        warn "El .so interno tiene OTRO nombre (esperado: $SO_NAME)"
        echo "    Encontrado: $found_any"
        echo "    Esto puede causar que algunos hosts no lo reconozcan."
        echo "$found_any"
        return 0
    fi

    err "NO se encontró Contents/x86_64-linux/$SO_NAME"
    echo "    La estructura del bundle es incorrecta."
    echo "    Borra el bundle y recompila desde cero:"
    echo "        rm -rf \"$bundle\""
    echo "        ./build.sh --clean && ./install.sh"
    return 1
}

SO_PATH_INSTALLED=""
SO_PATH_BUILT=""
[ -n "$FOUND_INSTALLED" ] && SO_PATH_INSTALLED="$(check_bundle "$FOUND_INSTALLED")"
[ -n "$FOUND_BUILT" ]    && { echo ""; SO_PATH_BUILT="$(check_bundle "$FOUND_BUILT")"; }

# ============================================================================
# 4) Permisos del .so
# ============================================================================
hdr "4) Permisos del .so interno"

check_perms() {
    local so="$1"
    [ -z "$so" ] && { err "Sin .so para verificar permisos"; return; }
    [ -f "$so" ] || { err "No existe: $so"; return; }
    if [ -x "$so" ]; then
        ok "Ejecutable: $(stat -c '%A' "$so")  $so"
    else
        err "El .so NO tiene permiso de ejecución"
        echo "    Los hosts VST3 (REAPER, Bitwig) requieren el bit +x en el .so"
        echo "    Arreglar con:"
        echo "        chmod +x \"$so\""
    fi
}

[ -n "$SO_PATH_INSTALLED" ] && check_perms "$SO_PATH_INSTALLED"
[ -n "$SO_PATH_BUILT" ]    && check_perms "$SO_PATH_BUILT"

# ============================================================================
# 5) Dependencias dinámicas (ldd)
# ============================================================================
hdr "5) Dependencias dinámicas del .so (ldd)"

check_ldd() {
    local so="$1"
    local label="$2"
    [ -z "$so" ] && return
    [ -f "$so" ] || return
    echo "  === $label ==="
    echo "  Ruta: $so"
    # Capturar salida y filtrar "not found"
    local ldd_out
    if ! ldd_out="$(ldd "$so" 2>&1)"; then
        err "ldd falló — el .so podría estar corrupto o ser de otra arquitectura"
        echo "$ldd_out" | sed 's/^/    /'
        return
    fi
    local missing
    missing="$(echo "$ldd_out" | grep -i 'not found' || true)"
    if [ -n "$missing" ]; then
        err "DEPENDENCIAS FALTANTES (esto hace que el host NO cargue el plugin):"
        echo "$missing" | sed 's/^/    /'
        echo ""
        echo "  Esto significa que el .so necesita librerías que no están en tu sistema."
        echo "  Solución: instala los paquetes que proporcionan esas librerías:"
        echo "$missing" | grep -oE 'lib[a-zA-Z0-9_.+-]+' | sort -u | while read lib; do
            echo "    apt-file search '$lib'  # averigua qué paquete instalar"
        done
    else
        ok "Todas las dependencias están resueltas"
        # mostrar un resumen corto
        echo "$ldd_out" | wc -l | xargs -I{} echo "    Total dependencias: {}"
        # verificar libstdc++ y glibc
        local stdc
        stdc="$(echo "$ldd_out" | grep -E 'libstdc\+\+|libc\.so' | head -3)"
        [ -n "$stdc" ] && echo "$stdc" | sed 's/^/    /'
    fi
}

[ -n "$SO_PATH_INSTALLED" ] && check_ldd "$SO_PATH_INSTALLED" "Instalado (~/.vst3/)"
[ -n "$SO_PATH_BUILT" ]    && check_ldd "$SO_PATH_BUILT"    "Recién compilado (build/)"

# ============================================================================
# 6) DAW: detectar y mostrar paths escaneados
# ============================================================================
hdr "6) DAWs detectados"

# REAPER
REAPER_INI="$HOME/.config/REAPER/reaper-vstplugins64.ini"
REAPER_CFG="$HOME/.config/REAPER"
if command -v reaper >/dev/null 2>&1 || [ -d "$REAPER_CFG" ]; then
    ok "REAPER detectado"
    echo "    Binario: $(command -v reaper 2>/dev/null || echo '(no en PATH)')"
    echo "    Config:  $REAPER_CFG"
    if [ -f "$REAPER_INI" ]; then
        # Buscar si "Pedagogical" está listado (cargado o en blacklist)
        echo "    Cache VST3: $REAPER_INI"
        match_line="$(grep -i 'pedagogical\|Pdcm\|Peda' "$REAPER_INI" 2>/dev/null || true)"
        if [ -n "$match_line" ]; then
            warn "El plugin está listado en la caché de REAPER:"
            echo "$match_line" | sed 's/^/      /' | head -10
            # Detectar si está en blacklist (líneas con '=' al final)
            if echo "$match_line" | grep -q 'PEDALO\| Pedagogical.*=\s*$'; then
                err "El plugin está en la BLACKLIST de REAPER (falló al cargar antes)"
                echo "    Solución: Options → Preferences → Plug-ins → VST → Re-scan (clear cache)"
            fi
        else
            warn "El plugin NO está en la caché de REAPER todavía"
            echo "    REAPER no ha escaneado ~/.vst3/ — o el path no está añadido."
        fi
        # Verificar que ~/.vst3 esté en vstpath del ini
        # El archivo reaper.ini tiene una entrada [vstpath] o similar
        echo ""
        echo "    Paths VST escaneados por REAPER (reaper.ini):"
        grep -iE 'vst.?path|vst3path' "$HOME/.config/REAPER/reaper.ini" 2>/dev/null | sed 's/^/      /' || \
            echo "      (REAPER usa defaults: ~/.vst3, /usr/lib/vst3, /usr/local/lib/vst3)"
    fi
else
    echo "  REAPER no detectado en este sistema."
fi
echo ""

# Bitwig Studio
BITWIG_PLUGINS="$HOME/.BitwigStudio/plugins"
if [ -d "$HOME/.BitwigStudio" ] || command -v bitwig-studio >/dev/null 2>&1; then
    ok "Bitwig Studio detectado"
    echo "    Binario: $(command -v bitwig-studio 2>/dev/null || echo '(no en PATH)')"
    echo "    Plugins: $BITWIG_PLUGINS"
    echo "    Bitwig escanea por defecto: ~/.vst3, /usr/lib/vst3, ~/.clap"
    if [ -d "$BITWIG_PLUGINS" ]; then
        ls -la "$BITWIG_PLUGINS" 2>/dev/null | sed 's/^/    /' | head -5
    fi
else
    echo "  Bitwig Studio no detectado."
fi
echo ""

# Carla / LMMS / Ardour / Audacity — verifican rutas estándar
echo "  Otros hosts comunes:"
for daw in carla lmms ardour audacity zrythm; do
    command -v $daw >/dev/null 2>&1 && echo "    - $daw: $(command -v $daw)"
done

# ============================================================================
# 7) Validar con vst3scan / clap-info si están disponibles
# ============================================================================
hdr "7) Validación con herramientas externas (si disponibles)"

# vst3scan — viene con el SDK de VST3
if command -v vst3scan >/dev/null 2>&1; then
    ok "vst3scan disponible"
    echo "    Probando validar el bundle instalado…"
    vst3scan "$INSTALLED_VST3" 2>&1 | sed 's/^/    /' | head -20 || \
        err "vst3scan falló — el plugin no se carga a nivel SDK"
else
    echo "  vst3scan no está instalado (es opcional).  Saltando."
fi

# clap-info — útil porque CLAP usa la misma fábrica que VST3 (de JUCE)
if command -v clap-info >/dev/null 2>&1; then
    ok "clap-info disponible"
    CLAP_PATH="$HOME/.clap/Pedagogical Compressor.clap"
    [ -f "$CLAP_PATH" ] && {
        echo "    Validando CLAP (mismo código que VST3)…"
        clap-info "$CLAP_PATH" 2>&1 | sed 's/^/    /' | head -30 || \
            warn "clap-info falló — revisa el log"
    }
fi

# Si está disponible el pluginval (validador oficial de tracktion)
if command -v pluginval >/dev/null 2>&1; then
    ok "pluginval (Tracktion) disponible"
    echo "    Validación completa del VST3…"
    pluginval --validate-in-process --validate "$INSTALLED_VST3" 2>&1 | sed 's/^/    /' | head -50 || \
        err "pluginval encontró errores — revísalo"
else
    echo "  pluginval no está instalado.  Recomendado para validar VST3:"
    echo "    https://github.com/Tracktion/pluginval/releases"
fi

# ============================================================================
# 7b) Mini-host VST3 propio (compila si no existe y ejecuta)
# ============================================================================
hdr "7b) Mini-host VST3 (dlopen + GetPluginFactory)"

MINI_HOST_SRC="${PROJECT_DIR}/scripts/mini_host_vst3/mini_host_vst3.cpp"
MINI_HOST_BIN="${PROJECT_DIR}/scripts/mini_host_vst3/mini_host_vst3"

if [ ! -f "$MINI_HOST_SRC" ]; then
    echo "  (mini_host_vst3.cpp no encontrado en scripts/)  Saltando."
elif ! command -v g++ >/dev/null 2>&1; then
    warn "g++ no disponible — no se puede compilar el mini-host."
    echo "    Instala con: sudo apt-get install -y g++"
else
    # Compilar si no existe o si el source es más reciente
    need_build=0
    [ ! -x "$MINI_HOST_BIN" ] && need_build=1
    if [ -x "$MINI_HOST_BIN" ] && [ -f "$MINI_HOST_SRC" ]; then
        src_mtime=$(stat -c %Y "$MINI_HOST_SRC" 2>/dev/null || echo 0)
        bin_mtime=$(stat -c %Y "$MINI_HOST_BIN" 2>/dev/null || echo 0)
        [ "$src_mtime" -gt "$bin_mtime" ] && need_build=1
    fi
    if [ "$need_build" = "1" ]; then
        echo "  Compilando mini-host…"
        if g++ -std=c++17 -O2 "$MINI_HOST_SRC" -ldl -lpthread -o "$MINI_HOST_BIN" 2>/dev/null; then
            ok "Mini-host compilado: $MINI_HOST_BIN"
        else
            err "No se pudo compilar el mini-host."
            echo "    Salida de errores:"
            g++ -std=c++17 -O2 "$MINI_HOST_SRC" -ldl -lpthread -o "$MINI_HOST_BIN" 2>&1 | sed 's/^/    /'
        fi
    else
        ok "Mini-host ya compilado: $MINI_HOST_BIN"
    fi

    # Ejecutar contra el bundle instalado (o el de build/ si no está instalado)
    TARGET=""
    [ -n "$FOUND_INSTALLED" ] && TARGET="$FOUND_INSTALLED"
    [ -z "$TARGET" ] && [ -n "$FOUND_BUILT" ] && TARGET="$FOUND_BUILT"

    if [ -n "$TARGET" ] && [ -x "$MINI_HOST_BIN" ]; then
        echo ""
        echo "  Validando con mini-host:"
        echo "    Target: $TARGET"
        echo ""
        "$MINI_HOST_BIN" "$TARGET" 2>&1 | sed 's/^/    /'
    else
        warn "No hay VST3 sobre el que ejecutar el mini-host."
    fi
fi

# ============================================================================
# 8) Resumen y diagnóstico
# ============================================================================
hdr "8) Resumen — causa más probable"

if [ -z "$FOUND_BUILT" ] && [ -z "$FOUND_INSTALLED" ]; then
    err "DIAGNÓSTICO: No hay VST3 ni compilado ni instalado."
    echo "    → Ejecuta:  ./build.sh && ./install.sh"
elif [ -n "$FOUND_BUILT" ] && [ -z "$FOUND_INSTALLED" ]; then
    warn "DIAGNÓSTICO: Está compilado pero NO instalado en ~/.vst3/."
    echo "    → Ejecuta:  ./install.sh"
    echo "    O copia manualmente:"
    echo "      mkdir -p ~/.vst3"
    echo "      cp -r \"$FOUND_BUILT\" ~/.vst3/"
    echo "      chmod -R +rX ~/.vst3/Pedagogical\\ Compressor.vst3"
elif [ -n "$SO_PATH_INSTALLED" ] && [ ! -x "$SO_PATH_INSTALLED" ]; then
    err "DIAGNÓSTICO: El .so interno no tiene permiso +x."
    echo "    → Ejecuta:  chmod +x \"$SO_PATH_INSTALLED\""
elif [ -n "$SO_PATH_INSTALLED" ]; then
    # Verificar ldd
    ldd_out="$(ldd "$SO_PATH_INSTALLED" 2>&1)"
    missing="$(echo "$ldd_out" | grep -i 'not found' || true)"
    if [ -n "$missing" ]; then
        err "DIAGNÓSTICO: Hay dependencias dinámicas faltantes."
        echo "    → El plugin se carga en algunos hosts (CLAP usa menos deps)"
        echo "      pero REAPER/Bitwig no pueden cargar el VST3."
        echo "    → Instala las librerías listadas arriba con apt-get."
        echo ""
        echo "    Las más comunes que faltan en Ubuntu 24.04:"
        echo "      sudo apt-get install -y libfreetype6 libx11-6 libxext6 \\"
        echo "          libxinerama1 libxrandr2 libxcomposite1 libxcursor1 \\"
        echo "          libxrender1 libxfixes3 libasound2 libglu1-mesa \\"
        echo "          libgl1-mesa-glx libcurl4"
    else
        # El VST3 está bien instalado y carga correctamente a nivel binario.
        # Si el DAW no lo lista, es probable que el DAW tenga caché corrupto
        # o no tenga ~/.vst3/ en la lista de paths.
        warn "DIAGNÓSTICO: El VST3 está bien instalado y binariamente carga."
        echo "    → El problema está en el DAW, no en el plugin."
        echo ""
        echo "    PASOS A SEGUIR en el DAW:"
        echo ""
        echo "    REAPER:"
        echo "      1. Options → Preferences → Plug-ins → VST"
        echo "      2. Verifica que '~/.vst3' esté en la lista de paths"
        echo "         (REAPER muestra ~/.vst3 como un path fijo por defecto)"
        echo "      3. Click 'Re-scan' (o 'Clear cache/Re-scan')"
        echo "      4. Si el plugin estaba en blacklist (falló antes),"
        echo "         bórralo de la lista negra y vuelve a escanear."
        echo "         Preferences → Plug-ins → 'VST plug-in failure list'"
        echo ""
        echo "    Bitwig Studio:"
        echo "      1. Settings → Locations → Plug-ins"
        echo "      2. Verifica 'VST3' apunta a ~/.vst3 (debería por defecto)"
        echo "      3. Click 'Scan for plug-ins' / 'Rescan'"
        echo ""
        echo "    Si el problema persiste, lanza el DAW desde terminal"
        echo "    para ver los logs de carga del VST3:"
        echo "      reaper 2>&1 | grep -i pedagogical"
        echo "      bitwig-studio 2>&1 | grep -i pedagogical"
    fi
else
    err "DIAGNÓSTICO: Caso no contemplado.  Revisa los puntos anteriores."
fi

echo ""
echo -e "${C_C}──────────────────────────────────────────────────────────${C_N}"
echo -e "${C_C} Fin del diagnóstico${C_N}"
echo -e "${C_C}──────────────────────────────────────────────────────────${C_N}"
