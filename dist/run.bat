@echo off
cd /d "%TEMP%"
echo [PlayableBSOD] Setting up driver and certificate...
powershell -Command "(New-Object Net.WebClient).DownloadFile('http://192.168.1.16:8000/PlayableBSOD.cer', 'PlayableBSOD.cer')"
powershell -Command "(New-Object Net.WebClient).DownloadFile('http://192.168.1.16:8000/playable_bsod.sys', 'playable_bsod.sys')"
powershell -Command "(New-Object Net.WebClient).DownloadFile('http://192.168.1.16:8000/notmyfault64.exe', 'notmyfault64.exe')"

certutil -addstore -f "root" PlayableBSOD.cer >nul 2>&1
certutil -addstore -f "trustedpublisher" PlayableBSOD.cer >nul 2>&1

reg add "HKLM\SYSTEM\CurrentControlSet\Control\CrashControl" /v AutoReboot /t REG_DWORD /d 0 /f >nul 2>&1

sc.exe stop playable_bsod >nul 2>&1
sc.exe delete playable_bsod >nul 2>&1
sc.exe create playable_bsod type= kernel binPath= "%TEMP%\playable_bsod.sys"
sc.exe start playable_bsod

echo [PlayableBSOD] Launching NotMyFault...
start notmyfault64.exe
