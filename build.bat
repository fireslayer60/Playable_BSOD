@echo off
setlocal enabledelayedexpansion

echo [PlayableBSOD] Initializing build environment...

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do (
    set "VS_PATH=%%i"
)

if not exist "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat" (
    echo [ERROR] Could not locate vcvars64.bat in !VS_PATH!
    exit /b 1
)

call "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat" >nul

set "WDK_INC=C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0"
set "WDK_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\km\x64"

if not exist "bin\x64\Release" mkdir bin\x64\Release
if not exist "obj\x64\Release" mkdir obj\x64\Release

echo [PlayableBSOD] Compiling src\driver.c...
cl.exe /nologo /c /kernel /W3 /WX- /O2 /Oi /Oy- /D_AMD64_ /D_WIN64 /DPOOL_NX_OPTIN=1 /I"!WDK_INC!\km" /I"!WDK_INC!\shared" /I"src" src\driver.c /Fo"obj\x64\Release\driver.obj"
if errorlevel 1 (
    echo [ERROR] Compilation failed.
    exit /b %errorlevel%
)

echo [PlayableBSOD] Linking bin\x64\Release\playable_bsod.sys...
link.exe /nologo /DRIVER /SUBSYSTEM:NATIVE /ENTRY:DriverEntry /MACHINE:X64 /NODEFAULTLIB /OPT:REF /OPT:ICF /LIBPATH:"!WDK_LIB!" obj\x64\Release\driver.obj ntoskrnl.lib hal.lib bufferoverflowfastfailk.lib /OUT:"bin\x64\Release\playable_bsod.sys"
if errorlevel 1 (
    echo [ERROR] Linking failed.
    exit /b %errorlevel%
)

echo [PlayableBSOD] Build completed successfully: bin\x64\Release\playable_bsod.sys
