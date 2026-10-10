# Publishes only the reviewed Git commit and the prepared release ZIP.
param([string]$Repository = 'pingvis/make-grain-now')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $projectRoot
function Invoke-OptionalCommand {
    param([string]$Program, [string[]]$Arguments)
    # Expected nonzero probes must not abort on native stderr in Windows PowerShell 5.1.
    $ErrorActionPreference = 'Continue'
    & $Program @Arguments 2>$null
    $script:optionalExitCode = $LASTEXITCODE
}
$tag = 'v0.1.0-alpha.1'
$asset = Join-Path $projectRoot 'dist\MakeGrainNow-v0.1.0-alpha.1-windows.zip'
$checksum = "$asset.sha256"
if (-not (Test-Path -LiteralPath $asset) -or -not (Test-Path -LiteralPath $checksum)) { throw 'Prepared release asset/checksum is missing.' }
$expected = ((Get-Content -LiteralPath $checksum -Raw).Trim() -split '\s+')[0]
if ((Get-FileHash -LiteralPath $asset).Hash -ne $expected) { throw 'Release asset changed after preparation.' }
$sourceManifestPath = Join-Path $projectRoot 'docs\PUBLICATION_MANIFEST.json'
$sourceManifest = Get-Content -LiteralPath $sourceManifestPath -Raw | ConvertFrom-Json
foreach ($file in $sourceManifest.files) {
    if ((Get-FileHash -LiteralPath (Join-Path $projectRoot $file.path)).Hash -ne $file.sha256) { throw "Reviewed source changed: $($file.path)" }
}
Invoke-OptionalCommand gh @('auth','status')
if ($script:optionalExitCode) {
    & gh auth login --hostname github.com --git-protocol https --web
    if ($LASTEXITCODE) { throw 'Complete GitHub sign-in and rerun this script.' }
}
$login = & gh api user --jq .login
if ($LASTEXITCODE -or $login -ne ($Repository -split '/')[0]) { throw 'Signed-in GitHub account does not match the requested repository owner.' }
& gh auth setup-git
if ($LASTEXITCODE) { throw 'GitHub Git authentication setup failed.' }
$head = Invoke-OptionalCommand git @('rev-parse','--verify','HEAD')
if ($script:optionalExitCode) {
    $sourcePaths = @($sourceManifest.files | ForEach-Object { $_.path }) + 'docs/PUBLICATION_MANIFEST.json'
    & git add -- $sourcePaths
    if ($LASTEXITCODE) { throw 'Unable to stage reviewed source.' }
    $staged = @(& git diff --cached --name-only)
    if ($LASTEXITCODE -or @($staged | Where-Object { $_ -notin $sourcePaths }).Count) { throw 'Unreviewed files are staged. Inspect the Git index before publishing.' }
    & git -c user.name=Augis -c user.email=210446408+pingvis@users.noreply.github.com commit -m 'Initial MIT release: CUDA grain, halation and bloom'
    if ($LASTEXITCODE) { throw 'Initial source commit failed.' }
    $head = & git rev-parse HEAD
}
$existingRemote = Invoke-OptionalCommand git @('remote','get-url','origin')
if ($script:optionalExitCode -eq 0) {
    if ($existingRemote -notin @("https://github.com/$Repository.git","https://github.com/$Repository","git@github.com:$Repository.git")) { throw 'Existing origin points to another repository.' }
    $visibility = & gh repo view $Repository --json visibility --jq .visibility
    if ($LASTEXITCODE -or $visibility -ne 'PUBLIC') { throw 'Existing repository is not the expected public repository.' }
    & git push -u origin main
    if ($LASTEXITCODE) { throw 'Source push failed.' }
} else {
    Invoke-OptionalCommand gh @('repo','view',$Repository,'--json','url')
    if ($script:optionalExitCode -eq 0) { throw 'That repository already exists. Inspect it before attaching this source.' }
    & gh repo create $Repository --public --description 'CUDA grain, halation and bloom for Premiere Pro and After Effects (Windows, MIT)' --source $projectRoot --remote origin --push
    if ($LASTEXITCODE) { throw 'Repository creation/source push failed.' }
}
Invoke-OptionalCommand gh @('release','view',$tag,'--repo',$Repository)
if ($script:optionalExitCode -eq 0) { Write-Output "Release already exists: https://github.com/$Repository/releases/tag/$tag"; return }
& gh release create $tag $asset $checksum --repo $Repository --target $head --title 'Make Grain Now v0.1.0-alpha.1 (Windows CUDA)' --prerelease --notes-file (Join-Path $projectRoot 'docs\RELEASE_NOTES.md')
if ($LASTEXITCODE) { throw 'Release upload failed; repository source is already published.' }
Write-Output "Published https://github.com/$Repository and its Windows alpha release."
