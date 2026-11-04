[CmdletBinding()]
param([Parameter(Mandatory)][int]$AppPid,[ValidateSet('Build','Playground')][string]$Area='Build')
$support = Join-Path $PSScriptRoot 'Support/BuildUi.ps1'
if ($Area -eq 'Build') {
    . $support -AppPid $AppPid -OutputName 'migration-build-ui'
} else {
    . $support -AppPid $AppPid -OutputName 'migration-playground-ui' -WindowTitle 'AetherNexus OFFLINE Playground Tests'
}
$routes = if ($Area -eq 'Build') { @('prompts','assistants','evaluations','fine-tuning') } else { @('agent-builder','audio','video','images','chat') }
$button = if ($Area -eq 'Build') { 'BuildMigration' } else { 'PlaygroundMigration' }
foreach ($route in $routes) {
    Test-Ui "$route migration guide opens and closes" {
        Ui invoke "OfflineNav-$route"
        Start-Sleep -Milliseconds 200
        Ui invoke $button
        Ui wait-for CloseButton -t 3000
        $text = @(Get-UiElements | Where-Object { $_.type -eq 'Text' } | ForEach-Object name) -join "`n"
        if ($text -notmatch '1\. ' -or $text -notmatch '3\. ' -or $text -notmatch '2026-11-05') { throw 'Guide steps or source date missing' }
        if ($route -in @('evaluations','agent-builder','audio')) { Ui screenshot '-o' (Join-Path $outputRoot "$route-guide.png") }
        Ui invoke CloseButton
    }
}
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot 'results.json') -Encoding utf8
if (@($results | Where-Object status -eq FAIL).Count) { throw 'Migration UI tests failed' }
