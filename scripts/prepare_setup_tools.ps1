$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolRoot = Join-Path $projectRoot '.deps\setup-tools'
New-Item -ItemType Directory -Path $toolRoot -Force | Out-Null
$downloads = @(
    @{ Name='innosetup-6.7.3.exe'; Url='https://github.com/jrsoftware/issrc/releases/download/is-6_7_3/innosetup-6.7.3.exe'; SHA256='9C73C3BAE7ED48D44112A0F48E66742C00090BDB5BEF71D9D3C056C66E97B732'; Publisher='CN=Pyrsys B.V.' },
    @{ Name='vc_redist.x64.exe'; Url='https://aka.ms/vc14/vc_redist.x64.exe'; SHA256='843068991DAAA1F73AD9F6239BCE4D0F6A07A51F18C37EA2A867E9BECA71295C'; Publisher='CN=Microsoft Corporation' }
)
foreach ($download in $downloads) {
    $path = Join-Path $toolRoot $download.Name
    if (-not (Test-Path -LiteralPath $path)) { Invoke-WebRequest -Uri $download.Url -OutFile $path }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $download.SHA256) { throw ($download.Name + ' differs from the reviewed download. Review the new vendor release before updating the pinned hash.') }
    $signature = Get-AuthenticodeSignature -LiteralPath $path
    if ($signature.Status -ne 'Valid' -or -not $signature.SignerCertificate.Subject.Contains($download.Publisher)) { throw ($download.Name + ' has an unexpected or invalid vendor signature.') }
}
$compilerRoot = Join-Path $toolRoot 'inno-6.7.3'
$compiler = Join-Path $compilerRoot 'ISCC.exe'
if (-not (Test-Path -LiteralPath $compiler)) {
    $arguments = @('/PORTABLE=1','/CURRENTUSER','/VERYSILENT','/SUPPRESSMSGBOXES','/SP-','/NORESTART','/NOICONS',('/DIR="' + $compilerRoot + '"'),'/TASKS=""')
    $process = Start-Process -FilePath (Join-Path $toolRoot 'innosetup-6.7.3.exe') -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw ('Portable compiler setup failed: ' + $process.ExitCode) }
}
Write-Output $compiler
