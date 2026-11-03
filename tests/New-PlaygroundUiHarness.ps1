[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$harnessRoot = Join-Path $PSScriptRoot 'PlaygroundHarness'
$sourceRoot = Join-Path $repoRoot 'src\AetherNexus'
[xml]$project = Get-Content -LiteralPath (Join-Path $sourceRoot 'AetherNexus.vcxproj') -Raw
$ns = [System.Xml.XmlNamespaceManager]::new($project.NameTable)
$ns.AddNamespace('m', $project.DocumentElement.NamespaceURI)
$includeTypes = @('ClInclude', 'ClCompile', 'Page', 'ApplicationDefinition', 'Midl', 'PRIResource', 'Image', 'Manifest', 'AppxManifest', 'None', 'Text')
foreach ($node in @($project.SelectNodes('//m:ItemGroup/*[@Include]', $ns))) {
    if ($node.LocalName -notin $includeTypes) { continue }
    $relative = $node.GetAttribute('Include')
    if ($relative.StartsWith('$(')) { continue }
    if ($relative -match 'Views\\(MainWindow|DashboardPage|SettingsPage|PlaceholderPage)|CredentialStore|WindowsHttpTransport|WindowsGenerationTransport|BudgetService|ThemeService|DashboardViewModel|UsageSnapshot|Views\\Charts') { [void]$node.ParentNode.RemoveChild($node); continue }
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
foreach ($node in $project.SelectNodes('//m:ProjectGuid', $ns)) { $node.InnerText = '{416DF63B-4324-4808-B7DA-89B14BEE7D27}' }
$group = $project.CreateElement('ItemDefinitionGroup', $project.DocumentElement.NamespaceURI)
$compile = $project.CreateElement('ClCompile', $project.DocumentElement.NamespaceURI)
$dirs = $project.CreateElement('AdditionalIncludeDirectories', $project.DocumentElement.NamespaceURI)
$dirs.InnerText = $sourceRoot + ';' + (Join-Path $sourceRoot 'Views') + ';%(AdditionalIncludeDirectories)'
[void]$compile.AppendChild($dirs); [void]$group.AppendChild($compile)
[void]$project.DocumentElement.InsertBefore($group, $project.SelectSingleNode('//m:Import[@Project="$(VCTargetsPath)\Microsoft.Cpp.targets"]', $ns))
$project.Save((Join-Path $harnessRoot 'Playground.UI.Tests.vcxproj'))
[xml]$manifest = Get-Content -LiteralPath (Join-Path $sourceRoot 'Package.appxmanifest') -Raw
$manifest.Package.Identity.Name = 'AetherNexus.Playground.OfflineTests'
$manifest.Save((Join-Path $harnessRoot 'Package.appxmanifest'))
Write-Output 'Generated isolated UI harness: no credential store or live transport is compiled.'
