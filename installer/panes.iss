#define AppVersion "1.0.0"
[Setup]
AppId={{256D9795-5629-48D9-881D-E0BB70DB1F93}
AppName=panes
AppVersion={#AppVersion}
AppPublisher=panes
DefaultDirName={localappdata}\Programs\panes
DefaultGroupName=panes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.22000
OutputDir=..\dist
OutputBaseFilename=panes-Setup-{#AppVersion}-x64
SetupIconFile=..\assets\app.ico
UninstallDisplayIcon={app}\panes.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes
CloseApplications=yes
RestartApplications=no
AppMutex=Local\panes.v1
VersionInfoVersion={#AppVersion}

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked
Name: "startup"; Description: "Launch widgets at sign-in"; GroupDescription: "Startup:"; Flags: unchecked

[Files]
Source: "..\dist\panes.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\panes.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\panes.svg"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\panes-icon.png"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\VALIDATION.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD-PARTY-NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\third_party\json.LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\panes"; Filename: "{app}\panes.exe"; Parameters: "--settings"
Name: "{autodesktop}\panes"; Filename: "{app}\panes.exe"; Parameters: "--settings"; Tasks: desktopicon

[Registry]
; Always record cleanup, including startup enabled later from Settings.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "panes"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "panes"; ValueData: """{app}\panes.exe"""; Tasks: startup; Flags: uninsdeletevalue

[Run]
Filename: "{app}\panes.exe"; Parameters: "--settings"; Description: "Open panes"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{app}\panes.exe"; Parameters: "--quit"; Flags: runhidden; RunOnceId: "Quitpanes"

[Code]
function InitializeSetup(): Boolean;
var
  ResultCode: Integer;
  Existing: String;
begin
  Existing := ExpandConstant('{localappdata}\Programs\panes\panes.exe');
  if FileExists(Existing) then
    Exec(Existing, '--quit', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Result := True;
end;
