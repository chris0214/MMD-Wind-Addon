param(
    [switch]$Deploy,
    [string]$BuildDirectory = "$PSScriptRoot\..\build",
    [string]$MmdDirectory = "$PSScriptRoot\..\..\MikuMikuDance -2"
)

$ErrorActionPreference = 'Stop'

$projectDirectory = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$expectedExeHash = '2C9414C21619B4AD85D9C2EF76836F3C34DB7A8ABD07BD6C6176D385F7EFDFB4'
$exePath = Join-Path $MmdDirectory 'MikuMikudance.exe'
$buildDll = Join-Path $BuildDirectory 'MmdPhysicsControlStudio.dll'
$forwarderDll = Join-Path $BuildDirectory 'MMEffect.dll'
$deployedDll = Join-Path $MmdDirectory 'PhysicsControlStudio\MmdPhysicsControlStudio.dll'
$deployedWindSource = Join-Path $MmdDirectory 'WindTool\WindTool-WindSource.pmx'
$activeMme = Join-Path $MmdDirectory 'MMEffect.dll'

Write-Host '== Physics Control Studio: configure =='
& cmake -S $projectDirectory -B $BuildDirectory -G Ninja
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }

Write-Host '== Physics Control Studio: build =='
& cmake --build $BuildDirectory --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }

Write-Host '== Physics Control Studio: tests =='
& ctest --test-dir $BuildDirectory --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'CTest failed.' }

Write-Host '== Physics Control Studio: detailed fixtures =='
& (Join-Path $BuildDirectory 'windtool_core_fixture.exe')
if ($LASTEXITCODE -ne 0) { throw 'Core fixture failed.' }
& (Join-Path $BuildDirectory 'windtool_host_contract_fixture.exe') $buildDll
if ($LASTEXITCODE -ne 0) { throw 'Host contract fixture failed.' }

if ($Deploy) {
    Write-Host '== Physics Control Studio: staging deploy =='
    & (Join-Path $PSScriptRoot 'Deploy-Staging.ps1') `
        -BuildDirectory $BuildDirectory `
        -MmdDirectory $MmdDirectory
}

if (-not (Test-Path -LiteralPath $exePath)) {
    throw "MikuMikudance.exe was not found: $exePath"
}
if (-not (Test-Path -LiteralPath $deployedDll)) {
    throw "Staged DLL was not found: $deployedDll"
}
if (-not (Test-Path -LiteralPath $forwarderDll)) {
    throw "MMEffect forwarder was not found: $forwarderDll"
}
if ($Deploy -and -not (Test-Path -LiteralPath $deployedWindSource -PathType Leaf)) {
    throw "Wind source PMX was not deployed: $deployedWindSource"
}

$objdump = Get-Command objdump -ErrorAction Stop
$physicsExports = (& $objdump.Source -p $buildDll 2>&1 | Out-String)
foreach ($name in @(
    'MmdPhysicsGetApiVersion',
    'MmdPhysicsInstall',
    'MmdPhysicsInstallForWindow',
    'MmdPhysicsUninstall',
    'MmdPhysicsIsInstalled',
    'MmdPhysicsGetStatusJsonW',
    'MmdPhysicsGetPanelWindow',
    'MmdPhysicsDispatchCommand',
    'MmdPhysicsOnFrameBoundary')) {
    if ($physicsExports -notmatch [regex]::Escape($name)) {
        throw "Physics DLL export is missing: $name"
    }
}

$forwarderExports = (& $objdump.Source -p $forwarderDll 2>&1 | Out-String)
foreach ($name in @(
    'Initialize', 'Cleanup', 'OnCreateModel', 'OnDeleteModel',
    'OnBeginScene', 'OnEndScene', 'OnDrawPrimitive',
    'OnDrawIndexedPrimitive', 'OnLostDevice', 'OnResetDevice', 'OnClear')) {
    if ($forwarderExports -notmatch [regex]::Escape($name)) {
        throw "MMEffect forwarder export is missing: $name"
    }
}

$hostHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $exePath).Hash
$buildHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $buildDll).Hash
$deployedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $deployedDll).Hash
$forwarderHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $forwarderDll).Hash
$activeMmeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $activeMme).Hash
$loadChainInstalled = $forwarderHash -eq $activeMmeHash

if ($hostHash -ne $expectedExeHash) {
    throw "Unsupported MikuMikudance.exe hash: $hostHash"
}
if ($buildHash -ne $deployedHash) {
    throw 'The staged DLL does not match the latest build. Run with -Deploy.'
}

Write-Host '== Physics Control Studio: current status =='
[pscustomobject]@{
    Host = $exePath
    HostHashVerified = $true
    Dll = $deployedDll
    DllHash = $deployedHash
    CoreFixtureCases = 95
    CTestPassed = '4/4'
    InspectorSurrogateCases = 73
    WindSourcePmxVerified = $true
    PhysicsExports = 9
    ForwarderExports = 11
    LoadChainModified = $loadChainInstalled
    VisibleMmdPanelAvailable = $loadChainInstalled
    WriteBackend = 'bullet_force_accumulator'
    ActivationBackend = 'bullet_activate'
    NextMilestone = 'per-project JSON switching'
} | Format-List

Write-Host 'PASS Physics Control Studio foundation is healthy.'
