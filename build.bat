@echo off
REM ============================================================
REM  build.bat — Compila el Pedagogical Compressor en Windows
REM
REM  Requisitos (instalar una sola vez):
REM    - Visual Studio 2022 (workload "Desktop development with C++")
REM    - CMake 3.22+ (https://cmake.org/download/)
REM    - Git (https://git-scm.com/download/win)
REM ============================================================

setlocal

set "PROJECT_DIR=%~dp0"
set "BUILD_DIR=%PROJECT_DIR%build"
set "BUILD_TYPE=Release"

echo ----------------------------------------------------------
echo  Pedagogical Compressor — Build script (Windows x64)
echo ----------------------------------------------------------

echo -^> Configurando CMake (Visual Studio 17 2022 - x64)...
cmake -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -G "Visual Studio 17 2022" -A x64 -S "%PROJECT_DIR%"
if errorlevel 1 (
    echo ERROR: cmake configure fallo.
    pause
    exit /b 1
)

echo -^> Compilando (puede tardar varios minutos la primera vez)...
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% -j
if errorlevel 1 (
    echo ERROR: build fallo.
    pause
    exit /b 1
)

echo.
echo ----------------------------------------------------------
echo  √ Build completo. Binarios en:
echo    %BUILD_DIR%\PedagogicalCompressor_artefacts\Release\VST3\
echo    %BUILD_DIR%\PedagogicalCompressor_artefacts\Release\CLAP\
echo    %BUILD_DIR%\PedagogicalCompressor_artefacts\Release\Standalone\
echo.
echo  Para instalar ejecuta:  install.bat
echo ----------------------------------------------------------
pause
