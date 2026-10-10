param([switch]$Build, [string]$BuildDir = 'build', [string]$OutputDir = 'tests\outputs', [string]$PythonExe = 'python', [string]$CMakeExe = 'cmake')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $projectRoot
$buildRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot $BuildDir))
$outputRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot $OutputDir))
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$oldBuildDir = $env:MGN_BUILD_DIR
$oldOutputDir = $env:MGN_OUTPUT_DIR
try {
$env:MGN_BUILD_DIR = $buildRoot
$env:MGN_OUTPUT_DIR = $outputRoot
if ($Build) { & (Join-Path $PSScriptRoot 'build.ps1') -BuildDir $BuildDir -PythonExe $PythonExe -CMakeExe $CMakeExe }
$checks = @('verify_binary.py','core_tests.py','host_buffer_tests.py','particle_reference.py','stress_tests.py')
foreach ($check in $checks) {
    & $PythonExe (Join-Path $PSScriptRoot $check)
    if ($LASTEXITCODE) { throw "Verification failed: $check" }
}
$nativeOutput = & (Join-Path $buildRoot 'Release\native_adobe_tests.exe') (Join-Path $buildRoot 'Release\MakeGrainNow.aex')
$nativeExitCode = $LASTEXITCODE
$nativeOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'native-adapter-report.txt')
$nativeOutput | Write-Output
if ($nativeExitCode) { throw 'Native adapter selector tests failed.' }
$binaryHash = (Get-FileHash -LiteralPath (Join-Path $buildRoot 'Release\MakeGrainNow.aex')).Hash
@{
    verifiedAtUtc = [DateTime]::UtcNow.ToString('o')
    binarySHA256 = $binaryHash
    workspaceChecks = $checks + 'native_adobe_tests.exe'
    adobeHostRenderingVerified = $false
    scope = 'Standalone CUDA and shipped entry points with mock SDK callbacks. Actual Adobe evidence is recorded separately in ae-host-report.json and ae-float-report.json; this manifest does not certify host qualification.'
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $outputRoot 'verification-manifest.json')
Write-Output 'Workspace verification passed. These checks do not establish actual Adobe host rendering or playback.'
} finally {
    $env:MGN_BUILD_DIR = $oldBuildDir
    $env:MGN_OUTPUT_DIR = $oldOutputDir
}
