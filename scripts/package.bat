@echo off
echo === Enjin Engine Binary Distribution Builder ===
echo.

set BUILD_DIR=%~dp0..\build-dist
set SOURCE_DIR=%~dp0..

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo [1/3] Configuring CMake...
cmake -S "%SOURCE_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="%BUILD_DIR%/install"

echo [2/3] Building...
cmake --build "%BUILD_DIR%" --config Release --parallel

echo [3/3] Packaging...
cd "%BUILD_DIR%" && cpack -C Release

echo.
echo Done! Package is in %BUILD_DIR%
pause
