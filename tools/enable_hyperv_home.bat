@echo off
:: Enable Hyper-V on Windows 11 Home Editions
echo [PlayableBSOD] Enabling Hyper-V on Windows 11 Home...
echo [PlayableBSOD] Please ensure you are running this Command Prompt as Administrator.

pushd "%~dp0"

dir /b %SystemRoot%\servicing\Packages\*Hyper-V*.mum >hyper-v.txt

for /f %%i in ('findstr /i . hyper-v.txt 2^>nul') do (
    echo Adding package: %%i
    dism /online /norestart /add-package:"%SystemRoot%\servicing\Packages\%%i"
)

del hyper-v.txt

echo [PlayableBSOD] Activating Hyper-V features...
dism /online /enable-feature /featurename:Microsoft-Hyper-V-All /LimitAccess /ALL

echo.
echo ===================================================================
echo [PlayableBSOD] Hyper-V installation complete!
echo Please RESTART your computer to finalize Hyper-V activation.
echo After rebooting, search for 'Hyper-V Manager' in your Start Menu.
echo ===================================================================
pause
