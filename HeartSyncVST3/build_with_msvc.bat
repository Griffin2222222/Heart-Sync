@echo off
setlocal enableextensions enabledelayedexpansion

echo Setting up MSVC environment...
set "VSBT_VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "VSBT_VCVARS_ALT=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "VSCOMM_VSDEVCMD=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"

if exist "%VSBT_VCVARS%" (
    call "%VSBT_VCVARS%"
) else if exist "%VSBT_VCVARS_ALT%" (
    call "%VSBT_VCVARS_ALT%"
) else if exist "%VSCOMM_VSDEVCMD%" (
    call "%VSCOMM_VSDEVCMD%" -arch=x64
) else (
    echo Could not find a suitable Visual Studio environment script.
    echo Please install Visual Studio Build Tools 2022 (C++), then retry.
    pause
    exit /b 1
)

where cl >nul 2>&1
if errorlevel 1 (
    echo MSVC compiler not available after environment setup.
    echo A reboot may be required after installing Build Tools.
    pause
    exit /b 1
)

cd /d "%~dp0"
if exist build rmdir /s /q build
mkdir build

echo Configuring CMake (Visual Studio 17 2022, x64)...
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo CMake configure failed.
    pause
    exit /b 1
)

echo Building Release...
cmake --build build --config Release --parallel
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

echo.
echo ===== BUILD SUCCESS =====
echo Searching for artefacts (.vst3 and .exe):
dir /s /b build\*.vst3 2>nul
dir /s /b build\*.exe 2>nul
echo.
pause
endlocal
