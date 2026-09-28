param([string]$IsccPath = '')
$ErrorActionPreference = 'Stop'
try {
    $root = Split-Path $PSScriptRoot -Parent
    $bundle = Join-Path $root 'build/Vocalis_artefacts/Release/VST3/Vocalis.vst3'
    foreach ($relative in @('Contents/x86_64-win/Vocalis.vst3','Contents/Resources/moduleinfo.json','Contents/Resources/THIRD_PARTY_NOTICES.md','Contents/Resources/Licenses')) {
        if (-not (Test-Path -LiteralPath (Join-Path $bundle $relative))) { throw "Incomplete Release bundle: $relative" }
    }
    $module = Join-Path $bundle 'Contents/x86_64-win/Vocalis.vst3'
    if ((Get-Item $module).VersionInfo.ProductVersion -ne '1.0.0') { throw 'Expected Vocalis 1.0.0 module.' }
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
            if ($base) { foreach ($version in @('6','7')) { $candidates += Join-Path $base "Inno Setup $version/ISCC.exe"; $candidates += Join-Path $base "Programs/Inno Setup $version/ISCC.exe" } }
        }
        $IsccPath = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    }
    if (-not $IsccPath -or -not (Test-Path -LiteralPath $IsccPath)) {
        throw 'Inno Setup compiler not found. Install Inno Setup 6.3 or newer manually from https://jrsoftware.org/isdl.php, then rerun this script (or pass -IsccPath C:\path\ISCC.exe). Nothing was downloaded.'
    }
    $before = Get-ChildItem -LiteralPath $bundle -Recurse -File | Get-FileHash
    $started = [DateTime]::UtcNow
    & $IsccPath (Join-Path $PSScriptRoot 'Vocalis.iss')
    if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE" }
    $output = Join-Path $root 'dist/Vocalis-1.0.0-Setup.exe'
    if (-not (Test-Path $output) -or (Get-Item $output).Length -eq 0 -or (Get-Item $output).LastWriteTimeUtc -lt $started) { throw 'Fresh installer EXE was not created.' }
    foreach ($entry in $before) { if ((Get-FileHash -LiteralPath $entry.Path).Hash -ne $entry.Hash) { throw 'Source bundle changed during compilation.' } }
    $sums = Join-Path $root 'dist/SHA256SUMS.txt'
    $names = @('Vocalis-1.0.0-Windows-x64.zip','Vocalis-1.0.0-Setup.exe')
    $lines = @()
    if (Test-Path $sums) { $lines = @(Get-Content $sums | Where-Object { ($_ -split '\s+',2)[1] -notin $names }) }
    foreach ($name in $names) {
        $file = Join-Path $root "dist/$name"
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

