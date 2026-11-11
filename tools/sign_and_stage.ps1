# Signs bin\x64\Release\playable_bsod.sys with the PlayableBSOD test certificate,
# copies it into dist\, and (with -Serve) hosts dist\ on port 8000 for the VM.
param([switch]$Serve, [int]$Port = 8000)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$sys  = Join-Path $root 'bin\x64\Release\playable_bsod.sys'
$dist = Join-Path $root 'dist'
$cerPath = Join-Path $dist 'PlayableBSOD.cer'

function Step($msg) { Write-Host "[PlayableBSOD] $msg" -ForegroundColor Cyan }

if (-not $Serve) {
    if (-not (Test-Path $sys)) { throw "Build output not found: $sys (did build.bat fail?)" }

    # ---- 1. Find the test-signing certificate (the one the VM already trusts) ----
    $knownThumb = 'EC96E02D97A0AF268D18603F374A06BC267E5ADC'
    $stores = 'Cert:\CurrentUser\My', 'Cert:\LocalMachine\My'
    $all = Get-ChildItem $stores -ErrorAction SilentlyContinue | Where-Object { $_.HasPrivateKey }
    $cert = $all | Where-Object { $_.Thumbprint -eq $knownThumb } | Select-Object -First 1
    if (-not $cert) {
        $cert = $all | Where-Object { $_.Subject -eq 'CN=PlayableBSOD Test' -and $_.NotAfter -gt (Get-Date) } |
            Sort-Object NotAfter -Descending | Select-Object -First 1
    }
    if (-not $cert) {
        Step "Test certificate not found in your store - creating a new one."
        $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=PlayableBSOD Test' `
            -CertStoreLocation 'Cert:\CurrentUser\My' -KeyAlgorithm RSA -KeyLength 2048 `
            -HashAlgorithm SHA256 -NotAfter (Get-Date).AddYears(1)
        Export-Certificate -Cert $cert -FilePath $cerPath -Type CERT | Out-Null
        Write-Host "  NOTE: new certificate exported to dist\PlayableBSOD.cer. run.bat installs it in the VM." -ForegroundColor Yellow
    }
    $machineStore = $cert.PSParentPath -like '*LocalMachine*'
    Step ("Signing with '{0}' ({1}, {2} store)" -f $cert.Subject, $cert.Thumbprint, $(if ($machineStore) {'machine'} else {'user'}))

    # ---- 2. Sign (signtool from the newest Windows Kit, else PowerShell's Authenticode) ----
    $signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1
    if ($signtool) {
        $stArgs = @('sign', '/fd', 'SHA256', '/s', 'My', '/sha1', $cert.Thumbprint)
        if ($machineStore) { $stArgs += '/sm' }
        $stArgs += $sys
        & $signtool.FullName @stArgs
        if ($LASTEXITCODE -ne 0) { throw "signtool failed with exit code $LASTEXITCODE" }
    } else {
        Step "signtool.exe not found - using Set-AuthenticodeSignature"
        $r = Set-AuthenticodeSignature -FilePath $sys -Certificate $cert -HashAlgorithm SHA256
        if (-not $r.SignerCertificate) { throw "Signing failed: $($r.StatusMessage)" }
    }

    $sig = Get-AuthenticodeSignature $sys
    if ($sig.SignerCertificate.Thumbprint -ne $cert.Thumbprint) { throw "Signature check failed on $sys" }
    Step "Signed OK (on this PC it shows as untrusted; that's expected for a test cert)."

    # ---- 3. Stage into dist\ ----
    Copy-Item $sys (Join-Path $dist 'playable_bsod.sys') -Force
    if (-not (Test-Path $cerPath)) { Export-Certificate -Cert $cert -FilePath $cerPath -Type CERT | Out-Null }
    foreach ($exe in 'notmyfault64.exe', 'notmyfaultc64.exe') {
        $src = Join-Path $root "tools\notmyfault\$exe"
        if (-not (Test-Path (Join-Path $dist $exe)) -and (Test-Path $src)) { Copy-Item $src $dist }
    }
    Step "dist\ is ready:"
    Get-ChildItem $dist | Format-Table Name, Length, LastWriteTime -AutoSize | Out-String | Write-Host
    exit 0
}

# ---- -Serve: show addresses, then host dist\ ----
$ips = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
    Where-Object { $_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.254.*' } |
    Select-Object IPAddress, InterfaceAlias
Step "This PC's addresses:"
$ips | Format-Table -AutoSize | Out-String | Write-Host
$runBat = Get-Content (Join-Path $dist 'run.bat') -Raw
if ($runBat -match 'http://([\d\.]+):') {
    $want = $Matches[1]
    if ($ips.IPAddress -contains $want) {
        Step "run.bat points at $want - that's this PC. Good."
    } else {
        Write-Host "  WARNING: run.bat downloads from $want, which is NOT one of this PC's addresses." -ForegroundColor Yellow
        Write-Host "  Edit dist\run.bat to use the address above that the VM can reach." -ForegroundColor Yellow
    }
}

Step "Serving dist\ on port $Port. In the VM (admin Command Prompt) run:"
Write-Host "  powershell -c `"iwr http://<this-PC-IP>:$Port/run.bat -OutFile `$env:TEMP\run.bat`"; %TEMP%\run.bat" -ForegroundColor Green
Write-Host "Leave this window open until the VM has downloaded everything. Ctrl+C to stop." -ForegroundColor Cyan

# Prefer Python's http.server (the firewall likely already allows it); the
# real python, not the Microsoft Store stub, is required.
$py = Get-Command python -ErrorAction SilentlyContinue
if ($py) {
    & $py.Source -c "import http.server" 2>$null
    if ($LASTEXITCODE -eq 0) {
        Step "Using Python http.server ($($py.Source))"
        Push-Location $dist
        try { & $py.Source -m http.server $Port } finally { Pop-Location }
        exit 0
    }
}
Step "Python not available - using built-in PowerShell server"

# Minimal static file server (no admin rights or URL ACLs needed)
$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Any, $Port)
$listener.Start()
try {
    while ($true) {
        $client = $listener.AcceptTcpClient()
        try {
            $stream = $client.GetStream()
            $reader = [System.IO.StreamReader]::new($stream)
            $requestLine = $reader.ReadLine()
            while (($line = $reader.ReadLine()) -ne $null -and $line -ne '') { }
            $name = if ($requestLine -match '^GET /([^ ?]*)') { [Uri]::UnescapeDataString($Matches[1]) } else { '' }
            $file = Join-Path $dist ([System.IO.Path]::GetFileName($name))
            if ($name -and (Test-Path $file -PathType Leaf)) {
                $bytes = [System.IO.File]::ReadAllBytes($file)
                $hdr = "HTTP/1.1 200 OK`r`nContent-Type: application/octet-stream`r`nContent-Length: $($bytes.Length)`r`nConnection: close`r`n`r`n"
                $h = [Text.Encoding]::ASCII.GetBytes($hdr)
                $stream.Write($h, 0, $h.Length); $stream.Write($bytes, 0, $bytes.Length)
                Write-Host ("  {0}  200  {1}  ({2:N0} bytes) -> {3}" -f (Get-Date -Format HH:mm:ss), $name, $bytes.Length, $client.Client.RemoteEndPoint)
            } else {
                $h = [Text.Encoding]::ASCII.GetBytes("HTTP/1.1 404 Not Found`r`nContent-Length: 0`r`nConnection: close`r`n`r`n")
                $stream.Write($h, 0, $h.Length)
                Write-Host ("  {0}  404  {1}" -f (Get-Date -Format HH:mm:ss), $name) -ForegroundColor Yellow
            }
        } catch {
            Write-Host "  request error: $_" -ForegroundColor Yellow
        } finally {
            $client.Close()
        }
    }
} finally {
    $listener.Stop()
}
