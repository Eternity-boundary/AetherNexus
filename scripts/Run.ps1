[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [ValidateSet('x64', 'ARM64')]
    [string]$Platform = 'x64',
    [switch]$DebugOutput
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputPath = Join-Path $repoRoot "artifacts\bin\AetherNexus\$Platform\$Configuration"
$manifestPath = Join-Path $outputPath 'AppxManifest.xml'
if (-not (Test-Path -LiteralPath $manifestPath)) {
    throw 'Build the matching configuration with scripts/Build.ps1 first.'
}
$winappCommand = Get-Command winapp.exe -ErrorAction Stop
# Use the generated manifest: it contains the resolved Windows App Runtime dependencies.
$nativeArgs = @('run', $outputPath, '--manifest', $manifestPath, '--exe', 'AetherNexus.exe')
$nativeArgs += if ($DebugOutput) { '--debug-output' } else { '--detach' }
& $winappCommand.Source @nativeArgs
$launchExitCode = $LASTEXITCODE
if ($launchExitCode -ne 0) { throw "WinApp launch failed: $launchExitCode" }
