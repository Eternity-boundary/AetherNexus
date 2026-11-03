[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$msbuild = Get-Command MSBuild.exe -ErrorAction Stop
$nativeArgs = @((Join-Path $PSScriptRoot 'Management.Tests.vcxproj'), '/nologo', '/v:minimal', '/p:Configuration=Debug', '/p:Platform=x64')
& $msbuild.Source @nativeArgs
$buildExitCode = $LASTEXITCODE
if ($buildExitCode -ne 0) { throw "Management test build failed: $buildExitCode" }
& (Join-Path $repoRoot 'artifacts\tests\management\Management.Tests.exe')
$testExitCode = $LASTEXITCODE
if ($testExitCode -ne 0) { throw "Management tests failed: $testExitCode" }
