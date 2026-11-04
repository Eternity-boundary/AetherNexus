[CmdletBinding()]
param([Parameter(Mandatory)][int]$AppPid)
. (Join-Path $PSScriptRoot 'Support/BuildUi.ps1') -AppPid $AppPid -OutputName 'build-inspection-ui'
function Updated { Ui wait-for Message --value '資料已更新' --contains -t 5000 }
Test-Ui 'Saving a new Prompt revision selects the committed version' {
    Ui invoke OfflineLight
    Ui invoke OfflineNav-prompts
    Ui invoke BuildCreate
    Ui set-value PromptName 'Version selection check'
    Ui set-value PromptModel 'offline-model'
    Ui set-value PromptInput 'revision one'
    Ui invoke PromptSave
    Updated
    Ui wait-for PromptVersion --value '1' -t 3000
    Ui set-value PromptInput 'revision two'
    Ui invoke PromptSave
    Updated
    Ui wait-for PromptVersion --value '2' -t 3000
    Ui screenshot '-o' (Join-Path $outputRoot '00-prompt-version-light.png')
}
Test-Ui 'Trace hierarchy has a bounded, visible native TreeView' {
    Ui invoke OfflineLight
    Ui invoke OfflineNav-traces
    Choose BuildView 'Agent sessions'
    Ui invoke BuildRefresh
    Updated
    Invoke-Item 'sess_offline'
    Ui invoke BuildChildren
    Updated
    Invoke-Item 'turn_offline'
    Ui wait-for BuildDetailSummary --value 'OTLP spans：2' --contains -t 3000
    # WinUI TreeView's UIA peer is its internal TreeViewList, named ListControl.
    Ui wait-for ListControl -t 3000
    Ui screenshot '-o' (Join-Path $outputRoot '01-traces-light.png')
}
Test-Ui 'Delayed scope events preserve valid Eval run selection' {
    Ui invoke OfflineNav-evaluations
    Choose BuildView '既有評估'
    Ui invoke BuildRefresh
    Updated
    Invoke-Item 'Offline evaluation'
    Ui invoke BuildChildren
    Updated
    Invoke-Item 'Offline eval run'
    Start-Sleep -Milliseconds 350
    Ui invoke BuildChildren
    Updated
    Invoke-Item 'output_offline'
    Ui wait-for BuildJson --value 'grader' --contains -t 3000
    Ui screenshot '-o' (Join-Path $outputRoot '02-eval-output-light.png')
}
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot 'results.json') -Encoding utf8
$failures = @($results | Where-Object status -eq FAIL)
if ($failures.Count -gt 0) { throw 'BUILD inspection checks failed.' }
