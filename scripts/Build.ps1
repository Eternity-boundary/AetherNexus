[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [ValidateSet('x64', 'ARM64')]
    [string]$Platform = 'x64',
    [switch]$Restore,
    [switch]$Rebuild
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$projectPath = Join-Path $repoRoot 'src\AetherNexus\AetherNexus.vcxproj'
$msbuildCommand = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
$msbuildPath = if ($msbuildCommand) { $msbuildCommand.Source } else { $null }

if (-not $msbuildPath) {
    $vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswherePath) {
        $msbuildCandidates = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\MSBuild.exe'
        $vswhereExitCode = $LASTEXITCODE
        if ($vswhereExitCode -ne 0) { throw "vswhere failed: $vswhereExitCode" }
        $msbuildPath = $msbuildCandidates | Select-Object -First 1
    }
}
if (-not $msbuildPath) {
    throw 'MSBuild with the Visual C++ toolchain was not found. Install the WinUI C++ development components in Visual Studio.'
}

$buildTarget = if ($Rebuild) { 'Rebuild' } else { 'Build' }
$nativeArgs = @(
    $projectPath,
    '/nologo',
    '/m',
    "/t:$buildTarget",
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform",
    '/p:AppxPackageSigningEnabled=false'
)
if ($Restore) {
    $nativeArgs += '/restore'
    $nativeArgs += '/p:RestorePackagesConfig=true'
    $nativeArgs += "/p:RestoreRepositoryPath=$(Join-Path $repoRoot 'packages')"
}

& $msbuildPath @nativeArgs
$buildExitCode = $LASTEXITCODE
if ($buildExitCode -ne 0) { throw "MSBuild failed: $buildExitCode" }
