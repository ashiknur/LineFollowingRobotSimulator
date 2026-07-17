; LineFollowerSim.iss
;
; Builds the Windows installer for the Line Following Robot Simulator.
; Two variants from the same script:
;
;   ISCC LineFollowerSim.iss          -> full offline installer
;                                        (bundles the MinGW toolchain, ~90 MB)
;   ISCC /DSlim LineFollowerSim.iss   -> slim installer (~15 MB) that downloads
;                                        the toolchain zip during setup
;
; The slim variant downloads {#ToolchainUrl}. Host lfr-toolchain-x64.zip as a
; GitHub release asset of the project repo (tag: toolchain-v1) or adjust the
; URL below.
;
; Prerequisites for building:
;   - The app built Release x64 (exe + SFML/vcpkg DLLs + VC++ CRT DLLs in
;     x64\Release)
;   - x64\Release\usersrc\   populated with the user-code template sources
;   - x64\Release\toolchain\ populated with the pruned MinGW toolchain
;     (full variant only)

#define AppName        "Line Following Robot Simulator"
#define AppVersion     "1.1.1"
#define AppPublisher   "Ashiknur"
#define AppExe         "LineFollowingRobotSimulator.exe"
#define ReleaseDir     "..\LineFollowingRobotSimulator\x64\Release"
#define ToolchainUrl   "https://github.com/ashiknur/LineFollowingRobotSimulator/releases/download/toolchain-v1/lfr-toolchain-x64.zip"

[Setup]
AppId={{8E1F4C3A-6D2B-4F0E-9A57-C4D81B2F6A31}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\LineFollowingRobotSimulator
DefaultGroupName={#AppName}
#ifdef Slim
OutputBaseFilename=LineFollowerSimulator-{#AppVersion}-Setup-Slim
#else
OutputBaseFilename=LineFollowerSimulator-{#AppVersion}-Setup
#endif
OutputDir=Output
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\{#AppExe}
DisableProgramGroupPage=yes

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#ReleaseDir}\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
; SFML + vcpkg dependency DLLs and app-local VC++ CRT DLLs
Source: "{#ReleaseDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#ReleaseDir}\LFR body.png"; DestDir: "{app}"; Flags: ignoreversion
; Template sources copied to the user's data dir on first run
Source: "{#ReleaseDir}\usersrc\*"; DestDir: "{app}\usersrc"; Flags: ignoreversion
#ifndef Slim
; Self-contained MinGW toolchain used for runtime compilation of user code
Source: "{#ReleaseDir}\toolchain\*"; DestDir: "{app}\toolchain"; Flags: recursesubdirs ignoreversion
#endif

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Compiled user DLLs etc. live under %LOCALAPPDATA%\LineFollowingRobotSimulator
; and are intentionally left behind (user's own code); the toolchain dir may
; contain nothing else but is removed if the slim installer created it.
Type: filesandordirs; Name: "{app}\toolchain"

#ifdef Slim
[Code]
var
  DownloadPage: TDownloadWizardPage;

procedure InitializeWizard;
begin
  DownloadPage := CreateDownloadPage(
    SetupMessage(msgWizardPreparing), SetupMessage(msgPreparingDesc), nil);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpReady then
  begin
    DownloadPage.Clear;
    DownloadPage.Add('{#ToolchainUrl}', 'toolchain.zip', '');
    DownloadPage.Show;
    try
      try
        DownloadPage.Download;
        Result := True;
      except
        if DownloadPage.AbortedByUser then
          Log('Toolchain download aborted by user.')
        else
          SuppressibleMsgBox(
            'Downloading the compiler toolchain failed:' + #13#10 +
            AddPeriod(GetExceptionMessage) + #13#10#13#10 +
            'Check your internet connection and try again.',
            mbCriticalError, MB_OK, IDOK);
        Result := False;
      end;
    finally
      DownloadPage.Hide;
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
  ZipPath, Args: String;
begin
  if CurStep = ssPostInstall then
  begin
    ZipPath := ExpandConstant('{tmp}\toolchain.zip');
    if FileExists(ZipPath) then
    begin
      WizardForm.StatusLabel.Caption := 'Extracting compiler toolchain...';
      Args := '-NoProfile -ExecutionPolicy Bypass -Command "' +
        'Expand-Archive -LiteralPath ''' + ZipPath + ''' ' +
        '-DestinationPath ''' + ExpandConstant('{app}') + ''' -Force"';
      if not Exec('powershell.exe', Args, '', SW_HIDE,
                  ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then
        SuppressibleMsgBox(
          'Extracting the compiler toolchain failed (code ' +
          IntToStr(ResultCode) + ').' + #13#10 +
          'The simulator will run, but compiling user code will not work ' +
          'until a toolchain is available. Reinstall, or set the ' +
          'LFR_TOOLCHAIN environment variable to a MinGW installation.',
          mbError, MB_OK, IDOK);
    end;
  end;
end;
#endif
