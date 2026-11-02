[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$msbuildCommand = Get-Command MSBuild.exe -ErrorAction Stop
$nativeArgs = @((Join-Path $repoRoot 'tests\SettingsStorage.Tests.vcxproj'), '/nologo', '/v:minimal', '/p:Configuration=Debug', '/p:Platform=x64')
& $msbuildCommand.Source @nativeArgs
$buildExitCode = $LASTEXITCODE
if ($buildExitCode -ne 0) { throw "Test build failed: $buildExitCode" }
# This is an unpackaged native console test, not the WinUI application executable.
& (Join-Path $repoRoot 'artifacts\tests\x64\SettingsStorage.Tests.exe')
$testExitCode = $LASTEXITCODE
if ($testExitCode -ne 0) { throw "Storage tests failed: $testExitCode" }
