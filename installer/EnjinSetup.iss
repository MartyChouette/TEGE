; ============================================================
; Enjin Engine - Inno Setup Installer Script
; ============================================================
; Requirements:
;   - Inno Setup 6.x (https://jrsoftware.org/isinfo.php)
;   - Build the project in Release first:
;       cd build && cmake --build . --config Release
;   - Compile shaders (if not already done)
;
; To build the installer:
;   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\EnjinSetup.iss
;
; Or open EnjinSetup.iss in the Inno Setup Compiler GUI and hit Compile.
; ============================================================

#define AppName      "TEGE"
#define AppVersion   "0.9.7"
#define AppPublisher "Marty Scott"
#define AppURL       "https://www.marty64.net/enjin"
#define AppExeName   "EnjinEditor.exe"
#define SourceRoot   ".."

[Setup]
AppId={{B7E2F4A1-9C3D-4E8F-A5B2-1D7C6E9F0A3B}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir={#SourceRoot}\installer\output
OutputBaseFilename=TEGESetup-{#AppVersion}
SetupIconFile={#SourceRoot}\installer\enjin.ico
UninstallDisplayIcon={app}\enjin.ico
Compression=lzma2/ultra64
SolidCompression=yes
LZMANumBlockThreads=4
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
LicenseFile={#SourceRoot}\LICENSE
UninstallDisplayName={#AppName}
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full";    Description: "Full installation"
Name: "compact"; Description: "Editor only"
Name: "custom";  Description: "Custom installation"; Flags: iscustom

[Components]
Name: "editor";   Description: "TEGE Editor";             Types: full compact custom; Flags: fixed
Name: "player";   Description: "Standalone Player";      Types: full
Name: "shaders";  Description: "Compiled Shaders (SPV)"; Types: full
Name: "scripts";  Description: "Script Templates";       Types: full
Name: "docs";     Description: "Documentation";          Types: full

; ============================================================
; Files
; ============================================================

[Files]
; --- Editor ---
Source: "{#SourceRoot}\build\bin\Release\EnjinEditor.exe"; DestDir: "{app}\bin"; Components: editor; Flags: ignoreversion

; --- Player ---
Source: "{#SourceRoot}\build\bin\Release\EnjinPlayer.exe"; DestDir: "{app}\bin"; Components: player; Flags: ignoreversion

; --- DLLs (if any appear in future builds) ---
; Source: "{#SourceRoot}\build\bin\Release\*.dll"; DestDir: "{app}\bin"; Components: editor; Flags: ignoreversion skipifsourcedoesntexist

; --- Compiled Shaders ---
Source: "{#SourceRoot}\Engine\shaders\*.spv"; DestDir: "{app}\shaders"; Components: shaders; Flags: ignoreversion

; --- Scripts ---
Source: "{#SourceRoot}\build\bin\Release\scripts\*"; DestDir: "{app}\scripts"; Components: scripts; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; --- Documentation ---
; Built-in templates. The editor reads these from disk; without them a fresh
; install has no templates to start a project from.
Source: "{#SourceRoot}\builtin_templates\*"; DestDir: "{app}\builtin_templates"; Components: editor; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#SourceRoot}\installer\enjin.ico";     DestDir: "{app}"; Components: editor; Flags: ignoreversion
Source: "{#SourceRoot}\LICENSE";                DestDir: "{app}"; Components: editor; Flags: ignoreversion
Source: "{#SourceRoot}\docs\USER_MANUAL.md";   DestDir: "{app}\docs"; Components: docs; Flags: ignoreversion
Source: "{#SourceRoot}\docs\API_REFERENCE.md";  DestDir: "{app}\docs"; Components: docs; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceRoot}\docs\ARCHITECTURE.md";   DestDir: "{app}\docs"; Components: docs; Flags: ignoreversion
Source: "{#SourceRoot}\docs\BUILD.md";           DestDir: "{app}\docs"; Components: docs; Flags: ignoreversion
Source: "{#SourceRoot}\docs\SCRIPTING_API.md";   DestDir: "{app}\docs"; Components: docs; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceRoot}\docs\ROADMAP.md";         DestDir: "{app}\docs"; Components: docs; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceRoot}\docs\manual.html";        DestDir: "{app}\docs"; Components: docs; Flags: ignoreversion skipifsourcedoesntexist

; ============================================================
; Shortcuts
; ============================================================

[Icons]
Name: "{group}\TEGE Editor";             Filename: "{app}\bin\{#AppExeName}"; IconFilename: "{app}\enjin.ico"
Name: "{group}\Documentation";           Filename: "{app}\docs\USER_MANUAL.md";  Components: docs
Name: "{group}\Uninstall TEGE";          Filename: "{uninstallexe}"
Name: "{autodesktop}\TEGE Editor";       Filename: "{app}\bin\{#AppExeName}"; Tasks: desktopicon; IconFilename: "{app}\enjin.ico"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"

; ============================================================
; Post-install
; ============================================================

[Run]
Filename: "{app}\bin\{#AppExeName}"; Description: "Launch TEGE Editor"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\logs"
Type: filesandordirs; Name: "{app}\cache"

[Registry]
; Associate .enjinproject project manifests with the editor
Root: HKCU; Subkey: "Software\Classes\.enjinproject"; ValueType: string; ValueData: "EnjinProject"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\EnjinProject"; ValueType: string; ValueData: "TEGE Project"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\EnjinProject\DefaultIcon"; ValueType: string; ValueData: "{app}\enjin.ico"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\EnjinProject\shell\open\command"; ValueType: string; ValueData: """{app}\bin\{#AppExeName}"" ""%1"""; Flags: uninsdeletevalue

; Associate .enjin scene files with the editor
Root: HKCU; Subkey: "Software\Classes\.enjin"; ValueType: string; ValueData: "EnjinScene"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\EnjinScene"; ValueType: string; ValueData: "TEGE Scene"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\EnjinScene\DefaultIcon"; ValueType: string; ValueData: "{app}\enjin.ico"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\EnjinScene\shell\open\command"; ValueType: string; ValueData: """{app}\bin\{#AppExeName}"" ""%1"""; Flags: uninsdeletevalue

; Remove the stale .enjscene association left behind by installers <= 0.9.6
; (the engine never creates .enjscene files)
Root: HKCU; Subkey: "Software\Classes\.enjscene"; ValueType: none; Flags: deletekey
