# Run as Administrator inside target Hyper-V / QEMU Guest VM
Write-Host "[PlayableBSOD] Configuring Guest OS for Kernel Driver Testing..." -ForegroundColor Cyan

# Enable test signing for unsigned driver binaries
bcdedit /set testsigning on

# Enable kernel debugging
bcdedit /debug on

# Disable automatic reboot on system failure so the display state persists
Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\CrashControl" -Name "AutoReboot" -Value 0
Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\CrashControl" -Name "CrashDumpEnabled" -Value 1

Write-Host "[PlayableBSOD] Configuration complete. Please restart the guest VM." -ForegroundColor Green
