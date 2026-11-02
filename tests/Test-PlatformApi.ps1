[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$msbuildCommand = Get-Command MSBuild.exe -ErrorAction Stop
$nativeArgs = @((Join-Path $repoRoot 'tests\PlatformApi.Tests.vcxproj'), '/nologo', '/v:minimal', '/p:Configuration=Debug', '/p:Platform=x64')
& $msbuildCommand.Source @nativeArgs
$buildExitCode = $LASTEXITCODE
if ($buildExitCode -ne 0) { throw "API test build failed: $buildExitCode" }
& (Join-Path $repoRoot 'artifacts\tests\api\PlatformApi.Tests.exe')
$testExitCode = $LASTEXITCODE
if ($testExitCode -ne 0) { throw "API virtual tests failed: $testExitCode" }
