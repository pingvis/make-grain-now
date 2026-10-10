; Build with scripts/build_setup.ps1. Test fixtures are never distributed.
#ifndef PayloadDir
  #error PayloadDir is required
#endif
#ifndef RuntimePath
  #error RuntimePath is required
#endif
#ifndef PluginSHA256
  #error PluginSHA256 is required
#endif
#ifndef RuntimeSHA256
  #error RuntimeSHA256 is required
#endif
#ifndef AppVersion
  #define AppVersion "0.2"
#endif
#ifndef OutputName
  #define OutputName "MakeGrainNow-Setup"
#endif

[Setup]
#ifdef TestFixtureRoot
AppId=MakeGrainNow-Installer-Disposable-Test
AppName=MakeGrainNow installer TEST FIXTURE
DefaultDirName={#TestFixtureRoot}\app
PrivilegesRequired=lowest
#else
AppId={{3BC9977C-53EA-4966-B62C-58B0DB5B7206}
AppName=MakeGrainNow
DefaultDirName={autopf}\MakeGrainNow
PrivilegesRequired=admin
#endif
AppVersion={#AppVersion}
AppPublisher=MakeGrainNow contributors
AppComments=Creative grain, halation and bloom for Premiere Pro and After Effects
UninstallDisplayName=MakeGrainNow
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0
DisableDirPage=yes
DisableProgramGroupPage=yes
UsePreviousAppDir=no
LicenseFile={#PayloadDir}\LICENSE
WizardStyle=modern
WizardResizable=no
DisableWelcomePage=no
CloseApplications=no
RestartApplications=no
RestartIfNeededByRun=no
SetupMutex=Global\MakeGrainNowSetup
Uninstallable=yes
CreateUninstallRegKey=yes
UninstallLogMode=append
Compression=lzma2
SolidCompression=yes
OutputBaseFilename={#OutputName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
WelcomeLabel2=This installs creative grain, halation and bloom for Adobe Premiere Pro and After Effects.%n%nClose both apps and background renders before installing.%n%nRequires Windows x64, an NVIDIA GPU with compute capability 7.5 or newer, and a recent NVIDIA driver. The required Microsoft Visual C++ runtime is included.
FinishedLabel=MakeGrainNow is installed.%n%nOpen Premiere Pro or After Effects and find Film Texture > Make Grain Now.%n%nTo remove it later, open Windows Settings > Apps > Installed apps > MakeGrainNow > Uninstall.

[Dirs]
Name: "{code:PluginDir}"

[Files]
; An exact allowlist keeps footage, LUTs, private notes and SDKs out of Setup.
Source: "{#PayloadDir}\MakeGrainNow.aex"; DestDir: "{code:PluginDir}"; Flags: ignoreversion
Source: "{#PayloadDir}\START_HERE.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\LOOKS.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\INNO_LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\THIRD_PARTY.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\BUILD_INFO.json"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\AE_PRESET_TESTS.json"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\SHA256.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RuntimePath}"; DestName: "vc_redist.x64.exe"; Flags: dontcopy

[Code]
type
  TProcessEntry = record
    Size, Usage, ProcessID, DefaultHeapID, ModuleID, Threads, ParentID: LongWord;
    Priority: LongInt;
    Flags: LongWord;
    ExeName: array[0..259] of Char;
  end;

function CreateToolhelp32Snapshot(Flags, ProcessID: LongWord): THandle;
  external 'CreateToolhelp32Snapshot@kernel32.dll stdcall';
function Process32First(Snapshot: THandle; var Entry: TProcessEntry): Boolean;
  external 'Process32FirstW@kernel32.dll stdcall';
function Process32Next(Snapshot: THandle; var Entry: TProcessEntry): Boolean;
  external 'Process32NextW@kernel32.dll stdcall';
function CloseHandle(Handle: THandle): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';
function GetFileAttributes(Name: String): LongWord;
  external 'GetFileAttributesW@kernel32.dll stdcall';

var
  PriorMoved, ReplacementPrepared, InstallCommitted: Boolean;

function PluginDir(Param: String): String;
begin
#ifdef TestFixtureRoot
  Result := '{#TestFixtureRoot}\plugin';
#else
  Result := ExpandConstant('{autopf}\Adobe\Common\Plug-ins\7.0\MediaCore\MakeGrainNow');
#endif
end;

function PluginPath: String;
begin
  Result := PluginDir('') + '\MakeGrainNow.aex';
end;

function BackupPath: String;
begin
  Result := PluginPath + '.mgn-previous';
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo,
  MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
begin
  Result := 'Plugin for Premiere Pro and After Effects:' + NewLine + Space + PluginDir('') +
    NewLine + NewLine + 'Documentation and uninstaller:' + NewLine + Space + WizardDirValue +
    NewLine + NewLine + 'The Microsoft Visual C++ x64 runtime is installed only if needed.';
end;

function PathExistsEvenIfLinkBroken(Path: String): Boolean;
begin
  Result := GetFileAttributes(Path) <> $FFFFFFFF;
end;

function AdobeGuardMessage: String;
var
  Snapshot: THandle;
  Entry: TProcessEntry;
  Name: String;
  I: Integer;
begin
  Result := '';
  Snapshot := CreateToolhelp32Snapshot(2, 0);
  if Snapshot = THandle(-1) then begin
    Result := 'Windows could not check running applications. Close Adobe apps and retry.';
    Exit;
  end;
  try
    Entry.Size := SizeOf(Entry);
    if not Process32First(Snapshot, Entry) then begin
      Result := 'Windows could not read running applications. Retry Setup.';
      Exit;
    end;
    repeat
      Name := '';
      for I := 0 to 259 do begin
        if Entry.ExeName[I] = #0 then Break;
        Name := Name + Entry.ExeName[I];
      end;
      Name := LowerCase(Name);
#ifdef TestFixtureRoot
      { Fixture hosts are our disposable executables, never the user's Adobe apps. }
      if Pos('mgn-guard-', Name) = 1 then Delete(Name, 1, 10) else Name := '';
#endif
      if (Name = 'afterfx.exe') or (Name = 'afterfx.com') or
         (Name = 'adobe premiere pro.exe') or (Name = 'aerender.exe') then begin
        Result := 'Close Premiere Pro, After Effects and background renders before continuing.';
        Exit;
      end;
    until not Process32Next(Snapshot, Entry);
  finally
    CloseHandle(Snapshot);
  end;
end;

function InitializeSetup: Boolean;
var
  Message: String;
begin
  Message := AdobeGuardMessage;
  if CheckForMutexes('Global\MakeGrainNowUninstall') then
    Message := 'Wait for the MakeGrainNow uninstaller to finish, then retry.';
  Result := Message = '';
  if not Result then begin
    Log(Message);
    SuppressibleMsgBox(Message, mbError, MB_OK, IDOK);
  end;
end;

function RuntimeReady: Boolean;
var
  Installed, Major, Minor: Cardinal;
  Key: String;
begin
  Key := 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64';
  Result := RegQueryDWordValue(HKLM64, Key, 'Installed', Installed) and
    RegQueryDWordValue(HKLM64, Key, 'Major', Major) and
    RegQueryDWordValue(HKLM64, Key, 'Minor', Minor);
  if Result then Result := (Installed = 1) and
    ((Major > 14) or ((Major = 14) and (Minor >= 44)));
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ExitCode: Integer;
  TempRuntime, TempPlugin: String;
begin
  Result := AdobeGuardMessage;
  if Result <> '' then Exit;
  ExtractTemporaryFile('MakeGrainNow.aex');
  TempPlugin := ExpandConstant('{tmp}\MakeGrainNow.aex');
  if CompareText(GetSHA256OfFile(TempPlugin), '{#PluginSHA256}') <> 0 then begin
    Result := 'The plugin checksum does not match. Download a fresh Setup.exe.';
    Exit;
  end;
#ifndef TestFixtureRoot
  if not RuntimeReady then begin
    ExtractTemporaryFile('vc_redist.x64.exe');
    TempRuntime := ExpandConstant('{tmp}\vc_redist.x64.exe');
    if CompareText(GetSHA256OfFile(TempRuntime), '{#RuntimeSHA256}') <> 0 then begin
      Result := 'The Microsoft runtime checksum does not match. Download a fresh Setup.exe.';
      Exit;
    end;
    Log('Installing the Microsoft Visual C++ x64 runtime.');
    if not Exec(TempRuntime, '/install /quiet /norestart', '', SW_HIDE, ewWaitUntilTerminated, ExitCode) then begin
      Result := 'Could not start the Microsoft runtime installer. Your previous plugin has been preserved.';
      Exit;
    end;
    if (ExitCode <> 0) and (ExitCode <> 3010) then begin
      Result := Format('Microsoft runtime installation failed (code %d). Your previous plugin has been preserved.', [ExitCode]);
      Exit;
    end;
    if ExitCode = 3010 then begin
      { Do not let silent Setup reboot the PC. Keep the prior plugin until the user restarts. }
      Result := 'The Microsoft runtime needs a Windows restart. Restart Windows, then run Setup again. Your previous plugin has been preserved.';
      Exit;
    end;
    if not RuntimeReady then begin
      Result := 'The Microsoft runtime is not ready. Restart Windows and retry Setup.';
      Exit;
    end;
  end else Log('The required Microsoft runtime is already installed.');
#else
  Log('TEST FIXTURE: shared Microsoft runtime installation is disabled.');
#endif
  Result := AdobeGuardMessage;
  if Result <> '' then Exit;
  if ReplacementPrepared then Exit;
  if PathExistsEvenIfLinkBroken(BackupPath) then begin
    Result := 'An earlier plugin backup exists: ' + BackupPath + '. Keep it and resolve the earlier installation before retrying.';
    Exit;
  end;
  if not ForceDirectories(PluginDir('')) then begin
    Result := 'Could not create the Adobe plugin folder.';
    Exit;
  end;
  if PathExistsEvenIfLinkBroken(PluginPath) then begin
    if (GetFileAttributes(PluginPath) and $10) <> 0 then begin
      Result := 'The plugin path is a directory. Setup will not replace it.';
      Exit;
    end;
    { Rename the file/link itself. Never copy through a development symlink. }
    if not RenameFile(PluginPath, BackupPath) then begin
      Result := 'Could not replace the existing plugin. Close Adobe and retry. Your previous plugin is unchanged.';
      Exit;
    end;
    PriorMoved := True;
  end;
  ReplacementPrepared := True;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
#ifdef TestFixtureRoot
  if (CurStep = ssInstall) and (ExpandConstant('{param:MGNTESTFAILAFTERBACKUP|0}') = '1') then
    RaiseException('Injected test failure after backup.');
#endif
  if CurStep = ssPostInstall then begin
    if CompareText(GetSHA256OfFile(PluginPath), '{#PluginSHA256}') <> 0 then
      RaiseException('Installed plugin checksum does not match.');
    InstallCommitted := True;
    if PriorMoved and not DeleteFile(BackupPath) then
      Log('Could not remove the old plugin backup: ' + BackupPath);
  end;
end;

procedure DeinitializeSetup;
begin
  if ReplacementPrepared and not InstallCommitted then begin
    if PathExistsEvenIfLinkBroken(PluginPath) then DeleteFile(PluginPath);
    if PriorMoved then begin
      if RenameFile(BackupPath, PluginPath) then Log('Restored the previous plugin.')
      else SuppressibleMsgBox('Could not restore the previous plugin. It is preserved at: ' + BackupPath, mbError, MB_OK, IDOK);
    end;
  end;
end;

function InitializeUninstall: Boolean;
var
  Message: String;
begin
  Message := AdobeGuardMessage;
  if CheckForMutexes('Global\MakeGrainNowSetup') then
    Message := 'Wait for MakeGrainNow Setup to finish, then retry.';
  Result := Message = '';
  if Result then CreateMutex('Global\MakeGrainNowUninstall') else begin
    Log(Message);
    SuppressibleMsgBox(Message, mbError, MB_OK, IDOK);
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Message: String;
begin
  if CurUninstallStep = usUninstall then begin
    Message := AdobeGuardMessage;
    if Message <> '' then RaiseException(Message);
  end;
end;
