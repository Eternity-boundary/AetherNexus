[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$harnessRoot = Join-Path $PSScriptRoot 'ManageHarness'
$sourceRoot = Join-Path $repoRoot 'src\AetherNexus'
[xml]$project = Get-Content -LiteralPath (Join-Path $sourceRoot 'AetherNexus.vcxproj') -Raw
$ns = [System.Xml.XmlNamespaceManager]::new($project.NameTable)
$ns.AddNamespace('m', $project.DocumentElement.NamespaceURI)
$includeTypes = @('ClInclude', 'ClCompile', 'Page', 'ApplicationDefinition', 'Midl', 'PRIResource', 'Image', 'Manifest', 'AppxManifest', 'None', 'Text')
foreach ($node in @($project.SelectNodes('//m:ItemGroup/*[@Include]', $ns))) {
    if ($node.LocalName -notin $includeTypes) { continue }
    $relative = $node.GetAttribute('Include')
    if ($relative.StartsWith('$(')) { continue }
    if ($relative -match 'Views\\(MainWindow|DashboardPage|SettingsPage|PlaceholderPage|PlaygroundPage)|GenerationClient|WorkflowService|WindowsBuildTransport|BuildClient|PromptStore|Views\\BuildPage|WindowsGenerationTransport|CredentialStore|WindowsHttpTransport|WindowsManagementTransport|PlatformApiClient|BudgetService|ThemeService|DashboardViewModel|Views\\Charts') { [void]$node.ParentNode.RemoveChild($node); continue }
    if ($node.LocalName -eq 'ClCompile' -and $relative -eq 'App.xaml.cpp') { $node.SetAttribute('Include', 'App.xaml.cpp'); continue }
    if ($node.LocalName -eq 'AppxManifest') { $node.SetAttribute('Include', 'Package.appxmanifest'); continue }
    $absolute = [System.IO.Path]::GetFullPath((Join-Path $sourceRoot $relative))
    $node.SetAttribute('Include', $absolute)
    if ($node.LocalName -in @('Page', 'ApplicationDefinition', 'PRIResource', 'Image')) {
        $link = $project.CreateElement('Link', $project.DocumentElement.NamespaceURI)
        $link.InnerText = $relative
        [void]$node.AppendChild($link)
    }
}
foreach ($node in $project.SelectNodes('//m:ProjectGuid', $ns)) { $node.InnerText = '{6B8B0632-779B-419F-84B0-25219AB8AF70}' }
$group = $project.CreateElement('ItemDefinitionGroup', $project.DocumentElement.NamespaceURI)
$compile = $project.CreateElement('ClCompile', $project.DocumentElement.NamespaceURI)
$dirs = $project.CreateElement('AdditionalIncludeDirectories', $project.DocumentElement.NamespaceURI)
$dirs.InnerText = $sourceRoot + ';' + (Join-Path $sourceRoot 'Views') + ';' + (Join-Path $sourceRoot 'Models') + ';%(AdditionalIncludeDirectories)'
[void]$compile.AppendChild($dirs); [void]$group.AppendChild($compile)
[void]$project.DocumentElement.InsertBefore($group, $project.SelectSingleNode('//m:Import[@Project="$(VCTargetsPath)\Microsoft.Cpp.targets"]', $ns))
$project.Save((Join-Path $harnessRoot 'Manage.UI.Tests.vcxproj'))
[xml]$manifest = Get-Content -LiteralPath (Join-Path $sourceRoot 'Package.appxmanifest') -Raw
$manifest.Package.Identity.Name = 'AetherNexus.Manage.OfflineTests'
$manifest.Save((Join-Path $harnessRoot 'Package.appxmanifest'))
Write-Output 'Generated isolated UI harness: no credential store or live transport is compiled.'
