param(
    [Parameter(Mandatory)]
    [string]$MmdDirectory
)

$ErrorActionPreference = 'Stop'

$expectedExeHash = '2C9414C21619B4AD85D9C2EF76836F3C34DB7A8ABD07BD6C6176D385F7EFDFB4'
$expectedOriginalMmeHash = 'A20D77FB6C6919B7894EEADCFB852F5EA6D56E93C5A65142BC2DAE75C6F54D25'
$mmdDirectory = (Resolve-Path -LiteralPath $MmdDirectory).Path
$exePath = Join-Path $mmdDirectory 'MikuMikudance.exe'
$activeMmePath = Join-Path $mmdDirectory 'MMEffect.dll'
$originalMmePath = Join-Path $mmdDirectory 'MMEffect.original.dll'

foreach ($path in @($exePath, $originalMmePath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required file is missing: $path"
    }
}

$runningTarget = Get-Process MikuMikudance -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -eq $exePath }
if ($runningTarget) {
    throw "Close MikuMikudance.exe process $($runningTarget.Id) before uninstalling."
}

$exeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $exePath).Hash
if ($exeHash -ne $expectedExeHash) {
    throw "Unsupported MikuMikudance.exe SHA-256: $exeHash"
}

$originalHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $originalMmePath).Hash
if ($originalHash -ne $expectedOriginalMmeHash) {
    throw "MMEffect.original.dll has an unexpected SHA-256: $originalHash"
}

Copy-Item -LiteralPath $originalMmePath -Destination $activeMmePath -Force

[pscustomobject]@{
    MmdDirectory = $mmdDirectory
    RestoredMmeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $activeMmePath).Hash
    WindToolFilesPreserved = $true
    Status = 'PASS'
}
