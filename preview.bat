@echo off
setlocal enabledelayedexpansion

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do (
    set "VS_PATH=%%i"
)

if not exist "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat" (
    echo [ERROR] Could not locate vcvars64.bat in !VS_PATH!
    exit /b 1
)

call "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat" >nul

if not exist "bin" mkdir bin

echo [PlayableBSOD] Compiling preview tool...
cl.exe /nologo /O2 /W3 /I"src" tools\preview.c src\driver.c user32.lib gdi32.lib /Fe"bin\preview.exe" /Fo"bin\\"
if errorlevel 1 (
    echo [ERROR] Compilation of preview tool failed.
    exit /b %errorlevel%
)

if "%1"=="--build-only" (
    echo [PlayableBSOD] Build completed: bin\preview.exe
    exit /b 0
)

echo [PlayableBSOD] Launching preview tool...
bin\preview.exe %*
