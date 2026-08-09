param(
    [string]$BuildDirectory = "$PSScriptRoot\..\build",
    [string]$Configuration = 'Release',
    [string]$PackageDirectory = "$PSScriptRoot\..\dist",
    [ValidateSet('Auto', 'Ninja', 'Visual Studio 17 2022')]
    [string]$Generator = 'Auto'
)

$ErrorActionPreference = 'Stop'

$projectDirectory = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

$selectedGenerator = $Generator
if ($selectedGenerator -eq 'Auto') {
    $hasNinja = $null -ne (Get-Command ninja -ErrorAction SilentlyContinue)
    $hasGxx = $null -ne (Get-Command g++ -ErrorAction SilentlyContinue)
    $selectedGenerator = if ($hasNinja -and $hasGxx) {
        'Ninja'
    } else {
        'Visual Studio 17 2022'
    }
}

$configureArguments = @('-S', $projectDirectory, '-B', $BuildDirectory)
if ($selectedGenerator -eq 'Ninja') {
    $configureArguments += @('-G', 'Ninja', "-DCMAKE_BUILD_TYPE=$Configuration")
} else {
    $configureArguments += @('-G', $selectedGenerator, '-A', 'x64')
}

& cmake @configureArguments
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }

& cmake --build $BuildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }

$windSourceGenerator = if ($selectedGenerator -eq 'Ninja') {
    Join-Path $BuildDirectory 'generate_wind_source_pmx.exe'
} else {
    Join-Path $BuildDirectory "$Configuration\generate_wind_source_pmx.exe"
}
if (-not (Test-Path -LiteralPath $windSourceGenerator -PathType Leaf)) {
    throw "Wind source generator was not found: $windSourceGenerator"
}
& $windSourceGenerator (Join-Path $projectDirectory 'assets\WindTool-WindSource.pmx')
if ($LASTEXITCODE -ne 0) { throw 'Wind source PMX generation failed.' }

& ctest --test-dir $BuildDirectory -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }

& cmake --install $BuildDirectory --config $Configuration --prefix $PackageDirectory
if ($LASTEXITCODE -ne 0) { throw 'Package staging failed.' }

[pscustomobject]@{
    Build = (Resolve-Path $BuildDirectory).Path
    Package = (Resolve-Path $PackageDirectory).Path
    Configuration = $Configuration
    Generator = $selectedGenerator
    Status = 'PASS'
}
