# Pedagogical Compressor — VST3 / LV2 / CLAP / AU multiplataforma

Compresor multibanda pedagógico construido con **JUCE 8**. Diseñado para enseñar
el funcionamiento interno de un compresor profesional mientras se usa.

Compila nativamente en **Linux**, **Windows** y **macOS**.

## v1.2.4 — Mejoras de portabilidad Windows y macOS

### Fixes cross-platform

1. **CMakeLists limpio de rutas del sandbox**: se eliminó la referencia a
   `/home/z/deps/root/...` que solo existía en el entorno de desarrollo del
   sandbox. Ahora `find_library(ALSA_LIB asound)` busca solo en rutas estándar
   del sistema, lo que permite compilar en cualquier Linux sin configuración
   extra.

2. **`install.bat` (Windows) usa rutas estándar VST3**:
   - Antes: `%USERPROFILE%\Documents\VST3\` y `%USERPROFILE%\Documents\CLAP\`
     (no todos los DAWs escanean aquí por defecto)
   - Ahora: `%LOCALAPPDATA%\Programs\Common\VST3\` y `%LOCALAPPDATA%\Programs\Common\CLAP\`
     (rutas estándar VST3 3.6.5+ que REAPER, Bitwig, Studio One, Cubase 10+,
     Ableton Live 11+ y Cakewalk escanean automáticamente)
   - El instalador InnoSetup (`PedagogicalCompressor-Setup.exe`) instala
     system-wide en `%COMMONPROGRAMFILES%\VST3\` (requiere admin)

3. **`build-macos.sh` más robusto**:
   - Verifica que Xcode Command Line Tools estén instalados antes de empezar
   - Detecta la ruta de Xcode con `xcode-select -p`
   - Verifica que los 4 artefactos (VST3, AU, CLAP, Standalone) se generaron
   - Verifica que el binario es realmente universal (arm64 + x86_64)

4. **`install-macos.sh` con manejo de Gatekeeper**:
   - Quita el atributo `com.apple.quarantine` de los bundles copiados
     (evita el diálogo "no se puede abrir porque es de un desarrollador
     no identificado" en macOS 11+)
   - Maneja `/Applications/` sin permisos (cae a `~/Applications/`)
   - Documenta cómo validar el AU con `auval -vt aumf Pdcm Peda` (necesario
     para Logic Pro)

5. **Instalador InnoSetup actualizado a v1.2.3** (estaba en 1.1.1)

6. **GitHub Actions CI** verifica los 3 SOs en paralelo:
   - Linux: build + test CLAP carga + regression test saturación multibanda
   - Windows: build + InnoSetup installer .exe
   - macOS: build universal + verificación arquitectura

## v1.2.2 — FIX DEFINITIVO de la saturación multibanda (medido)

### Causa raíz real (encontrada con medición, no a ciegas)

Se construyó un **host de medición real** (`extras/saturation_meter.c`) que carga
el .clap, cambia el número de bandas vía eventos de parámetros CLAP y mide
peak/RMS/DC/NaN/clipping del audio de salida. Así se encontraron **3 bugs
encadenados** que juntos saturaban los parlantes al activar más de una banda:

1. **Crossover mal construido para 3-4 bandas** (el bug principal):
   la estructura anterior aplicaba a cada banda sólo UN filtro HP y UN LP.
   La banda 3 quedaba como `HP(c2)` a secas, cuando lo correcto es
   `HP(c0)·HP(c1)·HP(c2)`. Resultado: las bandas NO eran complementarias —
   contenían contenido duplicado sin control de fase y la suma podía subir
   varios dB sobre la señal original.
   **Fix**: estructura Linkwitz-Riley en cascada correcta:
   `banda b = HP(c0)·…·HP(c_{b-1})·LP(c_b)`. La suma telescópica reconstruye
   la señal original EXACTA (en magnitud y fase). Cada banda tiene sus
   PROPIAS instancias de filtro (estado independiente).

2. **Saturación que REALZABA la señal hasta +7 dB**: la normalización
   `tanh(x·d)/tanh(d)` tiene ganancia de pequeña señal `d/tanh(d)` — un realce
   que multiplicado por 4 bandas disparaba el nivel directo contra el limiter.
   **Fix**: `y = tanh(x·d)/d` → pendiente exactamente 1 en el origen,
   redondea picos sin realzar.

3. **Detector RMS con dB duplicados**: la envolvente RMS guarda x² pero se
   convertía con `20·log10` (la conversión correcta para potencia es
   `10·log10`). El detector leía 6 dB por debajo del nivel real.

Además: fade-in de 512 muestras al cambiar el número de bandas (anti-click),
guard NaN/Inf en el limiter (protección de parlantes), Auto-Gain limitado a
±12 dB e integrador más lento, y defensa contra hosts que activan el bus
sidechain sin conectar los pines.

### Medición antes/después (señal multitono+kick, pico -1.4 dBFS)

| Configuración | RMS salida | Limiter | NaN |
|---|---|---|---|
| 1 banda (defaults) | -11.72 dBFS | 0.1% | 0 |
| 2 bandas | -11.73 dBFS | 0.1% | 0 |
| 3 bandas | -11.64 dBFS | 0.1% | 0 |
| 4 bandas | **-11.80 dBFS** | 0.1% | 0 |
| Preset «4-Band Stereo Master» | -11.14 dBFS | 0.1% | 0 |
| + Auto-Makeup ×4 | -12.00 dBFS | 0.1% | 0 |
| + Sat 100% ×4 | -12.76 dBFS (los picos BAJAN) | 0.0% | 0 |
| + Auto-Gain (todo activo) | -12.27 dBFS | 0.0% | 0 |

**El nivel es idéntico con 1, 2, 3 o 4 bandas** (diferencia 0.08 dB):
la reconstrucción es plana. Este test corre automáticamente en GitHub Actions
en cada push (falla el build si la diferencia supera 1.5 dB).

## v1.2.1 — Vista pedagógica

Esta versión rediseñó la UI con enfoque pedagógico e incluyó un primer intento
de fix de saturación (compensación 1/√N + limiter cúbico) que resultó
insuficiente — la causa raíz real (el crossover) se encontró y corrigió en
la v1.2.2 de arriba.

### 📚 Mejoras pedagógicas

1. **"PEDAGOGICAL COMPRESSOR" en grande** (28pt, naranja-ámbar) arriba del todo.
2. **Preset selector grande y visible** con etiqueta "PRESET:" + combo amplio.
3. **Fuentes mucho más legibles**:
   - Labels de slider: 13pt (antes 10pt)
   - Labels globales: 14pt
   - Valores de sliders globales en dB siempre visibles debajo de cada knob
   - Título: 28pt
4. **Tooltips pedagógicos en español** en cada control, explicando:
   - Qué hace el parámetro
   - Cuándo subirlo/bajarlo
   - Rangos típicos para mastering/voz/drums
5. **GR meter GRANDE y central** con:
   - Etiqueta "REDUCCIÓN" en grande (14pt naranja)
   - Valor numérico en grande (22pt, color según intensidad: verde→naranja→rojo)
   - Etiqueta "Ganancia" debajo
6. **Nombres de banda descriptivos**: "Banda 1 — Bajos", "Banda 2 — Medios
   graves", etc., en vez de solo "Low", "Mid-Low".
7. **Bordes de banda con color**: cada GroupComponent tiene un borde del color
   de la banda (azul/verde/naranja/rojo) para identificación inmediata.
8. **Valores numéricos en dB visibles debajo de cada slider global**
   (Input: +0.0 dB, Output: +0.0 dB, Ceiling: -1.0 dB, Mix: 100%).
9. **Ventana más grande** (1500×950, antes 1440×880) para acomodar todo.

### ✅ Conserva todas las funciones de v1.2.0

- Saturación analógica por banda
- Auto-Makeup por banda
- Auto-Gain global
- Knee visualmente en la curva
- Visualizador de envolvente
- Sidechain externo
- Peak/RMS selector

## Características completas

### DSP

| Feature | Implementación |
|---|---|
| 1–4 bandas | Crossovers Linkwitz-Riley 4º orden + **normalización 1/√N** (v1.2.1) |
| 5 tipos de compresión | Digital, VCA (SSL), FET (1176), Optical (LA-2A), Vari-Mu (Fairchild) |
| Threshold/Ratio/Attack/Release/Knee/Makeup/Mix | Por banda |
| **Saturación analógica** | Slider por banda 0..1, drive dependiente del tipo |
| **Auto-Makeup por banda** (máx +6 dB, v1.2.1) | Compensación automática |
| **Auto-Gain global** | Compara RMS dry/wet y mantiene loudness |
| Detección Peak o RMS | Conmutable por banda |
| Sidechain interno (HPF) + externo | Por banda, con botón ExtSC visible |
| Lookahead 0–20 ms | Con delay compensado |
| Stereo link / Dual mono | Continuo 0–1 |
| Input/Output gain + Global mix con coherencia de fase | Globales |
| **Limiter cúbico transparente** (v1.2.1) | Soft-clip al 95% del ceiling + brick-wall |

### GUI pedagógica

- **"PEDAGOGICAL COMPRESSOR"** título grande 28pt
- **Preset selector grande y visible** con etiqueta clara
- **GR meter central GRANDE** con valor numérico 22pt en color
- **Mini GR meter por banda** en cada GroupComponent
- **Visualizador de bandas** con crossovers arrastrables y GR overlay
- **Curva de transferencia In→Out** con knee sombreado, grid y punto rojo
- **Visualizador de envolvente** Attack/Release en tiempo real (256 muestras)
- **Meters** Input/Output con peak-hold
- **Sliders por banda** color-coded con tooltips pedagógicos en español
- **Valores numéricos en dB** visibles debajo de cada slider global

## Soporte multiplataforma

| SO | Arquitectura | Formatos |
|---|---|---|
| Linux | x86_64 | VST3, LV2, CLAP, Standalone |
| Windows | x64 | VST3, CLAP, Standalone |
| macOS | Universal (arm64 + x86_64) | VST3, AU, CLAP, Standalone |

## Build

### Linux
```bash
./build.sh            # compila
./install.sh           # instala en ~/.vst3, ~/.lv2, ~/.clap, ~/.local/bin
```

### Windows
1. Instalar Visual Studio 2022 + CMake + Git.
2. Doble clic en `build.bat`.
3. Doble clic en `install.bat` → copia VST3, CLAP y Standalone.

### macOS
```bash
./build-macos.sh
./install-macos.sh
```

### GitHub Actions
El workflow `.github/workflows/build.yml` compila en los 3 SO en paralelo.
Descarga los binarios desde la pestaña Actions → Artifacts.

## Troubleshooting: "el DAW no reconoce el VST3"

Si compilaste (build OK) y el CLAP funciona pero el VST3 no aparece en el
DAW, ejecuta:

```bash
bash scripts/diagnostico_vst3.sh
```

El script verifica automáticamente: si está compilado, si está instalado en
`~/.vst3/`, la estructura del bundle, permisos del `.so`, dependencias
dinámicas (`ldd`), caché/blacklist de REAPER y Bitwig, y compila+ejecuta un
mini-host que hace `dlopen()` + valida el símbolo `GetPluginFactory` del SDK
VST3.  Al final imprime la causa más probable y los pasos a seguir.

Ver `scripts/SOLUCION_VST3.md` para el detalle de cada caso.

── Cómo usar las nuevas funciones ──

### Sidechain externo (ducking clásico)
1. En REAPER: añade una pista con la voz (o lo que quieras comprimir).
2. Añade otra pista con el kick (o cualquier fuente que disparará la compresión).
3. En el plugin, en la banda relevante activa el botón **ExtSC**.
4. En REAPER → routing del plugin → añade un send desde la pista del kick
   al bus 3/4 (Sidechain) del plugin.
5. Ajusta Threshold y Ratio — ahora la compresión reacciona al kick.

### Auto-Makeup
- Activar "AutoMK" en la banda → compensa automáticamente la pérdida de volumen.
- Si combinas AutoMK con Makeup manual, el AutoMK tiene prioridad.
- Útil para presets con ratio alto donde el Makeup manual es tedioso de ajustar.

### Auto-Gain global
- Activar "AUTO GAIN" arriba a la derecha.
- Compara el RMS de la señal pre-compresión vs post-compresión y sube/baja
  para mantener el mismo loudness.
- Readout muestra cuántos dB está aplicando (ej: "+3.2 dB").

### Saturación analógica
- Slider "Sat" en cada banda, de 0 (limpio) a 1 (saturado).
- FET y Vari-Mu saturan más agresivamente (transformador/tubo emulado).
- VCA y Digital son más sutiles (color sin distorsión obvia).
- Para paralelo: usa Mix + Saturation juntas.

### Visualización de envolvente
- Debajo de la curva de transferencia.
- Muestra las 3 trazas en tiempo real: Input, Envelope (post-attack/release), GR.
- Útil para entender si el ataque es lineal, exponencial, etc.
