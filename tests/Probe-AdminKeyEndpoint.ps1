[CmdletBinding()]
param([switch]$AllowNetwork)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$msbuild = Get-Command MSBuild.exe -ErrorAction Stop
$nativeArgs = @((Join-Path $PSScriptRoot 'AdminKeyEndpoint.Probe.vcxproj'), '/nologo', '/v:minimal', '/p:Configuration=Debug', '/p:Platform=x64')
& $msbuild.Source @nativeArgs
$buildExit = $LASTEXITCODE
if ($buildExit -ne 0) { throw "Synthetic endpoint probe build failed: $buildExit" }
$probeArgs = @()
if ($AllowNetwork) { $probeArgs += '--run-synthetic-network-probe' }
& (Join-Path $repoRoot 'artifacts/tests/admin-endpoint-probe/AdminKeyEndpoint.Probe.exe') @probeArgs
$probeExit = $LASTEXITCODE
if ($probeExit -ne 0) { throw "Synthetic endpoint probe failed: $probeExit" }
