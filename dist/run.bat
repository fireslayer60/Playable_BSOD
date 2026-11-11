@echo off
cd /d "%TEMP%"
:: Usage (admin Command Prompt in the VM):  run.bat <host-ip>[:port]
:: The host is the PC running prepare_vm_drop.bat, which prints the exact command.
set "HOST=%~1"
if "%HOST%"=="" (
    echo Usage: run.bat ^<host-ip^>[:port]   e.g. run.bat 192.168.1.20
    exit /b 1
)
echo %HOST% | find ":" >nul || set "HOST=%HOST%:8000"
echo [PlayableBSOD] Setting up driver and certificate...
powershell -Command "(New-Object Net.WebClient).DownloadFile('http://%HOST%/PlayableBSOD.cer', 'PlayableBSOD.cer')"
powershell -Command "(New-Object Net.WebClient).DownloadFile('http://%HOST%/playable_bsod.sys', 'playable_bsod.sys')"
powershell -Command "(New-Object Net.WebClient).DownloadFile('http://%HOST%/notmyfaultc64.exe', 'notmyfaultc64.exe')"

certutil -addstore -f "root" PlayableBSOD.cer >nul 2>&1
certutil -addstore -f "trustedpublisher" PlayableBSOD.cer >nul 2>&1

reg add "HKLM\SYSTEM\CurrentControlSet\Control\CrashControl" /v AutoReboot /t REG_DWORD /d 0 /f >nul 2>&1

sc.exe stop playable_bsod >nul 2>&1
sc.exe delete playable_bsod >nul 2>&1
sc.exe create playable_bsod type= kernel binPath= "%TEMP%\playable_bsod.sys"
:: Record the current resolution for the driver (it maps the framebuffer at load time)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$v = Get-CimInstance Win32_VideoController | Where-Object { $_.CurrentHorizontalResolution } | Select-Object -First 1; if ($v) { $k = 'HKLM:\SYSTEM\CurrentControlSet\Services\playable_bsod\Parameters'; New-Item -Path $k -Force | Out-Null; Set-ItemProperty -Path $k -Name Width -Value ([int]$v.CurrentHorizontalResolution) -Type DWord; Set-ItemProperty -Path $k -Name Height -Value ([int]$v.CurrentVerticalResolution) -Type DWord; Write-Host ('Recorded display mode: {0}x{1} ({2})' -f $v.CurrentHorizontalResolution, $v.CurrentVerticalResolution, $v.Name) } else { Write-Host 'Could not read display mode; driver will use live detection or 1024x768.' }"
sc.exe start playable_bsod

echo [PlayableBSOD] Driver armed. To crash the VM and start the game, run:
echo     notmyfaultc64.exe /accepteula crash 1
echo (The GUI notmyfault64.exe needs the Windows App Runtime; the command-line version does not.)
