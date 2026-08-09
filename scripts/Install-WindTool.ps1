param(
    [Parameter(Mandatory)]
    [string]$MmdDirectory,
    [string]$PackageDirectory = "$PSScriptRoot\..\dist"
)

$ErrorActionPreference = 'Stop'

$expectedExeHash = '2C9414C21619B4AD85D9C2EF76836F3C34DB7A8ABD07BD6C6176D385F7EFDFB4'
$expectedOriginalMmeHash = 'A20D77FB6C6919B7894EEADCFB852F5EA6D56E93C5A65142BC2DAE75C6F54D25'
$mmdDirectory = (Resolve-Path -LiteralPath $MmdDirectory).Path
$packageDirectory = (Resolve-Path -LiteralPath $PackageDirectory).Path

$exePath = Join-Path $mmdDirectory 'MikuMikudance.exe'
$activeMmePath = Join-Path $mmdDirectory 'MMEffect.dll'
$originalMmePath = Join-Path $mmdDirectory 'MMEffect.original.dll'
$packageForwarder = Join-Path $packageDirectory 'MMEffect.dll'
$packagePhysics = Join-Path $packageDirectory 'PhysicsControlStudio\MmdPhysicsControlStudio.dll'
$packageWindSource = Join-Path $packageDirectory 'WindTool\WindTool-WindSource.pmx'
$physicsDirectory = Join-Path $mmdDirectory 'PhysicsControlStudio'
$installedPhysics = Join-Path $physicsDirectory 'MmdPhysicsControlStudio.dll'
$windToolDirectory = Join-Path $mmdDirectory 'WindTool'
$installedWindSource = Join-Path $windToolDirectory 'WindTool-WindSource.pmx'

foreach ($path in @(
    $exePath,
    $activeMmePath,
    $packageForwarder,
    $packagePhysics,
    $packageWindSource)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required file is missing: $path"
    }
}

$runningTarget = Get-Process MikuMikudance -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -eq $exePath }
if ($runningTarget) {
    throw "Close MikuMikudance.exe process $($runningTarget.Id) before installation."
}

$exeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $exePath).Hash
if ($exeHash -ne $expectedExeHash) {
    throw "Unsupported MikuMikudance.exe SHA-256: $exeHash"
}

if (-not (Test-Path -LiteralPath $originalMmePath -PathType Leaf)) {
    $activeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $activeMmePath).Hash
    if ($activeHash -ne $expectedOriginalMmeHash) {
        throw "Refusing to replace an unknown MMEffect.dll: $activeHash"
    }
    Copy-Item -LiteralPath $activeMmePath -Destination $originalMmePath
}

$originalHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $originalMmePath).Hash
if ($originalHash -ne $expectedOriginalMmeHash) {
    throw "MMEffect.original.dll has an unexpected SHA-256: $originalHash"
}

New-Item -ItemType Directory -Path $physicsDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $windToolDirectory -Force | Out-Null
Copy-Item -LiteralPath $packagePhysics -Destination $installedPhysics -Force
Copy-Item -LiteralPath $packageWindSource -Destination $installedWindSource -Force
Copy-Item -LiteralPath $packageForwarder -Destination $activeMmePath -Force

[pscustomobject]@{
    MmdDirectory = $mmdDirectory
    HostHash = $exeHash
    ForwarderHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $activeMmePath).Hash
    WindToolHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $installedPhysics).Hash
    WindSource = $installedWindSource
    WindSourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $installedWindSource).Hash
    OriginalMmePreserved = $true
    Status = 'PASS'
}
