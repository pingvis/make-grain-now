param([string]$PluginRoot = 'C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\MakeGrainNow', [string]$BinaryPath = '', [switch]$DevelopmentLink)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $BinaryPath) {
    $BinaryPath = Join-Path $PSScriptRoot 'MakeGrainNow.aex'
    if (-not (Test-Path -LiteralPath $BinaryPath)) { $BinaryPath = Join-Path $projectRoot 'build\Release\MakeGrainNow.aex' }
}
if (-not (Test-Path -LiteralPath $BinaryPath)) { throw 'Build MakeGrainNow.aex first.' }
if (Get-Process AfterFX,'AfterFX.com','Adobe Premiere Pro' -ErrorAction SilentlyContinue) { throw 'Close Premiere and After Effects, including background AfterFX.com renders, before installing this build.' }
$resolvedBinaryPath = (Resolve-Path -LiteralPath $BinaryPath).Path
$checksumPath = Join-Path (Split-Path -Parent $resolvedBinaryPath) 'SHA256.txt'
if (Test-Path -LiteralPath $checksumPath) {
    $checksum = (Get-Content -LiteralPath $checksumPath -Raw).Trim()
    if ($checksum -notmatch '^(?<hash>[A-Fa-f0-9]{64})\s+MakeGrainNow\.aex$') { throw 'Invalid package checksum file.' }
    $expectedHash = $Matches.hash
    if ((Get-FileHash -LiteralPath $resolvedBinaryPath -Algorithm SHA256).Hash -ne $expectedHash) { throw 'Plugin checksum mismatch. Extract a fresh copy of the test ZIP before installing.' }
}
New-Item -ItemType Directory -Path $PluginRoot -Force | Out-Null
$destinationPath = Join-Path $PluginRoot 'MakeGrainNow.aex'
if ($resolvedBinaryPath -eq [IO.Path]::GetFullPath($destinationPath)) { throw 'Source and destination must differ.' }
if (Test-Path -LiteralPath $destinationPath) {
    $existingItem = Get-Item -LiteralPath $destinationPath
    if ($existingItem.PSIsContainer) { throw 'Expected a plugin file, not a directory.' }
    if ($DevelopmentLink -and $existingItem.LinkType -eq 'SymbolicLink' -and $existingItem.Target -eq $resolvedBinaryPath) {
        Get-FileHash -LiteralPath $destinationPath -Algorithm SHA256
        return
    }
}
$stagedPath = Join-Path $PluginRoot 'MakeGrainNow.aex.staged'
$previousPath = Join-Path $PluginRoot 'MakeGrainNow.aex.previous'
if ((Test-Path -LiteralPath $stagedPath) -or (Test-Path -LiteralPath $previousPath)) {
    throw 'A staged/previous plugin exists. Inspect it before trying another install.'
}
# Create the new file/link before touching the installed file. A permission/link-creation failure preserves the old plugin.
if ($DevelopmentLink) { New-Item -ItemType SymbolicLink -Path $stagedPath -Value $resolvedBinaryPath | Out-Null }
else { Copy-Item -LiteralPath $resolvedBinaryPath -Destination $stagedPath }
try {
    if (Test-Path -LiteralPath $destinationPath) { Rename-Item -LiteralPath $destinationPath -NewName 'MakeGrainNow.aex.previous' }
    Move-Item -LiteralPath $stagedPath -Destination $destinationPath
} catch {
    if ((Test-Path -LiteralPath $previousPath) -and -not (Test-Path -LiteralPath $destinationPath)) {
        Move-Item -LiteralPath $previousPath -Destination $destinationPath
    }
    throw
} finally {
    if (Test-Path -LiteralPath $stagedPath) { Remove-Item -LiteralPath $stagedPath }
}
if (Test-Path -LiteralPath $previousPath) { Remove-Item -LiteralPath $previousPath }
Get-FileHash -LiteralPath (Join-Path $PluginRoot 'MakeGrainNow.aex') -Algorithm SHA256
