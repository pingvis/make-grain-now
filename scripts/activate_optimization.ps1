# Run after closing Adobe. The existing development link points at build/Release.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (Get-Process AfterFX,'AfterFX.com','Adobe Premiere Pro' -ErrorAction SilentlyContinue) {
    throw 'Close After Effects and Premiere, including background renders, before activating the candidate.'
}
$candidate = Join-Path $projectRoot 'build-optimization'
$manifestPath = Join-Path $projectRoot 'tests\outputs\optimization\verification-manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$candidateHash = (Get-FileHash -LiteralPath (Join-Path $candidate 'Release\MakeGrainNow.aex')).Hash
if ($candidateHash -ne $manifest.binarySHA256) { throw 'Candidate changed after verification. Verify it again first.' }
$files = @('Release\MakeGrainNow.aex','Release\texture_core.dll','Release\native_adobe_tests.exe','kernels.ptx','kernel_source.h')
$destination = Join-Path $projectRoot 'build'
$backup = Join-Path $projectRoot ('tests\outputs\activation-backup-' + [Guid]::NewGuid().ToString('N'))
foreach ($file in $files) {
    if (-not (Test-Path -LiteralPath (Join-Path $candidate $file)) -or -not (Test-Path -LiteralPath (Join-Path $destination $file))) {
        throw "Missing candidate/current build file: $file"
    }
}
New-Item -ItemType Directory -Path (Join-Path $backup 'Release') -Force | Out-Null
foreach ($file in $files) { Copy-Item -LiteralPath (Join-Path $destination $file) -Destination (Join-Path $backup $file) }
try {
    foreach ($file in $files) { Copy-Item -LiteralPath (Join-Path $candidate $file) -Destination (Join-Path $destination $file) }
    if ((Get-FileHash -LiteralPath (Join-Path $destination 'Release\MakeGrainNow.aex')).Hash -ne $candidateHash) { throw 'Activation hash mismatch.' }
} catch {
    foreach ($file in $files) { Copy-Item -LiteralPath (Join-Path $backup $file) -Destination (Join-Path $destination $file) }
    throw
}
Write-Output "Activated $candidateHash. Previous build saved at $backup."
Write-Output 'Run scripts/verify.ps1 before repackaging. Actual Adobe testing of this candidate is still pending.'
