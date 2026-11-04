[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$productionPath = Join-Path $repoRoot 'src/AetherNexus/Services/WindowsManagementTransport.cpp'
$testPath = Join-Path $PSScriptRoot 'WindowsManagementTransport.Tests.cpp'
$productionSource = Get-Content -LiteralPath $productionPath -Raw
$testSource = Get-Content -LiteralPath $testPath -Raw
$calledApis = @([regex]::Matches($productionSource, '\b(WinHttp[A-Za-z0-9_]+)\s*\(') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
$mappedApis = @([regex]::Matches($testSource, '(?m)^#define\s+(WinHttp[A-Za-z0-9_]+)\s+') | ForEach-Object { $_.Groups[1].Value })
$unmapped = @($calledApis | Where-Object { $_ -notin $mappedApis })
if ($unmapped.Count) { throw ('Refusing to build: unmapped WinHTTP APIs: ' + ($unmapped -join ', ')) }
$msbuild = Get-Command MSBuild.exe -ErrorAction Stop
$nativeArgs = @((Join-Path $PSScriptRoot 'WindowsManagementTransport.Tests.vcxproj'), '/nologo', '/v:minimal', '/p:Configuration=Debug', '/p:Platform=x64')
& $msbuild.Source @nativeArgs
$buildExit = $LASTEXITCODE
if ($buildExit -ne 0) { throw "Offline native transport test build failed: $buildExit" }
& (Join-Path $repoRoot 'artifacts/tests/management-transport/WindowsManagementTransport.Tests.exe')
$testExit = $LASTEXITCODE
if ($testExit -ne 0) { throw "Offline native transport tests failed: $testExit" }
