param([string]$AfterFX = 'C:\Program Files\Adobe\Adobe After Effects 2026\Support Files\AfterFX.exe')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -in @('AfterFX','AfterFX.com') }) { throw 'Close AE, including headless render children, before launching the isolated validation run.' }
$installedPath = 'C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\MakeGrainNow\MakeGrainNow.aex'
$builtPath = Join-Path $projectRoot 'build\Release\MakeGrainNow.aex'
if (-not (Test-Path -LiteralPath $installedPath)) { throw 'Install the current plugin before running actual AE tests.' }
if ((Get-FileHash -LiteralPath $installedPath).Hash -ne (Get-FileHash -LiteralPath $builtPath).Hash) { throw 'Installed plugin differs from the tested workspace build. Update it first.' }
$scriptPath = Join-Path $PSScriptRoot 'ae_validation.jsx'
# Waiting keeps the launch attached until our guarded JSX saves its project and quits.
$testStartedUtc = [DateTime]::UtcNow
Start-Process -FilePath $AfterFX -ArgumentList '-r',('"' + $scriptPath + '"') -WindowStyle Hidden -Wait
$smokeLogPath = Join-Path $projectRoot 'tests\outputs\ae-smoke.txt'
if (-not (Test-Path -LiteralPath $smokeLogPath) -or (Get-Item -LiteralPath $smokeLogPath).LastWriteTimeUtc -lt $testStartedUtc) {
    throw 'AE did not execute a fresh test script. Run scripts/ae_validation.jsx through File > Scripts > Run Script File in an empty project.'
}
$pythonExe = 'C:\Users\augus\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $pythonExe (Join-Path $PSScriptRoot 'check_ae_outputs.py')
if ($LASTEXITCODE) { throw 'AE host render checks failed; inspect tests\outputs\ae-*.txt and the disposable projects.' }
