@echo off
setlocal
where cl >nul 2>&1
if errorlevel 1 call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set "SDK=%~1"
set "SRC=%~2"
set "OUT=%~3"
cl /nologo /TC /I "%SDK%\Headers" /EP "%SRC%" > "%OUT%.rr"
if errorlevel 1 exit /b 1
pushd "%~dp3"
"%SDK%\Resources\PiPLTool.exe" "%~n3.rr" "%~n3.rrc"
if errorlevel 1 exit /b 1
cl /nologo /TC /D MSWindows /EP "%~n3.rrc" > "%~n3.rc"
if errorlevel 1 exit /b 1
rc /nologo /fo "%~n3.res" "%~n3.rc"
exit /b %errorlevel%
