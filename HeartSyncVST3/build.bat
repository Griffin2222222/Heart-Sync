@echo off
echo Attempting to build HeartSync Modulator...

REM Try to initialize any available VS environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64

echo Checking for available compilers...
echo.

REM Check what we have available
where cl 2>nul
if not errorlevel 1 (
    echo Found MSVC compiler!
    goto :build
)

where gcc 2>nul  
if not errorlevel 1 (
    echo Found GCC compiler, trying MinGW approach...
    goto :build_mingw
)

echo No suitable compiler found.
echo.
echo Let's try installing MinGW-w64 as an alternative approach:
echo This is lightweight and will work for JUCE projects.
echo.
set /p choice="Install MinGW-w64? (y/n): "
if /i "%choice%"=="y" goto :install_mingw
goto :end

:install_mingw
echo Downloading MinGW-w64...
powershell -Command "Invoke-WebRequest -Uri 'https://github.com/niXman/mingw-builds-binaries/releases/download/13.2.0-rt_v11-rev1/x86_64-13.2.0-release-posix-seh-ucrt-rt_v11-rev1.7z' -OutFile 'mingw64.7z'"

echo Please extract mingw64.7z to C:\mingw64 and add C:\mingw64\bin to your PATH
echo Then restart this script.
pause
goto :end

:build_mingw
echo Building with MinGW...
cd /d "%~dp0"
if exist build rmdir /s /q build
mkdir build
cd build

cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
if errorlevel 1 (
    echo CMake failed with MinGW
    pause
    goto :end
)

mingw32-make -j4
goto :check_output

:build
echo Building with MSVC...
cd /d "%~dp0"
if exist build rmdir /s /q build
mkdir build  
cd build

cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release ..
if errorlevel 1 (
    echo CMake failed
    pause
    goto :end
)

cmake --build . --config Release --parallel

:check_output
if errorlevel 1 (
    echo Build failed
    pause
    goto :end
)

echo.
echo ===== BUILD SUCCESS! =====
echo.
echo Looking for your HeartSync Modulator executable:
dir /s *.exe

echo.
echo Your build pipeline is working! 
echo You can now test with a Bluetooth heart rate monitor.

:end
pause
