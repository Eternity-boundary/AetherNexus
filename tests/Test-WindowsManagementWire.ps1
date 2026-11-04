[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$msbuild = Get-Command MSBuild.exe -ErrorAction Stop
$nativeArgs = @((Join-Path $PSScriptRoot 'WindowsManagementWire.Tests.vcxproj'), '/nologo', '/v:minimal', '/p:Configuration=Debug', '/p:Platform=x64')
& $msbuild.Source @nativeArgs
$buildExit = $LASTEXITCODE
if ($buildExit -ne 0) { throw "Loopback wire test build failed: $buildExit" }
& (Join-Path $repoRoot 'artifacts/tests/management-wire/WindowsManagementWire.Tests.exe')
$testExit = $LASTEXITCODE
if ($testExit -ne 0) { throw "Loopback wire tests failed: $testExit" }
