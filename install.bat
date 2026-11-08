@echo off
REM ============================================================
REM  install.bat — Instala el Pedagogical Compressor en Windows
REM
REM  Uso:  doble clic sobre este archivo  (o ejecútalo desde cmd)
REM  Requiere: que el build ya esté hecho (carpeta build\ existente)
REM ============================================================

setlocal EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "ART=%SCRIPT_DIR%build\PedagogicalCompressor_artefacts\Release"
set "DEST_VST3=%USERPROFILE%\Documents\VST3"
set "DEST_CLAP=%USERPROFILE%\Documents\CLAP"
set "DEST_EXE=%LOCALAPPDATA%\Programs\PedagogicalCompressor"

if not exist "%ART%" (
    echo ERROR: no existe %ART%
    echo Compila primero con:  build.bat
    exit /b 1
)

echo → Eliminando versiones anteriores...
if exist "%DEST_VST3%\Pedagogical Compressor.vst3" rmdir /s /q "%DEST_VST3%\Pedagogical Compressor.vst3"
if exist "%DEST_CLAP%\Pedagogical Compressor.clap" rmdir /s /q "%DEST_CLAP%\Pedagogical Compressor.clap"
if exist "%DEST_EXE%" rmdir /s /q "%DEST_EXE%"

echo → Copiando plugins...
if not exist "%DEST_VST3%" mkdir "%DEST_VST3%"
if not exist "%DEST_CLAP%" mkdir "%DEST_CLAP%"
if not exist "%DEST_EXE%"  mkdir "%DEST_EXE%"

if exist "%ART%\VST3\Pedagogical Compressor.vst3" (
    xcopy /E /I /Y "%ART%\VST3\Pedagogical Compressor.vst3" "%DEST_VST3%\Pedagogical Compressor.vst3\" >NUL
    echo   √ VST3       -^> %USERPROFILE%\Documents\VST3\
)

if exist "%ART%\CLAP\Pedagogical Compressor.clap" (
    xcopy /E /I /Y "%ART%\CLAP\Pedagogical Compressor.clap" "%DEST_CLAP%\Pedagogical Compressor.clap\" >NUL
    echo   √ CLAP       -^> %USERPROFILE%\Documents\CLAP\
)

if exist "%ART%\Standalone\Pedagogical Compressor.exe" (
    copy /Y "%ART%\Standalone\Pedagogical Compressor.exe" "%DEST_EXE%\PedagogicalCompressor.exe" >NUL
    echo   √ Standalone  -^> %LOCALAPPDATA%\Programs\PedagogicalCompressor\
)

echo.
echo ✔ Instalacion completa.
echo   En REAPER:  Options ^> Preferences ^> Plug-ins ^> VST (y CLAP) ^> Re-scan.
echo   Standalone: ejecuta PedagogicalCompressor.exe desde el menu Inicio.
echo.
pause
