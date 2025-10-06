@echo off
echo Checking Visual Studio Build Tools installation...

REM Check if we have the C++ compiler
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC" (
    echo C++ Build Tools are installed!
    goto :build
)

if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC" (
    echo C++ Build Tools are installed!
    goto :build
)

echo.
echo C++ Build Tools are not installed or incomplete.
echo For the best long-term pipeline, we need to install the C++ workload.
echo.
echo Option 1: Install C++ Build Tools automatically
echo Option 2: Manual installation guidance
echo Option 3: Try alternative approach with existing tools
echo.
set /p choice="Enter your choice (1, 2, or 3): "

if "%choice%"=="1" goto :auto_install
if "%choice%"=="2" goto :manual_guide
if "%choice%"=="3" goto :alternative
goto :end

:auto_install
echo.
echo Downloading and installing C++ Build Tools...
echo This will download the Visual Studio Installer and install C++ components.
echo.

REM Download and run Visual Studio Installer with C++ workload
powershell -Command "& {
    $url = 'https://aka.ms/vs/17/release/vs_buildtools.exe'
    $output = '$env:TEMP\vs_buildtools.exe'
    Write-Host 'Downloading Visual Studio Build Tools installer...'
    Invoke-WebRequest -Uri $url -OutFile $output
    Write-Host 'Running installer with C++ workload...'
    Start-Process $output -ArgumentList '--quiet', '--wait', '--add', 'Microsoft.VisualStudio.Workload.VCTools' -Wait
    Write-Host 'Installation complete!'
}"

echo.
echo Installation completed! Please restart this script to build your project.
pause
goto :end

:manual_guide
echo.
echo MANUAL INSTALLATION GUIDE:
echo.
echo 1. Open Visual Studio Installer
echo    - Search for "Visual Studio Installer" in Start Menu
echo    - Or run: "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vs_installer.exe"
echo.
echo 2. Modify your Build Tools 2022 installation
echo    - Click "Modify" next to "Build Tools for Visual Studio 2022"
echo.
echo 3. Select the C++ build tools workload:
echo    - Check "C++ build tools" workload
echo    - In the details panel, ensure these are selected:
echo      * MSVC v143 - VS 2022 C++ x64/x86 build tools
echo      * Windows 10/11 SDK (latest version)
echo      * CMake tools for C++
echo.
echo 4. Click "Modify" to install
echo.
echo 5. After installation, restart this script
echo.
pause
goto :end

:alternative
echo.
echo Trying alternative approach with existing tools...
echo This may have limitations but can work for basic builds.
echo.

REM Try to use what we have
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
    goto :build_alternative
)

echo No suitable build environment found.
echo Please choose option 1 or 2 for a complete installation.
pause
goto :end

:build_alternative
echo Attempting build with available tools...
where cl >nul 2>&1
if errorlevel 1 (
    echo C++ compiler still not available. Full installation required.
    pause
    goto :end
)
goto :build

:build
echo Setting up build environment...

REM Try Build Tools first
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo Build environment setup failed
    pause
    goto :end
)

echo Verifying compiler...
where cl >nul 2>&1
if errorlevel 1 (
    echo ERROR: C++ compiler not found
    pause
    goto :end
)

echo Compiler found: 
where cl

cd /d "%~dp0"
if exist build rmdir /s /q build
mkdir build
cd build

echo Configuring with CMake...
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release ..

if errorlevel 1 (
    echo CMake configuration failed
    pause
    goto :end
)

echo Building project...
cmake --build . --config Release --parallel

if errorlevel 1 (
    echo Build failed
    pause
    goto :end
)

echo.
echo ===== BUILD SUCCESS! =====
echo Looking for your HeartSync Modulator executable...
dir /s *.exe
echo.
echo Your long-term build pipeline is now ready!
echo Use this script anytime to build your project.

:end
pause
