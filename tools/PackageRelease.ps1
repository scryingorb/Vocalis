param([string]$Destination = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$cmakeText = Get-Content -LiteralPath (Join-Path $projectRoot 'CMakeLists.txt') -Raw
if ($cmakeText -notmatch 'project\(Vocalis VERSION ([0-9]+\.[0-9]+\.[0-9]+)') { throw 'Missing product version.' }
$version = $Matches[1]
if (-not $Destination) { $Destination = "dist/Vocalis-$version-Windows-x64" }
$target = [IO.Path]::GetFullPath((Join-Path $projectRoot $Destination))
if (-not $target.StartsWith($projectRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Package destination must be inside the project.' }
if ((Test-Path -LiteralPath $target) -or (Test-Path -LiteralPath ($target + '.zip'))) { throw 'Destination exists; do not overwrite a release.' }
$bundle = Join-Path $projectRoot 'build/Vocalis_artefacts/Release/VST3/Vocalis.vst3'
$module = Join-Path $bundle 'Contents/x86_64-win/Vocalis.vst3'
if (-not (Test-Path -LiteralPath $module)) { throw 'Build Release VST3 first.' }
if ((Get-Item -LiteralPath $module).VersionInfo.ProductVersion -ne $version) { throw 'Binary version does not match CMake.' }
$testLog = Join-Path $projectRoot 'docs/measurements/phase13/CTest.txt'
if (-not (Test-Path $testLog)) { throw 'Run the release regression suite first.' }
if ((Get-Content $testLog -Raw) -notmatch '100% tests passed, 0 tests failed') { throw 'Regression tests have not passed.' }
if ((Get-Item $testLog).LastWriteTimeUtc -lt (Get-Item $module).LastWriteTimeUtc) { throw 'Test evidence predates the module.' }
New-Item -ItemType Directory -Path $target | Out-Null
Copy-Item -LiteralPath $bundle -Destination $target -Recurse
foreach ($name in @('README.md','THIRD_PARTY_NOTICES.md','LICENSE.txt','LICENSES')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $name) -Destination $target -Recurse
}
$packageDocs = Join-Path $target 'docs'
New-Item -ItemType Directory -Path $packageDocs | Out-Null
foreach ($name in @('ReleaseChecklist.md','ReleaseCandidate.md')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot "docs/$name") -Destination $packageDocs
}
$lines = Get-ChildItem -LiteralPath $target -File -Recurse | Sort-Object FullName | ForEach-Object {
    '{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash, $_.FullName.Substring($target.Length+1).Replace('\','/')
}
$lines | Set-Content -LiteralPath (Join-Path $target 'SHA256SUMS.txt') -Encoding ASCII
Compress-Archive -LiteralPath $target -DestinationPath ($target + '.zip')
$zipHash = (Get-FileHash -LiteralPath ($target + '.zip') -Algorithm SHA256).Hash
$binaryHash = (Get-FileHash -LiteralPath $module -Algorithm SHA256).Hash
@("$zipHash  $([IO.Path]::GetFileName($target)).zip", "$binaryHash  $([IO.Path]::GetFileName($target))/Vocalis.vst3/Contents/x86_64-win/Vocalis.vst3") | Set-Content -LiteralPath (Join-Path (Split-Path $target -Parent) 'SHA256SUMS.txt') -Encoding ASCII
Write-Output "ZIP SHA-256: $zipHash"
Write-Output "Module SHA-256: $binaryHash"

