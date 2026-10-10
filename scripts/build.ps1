param([string]$BuildDir = 'build', [string]$PythonExe = 'python', [string]$CMakeExe = 'cmake')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $projectRoot
& $PythonExe scripts/compile_kernels.py --output-dir $BuildDir
if ($LASTEXITCODE) { throw 'Kernel compilation failed' }
& $CMakeExe -S . -B $BuildDir -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE) { throw 'CMake configure failed' }
& $CMakeExe --build $BuildDir --config Release
if ($LASTEXITCODE) { throw 'C++ build failed' }
