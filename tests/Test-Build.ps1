[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$msbuild = Get-Command MSBuild.exe -ErrorAction Stop
$nativeArgs = @((Join-Path $PSScriptRoot 'Build.Tests.vcxproj'), '/nologo', '/v:minimal', '/p:Configuration=Debug', '/p:Platform=x64')
& $msbuild.Source @nativeArgs
$buildExitCode = $LASTEXITCODE
if ($buildExitCode -ne 0) { throw "Build test build failed: $buildExitCode" }
& (Join-Path $repoRoot 'artifacts\tests\build\Build.Tests.exe')
$testExitCode = $LASTEXITCODE
if ($testExitCode -ne 0) { throw "Build tests failed: $testExitCode" }
