# VST3 no reconocido por el DAW — Diagnóstico

## Síntoma

Compilaste el Pedagogical Compressor (build OK), el CLAP funciona, pero el
DAW (REAPER / Bitwig / Carla / Ardour) **no lista el VST3** entre los plugins
disponibles.

---

## Causa más probable

En el 90% de los casos en Linux, esto ocurre por una de estas razones, en
orden de frecuencia:

1. **No ejecutaste `./install.sh`** después de compilar.
   El VST3 está en `build/PedagogicalCompressor_artefacts/Release/VST3/`
   pero NO en `~/.vst3/`. El DAW escanea `~/.vst3/` y no lo encuentra.

2. **El DAW tiene el plugin en blacklist** por un fallo anterior (cuando
   saturaba los parlantes).  REAPER y Bitwig cachean los fallos.

3. **El `.so` interno no tiene permiso de ejecución** (`+x`).  Esto pasó
   en algunos builds donde `cp -r` no preservó los permisos.

4. **El bundle está mal estructurado** (falta `Contents/x86_64-linux/`).

5. **El DAW no tiene `~/.vst3/` en la lista de paths de scan**.

6. **Falta una dependencia dinámica** (`libfreetype6`, `libx11`, etc.).

---

## Cómo diagnosticar en 1 comando

```bash
cd ~/Escritorio/"PRoyecto compresión - guía pedagógica"/PedagogicalCompressor
bash scripts/diagnostico_vst3.sh
```

El script:
- Verifica si el VST3 está compilado en `build/`.
- Verifica si está instalado en `~/.vst3/`.
- Inspecciona la estructura del bundle (que exista `Contents/x86_64-linux/*.so`).
- Verifica permisos `+x` del `.so` interno.
- Ejecuta `ldd` y lista dependencias faltantes.
- Detecta REAPER y Bitwig, y revisa si el plugin está en su caché.
- Compila y ejecuta un mini-host que hace `dlopen()` + busca el símbolo
  `GetPluginFactory` del SDK VST3 — esto detecta el 99% de los fallos de
  carga binaria.
- Imprime al final la causa más probable y los pasos a seguir.

---

## Soluciones rápidas

### Si el diagnóstico dice "NO está instalado en `~/.vst3/`":

```bash
./install.sh
```

O manualmente:

```bash
mkdir -p ~/.vst3
cp -r "build/PedagogicalCompressor_artefacts/Release/VST3/Pedagogical Compressor.vst3" ~/.vst3/
chmod -R +rX "$HOME/.vst3/Pedagogical Compressor.vst3"
```

### Si el diagnóstico dice "el plugin está en la BLACKLIST de REAPER":

```
REAPER → Options → Preferences → Plug-ins → VST
  → "Clear cache / Re-scan" (botón abajo del todo)
  → cerrar REAPER
  → borrar:  ~/.config/REAPER/reaper-vstplugins64.ini
  → abrir REAPER (re-escanea desde cero)
```

### Si el diagnóstico dice "DEPENDENCIAS FALTANTES":

```bash
sudo apt-get install -y \
    libfreetype6 libx11-6 libxext6 libxinerama1 libxrandr2 \
    libxcomposite1 libxcursor1 libxrender1 libxfixes3 \
    libasound2 libglu1-mesa libgl1 libcurl4
```

### Si el diagnóstico dice "el .so NO tiene permiso de ejecución":

```bash
chmod +x "$HOME/.vst3/Pedagogical Compressor.vst3/Contents/x86_64-linux/PedagogicalCompressor.so"
```

O para todos los plugins a la vez:

```bash
chmod -R +rX ~/.vst3/
```

### Si el diagnóstico dice "el binario VST3 carga correctamente":

El problema **no** está en el plugin, sino en el DAW. Pasos:

1. **Forzar re-scan**:
   - REAPER: `Preferences → Plug-ins → VST → Re-scan`
   - Bitwig: `Settings → Plug-ins → Rescan`

2. **Verificar que `~/.vst3/` esté en los paths**:
   - REAPER: `Preferences → Plug-ins → VST → Plug-in paths`
   - Bitwig: `Settings → Locations → Plug-ins → VST3`

3. **Lanzar el DAW desde terminal** para ver los logs de carga:
   ```bash
   reaper 2>&1 | grep -i pedagogical
   bitwig-studio 2>&1 | grep -i pedagogical
   ```
   Si aparece algo como `failed to load` o `dlopen error`, ahí está el
   mensaje exacto del fallo.

---

## Mini-host VST3 (validador binario)

Si quieres validar directamente que el `.so` carga a nivel de SDK VST3,
sin pasar por el DAW:

```bash
cd scripts/mini_host_vst3
g++ -std=c++17 -O2 mini_host_vst3.cpp -ldl -lpthread -o mini_host_vst3
./mini_host_vst3 "$HOME/.vst3/Pedagogical Compressor.vst3"
```

Salida esperada si todo está bien:

```
✔ Archivo presente
✔ Permisos OK (executable)
✔ dlopen OK — todas las dependencias resueltas
✔ Símbolo 'GetPluginFactory' encontrado
✔ Factory devuelta correctamente
✔ El binario VST3 carga correctamente.
```

Si el mini-host dice OK pero el DAW no lista el plugin, el problema es
100% de configuración del DAW (paths / caché / blacklist), no del plugin.

---

## Validación exhaustiva con pluginval (opcional)

Para validar el plugin contra la suite oficial de Tracktion (100+ tests):

```bash
# Descargar de:
# https://github.com/Tracktion/pluginval/releases
# Linux x64 tar.gz

pluginval --validate-in-process --validate "$HOME/.vst3/Pedagogical Compressor.vst3"
```

Salida: PASS o FAIL con detalles de qué test falló.

---

## Resumen de pasos (TL;DR)

```bash
# 1. Recompilar limpio (por si hubo cambios)
./build.sh --clean && ./install.sh

# 2. Diagnóstico
bash scripts/diagnostico_vst3.sh

# 3. Mini-host (si quieres validar el binario)
cd scripts/mini_host_vst3
g++ -std=c++17 -O2 mini_host_vst3.cpp -ldl -lpthread -o mini_host_vst3
./mini_host_vst3 "$HOME/.vst3/Pedagogical Compressor.vst3"

# 4. Borrar caché de REAPER si quedó blacklisteado
rm -f ~/.config/REAPER/reaper-vstplugins64.ini
# (REAPER lo recrea al abrir, re-escaneando todo)

# 5. Abrir el DAW y forzar Re-scan VST3
```
