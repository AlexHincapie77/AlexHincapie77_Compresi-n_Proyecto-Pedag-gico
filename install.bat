@echo off
REM ============================================================
REM  install.bat — Instala VST3, CLAP y Standalone en Windows.
REM
REM  Layout en Windows:
REM    VST3       -^> bundle (carpeta .vst3 con subestructura)
REM    CLAP       -^> ARCHIVO ÚNICO .clap (DLL renombrada, NO carpeta)
REM    Standalone -^> .exe
REM
REM  Rutas destino:
REM    - Instalación por usuario (no requiere admin):
REM        VST3: %LOCALAPPDATA%\Programs\Common\VST3\
REM        CLAP: %LOCALAPPDATA%\Programs\Common\CLAP\
REM    - El instalador .exe (InnoSetup) instala en system-wide:
REM        VST3: %COMMONPROGRAMFILES%\VST3\
REM        CLAP: %COMMONPROGRAMFILES%\CLAP\
REM    - Ambas rutas son escaneadas automáticamente por REAPER, Bitwig,
REM      Studio One, Cubase 10+, Ableton Live 11+ y Cakewalk.
REM ============================================================

setlocal EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "ART=%SCRIPT_DIR%build\PedagogicalCompressor_artefacts\Release"

REM Rutas estándar de plugins en Windows (per-user, no requiere admin)
set "DEST_VST3=%LOCALAPPDATA%\Programs\Common\VST3"
set "DEST_CLAP=%LOCALAPPDATA%\Programs\Common\CLAP"
set "DEST_STANDALONE=%LOCALAPPDATA%\Programs\PedagogicalCompressor"

if not exist "%ART%" (
    echo ERROR: no existe %ART%
    echo Compila primero con:  build.bat
    pause
    exit /b 1
)

echo -^> Eliminando versiones anteriores (archivo o carpeta)...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$ErrorActionPreference='SilentlyContinue';" ^
    "Remove-Item -LiteralPath '%DEST_VST3%\Pedagogical Compressor.vst3' -Force -Recurse;" ^
    "Remove-Item -LiteralPath '%DEST_CLAP%\Pedagogical Compressor.clap' -Force -Recurse;" ^
    "Remove-Item -LiteralPath '%DEST_STANDALONE%' -Force -Recurse"

echo -^> Creando carpetas destino...
if not exist "%DEST_VST3%" mkdir "%DEST_VST3%"
if not exist "%DEST_CLAP%" mkdir "%DEST_CLAP%"
if not exist "%DEST_STANDALONE%" mkdir "%DEST_STANDALONE%"

echo -^> Copiando plugins...

REM === VST3 (bundle/carpeta en Windows) ===
if exist "%ART%\VST3\Pedagogical Compressor.vst3" (
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "Copy-Item -LiteralPath '%ART%\VST3\Pedagogical Compressor.vst3' -Destination '%DEST_VST3%' -Recurse -Force"
    if exist "%DEST_VST3%\Pedagogical Compressor.vst3" (
        echo   √ VST3       -^> %DEST_VST3%\
    ) else (
        echo   × ERROR: VST3 no se copió. Revisa permisos.
    )
) else (
    echo   ! VST3 no encontrado en %ART%\VST3\
)

REM === CLAP (ARCHIVO ÚNICO en Windows, NO carpeta) ===
if exist "%ART%\CLAP\Pedagogical Compressor.clap" (
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "Copy-Item -LiteralPath '%ART%\CLAP\Pedagogical Compressor.clap' -Destination '%DEST_CLAP%' -Force"
    if exist "%DEST_CLAP%\Pedagogical Compressor.clap" (
        echo   √ CLAP       -^> %DEST_CLAP%\
    ) else (
        echo   × ERROR: CLAP no se copió.
    )
) else (
    echo   ! CLAP no encontrado en %ART%\CLAP\
)

REM === Standalone ===
if exist "%ART%\Standalone\Pedagogical Compressor.exe" (
    copy /Y "%ART%\Standalone\Pedagogical Compressor.exe" "%DEST_STANDALONE%\PedagogicalCompressor.exe" >NUL
    echo   √ Standalone  -^> %DEST_STANDALONE%\
) else (
    echo   ! Standalone no encontrado en %ART%\Standalone\
)

echo.
echo √ Instalacion completa.
echo   - VST3: %DEST_VST3%\Pedagogical Compressor.vst3
echo   - CLAP: %DEST_CLAP%\Pedagogical Compressor.clap
echo   - App:  %DEST_STANDALONE%\PedagogicalCompressor.exe
echo.
echo NOTA: Instalado por usuario (no requiere admin).
echo       Para instalar system-wide, usa el instalador .exe (InnoSetup):
echo         installer\Output\PedagogicalCompressor-Setup.exe
echo.
echo REAPER:  Options ^> Preferences ^> Plug-ins ^>
echo          VST plug-ins (Re-scan)
echo          CLAP plug-ins (Re-scan)   [REAPER 6.71+]
echo.
echo Bitwig:  Settings ^> Locations ^> Plug-ins ^> Rescan
echo.
pause
