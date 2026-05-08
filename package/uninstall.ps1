param(
    [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\LEGO Harry Potter Years 5-7"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $GameDir)) {
    throw "Game directory not found: $GameDir"
}

$GameDir = (Resolve-Path -LiteralPath $GameDir).Path
$GameD3D9 = Join-Path $GameDir "d3d9.dll"
$GameDxvk = Join-Path $GameDir "dxvk_d3d9.dll"
$GameConfig = Join-Path $GameDir "lhp2_fix.ini"
$GameLog = Join-Path $GameDir "lhp2_fix.log"
$GameDxvkConf = Join-Path $GameDir "dxvk.conf"
$DxvkConfMarker = Join-Path $GameDir ".lhp2_fix_installed_dxvk_conf"

$Backups = Get-ChildItem -LiteralPath $GameDir -Filter "d3d9.before-lhp2-fix-*.dll" |
    Sort-Object LastWriteTime -Descending

if ($Backups.Count -gt 0) {
    Write-Host "[*] Restoring latest d3d9.dll backup:"
    Write-Host "    $($Backups[0].FullName)"
    Copy-Item -LiteralPath $Backups[0].FullName -Destination $GameD3D9 -Force
} elseif (Test-Path -LiteralPath $GameD3D9) {
    Write-Host "[*] Removing d3d9.dll"
    Remove-Item -LiteralPath $GameD3D9 -Force
}

foreach ($Path in @($GameDxvk, $GameConfig, $GameLog)) {
    if (Test-Path -LiteralPath $Path) {
        Write-Host "[*] Removing $Path"
        Remove-Item -LiteralPath $Path -Force
    }
}

if ((Test-Path -LiteralPath $DxvkConfMarker) -and (Test-Path -LiteralPath $GameDxvkConf)) {
    Write-Host "[*] Removing dxvk.conf installed by this fix"
    Remove-Item -LiteralPath $GameDxvkConf -Force
    Remove-Item -LiteralPath $DxvkConfMarker -Force
}

Write-Host ""
Write-Host "Uninstalled. Steam Verify may not remove extra local DLLs, so this script is the preferred removal path."
