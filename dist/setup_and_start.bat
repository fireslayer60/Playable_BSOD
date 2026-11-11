@echo off
echo ===================================================================
echo [PlayableBSOD] VM Setup and Driver Installer
echo ===================================================================

:: 1. Install Test Certificate into Trusted Root & Trusted Publisher
echo [1/4] Installing Test Signing Certificate...
if exist "%~dp0PlayableBSOD.cer" (
    certutil -addstore -f "root" "%~dp0PlayableBSOD.cer" >nul 2>&1
    certutil -addstore -f "trustedpublisher" "%~dp0PlayableBSOD.cer" >nul 2>&1
    echo Test certificate installed successfully.
) else (
    echo [WARNING] PlayableBSOD.cer not found in this folder.
)

:: 2. Enable test signing & disable integrity checks
echo [2/4] Verifying Test Signing mode...
bcdedit /set testsigning on >nul 2>&1

:: 3. Disable AutoReboot on crash
echo [3/4] Disabling automatic reboot on crash...
reg add "HKLM\SYSTEM\CurrentControlSet\Control\CrashControl" /v AutoReboot /t REG_DWORD /d 0 /f >nul 2>&1

:: 4. Register and start driver
echo [4/4] Registering, recording display mode, and starting Playable BSOD kernel driver...
sc stop playable_bsod >nul 2>&1
sc delete playable_bsod >nul 2>&1
sc create playable_bsod type= kernel binPath= "%~dp0playable_bsod.sys"
:: Record the current resolution for the driver (it maps the framebuffer at load time)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$v = Get-CimInstance Win32_VideoController | Where-Object { $_.CurrentHorizontalResolution } | Select-Object -First 1; if ($v) { $k = 'HKLM:\SYSTEM\CurrentControlSet\Services\playable_bsod\Parameters'; New-Item -Path $k -Force | Out-Null; Set-ItemProperty -Path $k -Name Width -Value ([int]$v.CurrentHorizontalResolution) -Type DWord; Set-ItemProperty -Path $k -Name Height -Value ([int]$v.CurrentVerticalResolution) -Type DWord; Write-Host ('Recorded display mode: {0}x{1} ({2})' -f $v.CurrentHorizontalResolution, $v.CurrentVerticalResolution, $v.Name) } else { Write-Host 'Could not read display mode; driver will use live detection or 1024x768.' }"
sc start playable_bsod

echo.
echo ===================================================================
if %errorlevel% equ 0 (
    echo [SUCCESS] Driver is RUNNING and armed!
    echo Next: Run notmyfault64.exe and click "Crash" to start the game!
) else (
    echo [ERROR] Driver failed to start.
    echo Please make sure you right-clicked and selected "Run as administrator".
)
echo ===================================================================
pause
