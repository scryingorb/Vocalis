# Shared by ZIP staging and installer creation. No historical log is accepted.
function Get-VocalisBundleHashes([string]$Bundle) {
    @(Get-ChildItem -LiteralPath $Bundle -File -Recurse | Sort-Object FullName | ForEach-Object {
        '{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash, $_.FullName.Substring($Bundle.Length+1).Replace('\','/')
    })
}
function Assert-VocalisBundleUnchanged([string]$Bundle, [string[]]$Expected) {
    if (Compare-Object $Expected @(Get-VocalisBundleHashes $Bundle)) {
        throw 'Bundle changed after validation; rebuild and rerun packaging.'
    }
}
function Test-VocalisRelease([string]$Root, [string]$BuildRoot, [string]$CTestPath) {
    $cmakeText = Get-Content -LiteralPath (Join-Path $Root 'CMakeLists.txt') -Raw
    if ($cmakeText -notmatch 'project\(Vocalis VERSION ([0-9]+\.[0-9]+\.[0-9]+)') { throw 'Missing product version.' }
    $version = $Matches[1]
    $bundle = Join-Path $BuildRoot 'Vocalis_artefacts/Release/VST3/Vocalis.vst3'
    foreach ($relative in @('Contents/x86_64-win/Vocalis.vst3','Contents/Resources/moduleinfo.json','Contents/Resources/THIRD_PARTY_NOTICES.md','Contents/Resources/Licenses')) {
        if (-not (Test-Path -LiteralPath (Join-Path $bundle $relative))) { throw "Incomplete Release bundle: $relative" }
    }
    $module = Join-Path $bundle 'Contents/x86_64-win/Vocalis.vst3'
    if ((Get-Item -LiteralPath $module).VersionInfo.ProductVersion -ne $version) { throw 'Binary version does not match CMake.' }
    if (-not $CTestPath) {
        $command = Get-Command ctest.exe -CommandType Application -ErrorAction SilentlyContinue
        if ($command) { $CTestPath = $command.Source }
    }
    if (-not $CTestPath) {
        $bundled = Get-ChildItem -Path (Join-Path $Root 'tools/cmake-*-windows-x86_64/bin/ctest.exe') -File -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
        if ($bundled) { $CTestPath = $bundled.FullName }
    }
    if (-not $CTestPath -or -not (Test-Path -LiteralPath $CTestPath -PathType Leaf)) { throw 'CTest not found; pass -CTestPath or put CTest on PATH.' }
    $hashes = @(Get-VocalisBundleHashes $bundle)
    & $CTestPath --test-dir $BuildRoot -C Release --output-on-failure --no-tests=error -R '^VocalisReleaseRegression$' | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Release regression failed with exit code $LASTEXITCODE; no package created." }
    Assert-VocalisBundleUnchanged $bundle $hashes
    [pscustomobject]@{ Version=$version; Bundle=$bundle; Hashes=$hashes; ModuleHash=(Get-FileHash -LiteralPath $module -Algorithm SHA256).Hash }
}
