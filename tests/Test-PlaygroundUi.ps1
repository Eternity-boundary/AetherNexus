[CmdletBinding()]
param([Parameter(Mandatory)][int]$AppPid)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputRoot = Join-Path $repoRoot 'artifacts\tests\playground-ui'
[void](New-Item -ItemType Directory -Path $outputRoot -Force)
$results = [System.Collections.Generic.List[object]]::new()
function Invoke-Ui {
    param([Parameter(ValueFromRemainingArguments)][string[]]$Command)
    if ($Command[0] -eq 'screenshot') { Start-Sleep -Milliseconds 350 }
    $nativeArgs = @('ui') + $Command + @('-a', "$AppPid")
    $output = & winapp.exe @nativeArgs 2>&1
    if ($LASTEXITCODE -ne 0) { throw ($output -join [Environment]::NewLine) }
}
function Test-Ui {
    param([string]$Name, [scriptblock]$Action)
    try { & $Action; $results.Add(@{name=$Name;status='PASS'}); Write-Output "PASS: $Name" }
    catch { $results.Add(@{name=$Name;status='FAIL';detail="$($_.Exception.Message)"}); Write-Output "FAIL: $Name - $($_.Exception.Message)" }
}
Test-Ui 'Chat streaming and message editing' {
    Invoke-Ui invoke OfflineNav-chat
    Invoke-Ui wait-for PlaygroundInput -t 5000
    Invoke-Ui set-value PlaygroundInput 'Offline synthetic prompt'
    Invoke-Ui invoke PlaygroundRun
    Invoke-Ui wait-for MessageText1 --value 'Offline UI response' -t 5000
    Invoke-Ui screenshot --output (Join-Path $outputRoot 'chat.png')
}
Test-Ui 'Images generation and native preview' {
    Invoke-Ui invoke OfflineNav-images
    Invoke-Ui wait-for PlaygroundInput -t 5000
    Invoke-Ui set-value PlaygroundInput 'Offline image prompt'
    Invoke-Ui invoke PlaygroundRun
    Invoke-Ui wait-for PlaygroundAsset --value '1.png' -t 5000
    Invoke-Ui screenshot --output (Join-Path $outputRoot 'images.png')
}
Test-Ui 'Realtime voice event assembly and native playback' {
    Invoke-Ui invoke OfflineNav-audio
    Invoke-Ui wait-for PlaygroundInput -t 5000
    Invoke-Ui set-value PlaygroundInput 'Offline speech prompt'
    Invoke-Ui invoke PlaygroundRun
    Invoke-Ui wait-for PlaygroundOutput --value 'Offline voice' -t 5000
    Invoke-Ui wait-for PlaygroundAsset --value '1.wav' -t 5000
    Invoke-Ui screenshot --output (Join-Path $outputRoot 'audio.png')
}
Test-Ui 'Agent workflow preview and trace' {
    Invoke-Ui invoke OfflineNav-agent-builder
    Invoke-Ui wait-for WorkflowNodeagent -t 5000
    Invoke-Ui set-value PlaygroundInput 'Offline workflow input'
    Invoke-Ui invoke PlaygroundRun
    Invoke-Ui wait-for PlaygroundOutput --value 'Offline UI response' -t 5000
    Invoke-Ui invoke 工作區
    Invoke-Ui screenshot --output (Join-Path $outputRoot 'workflow.png')
}
Test-Ui 'Video local storyboard without retired API calls' {
    Invoke-Ui invoke OfflineNav-video
    Invoke-Ui wait-for PlaygroundStoryboard -t 5000
    Invoke-Ui set-value PlaygroundInput 'Offline local storyboard shot'
    Invoke-Ui invoke PlaygroundAddShot
    Invoke-Ui wait-for PlaygroundStoryboard --value 'Offline local storyboard shot' --contains -t 5000
    Invoke-Ui screenshot --output (Join-Path $outputRoot 'video.png')
}
Test-Ui 'Navigation preserves in-memory chat' {
    Invoke-Ui invoke OfflineNav-chat
    Invoke-Ui wait-for MessageText1 --value 'Offline UI response' -t 5000
}
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot 'results.json') -Encoding utf8
if (@($results | Where-Object status -eq 'FAIL').Count) { throw 'Offline Playground UI tests failed.' }
