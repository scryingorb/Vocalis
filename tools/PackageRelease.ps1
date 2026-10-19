param(
    [string]$Destination = '',
    [string]$BuildDirectory = 'build',
    [string]$CTestPath = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$cmakeText = Get-Content -LiteralPath (Join-Path $projectRoot 'CMakeLists.txt') -Raw
if ($cmakeText -notmatch 'project\(Vocalis VERSION ([0-9]+\.[0-9]+\.[0-9]+)') { throw 'Missing product version.' }
$version = $Matches[1]
if (-not $Destination) { $Destination = "dist/Vocalis-$version-Windows-x64" }
$target = [IO.Path]::GetFullPath((Join-Path $projectRoot $Destination))
if (-not $target.StartsWith($projectRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Package destination must be inside the project.' }
if ((Test-Path -LiteralPath $target) -or (Test-Path -LiteralPath ($target + '.zip'))) { throw 'Destination exists; do not overwrite a release.' }
$buildRoot = if ([IO.Path]::IsPathRooted($BuildDirectory)) { [IO.Path]::GetFullPath($BuildDirectory) } else { [IO.Path]::GetFullPath((Join-Path $projectRoot $BuildDirectory)) }
$bundle = Join-Path $buildRoot 'Vocalis_artefacts/Release/VST3/Vocalis.vst3'
$module = Join-Path $bundle 'Contents/x86_64-win/Vocalis.vst3'
if (-not (Test-Path -LiteralPath $module)) { throw 'Build Release VST3 first.' }
if ((Get-Item -LiteralPath $module).VersionInfo.ProductVersion -ne $version) { throw 'Binary version does not match CMake.' }
. (Join-Path $projectRoot 'installer/ReleaseValidation.ps1')
$validation = Test-VocalisRelease $projectRoot $buildRoot $CTestPath
$testedModuleHash = $validation.ModuleHash
New-Item -ItemType Directory -Path $target | Out-Null
Copy-Item -LiteralPath $bundle -Destination $target -Recurse
Assert-VocalisBundleUnchanged (Join-Path $target 'Vocalis.vst3') $validation.Hashes
$binaryHash = (Get-FileHash -LiteralPath (Join-Path $target 'Vocalis.vst3/Contents/x86_64-win/Vocalis.vst3') -Algorithm SHA256).Hash
if ($binaryHash -ne $testedModuleHash) { throw 'Packaged module differs from the tested module.' }
foreach ($name in @('README.md','THIRD_PARTY_NOTICES.md','LICENSE.txt','LICENSES')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $name) -Destination $target -Recurse
}
$packageDocs = Join-Path $target 'docs'
New-Item -ItemType Directory -Path $packageDocs | Out-Null
foreach ($name in @('ReleaseChecklist.md','ReleaseCandidate.md','CurrentAudit.md')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot "docs/$name") -Destination $packageDocs
}
$lines = Get-ChildItem -LiteralPath $target -File -Recurse | Sort-Object FullName | ForEach-Object {
    '{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash, $_.FullName.Substring($target.Length+1).Replace('\','/')
}
$lines | Set-Content -LiteralPath (Join-Path $target 'SHA256SUMS.txt') -Encoding ASCII
Compress-Archive -LiteralPath $target -DestinationPath ($target + '.zip')
$zipHash = (Get-FileHash -LiteralPath ($target + '.zip') -Algorithm SHA256).Hash
$zipName = "$([IO.Path]::GetFileName($target)).zip"
$moduleName = "$([IO.Path]::GetFileName($target))/Vocalis.vst3/Contents/x86_64-win/Vocalis.vst3"
$sums = Join-Path (Split-Path $target -Parent) 'SHA256SUMS.txt'
$checksumLines = @()
if (Test-Path -LiteralPath $sums) {
    $checksumLines = @(Get-Content -LiteralPath $sums | Where-Object { ($_ -split '\s+',2)[1] -notin @($zipName,$moduleName) })
}
$checksumLines += @("$zipHash  $zipName", "$binaryHash  $moduleName")
$checksumLines | Set-Content -LiteralPath $sums -Encoding ASCII
Write-Output "ZIP SHA-256: $zipHash"
Write-Output "Module SHA-256: $binaryHash"

