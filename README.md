# Pedagogical Compressor — VST3 / LV2 / CLAP / AU multiplataforma

Compresor multibanda pedagógico construido con **JUCE 8**. Diseñado para enseñar
el funcionamiento interno de un compresor profesional mientras se usa.

Compila nativamente en **Linux**, **Windows** y **macOS**.

Basado y corregido a partir del `PedagogicalCompressorAudioProcessor::processBlock`
que se compartió como punto de partida.

---

## Características

### DSP (lo que debe tener un compresor pro)

| Feature | Implementación |
| --- | --- |
| **1–4 bandas** | Crossovers **Linkwitz-Riley 4º orden (-24 dB/oct)** (reconstrucción perfecta) |
| **5 tipos de compresión** | Digital, VCA (SSL), FET (1176), Optical (LA-2A), Vari-Mu (Fairchild) |
| **Threshold / Ratio / Attack / Release / Knee / Makeup / Mix** | Por banda |
| **Detección Peak o RMS** | Conmutable por banda |
| **Sidechain interno (HPF en detector) + externo** | Por banda, con bus secundario |
| **Lookahead 0–20 ms** | Con delay compensado en la señal principal |
| **Oversampling 1x/2x/4x** | `juce::dsp::Oversampling` polifase IIR |
| **Soft-knee** | Modelo parabólico (solo tipo VCA) |
| **Ratio creciente con el nivel** | FET, Optical, Vari-Mu (modelos no lineales) |
| **Release program-dependiente** | Optical (más lento al final), FET (más rápido con GR alto) |
| **Coloración armónica** | `tanh` con drive distinto por tipo |
| **Stereo link / Dual mono** | Continuo 0–1 |
| **Input/Output gain + Global mix (dry/wet)** | Globales |
| **Limiter brick-wall a la salida** | Con ceiling configurable |
| **Bypass global y por banda** | Independientes |
| **Meters** | Input/Output/GR globales + por banda, con peak-hold |

### GUI pedagógica

| Elemento | Qué enseña |
| --- | --- |
| **Visualizador de bandas** | Espectro 20 Hz – 20 kHz con bandas coloreadas y crossovers arrastrables en escala logarítmica. Muestra GR por banda en vivo. |
| **Curva de transferencia In→Out** | Dibuja la curva en tiempo real. Línea 1:1 punteada, threshold marcado, punto rojo = nivel actual de entrada. |
| **Meters globales** | Input, Output y GR en una columna a la derecha. |
| **Sliders verticales por banda** | Threshold, Ratio, Attack, Release, Knee, Makeup, Mix, SC HPF, Lookahead. Color codificado por banda (azul, verde, naranja, rojo). |
| **Tooltips detallados** | Cada control explica su función y rango. |
| **Texto pedagógico** | Bajo el panel principal muestra la descripción del tipo de compresión activo. |
| **Meters por banda** | Cada banda muestra su Input/GR en miniatura. |

### Comportamiento por tipo de compresión

| Tipo | Modelado | Ataque típico | Release típico | Coloración |
| --- | --- | --- | --- | --- |
| **Digital** | Clean, ratio fijo | Variable | Variable | Ninguna |
| **VCA** | SSL 4000 bus, soft-knee parabólico | Medio | Medio | Mínima |
| **FET** | 1176, ratio crece ligeramente, saturación `tanh` | Muy rápido | Auto (más rápido con GR alta) | Media, agresiva |
| **Optical** | LA-2A, release program-dependiente (más lento al final) | Lento | Variable | Cálida |
| **Vari-Mu** | Fairchild 670, ratio crece fuertemente, tubo | Medio-lento | Medio | Marcada, "glue" |

---

## Soporte multiplataforma

| SO | Arquitectura | Formatos generados | Build con |
|---|---|---|---|
| **Linux** | x86_64 | VST3, LV2, CLAP, Standalone | `./build.sh` |
| **Windows** | x64 | VST3, CLAP, Standalone | `build.bat` |
| **macOS** | Universal (arm64 + x86_64) | VST3, AU, CLAP, Standalone | `./build-macos.sh` |

> LV2 es exclusivo de Linux. AU (Audio Unit) es exclusivo de macOS.

### GitHub Actions (compila en los 3 SO a la vez, sin instalar nada)

El workflow `.github/workflows/build.yml` compila en paralelo en Linux, Windows
y macOS en cada push. En la pestaña **Actions** del repo, descarga los binarios
ya compilados desde "Artifacts".

---

## Requisitos del sistema

### Linux x86_64
- Debian/Ubuntu/Mint, Fedora, Arch, etc.
- GCC ≥ 11 o Clang ≥ 14 (C++20)
- CMake ≥ 3.22, git
- Dependencias X11 + FreeType + OpenGL + ALSA + WebKit2GTK

### Windows x64
- Windows 10/11 x64
- Visual Studio 2022 con workload "Desktop development with C++"
- CMake ≥ 3.22, Git

### macOS
- macOS 11.0 (Big Sur) o superior
- Xcode 14+ con Command Line Tools
- CMake ≥ 3.22 (recomendado: `brew install cmake`)

### Instalación de dependencias (Linux)

#### Debian / Ubuntu / Mint
```bash
sudo apt update
sudo apt install -y cmake git build-essential pkg-config \
    libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev \
    libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
    libglu1-mesa-dev mesa-common-dev libasound2-dev \
    libwebkit2gtk-4.1-dev libcurl4-openssl-dev
```

#### Fedora / RHEL
```bash
sudo dnf install -y cmake git gcc-c++ pkg-config \
    freetype-devel libX11-devel libXcomposite-devel libXcursor-devel \
    libXext-devel libXinerama-devel libXrandr-devel libXrender-devel \
    mesa-libGLU-devel mesa-libGL-devel alsa-lib-devel \
    webkit2gtk4.1-devel libcurl-devel
```

#### Arch / Manjaro
```bash
sudo pacman -S --needed cmake git base-devel pkgconf \
    freetype2 libx11 libxcomposite libxcursor libxext libxinerama \
    libxrandr libxrender glu mesa alsa-lib webkit2gtk curl
```

---

## Build

El `CMakeLists.txt` se descarga automáticamente **JUCE 8** y **clap-juce-extensions**
mediante `FetchContent` — no necesitas instalar JUCE a mano.

### Linux

```bash
cd PedagogicalCompressor
./build.sh            # compila VST3 + LV2 + CLAP + Standalone
./install.sh          # copia a ~/.vst3, ~/.lv2, ~/.clap, ~/.local/bin
```

Build limpio (tras reemplazar archivos):
```bash
./build.sh --clean
```

### Windows

1. Instala Visual Studio 2022 + CMake + Git.
2. Doble clic en `build.bat` (o ejecútalo desde cmd).
3. Doble clic en `install.bat` para copiar a:
   - `%USERPROFILE%\Documents\VST3\`
   - `%USERPROFILE%\Documents\CLAP\`
   - `%LOCALAPPDATA%\Programs\PedagogicalCompressor\`

### macOS

```bash
cd PedagogicalCompressor
chmod +x build-macos.sh install-macos.sh
./build-macos.sh        # compila Universal Binary (VST3 + AU + CLAP + Standalone)
./install-macos.sh      # copia a ~/Library/Audio/Plug-Ins/VST3 | Components | CLAP
```

### Resultados de build

| SO | Formato | Ruta típica |
|---|---|---|
| Linux | VST3 | `build/PedagogicalCompressor_artefacts/Release/VST3/Pedagogical Compressor.vst3` |
| Linux | LV2 | `build/PedagogicalCompressor_artefacts/Release/LV2/Pedagogical Compressor.lv2` |
| Linux/Win/Mac | CLAP | `build/PedagogicalCompressor_artefacts/Release/CLAP/Pedagogical Compressor.clap` |
| macOS | AU | `build/PedagogicalCompressor_artefacts/Release/AU/Pedagogical Compressor.component` |
| Todos | Standalone | `build/PedagogicalCompressor_artefacts/Release/Standalone/Pedagogical Compressor[.exe/.app]` |

### Instalación manual (Linux)

```bash
# VST3
mkdir -p ~/.vst3
cp -r "build/PedagogicalCompressor_artefacts/Release/VST3/Pedagogical Compressor.vst3" ~/.vst3/

# LV2
mkdir -p ~/.lv2
cp -r "build/PedagogicalCompressor_artefacts/Release/LV2/Pedagogical Compressor.lv2" ~/.lv2/

# CLAP
mkdir -p ~/.clap
cp -r "build/PedagogicalCompressor_artefacts/Release/CLAP/Pedagogical Compressor.clap" ~/.clap/

# Instalación global (todos los usuarios):
# sudo cp -r "build/.../VST3/Pedagogical Compressor.vst3" /usr/lib/vst3/
# sudo cp -r "build/.../LV2/Pedagogical Compressor.lv2"  /usr/lib/lv2/
# sudo cp -r "build/.../CLAP/Pedagogical Compressor.clap" /usr/lib/clap/
```

### Uso de un JUCE local (opcional, build offline)

Si ya tienes JUCE clonado en algún lado y no quieres usar `FetchContent`:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DPEDACOMP_USE_SYSTEM_JUCE=ON \
      -DJUCE_SOURCE_DIR=/ruta/a/JUCE
cmake --build build -j$(nproc)
```

(Para esta forma tienes que editar el `CMakeLists.txt` y añadir `add_subdirectory(${JUCE_SOURCE_DIR})`.)

---

## Compatibilidad con DAWs

| DAW | VST3 | LV2 | CLAP | Notas |
| --- | --- | --- | --- | --- |
| **REAPER** | ✅ | ✅ | ✅ (≥6.0) | Soporta sidechain en los 3 formatos |
| **Bitwig Studio** | ✅ | ✅ | ✅ | Mejor soporte CLAP |
| **Ardour** | ✅ | ✅ | ❌ | LV2 recomendado |
| **Tracktion Waveform** | ✅ | ✅ | ✅ | |
| **Zrythm** | ✅ | ✅ | ✅ | |
| **LMMS** | ✅ | ❌ | ❌ | Sidechain limitado |
| **Carla** | ✅ | ✅ | ✅ | Para usar como host standalone |

---

## Validación (cómo se verificó este plugin)

| Prueba | Resultado |
| --- | --- |
| **pluginval rigor 10** (audio/API: bloques aleatorios, fuzz de parámetros, thread-safety, automatización, estado, buses) | ✅ SUCCESS — 0 fallos |
| **pluginval rigor 5 completo** (incluye pruebas de Editor con GUI) | ✅ SUCCESS |
| **Arnés de estrés propio** (25× abrir/redimensionar/cambiar presets/reproducir/cerrar el editor + 100 rondas de save/restore de estado exactas) | ✅ sin crashes |
| **Roundtrip de estado** (74 parámetros, valores exactos incluidos bools fraccionales) | ✅ exacto |
| **Limiter de salida** (pico ≤ ceiling con señal +0.8 dBFS sobrecargada) | ✅ respeta ceiling |

Los arneses están en `extras/` y se compilan con
`cmake -B build -DPEDACOMP_BUILD_TEST_HARNESS=ON` (OFF por defecto).

### Bugs reales encontrados y corregidos durante la validación

1. **Overflow del buffer circular del lookahead** — si el tamaño de bloque del
   host no dividía el tamaño del buffer, `copyFrom` escribía fuera del heap →
   corrupción de memoria → crashes aleatorios en REAPER/standalone.
2. **Limiter discontinuo** — saltaba de 0.9·ceiling a 0.68·ceiling → pops
   audibles con señal fuerte. Ahora es continuo (curva con asíntota).
3. **Carreras GUI↔audio** — la GUI escribía parámetros del DSP mientras el
   audio corría. Ahora TODO pasa por flags atómicos aplicados en el audio thread.
4. **Estado no restaurado** — el APVTS de JUCE 8 omite parámetros
   desincronizados (hosts que usan `setValue()` crudo) y su caché interna
   cuantiza los bools; además `replaceState()` muta el árbol del llamador
   vía referencia compartida. Solución: copia profunda + forzado de valores
   exactos leídos del XML guardado.

### Configurar sidechain externo

1. En el DAW, añade una pista de audio/instance y envíala al bus secundario ("Sidechain") del plugin.
2. En el plugin, activa **ExtSC** en la banda que quieras que responda a esa señal.
3. Todas las bandas que tengan **ExtSC** activado usarán la señal externa como detector.

---

## Estructura del proyecto

```
PedagogicalCompressor/
├── CMakeLists.txt           Build: VST3 + LV2 + CLAP + Standalone
├── README.md                Este archivo
├── build.sh                 Compila en 1 comando (instala deps si faltan)
├── install.sh               Instala los 4 formatos en ~/.vst3 ~/.lv2 ~/.clap
└── Source/
    ├── PluginProcessor.h    API pública del processor + APVTS
    ├── PluginProcessor.cpp  processBlock, parámetros, estado, sidechain
    ├── PluginEditor.h       Layout de la GUI
    ├── PluginEditor.cpp     Construcción + tooltips + bindings APVTS
    ├── CompressorTypes.h    Modelos de compresión (Digital/VCA/FET/Optical/Vari-Mu)
    ├── CompressorBand.h     1 banda completa (lookahead + OS + sidechain)
    ├── MultibandProcessor.h Crossovers LR4 + 1–4 bandas
    └── Gui/
        ├── CompressionCurveComponent.h  Curva In→Out en vivo
        ├── LevelMeterComponent.h         Meters peak + GR
        └── BandSplitVisualizer.h         Espectro + crossovers arrastrables
```

---

## Parámetros (todos automatizables en el DAW)

### Globales
- `BandCount` — 1/2/3/4 bandas
- `Crossover1` / `Crossover2` / `Crossover3` — Frecuencias (Hz, log)
- `InputGain` — -24..+24 dB
- `OutputGain` — -24..+24 dB
- `GlobalMix` — 0..1 (dry/wet)
- `GlobalBypass` — bool
- `LimiterOn` — bool
- `CeilingDb` — -6..0 dB

### Por banda (Band0..Band3)
- `Threshold` — -60..0 dB
- `Ratio` — 1..20
- `Attack` — 0.1..200 ms
- `Release` — 10..5000 ms
- `Knee` — 0..24 dB (solo tipo VCA)
- `Makeup` — 0..24 dB
- `Mix` — 0..1
- `CompType` — Digital / VCA / FET / Optical / Vari-Mu
- `Detection` — Peak / RMS
- `StereoLink` — 0..1
- `ScHpf` — 20..1000 Hz
- `ExtSc` — bool
- `Lookahead` — 0..20 ms
- `Oversampling` — 1x / 2x / 4x
- `Enabled` — bool (OFF = la banda queda **mute**; suena el resto del split)
- `Bypass` — bool (la banda suena **sin comprimir**)

---

## Resolución de problemas

### CMake no encuentra JUCE
El `FetchContent` necesita conexión a internet en la primera build. Si estás offline, descarga JUCE manualmente y usa `-DPEDACOMP_USE_SYSTEM_JUCE=ON`.

### El plugin no aparece en el DAW
1. Verifica que el binario esté en la ruta correcta (`~/.vst3/`, `~/.lv2/`, `~/.clap/`).
2. En el DAW, fuerza un "rescan" de plugins.
3. Verifica permisos: `ls -la ~/.vst3/`
4. Ejecuta el standalone para ver logs: `./Pedagogical\ Compressor`

### Latencia alta con lookahead
El lookahead + oversampling introducen latencia. El DAW debería compensarla automáticamente. Si no, desactiva lookahead o usa oversampling 1x.

### Clicks/audio corrupto
- Comprueba el sample rate en el DAW.
- Si usas 4x oversampling con lookahead, baja a 2x si tu CPU no da abasto.

---

## Licencia y créditos

- Código del plugin: libre uso / MIT.
- JUCE: licencia GPLv3 (open-source) o comercial (si vas a vender un producto cerrado, necesitas licencia de JUCE).
- `clap-juce-extensions`: licencia MIT.
- Modelos de compresión: emulaciones funcionales **inspiradas en** el comportamiento de los compresores clásicos, no son muestras ni replicas bit-a-bit de hardware específico.

---

## Próximos pasos sugeridos

1. **Probar el standalone** primero para ver la GUI sin DAW.
2. **Cargar audio** y empezar con 1 banda + tipo Digital para entender los controles básicos.
3. **Cambiar a 4 bandas** y arrastrar los crossovers para ver el espectro dividirse.
4. **Probar cada tipo** (FET, Optical, etc.) en una voz o batería y comparar el "carácter".
5. **Activar ExtSC** y enviar un kick a la banda baja para ducking clásico.
