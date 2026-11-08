@echo off
setlocal
set "PROJECT_DIR=%~dp0"
set "BUILD_DIR=%PROJECT_DIR%build"
set "BUILD_TYPE=Release"
echo ----------------------------------------------------------
echo  Pedagogical Compressor - Build (Windows x64)
echo ----------------------------------------------------------
echo -^> Configurando CMake (VS 2022 - x64)...
cmake -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -G "Visual Studio 17 2022" -A x64 -S "%PROJECT_DIR%"
if errorlevel 1 exit /b 1
echo -^> Compilando...
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% -j
if errorlevel 1 exit /b 1
echo.
echo  Build OK. Instala con: install.bat
pause
