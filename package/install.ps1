param(
    [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\LEGO Harry Potter Years 5-7"
)

$ErrorActionPreference = "Stop"

$PackageDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$FixDll = Join-Path $PackageDir "bin\d3d9.dll"
$DxvkDll = Join-Path $PackageDir "bin\dxvk_d3d9.dll"
$Config = Join-Path $PackageDir "lhp2_fix.ini"
$DxvkConf = Join-Path $PackageDir "dxvk.conf"

if (-not (Test-Path -LiteralPath $GameDir)) {
    throw "Game directory not found: $GameDir"
}

$GameDir = (Resolve-Path -LiteralPath $GameDir).Path
$HarryExe = Join-Path $GameDir "harry2.exe"
if (-not (Test-Path -LiteralPath $HarryExe)) {
    throw "harry2.exe not found in: $GameDir"
}

foreach ($Required in @($FixDll, $DxvkDll, $Config, $DxvkConf)) {
    if (-not (Test-Path -LiteralPath $Required)) {
        throw "Package file missing: $Required"
    }
}

$GameD3D9 = Join-Path $GameDir "d3d9.dll"
$GameDxvk = Join-Path $GameDir "dxvk_d3d9.dll"
$GameConfig = Join-Path $GameDir "lhp2_fix.ini"
$GameDxvkConf = Join-Path $GameDir "dxvk.conf"
$DxvkConfMarker = Join-Path $GameDir ".lhp2_fix_installed_dxvk_conf"

if (Test-Path -LiteralPath $GameD3D9) {
    $Backup = Join-Path $GameDir ("d3d9.before-lhp2-fix-{0:yyyyMMdd-HHmmss}.dll" -f (Get-Date))
    Write-Host "[*] Backing up existing d3d9.dll to:"
    Write-Host "    $Backup"
    Copy-Item -LiteralPath $GameD3D9 -Destination $Backup
}

Write-Host "[*] Installing LHP2 fix proxy as d3d9.dll"
Copy-Item -LiteralPath $FixDll -Destination $GameD3D9 -Force

Write-Host "[*] Installing DXVK as dxvk_d3d9.dll"
Copy-Item -LiteralPath $DxvkDll -Destination $GameDxvk -Force

if (-not (Test-Path -LiteralPath $GameConfig)) {
    Write-Host "[*] Installing lhp2_fix.ini"
    Copy-Item -LiteralPath $Config -Destination $GameConfig
} else {
    Write-Host "[*] Keeping existing lhp2_fix.ini"
}

if (-not (Test-Path -LiteralPath $GameDxvkConf)) {
    Write-Host "[*] Installing dxvk.conf"
    Copy-Item -LiteralPath $DxvkConf -Destination $GameDxvkConf
    New-Item -ItemType File -Path $DxvkConfMarker -Force | Out-Null
} else {
    Write-Host "[*] Keeping existing dxvk.conf"
}

Write-Host ""
Write-Host "Installed. Launch the game normally through Steam."
Write-Host ""
Write-Host "Active layout:"
Write-Host "  d3d9.dll       = LHP2 fix proxy"
Write-Host "  dxvk_d3d9.dll  = DXVK x32 d3d9.dll"
Write-Host "  lhp2_fix.ini   = frame limiter Sleep cap config"
