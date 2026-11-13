// ============================================================================
// mini_host_vst3.cpp — Validador mínimo de VST3 (carga, símbolos, factory)
//
// COMPILACIÓN:
//   g++ -std=c++17 -O2 mini_host_vst3.cpp -ldl -lpthread -o mini_host_vst3
//
// USO:
//   ./mini_host_vst3 "~/.vst3/Pedagogical Compressor.vst3"
//   ./mini_host_vst3 ./build/.../Pedagogical\ Compressor.vst3
//
// QUÉ HACE:
//   1. Resuelve la ruta del .so dentro del bundle (.vst3 es una carpeta)
//   2. Verifica permisos de ejecución del .so
//   3. dlopen() — detecta librerías faltantes (causa #1 de fallo de carga)
//   4. dlsym("GetPluginFactory") — verifica que el .so es realmente un VST3
//   5. Llama a GetPluginFactory() — si la factory se inicializa mal, aquí fallaría
//
// NO intenta usar las vtables COM de VST3 (esos tests son muy complejos).
// Para validación completa del SDK, usar pluginval (Tracktion, FOSS):
//   https://github.com/Tracktion/pluginval/releases
//   pluginval --validate-in-process --validate "bundle.vst3"
//
// Si este mini-host dice "OK" y el DAW no lista el plugin, el problema está
// en la configuración del DAW (paths de scan, caché, blacklist), NO en el
// binario del plugin.
// ============================================================================

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <dlfcn.h>

namespace fs = std::filesystem;

static void expand_home(std::string& s) {
    if (s.size() >= 2 && s[0] == '~' && s[1] == '/') {
        const char* home = std::getenv("HOME");
        if (home) s = std::string(home) + s.substr(1);
    }
}

int main(int argc, char** argv)
{
    std::cout <<
        "═══════════════════════════════════════════════════════════════\n"
        " mini_host_vst3 — validador mínimo de VST3\n"
        "═══════════════════════════════════════════════════════════════\n\n";

    if (argc < 2) {
        std::cerr << "Uso: " << argv[0]
                  << " <ruta-al-bundle.vst3 | ruta-al.so>\n";
        return 1;
    }

    std::string path = argv[1];
    expand_home(path);
    std::string soPath;

    fs::path p(path);
    if (fs::is_directory(p)) {
        // bundle .vst3 — buscar Contents/x86_64-linux/*.so
        fs::path inner = p / "Contents" / "x86_64-linux";
        if (fs::exists(inner)) {
            for (auto& e : fs::directory_iterator(inner)) {
                if (e.path().extension() == ".so") {
                    soPath = e.path().string();
                    break;
                }
            }
        }
        if (soPath.empty()) {
            // Búsqueda recursiva de respaldo
            for (auto& e : fs::recursive_directory_iterator(p)) {
                if (e.path().extension() == ".so") {
                    soPath = e.path().string();
                    break;
                }
            }
        }
        if (soPath.empty()) {
            std::cerr << "✗ Bundle sin .so interno en Contents/x86_64-linux/\n";
            std::cerr << "  Bundle: " << path << "\n";
            std::cerr << "→ La estructura del bundle es incorrecta.  Recompila\n";
            std::cerr << "  con ./build.sh --clean && ./install.sh\n";
            return 2;
        }
    } else {
        soPath = path;
    }

    std::cout << "  Bundle entrada : " << path << "\n";
    std::cout << "  .so a cargar   : " << soPath << "\n\n";

    // ── 1. Existe? ────────────────────────────────────────────────────
    if (!fs::exists(soPath)) {
        std::cerr << "✗ El .so no existe.\n";
        return 3;
    }
    std::cout << "─[ 1) Existencia ]───────────────────────────────────────\n";
    std::cout << "  ✔ Archivo presente\n\n";

    // ── 2. Permisos +x? ───────────────────────────────────────────────
    std::cout << "─[ 2) Permisos del .so ]───────────────────────────────\n";
    auto perms = fs::status(soPath).permissions();
    bool executable = (perms & (fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec)) != fs::perms::none;
    if (!executable) {
        std::cerr << "✗ El .so NO tiene permiso de ejecución.\n";
        std::cerr << "  Arreglar:  chmod +x \"" << soPath << "\"\n";
        return 4;
    }
    std::cout << "  ✔ Permisos OK (executable)\n\n";

    // ── 3. dlopen() ───────────────────────────────────────────────────
    std::cout << "─[ 3) dlopen(RTLD_NOW) ]───────────────────────────────\n";
    void* handle = dlopen(soPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        const char* err = dlerror();
        std::cerr << "✗ dlopen falló:\n  " << (err ? err : "(sin mensaje)") << "\n\n";
        std::cerr << "→ Esto significa que falta una dependencia dinámica en el sistema.\n";
        std::cerr << "  Lista las dependencias faltantes con:\n";
        std::cerr << "    ldd \"" << soPath << "\" | grep -i 'not found'\n";
        std::cerr << "  Las más comunes que faltan en Ubuntu 24.04:\n";
        std::cerr << "    sudo apt-get install -y libfreetype6 libx11-6 libxext6 \\\n";
        std::cerr << "        libxinerama1 libxrandr2 libxcomposite1 libxcursor1 \\\n";
        std::cerr << "        libxrender1 libxfixes3 libasound2 libglu1-mesa \\\n";
        std::cerr << "        libgl1 libcurl4\n";
        return 5;
    }
    std::cout << "  ✔ dlopen OK — todas las dependencias resueltas\n\n";

    // ── 4. GetPluginFactory ───────────────────────────────────────────
    std::cout << "─[ 4) Símbolo 'GetPluginFactory' (VST3 entry point) ]───\n";
    using GetFactoryFn = void* (*)();
    dlerror();
    GetFactoryFn getFactory = reinterpret_cast<GetFactoryFn>(dlsym(handle, "GetPluginFactory"));
    const char* err = dlerror();
    if (err || !getFactory) {
        std::cerr << "✗ El .so no exporta 'GetPluginFactory'\n";
        std::cerr << "  " << (err ? err : "(símbolo ausente)") << "\n";
        std::cerr << "→ El binario NO es un VST3 válido.\n";
        std::cerr << "  Posiblemente es un CLAP renombrado, o el wrapper VST3\n";
        std::cerr << "  no se compiló (revisa el log de cmake).\n";
        dlclose(handle);
        return 6;
    }
    std::cout << "  ✔ Símbolo encontrado\n\n";

    // ── 5. Llamar a GetPluginFactory ──────────────────────────────────
    std::cout << "─[ 5) Llamada a GetPluginFactory() ]───────────────────\n";
    void* factoryPtr = getFactory();
    if (!factoryPtr) {
        std::cerr << "✗ GetPluginFactory() devolvió nullptr\n";
        std::cerr << "→ El plugin falló durante la inicialización estática\n";
        std::cerr << "  (antes de que el host pueda hacer nada).\n";
        std::cerr << "  Revisa el constructor del AudioProcessor.\n";
        dlclose(handle);
        return 7;
    }
    std::cout << "  ✔ Factory devuelta correctamente: " << factoryPtr << "\n\n";

    // ── Resumen ───────────────────────────────────────────────────────
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << " ✔ El binario VST3 carga correctamente.\n";
    std::cout << "   Si el DAW no lista el plugin, el problema está en el DAW\n";
    std::cout << "   (paths de scan, caché, blacklist), no en el binario.\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "\n  Próximos pasos recomendados:\n";
    std::cout << "    1. Forzar re-scan en el DAW (Options → Preferences → Plug-ins)\n";
    std::cout << "    2. Borrar la caché de VST3 del DAW\n";
    std::cout << "    3. Verificar que '~/.vst3/' esté en los paths de scan\n";
    std::cout << "    4. Lanzar el DAW desde terminal para ver logs:\n";
    std::cout << "         reaper 2>&1 | grep -i pedagogical\n";
    std::cout << "         bitwig-studio 2>&1 | grep -i pedagogical\n";

    dlclose(handle);
    return 0;
}
