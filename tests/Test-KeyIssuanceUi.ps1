[CmdletBinding()]
param([Parameter(Mandatory)][int]$AppPid)
. (Join-Path $PSScriptRoot 'Support/ManageUi.ps1') -AppPid $AppPid -OutputName 'key-issuance-ui'
function Wait-Ready {
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        if (@(Get-UiElements | Where-Object { $_.automationId -eq 'ManageIssue' -and $_.isEnabled }).Count) { return }
        Start-Sleep -Milliseconds 80
    } while ([DateTime]::UtcNow -lt $deadline)
    throw 'Issue action did not become available.'
}
function Open-Issue {
    Ui invoke ManageIssue
    Ui wait-for ManageIssueName -t 5000
}
function Assert-Disclosure {
    Ui wait-for ManageIssuedKeyHint --value '•••• 5678' -t 5000
    $snapshot = Get-UiElements | ConvertTo-Json -Depth 40
    if ($snapshot.Contains('offline-issued-never-valid')) { throw 'Plaintext secret leaked into UI Automation.' }
}
Test-Ui 'Cancel issuance without changing keys' {
    Ui invoke OfflineNav-keys
    Choose ManageProject 'proj_offline'
    Ui invoke ManageRefresh
    Wait-Ready
    Open-Issue
    Ui invoke CloseButton
    Wait-Ready
}
Test-Ui 'New service account token and one-time masked disclosure' {
    Open-Issue
    Ui set-value ManageIssueName 'Offline service account'
    Ui screenshot --output (Join-Path $outputRoot 'service-account-form.png')
    Ui invoke PrimaryButton
    Assert-Disclosure
    Ui wait-for svc_new -t 5000
    Ui screenshot --output (Join-Path $outputRoot 'service-account-disclosure.png')
    Ui invoke CloseButton
    Wait-Ready
    if (@(Get-UiElements | Where-Object automationId -eq 'ManageIssuedKeyHint').Count) { throw 'Disclosure remained after close.' }
}
Test-Ui 'Existing service account token' {
    Open-Issue
    Ui set-value ManageIssueName 'Offline existing account key'
    Choose ManageIssueOwner '為既有服務帳戶簽發權杖'
    Ui set-value ManageIssueAccountId 'svc_existing'
    Ui invoke PrimaryButton
    Assert-Disclosure
    Ui wait-for svc_existing -t 5000
    Ui invoke CloseButton
    Wait-Ready
}
Test-Ui 'Admin token issuance and disclosure close' {
    Choose ManageView '組織管理金鑰目錄'
    Ui invoke ManageRefresh
    Wait-Ready
    Open-Issue
    Ui set-value ManageIssueName 'Offline admin key'
    Ui invoke PrimaryButton
    Assert-Disclosure
    Ui screenshot --output (Join-Path $outputRoot 'admin-disclosure.png')
    Ui invoke CloseButton
    Wait-Ready
}
# Never read or replace an existing user's clipboard while testing.
Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class IssuanceClipboardTest {
    [DllImport("user32.dll")] public static extern int CountClipboardFormats();
}
'@
if ([IssuanceClipboardTest]::CountClipboardFormats() -eq 0) {
    Test-Ui 'Explicit copy excludes history and is cleared on disclosure close' {
        Open-Issue
        Ui set-value ManageIssueName 'Offline copy test'
        Ui invoke PrimaryButton
        Assert-Disclosure
        Ui invoke PrimaryButton
        Ui wait-for ManageIssuedKeyStatus --value '已複製' --contains -t 5000
        if ([IssuanceClipboardTest]::CountClipboardFormats() -lt 4) { throw 'Clipboard opt-out formats were not set.' }
        Ui invoke CloseButton
        Wait-Ready
        if ([IssuanceClipboardTest]::CountClipboardFormats() -ne 0) { throw 'Owned clipboard was not cleared.' }
    }
} else {
    $results.Add(@{name='Clipboard copy';status='SKIP';detail='Existing user clipboard preserved.'})
    Write-Output 'SKIP: Clipboard copy (existing user clipboard preserved).'
}
Test-Ui 'Window close while disclosure is open' {
    Open-Issue
    Ui set-value ManageIssueName 'Offline window close'
    Ui invoke PrimaryButton
    Assert-Disclosure
    Ui invoke Close
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    while ((Get-Process -Id $AppPid -ErrorAction SilentlyContinue) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (Get-Process -Id $AppPid -ErrorAction SilentlyContinue) { throw 'Offline host did not close.' }
}
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot 'results.json') -Encoding utf8
if (@($results | Where-Object status -eq 'FAIL').Count) { throw 'Offline issuance UI tests failed.' }
