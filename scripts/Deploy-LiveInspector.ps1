param(
    [switch]$Uninstall,
    [string]$BuildDirectory = "$PSScriptRoot\..\build",
    [string]$MmdDirectory = "$PSScriptRoot\..\..\MikuMikuDance -2"
)

$ErrorActionPreference = 'Stop'

$expectedExeHash = '2C9414C21619B4AD85D9C2EF76836F3C34DB7A8ABD07BD6C6176D385F7EFDFB4'
$expectedOriginalMmeHash = 'A20D77FB6C6919B7894EEADCFB852F5EA6D56E93C5A65142BC2DAE75C6F54D25'
$exePath = Join-Path $MmdDirectory 'MikuMikudance.exe'
$activeMmePath = Join-Path $MmdDirectory 'MMEffect.dll'
$originalMmePath = Join-Path $MmdDirectory 'MMEffect.original.dll'
$buildForwarder = Join-Path $BuildDirectory 'MMEffect.dll'
$buildPhysicsDll = Join-Path $BuildDirectory 'MmdPhysicsControlStudio.dll'
$windSourceAsset = Join-Path $PSScriptRoot '..\assets\WindTool-WindSource.pmx'
$physicsDirectory = Join-Path $MmdDirectory 'PhysicsControlStudio'
$deployedPhysicsDll = Join-Path $physicsDirectory 'MmdPhysicsControlStudio.dll'
$windToolDirectory = Join-Path $MmdDirectory 'WindTool'
$deployedWindSource = Join-Path $windToolDirectory 'WindTool-WindSource.pmx'

foreach ($path in @($exePath, $activeMmePath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required MMD file is missing: $path"
    }
}

$runningTarget = Get-Process MikuMikudance -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -eq $exePath }
if ($runningTarget) {
    throw "The target MMD copy is running. Close process $($runningTarget.Id) before deployment."
}

$exeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $exePath).Hash
if ($exeHash -ne $expectedExeHash) {
    throw "Unsupported MikuMikudance.exe hash: $exeHash"
}

if ($Uninstall) {
    if (-not (Test-Path -LiteralPath $originalMmePath -PathType Leaf)) {
        throw "MMEffect.original.dll is unavailable: $originalMmePath"
    }
    $originalHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $originalMmePath).Hash
    if ($originalHash -ne $expectedOriginalMmeHash) {
        throw "MMEffect.original.dll has an unexpected hash: $originalHash"
    }
    Copy-Item -LiteralPath $originalMmePath -Destination $activeMmePath -Force
    [pscustomobject]@{
        Action = 'uninstall'
        ActiveMMEffectHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $activeMmePath).Hash
        OriginalPreserved = $true
        PhysicsDllPreservedForInspection = (Test-Path -LiteralPath $deployedPhysicsDll)
        Status = 'PASS'
    }
    exit 0
}

foreach ($path in @($buildForwarder, $buildPhysicsDll, $windSourceAsset)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Build artifact is missing: $path"
    }
}

if (-not (Test-Path -LiteralPath $originalMmePath -PathType Leaf)) {
    $activeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $activeMmePath).Hash
    if ($activeHash -ne $expectedOriginalMmeHash) {
        throw "Refusing to preserve an unknown MMEffect.dll: $activeHash"
    }
    Copy-Item -LiteralPath $activeMmePath -Destination $originalMmePath
}

$preservedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $originalMmePath).Hash
if ($preservedHash -ne $expectedOriginalMmeHash) {
    throw "Preserved MMEffect.original.dll has an unexpected hash: $preservedHash"
}

New-Item -ItemType Directory -Force -Path $physicsDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $windToolDirectory | Out-Null
Copy-Item -LiteralPath $buildPhysicsDll -Destination $deployedPhysicsDll -Force
Copy-Item -LiteralPath $windSourceAsset -Destination $deployedWindSource -Force
Copy-Item -LiteralPath $buildForwarder -Destination $activeMmePath -Force

$forwarderHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $activeMmePath).Hash
$physicsHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $deployedPhysicsDll).Hash
if ($forwarderHash -ne (Get-FileHash -Algorithm SHA256 -LiteralPath $buildForwarder).Hash) {
    throw 'Deployed MMEffect forwarder does not match the build artifact.'
}
if ($physicsHash -ne (Get-FileHash -Algorithm SHA256 -LiteralPath $buildPhysicsDll).Hash) {
    throw 'Deployed Physics Control Studio DLL does not match the build artifact.'
}

[pscustomobject]@{
    Action = 'install'
    HostHash = $exeHash
    OriginalMMEffectHash = $preservedHash
    ForwarderHash = $forwarderHash
    PhysicsDllHash = $physicsHash
    WindSource = $deployedWindSource
    WindSourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $deployedWindSource).Hash
    Menu = 'WindTool'
    WriteBackend = 'bullet_force_accumulator'
    ActivationBackend = 'bullet_activate'
    Status = 'PASS'
}
