param([Parameter(Mandatory=$true)][string]$FixtureRoot)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$root = [IO.Path]::GetFullPath($FixtureRoot)
$safeRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'tests\outputs')) + '\'
if (-not $root.StartsWith($safeRoot,[StringComparison]::OrdinalIgnoreCase)) { throw 'Tests require a workspace fixture root.' }
$setup = Join-Path $root 'setup\MakeGrainNow-Installer-TEST-FIXTURE.exe'
$app = Join-Path $root 'app'
$pluginDir = Join-Path $root 'plugin'
$plugin = Join-Path $pluginDir 'MakeGrainNow.aex'
$uninstall = Join-Path $app 'unins000.exe'
$binary = Join-Path $projectRoot 'build\Release\MakeGrainNow.aex'
$expectedHash = (Get-FileHash -LiteralPath $binary).Hash
$checks = [Collections.Generic.List[string]]::new()
$registryPath = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\MakeGrainNow-Installer-Disposable-Test_is1'
function registrationExists {
    $baseKey = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::CurrentUser,[Microsoft.Win32.RegistryView]::Registry64)
    try { $key = $baseKey.OpenSubKey($registryPath); if ($key) { $key.Dispose(); return $true }; return $false }
    finally { $baseKey.Dispose() }
}
function runInstaller([string]$exe,[string]$name,[string[]]$extra=@()) {
    $log = Join-Path $root ($name + '.log')
    $arguments = @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',('/LOG="' + $log + '"')) + $extra
    $process = Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(60000)) { throw ('Installer timed out; process ' + $process.Id + ', log ' + $log) }
    $process.Refresh()
    # The uninstaller's self-delete child can finish after its original process exits.
    $finishTimer = [Diagnostics.Stopwatch]::StartNew()
    while ((Get-Content -LiteralPath $log -Raw) -notmatch 'Log closed\.') {
        if ($finishTimer.Elapsed.TotalSeconds -ge 10) { throw ('Installer child did not finish; log ' + $log) }
        Start-Sleep -Milliseconds 100
    }
    return $process.ExitCode
}
function expectInstalled {
    if ((Get-FileHash -LiteralPath $plugin).Hash -ne $expectedHash) { throw 'Installed binary mismatch.' }
    if ((Get-Item -LiteralPath $plugin).LinkType) { throw 'Installer left a link rather than an independent copy.' }
    if (-not (Test-Path -LiteralPath $uninstall)) { throw 'Uninstaller missing.' }
    if (-not (registrationExists)) { throw 'Windows Installed apps registration missing.' }
}
if (Test-Path -LiteralPath $plugin) { throw 'Use a fresh fixture folder for these tests.' }
if ((runInstaller $setup '01-fresh') -ne 0) { throw 'Fresh install failed.' }
expectInstalled
$checks.Add('Fresh install: exact plugin hash, independent copy, uninstaller and HKCU Installed apps entry')
[IO.File]::WriteAllText($plugin,'our old disposable plugin')
$oldHash = (Get-FileHash -LiteralPath $plugin).Hash
if ((runInstaller $setup '02-rollback' @('/MGNTESTFAILAFTERBACKUP=1')) -eq 0) { throw 'Failure injection unexpectedly succeeded.' }
if ((Get-FileHash -LiteralPath $plugin).Hash -ne $oldHash -or (Test-Path -LiteralPath ($plugin + '.mgn-previous'))) { throw 'Failed install did not restore prior file.' }
$checks.Add('Injected failure after backup restores the prior regular file')
if ((runInstaller $setup '03-upgrade') -ne 0) { throw 'Upgrade failed.' }
expectInstalled
if (@(Get-ChildItem -LiteralPath $app -Filter 'unins*.exe').Count -ne 1) { throw 'Upgrade created duplicate uninstallers.' }
$checks.Add('Upgrade replaces the old file and keeps one uninstaller')
[IO.File]::WriteAllText(($plugin + '.mgn-previous'),'keep this existing backup')
if ((runInstaller $setup '03a-existing-backup') -eq 0) { throw 'Installer overwrote an existing backup.' }
expectInstalled
if ([IO.File]::ReadAllText($plugin + '.mgn-previous') -ne 'keep this existing backup') { throw 'Existing backup was modified.' }
Remove-Item -LiteralPath ($plugin + '.mgn-previous')
$checks.Add('Existing recovery backup blocks replacement and remains unchanged')
Remove-Item -LiteralPath $plugin
New-Item -ItemType Directory -Path $plugin | Out-Null
$directorySentinel = Join-Path $plugin 'preserve.txt'
[IO.File]::WriteAllText($directorySentinel,'keep this directory')
if ((runInstaller $setup '03b-directory-collision') -eq 0 -or [IO.File]::ReadAllText($directorySentinel) -ne 'keep this directory') { throw 'Plugin-directory collision was not preserved.' }
Remove-Item -LiteralPath $directorySentinel
Remove-Item -LiteralPath $plugin
if ((runInstaller $setup '03c-after-collision') -ne 0) { throw 'Recovery from directory collision failed.' }
$checks.Add('A directory at the plugin filename is refused without deleting its contents')
$source = Join-Path $root 'developer-source.aex'
Copy-Item -LiteralPath $binary -Destination $source
Remove-Item -LiteralPath $plugin
$linkKind = 'SymbolicLink'
try { New-Item -ItemType SymbolicLink -Path $plugin -Value $source -ErrorAction Stop | Out-Null }
catch {
    if ($_.Exception.Message -notmatch 'privilege|privileges|Administrator') { throw }
    $linkKind = 'HardLink'
    New-Item -ItemType HardLink -Path $plugin -Value $source | Out-Null
    Write-Output 'Symbolic-link creation requires Windows administrator rights here; exercising hard-link source preservation instead.'
}
if ((runInstaller $setup '04-link-rollback' @('/MGNTESTFAILAFTERBACKUP=1')) -eq 0) { throw 'Link failure injection unexpectedly succeeded.' }
if ((Get-Item -LiteralPath $plugin).LinkType -ne $linkKind -or (Get-FileHash -LiteralPath $source).Hash -ne $expectedHash) { throw 'Failed install did not preserve link/source.' }
$checks.Add('Injected failure restores a ' + $linkKind + ' without modifying its source')
if ((runInstaller $setup '05-link-upgrade') -ne 0) { throw 'Development link upgrade failed.' }
expectInstalled
if ((Get-FileHash -LiteralPath $source).Hash -ne $expectedHash) { throw 'Development source was modified.' }
$checks.Add($linkKind + ' upgrade installs an independent copy and preserves the source')
# Compile an idle Windows executable, then run only our named processes for real snapshot checks.
$guardDir = Join-Path $root 'guard'
New-Item -ItemType Directory -Path $guardDir -Force | Out-Null
$guardSource = Join-Path $guardDir 'guard.cs'
Set-Content -LiteralPath $guardSource -Value 'class Guard { static void Main() { System.Threading.Thread.Sleep(120000); } }' -Encoding ASCII
$guardExe = Join-Path $guardDir 'guard.exe'
& 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe' /nologo /target:winexe (('/out:' + $guardExe)) $guardSource
if ($LASTEXITCODE -ne 0) { throw 'Test host compilation failed.' }
foreach ($name in @('AfterFX.exe','AfterFX.com','Adobe Premiere Pro.exe','aerender.exe')) {
    $namedGuard = Join-Path $guardDir ('MGN-Guard-' + $name)
    Copy-Item -LiteralPath $guardExe -Destination $namedGuard -Force
    $guardProcess = Start-Process -FilePath $namedGuard -WindowStyle Hidden -PassThru
    try {
        if ((runInstaller $setup ('06-host-' + $name.Replace(' ','-'))) -eq 0) { throw ('Did not block host: ' + $name) }
        expectInstalled
        if ((runInstaller $uninstall ('07-uninstall-host-' + $name.Replace(' ','-'))) -eq 0) { throw ('Uninstall did not block host: ' + $name) }
        expectInstalled
    } finally {
        if (-not $guardProcess.HasExited) { Stop-Process -Id $guardProcess.Id -ErrorAction Stop }
    }
}
$checks.Add('Install and uninstall block all four disposable host processes using real Windows snapshots')
$sentinel = Join-Path $pluginDir 'other-plugin.aex'
$appSentinel = Join-Path $app 'user-kept-file.txt'
[IO.File]::WriteAllText($sentinel,'keep this unrelated plugin')
[IO.File]::WriteAllText($appSentinel,'keep this unrelated document')
# Uninstall a registered path that was subsequently changed back to a development link.
Remove-Item -LiteralPath $plugin
New-Item -ItemType $linkKind -Path $plugin -Value $source | Out-Null
if ((runInstaller $uninstall '08-uninstall') -ne 0) { throw 'Uninstall failed.' }
if ((Test-Path -LiteralPath $plugin) -or (registrationExists)) { throw 'Uninstall left the plugin or Installed apps entry.' }
if (-not (Test-Path -LiteralPath $sentinel) -or -not (Test-Path -LiteralPath $appSentinel) -or (Get-FileHash -LiteralPath $source).Hash -ne $expectedHash) { throw 'Uninstall removed unrelated files or development source.' }
$checks.Add('Uninstall removes the link and registration, preserving its source and unrelated files')
if ((runInstaller $setup '09-reinstall') -ne 0) { throw 'Reinstall failed.' }
expectInstalled
if ((runInstaller $uninstall '10-final-uninstall') -ne 0) { throw 'Final uninstall failed.' }
$checks.Add('Reinstall after uninstall succeeds; final fixture registration removed')
$report = [ordered]@{ result='passed'; verifiedAtUtc=[DateTime]::UtcNow.ToString('o'); scope='Actual compiled Inno installer/uninstaller in a workspace directory and unique HKCU test registration. Shared Adobe installation and Microsoft runtime were not changed. Fixture host names have a test-only prefix.'; setupSHA256=(Get-FileHash -LiteralPath $setup).Hash; installerSourceSHA256=(Get-FileHash -LiteralPath (Join-Path $projectRoot 'installer\MakeGrainNow.iss')).Hash; pluginSHA256=$expectedHash; linkKindTested=$linkKind; symbolicLinkTested=($linkKind -eq 'SymbolicLink'); checks=$checks.ToArray() }
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $root 'report.json') -Encoding UTF8
$checks | ForEach-Object { Write-Output ('PASS: ' + $_) }
