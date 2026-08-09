param(
    [string]$BuildDirectory = "$PSScriptRoot\..\build",
    [string]$MmdDirectory = "$PSScriptRoot\..\..\MikuMikuDance -2"
)

$ErrorActionPreference = 'Stop'

$expectedExeHash = '2C9414C21619B4AD85D9C2EF76836F3C34DB7A8ABD07BD6C6176D385F7EFDFB4'
$exePath = Join-Path $MmdDirectory 'MikuMikudance.exe'
$sourceDll = Join-Path $BuildDirectory 'MmdPhysicsControlStudio.dll'
$sourceWindSource = Join-Path $PSScriptRoot '..\assets\WindTool-WindSource.pmx'
$stagingDirectory = Join-Path $MmdDirectory 'PhysicsControlStudio'
$targetDll = Join-Path $stagingDirectory 'MmdPhysicsControlStudio.dll'
$windToolDirectory = Join-Path $MmdDirectory 'WindTool'
$targetWindSource = Join-Path $windToolDirectory 'WindTool-WindSource.pmx'

if (-not (Test-Path -LiteralPath $exePath)) {
    throw "MikuMikudance.exe was not found: $exePath"
}
if (-not (Test-Path -LiteralPath $sourceDll)) {
    throw "Build artifact was not found: $sourceDll"
}
if (-not (Test-Path -LiteralPath $sourceWindSource -PathType Leaf)) {
    throw "Wind source asset was not found: $sourceWindSource"
}

$actualExeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $exePath).Hash
if ($actualExeHash -ne $expectedExeHash) {
    throw "Unsupported MikuMikudance.exe hash: $actualExeHash"
}

New-Item -ItemType Directory -Force -Path $stagingDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $windToolDirectory | Out-Null
Copy-Item -LiteralPath $sourceDll -Destination $targetDll -Force
Copy-Item -LiteralPath $sourceWindSource -Destination $targetWindSource -Force

$deployedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $targetDll).Hash
[pscustomobject]@{
    Target = $targetDll
    Sha256 = $deployedHash
    HostHash = $actualExeHash
    WindSource = $targetWindSource
    WindSourceSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $targetWindSource).Hash
    LoadChainModified = $false
    WriteBackend = 'bullet_force_accumulator'
    ActivationBackend = 'bullet_activate'
}
