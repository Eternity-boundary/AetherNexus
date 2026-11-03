[CmdletBinding()]
param([Parameter(Mandatory)][int]$AppPid)
. (Join-Path $PSScriptRoot 'Support/ManageUi.ps1') -AppPid $AppPid
Test-Ui 'Usage UTC range and totals' {
    Ui invoke OfflineNav-usage
    Choose ManageView 'Completions 用量'
    Ui invoke ManageRefresh
    Ui wait-for ManageMetricTotal --value '840' -t 5000
    Ui screenshot --output (Join-Path $outputRoot 'usage.png')
}
Test-Ui 'Costs category applies matching unit' {
    Choose ManageView '所有成本（USD）'
    Ui invoke ManageRefresh
    Ui wait-for ManageMetricTotal --value '1.75 USD' -t 5000
}
Test-Ui 'Projects list and native editor' {
    Ui invoke OfflineNav-projects
    Choose ManageView '專案總覽'
    Ui invoke ManageRefresh
    Ui wait-for 'proj_offline' -t 5000
    Ui invoke ManageCreate
    Ui wait-for ManageEditName -t 5000
    Ui set-value ManageEditName 'Offline created project'
    Ui invoke PrimaryButton
    Ui wait-for 'Offline created project' -t 5000
    Ui screenshot --output (Join-Path $outputRoot 'projects.png')
}
Test-Ui 'Project keys and policy editor' {
    Ui invoke OfflineNav-keys
    Ui wait-for ManageProject -t 5000
    Choose ManageProject 'Offline created project · proj_offline'
    Ui invoke ManageRefresh
    Ui wait-for 'Offline key' -t 5000
    Invoke-Item 'Offline key'
    Ui invoke ManageEdit
    Ui wait-for ManageEditPermissions -t 5000
    Ui wait-for ManageEditExpiry -t 5000
    Ui invoke CloseButton
    Ui screenshot --output (Join-Path $outputRoot 'keys.png')
}
Test-Ui 'Project members and model rate limits' {
    Ui invoke OfflineNav-projects
    Choose ManageProject 'Offline created project · proj_offline'
    Choose ManageView '專案成員'
    Ui invoke ManageRefresh
    Ui wait-for 'Offline member' -t 5000
    Choose ManageView '模型速率限制'
    Ui invoke ManageRefresh
    Ui wait-for 'offline-model' -t 5000
    Invoke-Item 'offline-model'
    Ui invoke ManageEdit
    Ui wait-for ManageLimit_max_requests_per_1_minute -t 5000
    Ui invoke CloseButton
}
Test-Ui 'Theme and narrow layout' {
    Ui invoke OfflineLight
    Ui screenshot --output (Join-Path $outputRoot 'light.png')
    Ui invoke OfflineNarrow
    Ui wait-for ManageRefresh -t 5000
    Ui screenshot --output (Join-Path $outputRoot 'narrow.png')
    Ui invoke OfflineWide
    Ui invoke OfflineDark
}
Test-Ui 'Missing Admin Key guidance without reading credentials' {
    Ui invoke OfflineNav-usage
    Ui invoke OfflineMissingKey
    Ui invoke ManageRefresh
    Ui wait-for ManageSettings -t 5000
    Ui screenshot --output (Join-Path $outputRoot 'missing-key.png')
    Ui invoke OfflineRestoreKey
    Ui invoke ManageRefresh
}
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot 'results.json') -Encoding utf8
if (@($results | Where-Object status -eq 'FAIL').Count) { throw 'Offline Manage UI tests failed.' }

