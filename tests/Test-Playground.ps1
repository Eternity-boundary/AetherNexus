[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$msbuildCommand = Get-Command MSBuild.exe -ErrorAction Stop
$nativeArgs = @((Join-Path $repoRoot 'tests\Playground.Tests.vcxproj'), '/nologo', '/v:minimal', '/p:Configuration=Debug', '/p:Platform=x64')
& $msbuildCommand.Source @nativeArgs
$buildExitCode = $LASTEXITCODE
if ($buildExitCode -ne 0) { throw "Playground test build failed: $buildExitCode" }
Push-Location -LiteralPath $repoRoot
try {
    & (Join-Path $repoRoot 'artifacts\tests\playground\Playground.Tests.exe')
    $testExitCode = $LASTEXITCODE
    if ($testExitCode -ne 0) { throw "Playground virtual tests failed: $testExitCode" }
} finally { Pop-Location }
