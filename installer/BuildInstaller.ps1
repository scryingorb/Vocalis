param([string]$IsccPath = '', [string]$BuildDirectory = 'build', [string]$OutputDirectory = 'dist', [string]$CTestPath = '')
$ErrorActionPreference = 'Stop'
try {
    $root = Split-Path $PSScriptRoot -Parent
    $buildRoot = if ([IO.Path]::IsPathRooted($BuildDirectory)) { [IO.Path]::GetFullPath($BuildDirectory) } else { [IO.Path]::GetFullPath((Join-Path $root $BuildDirectory)) }
    $outputRoot = [IO.Path]::GetFullPath((Join-Path $root $OutputDirectory))
    if (-not $outputRoot.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Installer output must be inside the project.' }
    $cmakeText = Get-Content -LiteralPath (Join-Path $root 'CMakeLists.txt') -Raw
    if ($cmakeText -notmatch 'project\(Vocalis VERSION ([0-9]+\.[0-9]+\.[0-9]+)') { throw 'Missing product version.' }
    $version = $Matches[1]
    $outputName = "Vocalis-$version-Setup.exe"
    $output = Join-Path $outputRoot $outputName
    if (Test-Path -LiteralPath $output) { throw 'Installer exists; choose a new output directory rather than overwrite it.' }
    $bundle = Join-Path $buildRoot 'Vocalis_artefacts/Release/VST3/Vocalis.vst3'
    foreach ($relative in @('Contents/x86_64-win/Vocalis.vst3','Contents/Resources/moduleinfo.json','Contents/Resources/THIRD_PARTY_NOTICES.md','Contents/Resources/Licenses')) {
        if (-not (Test-Path -LiteralPath (Join-Path $bundle $relative))) { throw "Incomplete Release bundle: $relative" }
    }
    $module = Join-Path $bundle 'Contents/x86_64-win/Vocalis.vst3'
    if ((Get-Item $module).VersionInfo.ProductVersion -ne $version) { throw 'Binary version does not match CMake.' }
    foreach ($relative in @('README.md','THIRD_PARTY_NOTICES.md','LICENSES','docs/ReleaseChecklist.md','docs/ReleaseCandidate.md')) {
        if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) { throw "Missing documentation: $relative" }
    }
    if (-not (Test-Path (Join-Path $root 'LICENSE.txt'))) {
        throw 'Required owner-provided LICENSE.txt is missing.'
    }
    if (-not $IsccPath) {
        $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
        if ($command) { $IsccPath = $command.Source }
    }
    if (-not $IsccPath) {
        $candidates = @()
        foreach ($base in @(${env:ProgramFiles(x86)}, $env:ProgramFiles, $env:LOCALAPPDATA)) {
            if ($base) { foreach ($innoVersion in @('6','7')) { $candidates += Join-Path $base "Inno Setup $innoVersion/ISCC.exe"; $candidates += Join-Path $base "Programs/Inno Setup $innoVersion/ISCC.exe" } }
        }
        $IsccPath = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    }
    if (-not $IsccPath -or -not (Test-Path -LiteralPath $IsccPath)) {
        throw 'Inno Setup compiler not found. Install Inno Setup 6.3 or newer manually from https://jrsoftware.org/isdl.php, then rerun this script (or pass -IsccPath C:\path\ISCC.exe). Nothing was downloaded.'
    }
    . (Join-Path $PSScriptRoot 'ReleaseValidation.ps1')
    $validation = Test-VocalisRelease $root $buildRoot $CTestPath
    New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
    $started = [DateTime]::UtcNow
    & $IsccPath "/DProductVersion=$version" "/DBundleSource=$bundle" "/O$outputRoot" (Join-Path $PSScriptRoot 'Vocalis.iss')
    if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE" }
    if (-not (Test-Path $output) -or (Get-Item $output).Length -eq 0 -or (Get-Item $output).LastWriteTimeUtc -lt $started) { throw 'Fresh installer EXE was not created.' }
    # Inno pads the PE product-version string with spaces.
    if ((Get-Item -LiteralPath $output).VersionInfo.ProductVersion.Trim() -ne $validation.Version) { throw 'Installer version differs from the validated plugin.' }
    Assert-VocalisBundleUnchanged $bundle $validation.Hashes
    $sums = Join-Path $outputRoot 'SHA256SUMS.txt'
    $names = @($outputName)
    $lines = @()
    if (Test-Path $sums) { $lines = @(Get-Content $sums | Where-Object { ($_ -split '\s+',2)[1] -notin $names }) }
    foreach ($name in $names) {
        $file = Join-Path $outputRoot $name
        if (Test-Path $file) { $lines += '{0}  {1}' -f (Get-FileHash $file -Algorithm SHA256).Hash, $name }
    }
    $lines | Set-Content -LiteralPath $sums -Encoding ASCII
    Write-Output "Installer: $output"
    Get-FileHash $output -Algorithm SHA256
    exit 0
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}

