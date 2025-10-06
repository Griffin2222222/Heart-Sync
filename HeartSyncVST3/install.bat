@echo off
echo Installing C++ Build Tools for long-term development pipeline...
echo.

REM Create a PowerShell script file to avoid escaping issues
echo Write-Host "Downloading Visual Studio Build Tools installer..." > install_cpp.ps1
echo $url = "https://aka.ms/vs/17/release/vs_buildtools.exe" >> install_cpp.ps1
echo $output = "$env:TEMP\vs_buildtools.exe" >> install_cpp.ps1
echo Invoke-WebRequest -Uri $url -OutFile $output >> install_cpp.ps1
echo Write-Host "Downloaded installer to $output" >> install_cpp.ps1
echo Write-Host "Starting installation with C++ workload..." >> install_cpp.ps1
echo Start-Process $output -ArgumentList "--quiet", "--wait", "--add", "Microsoft.VisualStudio.Workload.VCTools", "--add", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "--add", "Microsoft.VisualStudio.Component.Windows10SDK.19041" -Wait >> install_cpp.ps1
echo Write-Host "C++ Build Tools installation completed!" >> install_cpp.ps1

echo Running PowerShell installation script...
powershell -ExecutionPolicy Bypass -File install_cpp.ps1

REM Clean up
del install_cpp.ps1

echo.
echo Installation complete! The C++ build tools should now be available.
echo Verifying installation...

REM Check if VC tools are now available
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC" (
    echo SUCCESS: C++ Build Tools found!
    goto :build_now
) else (
    echo Checking alternative location...
    if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC" (
        echo SUCCESS: C++ Build Tools found!
        goto :build_now
    ) else (
        echo Installation may need a few moments to complete.
        echo Please run this script again in a minute, or restart VS Code.
        pause
        exit /b 0
    )
)

:build_now
echo.
echo Ready to build! Setting up environment...

REM Setup build environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

REM Verify compiler
where cl >nul 2>&1
if errorlevel 1 (
    echo Compiler setup incomplete. May need system restart.
    echo Try running this script again or restart your computer.
    pause
    exit /b 1
)

echo Compiler ready! Building HeartSync Modulator...

cd /d "%~dp0"
if exist build rmdir /s /q build
mkdir build
cd build

cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release ..
if errorlevel 1 (
    echo CMake configuration failed
    pause
    exit /b 1
)

cmake --build . --config Release --parallel
if errorlevel 1 (
    echo Build failed
    pause
    exit /b 1
)

echo.
echo ===== SUCCESS! =====
echo Your HeartSync Modulator is built!
echo.
dir /s *.exe
echo.
echo Long-term pipeline is ready - use this script anytime to build!
pause
