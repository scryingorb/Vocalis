param([string]$BuildDirectory = 'build/audit')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'installer/ReleaseValidation.ps1')
$scratch = Join-Path $root ('build/release-validation-tests/' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch -Force | Out-Null
$fixture = Join-Path $scratch 'bundle'
New-Item -ItemType Directory -Path $fixture | Out-Null
Set-Content -LiteralPath (Join-Path $fixture 'resource.txt') -Value 'original' -Encoding ASCII
$expected = @(Get-VocalisBundleHashes $fixture)
Assert-VocalisBundleUnchanged $fixture $expected
Set-Content -LiteralPath (Join-Path $fixture 'resource.txt') -Value 'modified' -Encoding ASCII
$rejected = $false
try { Assert-VocalisBundleUnchanged $fixture $expected } catch {
    if ($_.Exception.Message -notmatch 'Bundle changed') { throw }; $rejected = $true
}
if (-not $rejected) { throw 'Modified resource was accepted' }
$expected = @(Get-VocalisBundleHashes $fixture)
Set-Content -LiteralPath (Join-Path $fixture 'extra.txt') -Value 'added' -Encoding ASCII
$rejected = $false
try { Assert-VocalisBundleUnchanged $fixture $expected } catch {
    if ($_.Exception.Message -notmatch 'Bundle changed') { throw }; $rejected = $true
}
if (-not $rejected) { throw 'Added resource was accepted' }
$fakeCTest = Join-Path $scratch 'FailingCTest.cmd'
Set-Content -LiteralPath $fakeCTest -Value '@exit /b 7' -Encoding ASCII
$rejected = $false
try { Test-VocalisRelease $root (Join-Path $root $BuildDirectory) $fakeCTest | Out-Null } catch {
    if ($_.Exception.Message -notmatch 'Release regression failed with exit code 7') { throw }; $rejected = $true
}
if (-not $rejected) { throw 'Failed tests were accepted' }
Write-Output 'PASS: unchanged bundle accepted; modified/added resources and failed regression rejected'
