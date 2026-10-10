# Activate a verified preset-first candidate through the existing development link.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (Get-Process AfterFX,'AfterFX.com','Adobe Premiere Pro' -ErrorAction SilentlyContinue) {
    throw 'Close Adobe hosts and background renders before activating.'
}
$candidate = Join-Path $projectRoot 'build-presets'
$manifest = Get-Content -LiteralPath (Join-Path $projectRoot 'tests\outputs\presets\verification-manifest.json') -Raw | ConvertFrom-Json
$candidateHash = (Get-FileHash -LiteralPath (Join-Path $candidate 'Release\MakeGrainNow.aex')).Hash
if ($candidateHash -ne $manifest.binarySHA256) { throw 'Candidate changed after verification.' }
$files = @('Release\MakeGrainNow.aex','Release\texture_core.dll','Release\native_adobe_tests.exe','kernels.ptx','kernel_source.h')
$destination = Join-Path $projectRoot 'build'
foreach ($file in $files) {
    if (-not (Test-Path -LiteralPath (Join-Path $candidate $file)) -or -not (Test-Path -LiteralPath (Join-Path $destination $file))) { throw "Missing build file: $file" }
}
$backup = Join-Path $projectRoot ('tests\outputs\preset-activation-backup-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $backup 'Release') -Force | Out-Null
foreach ($file in $files) { Copy-Item -LiteralPath (Join-Path $destination $file) -Destination (Join-Path $backup $file) }
try {
    foreach ($file in $files) { Copy-Item -LiteralPath (Join-Path $candidate $file) -Destination (Join-Path $destination $file) }
    if ((Get-FileHash -LiteralPath (Join-Path $destination 'Release\MakeGrainNow.aex')).Hash -ne $candidateHash) { throw 'Activation hash mismatch.' }
} catch {
    foreach ($file in $files) { Copy-Item -LiteralPath (Join-Path $backup $file) -Destination (Join-Path $destination $file) }
    throw
}
Write-Output "Activated $candidateHash. Previous build: $backup"
Write-Output 'Run scripts/verify.ps1 and record actual Adobe evidence separately before packaging.'
