param([string]$Version = '0.2', [string]$CompilerPath = '', [string]$TestFixtureRoot = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if ($Version -notmatch '^\d+\.\d+$') { throw 'Version must be major.minor.' }
if (-not $CompilerPath) { $CompilerPath = Join-Path $projectRoot '.deps\setup-tools\inno-6.7.3\ISCC.exe' }
if (-not (Test-Path -LiteralPath $CompilerPath)) { throw 'Install the official Inno Setup 6.7.3 compiler in portable mode, or pass -CompilerPath.' }
$binary = Join-Path $projectRoot 'build\Release\MakeGrainNow.aex'
$binaryHash = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
$manifestPath = Join-Path $projectRoot 'tests\outputs\verification-manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.binarySHA256 -ne $binaryHash) { throw 'Verify the current plugin before building Setup.' }
$tag = 'MakeGrainNow-' + $Version + '-test-' + $binaryHash.Substring(0,8).ToLowerInvariant()
$payload = Join-Path $projectRoot ('dist\' + $tag)
$files = @('MakeGrainNow.aex','START_HERE.md','LOOKS.md','LICENSE','INNO_LICENSE.txt','THIRD_PARTY.md','BUILD_INFO.json','AE_PRESET_TESTS.json','SHA256.txt')
foreach ($name in $files) {
    if (-not (Test-Path -LiteralPath (Join-Path $payload $name))) { throw ('Missing payload file: ' + $name + '. Run package.ps1 -FriendTest first.') }
}
if ((Get-FileHash -LiteralPath (Join-Path $payload 'MakeGrainNow.aex')).Hash -ne $binaryHash) { throw 'Packaged plugin differs from the verified build.' }
$payloadManifest = Get-Content -LiteralPath (Join-Path $payload 'BUILD_INFO.json') -Raw | ConvertFrom-Json
$hostEvidence = Get-Content -LiteralPath (Join-Path $payload 'AE_PRESET_TESTS.json') -Raw | ConvertFrom-Json
if ($payloadManifest.binarySHA256 -ne $binaryHash -or $hostEvidence.binary_sha256 -ne $binaryHash -or
    -not ($hostEvidence.passed -eq $true -or $hostEvidence.result -eq 'passed')) { throw 'Packaged verification evidence is stale or failed.' }
$runtime = Join-Path $projectRoot '.deps\setup-tools\vc_redist.x64.exe'
$runtimeHash = (Get-FileHash -LiteralPath $runtime -Algorithm SHA256).Hash
$pinnedRuntimeHash = '843068991DAAA1F73AD9F6239BCE4D0F6A07A51F18C37EA2A867E9BECA71295C'
if ($runtimeHash -ne $pinnedRuntimeHash) { throw 'Microsoft runtime differs from the reviewed download. Review its signature/version and update the pinned hash deliberately.' }
$argsList = @('/Qp',('/DPayloadDir=' + $payload),('/DRuntimePath=' + $runtime),('/DPluginSHA256=' + $binaryHash),('/DRuntimeSHA256=' + $runtimeHash),('/DAppVersion=' + $Version))
$outputDir = Join-Path $projectRoot 'dist'
$outputName = 'MakeGrainNow-' + $Version + '-test-' + $binaryHash.Substring(0,8).ToLowerInvariant() + '-Setup'
if ($TestFixtureRoot) {
    $resolvedTestRoot = [IO.Path]::GetFullPath($TestFixtureRoot)
    $safeRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'tests\outputs')) + '\'
    if (-not $resolvedTestRoot.StartsWith($safeRoot, [StringComparison]::OrdinalIgnoreCase) -or $resolvedTestRoot.Contains('"') -or $resolvedTestRoot.Contains("'")) { throw 'Test fixture must be a child of this workspace tests/outputs directory.' }
    $outputDir = Join-Path $resolvedTestRoot 'setup'
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
    $outputName = 'MakeGrainNow-Installer-TEST-FIXTURE'
    $argsList += '/DTestFixtureRoot=' + $resolvedTestRoot
}
$argsList += @(('/O' + $outputDir), ('/DOutputName=' + $outputName), (Join-Path $projectRoot 'installer\MakeGrainNow.iss'))
& $CompilerPath @argsList
if ($LASTEXITCODE -ne 0) { throw ('Inno Setup compilation failed: ' + $LASTEXITCODE) }
$setupPath = Join-Path $outputDir ($outputName + '.exe')
$setupHash = (Get-FileHash -LiteralPath $setupPath -Algorithm SHA256).Hash
Set-Content -LiteralPath ($setupPath + '.sha256') -Value ($setupHash + '  ' + (Split-Path -Leaf $setupPath)) -Encoding ASCII
$report = [ordered]@{
    builtAtUtc = [DateTime]::UtcNow.ToString('o'); setupPath = $setupPath; setupSHA256 = $setupHash;
    pluginSHA256 = $binaryHash; microsoftRuntimeSHA256 = $runtimeHash;
    compiler = 'Inno Setup 6.7.3 (reviewed portable distribution)';
    compilerSHA256 = (Get-FileHash -LiteralPath $CompilerPath -Algorithm SHA256).Hash;
    installerSourceSHA256 = (Get-FileHash -LiteralPath (Join-Path $projectRoot 'installer\MakeGrainNow.iss')).Hash;
    payloadFiles = $files; disposableTestFixture = [bool]$TestFixtureRoot;
    adobeHostRenderingRetested = $false; signed = $false
}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath ($setupPath + '.build.json') -Encoding UTF8
Write-Output $setupPath
