# Run as Administrator inside the target VirtualBox / VMware / QEMU guest VM.
# (Hyper-V Gen 2 guests are not supported: they have no 8042 keyboard controller.)
Write-Host "[PlayableBSOD] Configuring guest OS for kernel driver testing..." -ForegroundColor Cyan

# Allow loading drivers signed with a self-signed test certificate
bcdedit /set testsigning on

# Keep the crash screen up instead of rebooting immediately (the game reboots on its own)
Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\CrashControl" -Name "AutoReboot" -Value 0
Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\CrashControl" -Name "CrashDumpEnabled" -Value 1

# Optional: enable kernel debugging if you have a debugger attached over KDNET/serial.
# Without a debugger connected this can make a bugcheck stop and wait, so it is off by default.
# bcdedit /debug on

Write-Host "[PlayableBSOD] Configuration complete. Please restart the guest VM." -ForegroundColor Green
