param(
    [string]$Version = '',
    [string]$Configuration = 'Release',
    [string]$BuildDirectory = "$PSScriptRoot\..\build",
    [string]$DistDirectory = "$PSScriptRoot\..\dist",
    [string]$ReleaseDirectory = "$PSScriptRoot\..\release",
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'

$projectDirectory = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ([string]::IsNullOrWhiteSpace($Version)) {
    $cmakeText = Get-Content -Raw -LiteralPath (Join-Path $projectDirectory 'CMakeLists.txt')
    $versionMatch = [regex]::Match(
        $cmakeText,
        'project\(WindTool\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)')
    if (-not $versionMatch.Success) {
        throw 'WindTool version was not found in CMakeLists.txt.'
    }
    $Version = $versionMatch.Groups[1].Value
}

if ($Version -notmatch '^[0-9]+\.[0-9]+\.[0-9]+$') {
    throw "Invalid release version: $Version"
}

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'Build-WindTool.ps1') `
        -BuildDirectory $BuildDirectory `
        -Configuration $Configuration `
        -PackageDirectory $DistDirectory
    if ($LASTEXITCODE -ne 0) { throw 'WindTool build failed.' }
}

$distPath = [System.IO.Path]::GetFullPath($DistDirectory)
if (-not (Test-Path -LiteralPath $distPath -PathType Container)) {
    throw "Distribution directory was not found: $distPath"
}

$releaseRoot = [System.IO.Path]::GetFullPath($ReleaseDirectory)
New-Item -ItemType Directory -Path $releaseRoot -Force | Out-Null
$packageName = "WindTool-$Version-win64"
$packagePath = [System.IO.Path]::GetFullPath((Join-Path $releaseRoot $packageName))
$releasePrefix = $releaseRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if (-not $packagePath.StartsWith(
        $releasePrefix,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Package path escaped the release directory: $packagePath"
}

$zipPath = Join-Path $releaseRoot "$packageName.zip"
foreach ($generatedPath in @($packagePath, $zipPath)) {
    if (Test-Path -LiteralPath $generatedPath) {
        Remove-Item -LiteralPath $generatedPath -Recurse -Force
    }
}

New-Item -ItemType Directory -Path (Join-Path $packagePath 'dist') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $packagePath 'scripts') -Force | Out-Null
Copy-Item -Path (Join-Path $distPath '*') -Destination (Join-Path $packagePath 'dist') -Recurse -Force

foreach ($name in @('README.md', 'LICENSE', 'AUTHORS.md', 'SECURITY.md')) {
    Copy-Item -LiteralPath (Join-Path $projectDirectory $name) -Destination $packagePath
}
foreach ($name in @('Install-WindTool.ps1', 'Uninstall-WindTool.ps1')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $packagePath 'scripts')
}

$installNotes = Join-Path $releaseRoot "INSTALL_v$Version.md"
$releaseNotes = Join-Path $releaseRoot "RELEASE_NOTES_v$Version.md"
if (-not (Test-Path -LiteralPath $installNotes -PathType Leaf) -or
    -not (Test-Path -LiteralPath $releaseNotes -PathType Leaf)) {
    throw "Release documentation is incomplete for v$Version."
}
Copy-Item -LiteralPath $installNotes -Destination (Join-Path $packagePath 'INSTALL.md')
Copy-Item -LiteralPath $releaseNotes -Destination (Join-Path $packagePath 'RELEASE_NOTES.md')

$packageManifest = Join-Path $packagePath 'SHA256SUMS.txt'
$packageEntries = Get-ChildItem -Recurse -File -LiteralPath $packagePath |
    Where-Object { $_.FullName -ne $packageManifest } |
    Sort-Object FullName |
    ForEach-Object {
        $relative = [System.IO.Path]::GetRelativePath($packagePath, $_.FullName).Replace('\', '/')
        '{0}  {1}' -f (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant(), $relative
    }
Set-Content -LiteralPath $packageManifest -Value $packageEntries -Encoding utf8NoBOM

Compress-Archive -LiteralPath $packagePath -DestinationPath $zipPath -CompressionLevel Optimal

$releaseManifest = Join-Path $releaseRoot "SHA256SUMS_v$Version.txt"
$releaseFiles = @(
    $zipPath,
    (Join-Path $packagePath 'dist\MMEffect.dll'),
    (Join-Path $packagePath 'dist\PhysicsControlStudio\MmdPhysicsControlStudio.dll'),
    (Join-Path $packagePath 'dist\WindTool\WindTool-WindSource.pmx')
)
$releaseEntries = foreach ($file in $releaseFiles) {
    $relative = [System.IO.Path]::GetRelativePath($releaseRoot, $file).Replace('\', '/')
    '{0}  {1}' -f (Get-FileHash -Algorithm SHA256 -LiteralPath $file).Hash.ToLowerInvariant(), $relative
}
Set-Content -LiteralPath $releaseManifest -Value $releaseEntries -Encoding utf8NoBOM
Copy-Item -LiteralPath $releaseManifest -Destination (Join-Path $releaseRoot 'SHA256SUMS.txt') -Force

[pscustomobject]@{
    Version = $Version
    Package = $packagePath
    Zip = $zipPath
    ZipSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $zipPath).Hash
    DllSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $releaseFiles[2]).Hash
    WindSourceSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $releaseFiles[3]).Hash
    Author = '克里斯提亚娜'
    License = 'MIT'
}
