@echo off
:: One-click PC side of a VM test: build -> sign -> stage dist\ -> serve on :8000
cd /d "%~dp0"

echo ===================================================================
echo [PlayableBSOD] 1/3 Building driver
echo ===================================================================
call build.bat
if errorlevel 1 goto :fail

echo.
echo ===================================================================
echo [PlayableBSOD] 2/3 Signing and staging dist\
echo ===================================================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\sign_and_stage.ps1"
if errorlevel 1 goto :fail

echo.
echo ===================================================================
echo [PlayableBSOD] 3/3 Serving dist\ to the VM
echo ===================================================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\sign_and_stage.ps1" -Serve
goto :eof

:fail
echo.
echo [PlayableBSOD] FAILED - see the messages above.
pause
exit /b 1
