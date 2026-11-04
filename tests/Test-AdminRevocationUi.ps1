[CmdletBinding()]
param([Parameter(Mandatory)][int]$AppPid)
. (Join-Path $PSScriptRoot 'Support/ManageUi.ps1') -AppPid $AppPid -OutputName 'admin-revocation-ui'
function Wait-Action([string]$Id) {
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        if (@(Get-UiElements | Where-Object { $_.automationId -eq $Id -and $_.isEnabled }).Count) { return }
        Start-Sleep -Milliseconds 80
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Action did not become enabled: $Id"
}
function Select-NewKey {
    Ui invoke ManageRefresh
    Ui wait-for 'Offline newly issued admin' -t 5000
    Invoke-Item 'Offline newly issued admin'
    Wait-Action ManageRemove
}
function Revoke-NewKey {
    Ui invoke ManageRemove
    Ui wait-for ManageConfirmId -t 5000
    Ui set-value ManageConfirmId 'key_new'
    Ui invoke PrimaryButton
}
function Assert-RevocationGuidance([bool]$Visible) {
    foreach ($id in @('ManageAdminOfficialLink', 'ManageAdminBugReportLink')) {
        $shown = @(Get-UiElements | Where-Object { $_.automationId -eq $id -and -not $_.isOffscreen }).Count -ne 0
        if ($shown -ne $Visible) { throw "Unexpected Admin 404 guidance visibility: $id" }
    }
}
function Wait-RemoteStatus([int]$Status, [string]$Method, [string]$RequestId) {
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        $names = @(Get-UiElements | ForEach-Object name | Where-Object { $_ })
        if ($names -match "HTTP $Status") {
            if ($names -match '輸入無效|private rejection text|private verification failure|do-not-display|offline-secret') { throw 'Server failure was misclassified or private body leaked.' }
            if (-not ($names -match [regex]::Escape("$Method /v1/organization/admin_api_keys/{key_id}"))) { throw 'Failed request method/template was missing.' }
            if ($RequestId) {
                if (-not ($names -match [regex]::Escape("Request ID: $RequestId"))) { throw 'Safe response Request ID was missing.' }
            } else {
                foreach ($expected in @('Request ID 標頭：未回傳。', 'Client Request ID: 550e8400-e29b-41d4-a716-446655440000', '回應格式：未提供或無法辨識', '本文：空', '可能與已回報的服務問題有關')) {
                    if (-not ($names -match [regex]::Escape($expected))) { throw "Safe response classification missing: $expected" }
                }
            }
            if ($Method -eq 'GET') {
                if (-not ($names -match '尚未送出 DELETE') -or $names -match 'DELETE /v1/') { throw 'GET verification failure was presented as a sent DELETE.' }
            } elseif ($names -match 'GET /v1/') { throw 'DELETE failure was presented as GET verification.' }
            return
        }
        Start-Sleep -Milliseconds 80
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Expected safe HTTP $Status diagnostic."
}
Test-Ui 'Suffix-only Admin keys are classified by API objects' {
    Ui invoke OfflineNav-keys
    Choose ManageView '組織金鑰目錄（全部）'
    Ui invoke ManageRefresh
    Wait-Action ManageIssue
    foreach ($name in @('Offline admin', 'Mixed project API key', 'Mixed legacy API key', 'Offline generic-mask admin')) {
        Ui wait-for $name -t 5000
    }
    $nodes = @(Get-UiElements)
    if (@($nodes | Where-Object { $_.name -and $_.name.Contains('類型待確認（僅供檢視）') }).Count) { throw 'Official Admin objects were left unclassified because of their masks.' }
    foreach ($name in @('Offline admin', 'Offline generic-mask admin')) {
        Invoke-Item $name
        Wait-Action ManageRemove
    }
    if (@($nodes | Where-Object { $_.automationId -eq 'ManageProject' -and -not $_.isOffscreen }).Count) { throw 'Project scope remained visible for the organization directory.' }
    Ui screenshot --output (Join-Path $outputRoot 'object-typed-directory.png')
}
Test-Ui 'Issue another Admin key and retrieve it from the management list' {
    Ui invoke ManageIssue
    Ui wait-for ManageIssueName -t 5000
    Ui set-value ManageIssueName 'Offline newly issued admin'
    Ui invoke PrimaryButton
    Ui wait-for ManageIssuedKeyHint --value '•••• 5678' -t 5000
    Ui invoke CloseButton
    Ui wait-for 'Offline newly issued admin' -t 5000
}
Test-Ui 'Project ID matches stay API keys regardless of the mask' {
    foreach ($name in @('Mixed project API key', 'Mixed legacy API key')) {
        Invoke-Item $name
        $nodes = @(Get-UiElements)
        if (@($nodes | Where-Object { $_.automationId -in @('ManageRemove', 'ManageEdit') -and $_.isEnabled }).Count) { throw "Project key allowed an Admin mutation: $name" }
    }
    Ui screenshot --output (Join-Path $outputRoot 'organization-directory.png')
}
Test-Ui 'Revocation requires the exact object ID' {
    Invoke-Item 'Offline newly issued admin'
    Ui invoke ManageRemove
    Ui wait-for ManageConfirmId -t 5000
    Ui set-value ManageConfirmId 'not-the-key-id'
    if (@(Get-UiElements | Where-Object { $_.automationId -eq 'PrimaryButton' -and $_.isEnabled }).Count) { throw 'Invalid confirmation accepted.' }
    Ui set-value ManageConfirmId 'key_new'
    Wait-Action PrimaryButton
    Ui invoke CloseButton
}
Test-Ui 'Admin DELETE 400 preserves status without invalid-input or private body' {
    Ui invoke OfflineRejectAdminDelete
    Select-NewKey
    Revoke-NewKey
    Wait-RemoteStatus 400 'DELETE' 'req_offline_delete_400'
    Assert-RevocationGuidance $false
    Ui screenshot --output (Join-Path $outputRoot 'http-400.png')
}
Test-Ui 'Empty Admin DELETE 404 offers Platform and the bug report without losing safe diagnostics' {
    Ui invoke OfflineMissingAdminDelete
    Select-NewKey
    Revoke-NewKey
    Wait-RemoteStatus 404 'DELETE' ''
    Ui wait-for ManageAdminOfficialLink --value '開啟官方 Admin keys' -t 3000
    Ui wait-for ManageAdminBugReportLink --value '查看撤銷 API 問題回報' -t 3000
    Assert-RevocationGuidance $true
    Ui screenshot --output (Join-Path $outputRoot 'delete-404.png')
}
Test-Ui 'Admin DELETE JSON 404 also offers the workaround regardless of headers or body' {
    Ui invoke OfflineJsonAdminDelete
    Select-NewKey
    Assert-RevocationGuidance $false
    Revoke-NewKey
    Wait-RemoteStatus 404 'DELETE' 'req_offline_delete_json_404'
    Assert-RevocationGuidance $true
    Ui screenshot --output (Join-Path $outputRoot 'delete-json-404.png')
}
Test-Ui 'Failed Admin read verification stops before revocation' {
    Ui invoke OfflineRestoreAdminDelete
    Ui invoke OfflineMissingAdminVerification
    Select-NewKey
    Revoke-NewKey
    Wait-RemoteStatus 404 'GET' 'req_offline_verify_404'
    Assert-RevocationGuidance $false
    Ui screenshot --output (Join-Path $outputRoot 'verification-404.png')
    Ui invoke OfflineRestoreAdminVerification
    Select-NewKey
    Ui wait-for 'Offline newly issued admin' -t 5000
}
Test-Ui 'API recovery after 404 revokes the selected key normally and clears both guidance links' {
    Ui invoke OfflineRestoreAdminDelete
    Select-NewKey
    Assert-RevocationGuidance $false
    Revoke-NewKey
    Ui wait-for 'Offline admin' -t 5000
    Wait-Action ManageIssue
    Assert-RevocationGuidance $false
    if (@(Get-UiElements | Where-Object { $_.name -and $_.name.Contains('Offline newly issued admin') }).Count) { throw 'Revoked key remained in the list.' }
    Ui screenshot --output (Join-Path $outputRoot 'revoked.png')
}
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot 'results.json') -Encoding utf8
if (@($results | Where-Object status -eq 'FAIL').Count) { throw 'Offline Admin revocation UI tests failed.' }
