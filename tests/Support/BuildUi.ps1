[CmdletBinding()]
param([Parameter(Mandatory)][int]$AppPid,[string]$OutputName='build-ui')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$outputRoot = Join-Path $repoRoot ('artifacts\tests\' + $OutputName)
[void](New-Item -ItemType Directory -Path $outputRoot -Force)
$results = [System.Collections.Generic.List[object]]::new()
$windows = & winapp.exe ui list-windows -a "$AppPid" --json | ConvertFrom-Json
$mainWindow = @($windows | Where-Object title -eq 'AetherNexus OFFLINE Build Tests')
if ($mainWindow.Count -ne 1) { throw 'Expected exactly one offline Build window.' }
$windowHandle = "$($mainWindow[0].hwnd)"
function Ui {
    param([Parameter(ValueFromRemainingArguments)][string[]]$Command)
    if ($Command[0] -eq 'screenshot') { Start-Sleep -Milliseconds 400 }
    $nativeArgs = @('ui') + $Command + @('-w', $windowHandle)
    if ($Command -notcontains '--json') { $nativeArgs += '--quiet' }
    $output = & winapp.exe @nativeArgs 2>&1
    $commandExitCode = $LASTEXITCODE
    if ($commandExitCode -ne 0) { throw ($output -join [Environment]::NewLine) }
    return $output
}
function Test-Ui {
    param([string]$Name,[scriptblock]$Work)
    try { & $Work | Out-Null; $results.Add(@{name=$Name;status='PASS'}); Write-Output "PASS: $Name" }
    catch { $results.Add(@{name=$Name;status='FAIL';detail="$($_.Exception.Message)"}); Write-Output "FAIL: $Name - $($_.Exception.Message)" }
}
function Choose {
    param([string]$Picker,[string]$Value)
    # The CLI auto-selects the main window for -a, hiding WinUI's owned ComboBox popup.
    Ui click $Picker
    $deadline = [DateTime]::UtcNow.AddSeconds(3)
    do {
        $popups = @(& winapp.exe ui list-windows -a "$AppPid" --json | ConvertFrom-Json | Where-Object { $_.ownerHwnd -eq [long]$windowHandle -and $_.label -eq 'popup' })
        if ($popups.Count -eq 1) { break }
        Start-Sleep -Milliseconds 80
    } while ([DateTime]::UtcNow -lt $deadline)
    if ($popups.Count -ne 1) { throw 'Expected one owned ComboBox popup.' }
    Invoke-Item $Value "$($popups[0].hwnd)"
}
function Invoke-Item {
    param([string]$Name,[string]$TargetWindow=$windowHandle)
    function Test-ItemText($element) {
        if ($element.name -and $element.name.Contains($Name)) { return $true }
        foreach ($child in $element.children) { if (Test-ItemText $child) { return $true } }
        return $false
    }
    # WinUI's typed ListView containers may have no Name; the visible labels are descendants.
    $items = @(Get-UiElements $TargetWindow | Where-Object { $_.type -eq 'ListItem' -and -not $_.isOffscreen -and (Test-ItemText $_) } | Sort-Object selector -Unique)
    if ($items.Count -ne 1) { throw "Expected one list item for '$Name', found $($items.Count)." }
    # Runtime-generated slugs vary with inspection depth. Use the observed accessible
    # name for ComboBox items; anonymous typed ListView rows expose their title as text.
    if ($items[0].automationId -like 'BuildRow-*') {
        $output = & winapp.exe ui invoke $items[0].automationId -w $TargetWindow 2>&1
    } elseif ($items[0].name) {
        $output = & winapp.exe ui invoke $items[0].name -w $TargetWindow 2>&1
    } else {
        $labels = @(Get-UiElements $TargetWindow | Where-Object { $_.type -eq 'Text' -and $_.name -eq $Name -and -not $_.isOffscreen })
        if ($labels.Count -ne 1) { throw "Expected one visible label for '$Name', found $($labels.Count)." }
        $output = & winapp.exe ui click $Name -w $TargetWindow 2>&1
    }
    if ($LASTEXITCODE -ne 0) { throw ($output -join [Environment]::NewLine) }
}
function Get-UiElements {
    param([string]$TargetWindow=$windowHandle)
    $json = (& winapp.exe ui inspect -w $TargetWindow --depth 24 --json) -join [Environment]::NewLine
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect list items.' }
    $jsonStart = $json.IndexOf('{')
    if ($jsonStart -lt 0) { throw 'UIA snapshot was not JSON.' }
    $snapshot = $json.Substring($jsonStart) | ConvertFrom-Json
    function Expand-Elements($elements) {
        foreach ($element in $elements) { $element; if ($element.children) { Expand-Elements $element.children } }
    }
    Expand-Elements $snapshot.windows.elements
}

