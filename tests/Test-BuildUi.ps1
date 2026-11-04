[CmdletBinding()]
param([Parameter(Mandatory)][int]$AppPid)
. (Join-Path $PSScriptRoot 'Support/BuildUi.ps1') -AppPid $AppPid
function Wait-Notice([string]$Text) { Ui wait-for Message --value $Text --contains -t 5000 }
function Navigate([string]$Route) { Ui invoke "OfflineNav-$Route"; Start-Sleep -Milliseconds 250 }
function Command([string]$Id) {
    $element = @(Get-UiElements | Where-Object { $_.automationId -eq $Id -and -not $_.isOffscreen })
    if ($element.Count -eq 0) { Ui invoke MoreButton }
    Ui invoke $Id
}
function Capture([string]$Name) { Ui screenshot '-o' (Join-Path $outputRoot $Name) }
Ui invoke OfflineLight
Test-Ui 'Prompt catalog starts empty and saves versioned templates' {
    Ui wait-for BuildListStatus --value '沒有可顯示' --contains -t 3000
    Command BuildCreate
    Ui set-value PromptName 'Offline prompt'
    Ui set-value PromptModel 'offline-model'
    Ui set-value PromptInstructions 'Hello {{name}}'
    Ui set-value PromptInput 'Offline request'
    Ui set-value PromptVariables '{"name":"offline"}'
    Ui invoke PromptSave
    Wait-Notice '資料已更新'
    Ui wait-for BuildListStatus --value '1 / 1' --contains -t 3000
    Invoke-Item 'Offline prompt'
    Ui set-value PromptInstructions 'Version two {{name}}'
    Ui invoke PromptSave
    Wait-Notice '資料已更新'
    Capture '01-prompts-light.png'
}
Test-Ui 'Prompt test uses explicit confirmation and fake Responses result' {
    Ui invoke PromptRun
    Ui wait-for PrimaryButton -t 3000
    Ui invoke PrimaryButton
    Wait-Notice '操作已確認'
    Ui wait-for BuildJson --value 'Offline response' --contains -t 3000
}
Test-Ui 'Assistants migration shares the local prompt catalog' {
    Navigate assistants
    Ui wait-for BuildListStatus --value '1 / 1' --contains -t 3000
    Invoke-Item 'Offline prompt'
    Ui scroll-into-view PromptInstructions
    Ui wait-for PromptInstructions --value 'Version two {{name}}' -t 3000
}
Test-Ui 'Stored Chat logs load messages' {
    Navigate logs
    Invoke-Item 'offline-model'
    Command BuildChildren
    Wait-Notice '資料已更新'
    Invoke-Item 'msg_offline'
    Ui wait-for BuildJson --value 'Offline stored question' --contains -t 3000
}
Test-Ui 'Agent session traces show native span hierarchy' {
    Navigate traces
    Invoke-Item 'sess_offline'
    Command BuildChildren
    Wait-Notice '資料已更新'
    Invoke-Item 'turn_offline'
    Ui wait-for BuildDetailSummary --value 'OTLP spans：2' --contains -t 3000
    Ui wait-for ListControl -t 3000
    Capture '02-traces-light.png'
}
Test-Ui 'Batch creation and cancellation require confirmation' {
    Navigate batches
    Command BuildCreate
    Ui set-value BuildDialogFile 'file_batch'
    Ui invoke PrimaryButton
    Wait-Notice '操作已確認'
    Command BuildRefresh
    Wait-Notice '資料已更新'
    Invoke-Item 'batch_offline'
    Command BuildRemove
    Ui set-value BuildConfirmId 'wrong_id'
    Ui invoke PrimaryButton
    Wait-Notice '輸入資料無效'
    Command BuildRemove
    Ui set-value BuildConfirmId 'batch_offline'
    Ui invoke PrimaryButton
    Wait-Notice '操作已確認'
    Ui invoke OfflineDark
    Capture '03-batches-dark.png'
}
Test-Ui 'Existing Evals run and output inspection is read only' {
    Navigate evaluations
    Invoke-Item 'Offline evaluation'
    Command BuildChildren
    Wait-Notice '資料已更新'
    Invoke-Item 'Offline eval run'
    Command BuildChildren
    Wait-Notice '資料已更新'
    Invoke-Item 'output_offline'
    Ui wait-for BuildJson --value 'grader' --contains -t 3000
}
Test-Ui 'Fine tuning events and checkpoints load through job scope' {
    Navigate fine-tuning
    Invoke-Item 'offline-model'
    Command BuildChildren
    Wait-Notice '資料已更新'
    Invoke-Item 'ftevent_offline'
    Ui wait-for BuildJson --value 'Offline training step' --contains -t 3000
    Choose BuildView 'Checkpoints'
    Command BuildRefresh
    Wait-Notice '資料已更新'
    Invoke-Item 'ftckpt_offline'
    Ui wait-for BuildJson --value 'step_number' --contains -t 3000
}
Test-Ui 'Files and vector stores support listing and creation' {
    Navigate storage
    Invoke-Item 'offline-training.jsonl'
    Ui wait-for BuildJson --value 'fine-tune' --contains -t 3000
    Choose BuildView 'Vector stores'
    Command BuildRefresh
    Wait-Notice '資料已更新'
    Command BuildCreate
    Ui set-value BuildDialogName 'Offline knowledge'
    Ui set-value BuildExpiry '3'
    Ui invoke PrimaryButton
    Wait-Notice '操作已確認'
    Command BuildRefresh
    Wait-Notice '資料已更新'
    Invoke-Item 'Offline knowledge'
    Command BuildChildren
    Wait-Notice '資料已更新'
    Invoke-Item 'file_offline'
    Ui wait-for BuildJson --value 'completed' --contains -t 3000
}
Test-Ui 'Missing model key gives settings guidance and recovers' {
    Navigate batches
    Ui invoke OfflineMissingKey
    Command BuildRefresh
    Wait-Notice '一般 API 權杖'
    Ui invoke OfflineRestoreKey
    Command BuildRefresh
    Wait-Notice '資料已更新'
}
Test-Ui 'Cancelling a pending list does not retain old rows' {
    Ui invoke OfflineSlow
    Command BuildRefresh
    Ui invoke BuildStop
    Wait-Notice '已停止等待'
    Ui invoke OfflineFast
    Command BuildRefresh
    Wait-Notice '資料已更新'
}
Test-Ui 'Narrow layout keeps list and detail accessible' {
    Navigate storage
    Ui invoke OfflineNarrow
    Start-Sleep -Milliseconds 300
    Ui wait-for BuildRecords -t 3000
    Capture '04-storage-narrow-dark.png'
    Ui invoke OfflineWide
    Ui invoke OfflineLight
}
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot 'results.json') -Encoding utf8
$failures = @($results | Where-Object status -eq FAIL)
Write-Output "BUILD UI: $($results.Count - $failures.Count) passed, $($failures.Count) failed"
if ($failures.Count -gt 0) { throw 'BUILD UI tests failed; see results.json.' }
