param([switch]$FriendTest,[string]$Version='0.2',[string]$HostEvidencePath='tests\outputs\ae-preset-report.json')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if ($Version -notmatch '^[0-9]+\.[0-9]+$') { throw 'Version must be numeric major.minor.' }
$packagePath = Join-Path $projectRoot ('dist\MakeGrainNow-' + $Version + '-dev')
$manifestPath = Join-Path $projectRoot 'tests\outputs\verification-manifest.json'
if (-not (Test-Path -LiteralPath $manifestPath)) { throw 'Run scripts/verify.ps1 before packaging.' }
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.binarySHA256 -ne (Get-FileHash -LiteralPath (Join-Path $projectRoot 'build\Release\MakeGrainNow.aex')).Hash) { throw 'Workspace binary changed after verification. Verify it again before packaging.' }
New-Item -ItemType Directory -Path $packagePath -Force | Out-Null
foreach ($filePath in @('build\Release\MakeGrainNow.aex','README.md','THIRD_PARTY.md','LICENSE')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $filePath) -Destination $packagePath -Force
}
$hashValue = (Get-FileHash -LiteralPath (Join-Path $packagePath 'MakeGrainNow.aex') -Algorithm SHA256).Hash
Copy-Item -LiteralPath (Join-Path $projectRoot 'scripts\install.ps1') -Destination (Join-Path $packagePath 'Install.ps1') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'installer\INNO_LICENSE.txt') -Destination (Join-Path $packagePath 'INNO_LICENSE.txt') -Force
Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $packagePath 'BUILD_INFO.json') -Force
$floatReportPath = Join-Path $projectRoot $HostEvidencePath
$evidenceName = if ((Split-Path -Leaf $floatReportPath) -eq 'ae-preset-report.json') { 'AE_PRESET_TESTS.json' } else { 'AE_FLOAT_TESTS.json' }
if (Test-Path -LiteralPath $floatReportPath) {
    $floatReport = Get-Content -LiteralPath $floatReportPath -Raw | ConvertFrom-Json
    if ($floatReport.binary_sha256 -ne $hashValue) { throw 'AE evidence belongs to another binary. Refresh the report before packaging.' }
    if (-not ($floatReport.passed -eq $true -or $floatReport.result -eq 'passed')) { throw 'AE evidence did not pass.' }
    Copy-Item -LiteralPath $floatReportPath -Destination (Join-Path $packagePath $evidenceName) -Force
}
Set-Content -LiteralPath (Join-Path $packagePath 'SHA256.txt') -Value "$hashValue  MakeGrainNow.aex"
Write-Output $packagePath
if ($FriendTest) {
    $tag = 'MakeGrainNow-' + $Version + '-test-' + $hashValue.Substring(0,8).ToLowerInvariant()
    $friendPath = Join-Path $projectRoot ('dist\' + $tag)
    New-Item -ItemType Directory -Path $friendPath -Force | Out-Null
    # Explicit list: no source projects, private brief, footage, SDKs or runtime tools.
    $friendFiles = @('MakeGrainNow.aex','Install.ps1','THIRD_PARTY.md','LICENSE','INNO_LICENSE.txt','BUILD_INFO.json','SHA256.txt')
    if (Test-Path -LiteralPath $floatReportPath) { $friendFiles += $evidenceName }
    foreach ($name in $friendFiles) {
        Copy-Item -LiteralPath (Join-Path $packagePath $name) -Destination (Join-Path $friendPath $name) -Force
    }
    Copy-Item -LiteralPath (Join-Path $projectRoot 'TESTING_GUIDE.md') -Destination (Join-Path $friendPath 'START_HERE.md') -Force
    $friendFiles += 'START_HERE.md'
    Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\LOOKS.md') -Destination (Join-Path $friendPath 'LOOKS.md') -Force
    $friendFiles += 'LOOKS.md'
    $checksums = foreach ($name in $friendFiles) {
        '{0}  {1}' -f (Get-FileHash -LiteralPath (Join-Path $friendPath $name) -Algorithm SHA256).Hash, $name
    }
    Set-Content -LiteralPath (Join-Path $friendPath 'SHA256SUMS.txt') -Value $checksums -Encoding ASCII
    $friendFiles += 'SHA256SUMS.txt'
    $zipPath = Join-Path $projectRoot ('dist\' + $tag + '-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + '.zip')
    Add-Type -AssemblyName System.IO.Compression
    $zipStream = [IO.File]::Open($zipPath,[IO.FileMode]::CreateNew)
    $archive = New-Object IO.Compression.ZipArchive($zipStream,[IO.Compression.ZipArchiveMode]::Create,$false)
    try {
        foreach ($name in $friendFiles) {
            $entry = $archive.CreateEntry($tag + '/' + $name,[IO.Compression.CompressionLevel]::Optimal)
            $inputStream = [IO.File]::OpenRead((Join-Path $friendPath $name))
            $outputStream = $entry.Open()
            try { $inputStream.CopyTo($outputStream) }
            finally { $outputStream.Dispose(); $inputStream.Dispose() }
        }
    } finally { $archive.Dispose(); $zipStream.Dispose() }
    Write-Output $zipPath
}
