param([Parameter(Mandatory=$true)][string]$PackagePath)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$installer = Join-Path $PackagePath 'Install.ps1'
$scratch = Join-Path $PSScriptRoot ('outputs\package-install-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch -Force | Out-Null
$source = Join-Path $scratch 'source'
$target = Join-Path $scratch 'installed'
New-Item -ItemType Directory -Path $source -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PackagePath 'MakeGrainNow.aex') -Destination $source
Copy-Item -LiteralPath (Join-Path $PackagePath 'SHA256.txt') -Destination $source
$binary = Join-Path $source 'MakeGrainNow.aex'
$destination = Join-Path $target 'MakeGrainNow.aex'
$expected = (Get-FileHash -LiteralPath $binary).Hash
$global:mgnTestSimulateHost = $false
$global:mgnTestFailStagedMove = $false
# These mocks are scoped to this test process. No live Adobe host is closed.
function Get-Process {
    [CmdletBinding()] param([string[]]$Name)
    if ($global:mgnTestSimulateHost) { [pscustomobject]@{ ProcessName='AfterFX.com' } }
}
function Move-Item {
    [CmdletBinding()] param([string]$LiteralPath,[string]$Destination)
    if ($global:mgnTestFailStagedMove -and $LiteralPath.EndsWith('.staged')) { throw 'Injected replacement failure' }
    Microsoft.PowerShell.Management\Move-Item -LiteralPath $LiteralPath -Destination $Destination
}
function expectFailure([scriptblock]$action,[string]$message) {
    try { & $action | Out-Null } catch {
        if ($_.Exception.Message -notlike ('*' + $message + '*')) { throw }
        return
    }
    throw ('Expected failure: ' + $message)
}
& $installer -PluginRoot $target -BinaryPath $binary | Out-Null
if ((Get-FileHash -LiteralPath $destination).Hash -ne $expected) { throw 'Fresh install mismatch' }
[IO.File]::WriteAllText($destination,'our prior test plugin')
$previous = (Get-FileHash -LiteralPath $destination).Hash
$global:mgnTestFailStagedMove = $true
expectFailure { & $installer -PluginRoot $target -BinaryPath $binary } 'Injected replacement failure'
$global:mgnTestFailStagedMove = $false
if ((Get-FileHash -LiteralPath $destination).Hash -ne $previous) { throw 'Rollback did not preserve old plugin' }
if (Test-Path -LiteralPath ($destination + '.previous')) { throw 'Rollback left previous file' }
& $installer -PluginRoot $target -BinaryPath $binary | Out-Null
if ((Get-FileHash -LiteralPath $destination).Hash -ne $expected) { throw 'Update mismatch' }
[IO.File]::WriteAllText($binary,'our corrupt test plugin')
expectFailure { & $installer -PluginRoot $target -BinaryPath $binary } 'checksum mismatch'
if ((Get-FileHash -LiteralPath $destination).Hash -ne $expected) { throw 'Tamper check altered installed plugin' }
$global:mgnTestSimulateHost = $true
expectFailure { & $installer -PluginRoot $target -BinaryPath $binary } 'Close Premiere'
Write-Output 'Packaged installer: fresh install, replacement, rollback, corrupt-binary rejection and background-host guard passed. Workspace targets and mock process state only; not an actual administrator/Adobe installation on another machine.'

