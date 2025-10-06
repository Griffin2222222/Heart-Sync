@echo off
echo Setting up Visual Studio Build Tools Environment...

REM Clear any existing compiler variables
set CC=
set CXX=
set CMAKE_C_COMPILER=
set CMAKE_CXX_COMPILER=

REM Try different possible paths for Build Tools
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    echo Found Build Tools at x86 location
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    echo Found Build Tools at standard location
    call "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    echo Found Community with C++ tools
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    echo Found Community with C++ tools
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo ERROR: Could not find Visual Studio Build Tools
    echo Please ensure Visual Studio Build Tools 2022 is properly installed
    pause
    exit /b 1
)

REM Verify compiler is available
where cl >nul 2>&1
if errorlevel 1 (
    echo ERROR: MSVC compiler not found after environment setup
    echo PATH: %PATH%
    pause
    exit /b 1
)

echo MSVC Environment loaded successfully
echo Compiler found at:
where cl

REM Navigate to project directory
cd /d "%~dp0"

REM Clean build directory
if exist build rmdir /s /q build
mkdir build
cd build

REM Configure with Visual Studio generator
echo Configuring project with CMake...
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_CONFIGURATION_TYPES=Release ^
    ..

if errorlevel 1 (
    echo CMake configuration failed
    pause
    exit /b 1
)

REM Build the project
echo Building project...
cmake --build . --config Release --parallel

if errorlevel 1 (
    echo Build failed
    pause
    exit /b 1
)

echo Build completed successfully!
echo Looking for output files...
dir /s *.exe

echo.
echo Build pipeline is now working! You can use this script for future builds.
pause
