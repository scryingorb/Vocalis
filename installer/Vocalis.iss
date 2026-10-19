#define ProductName "Vocalis"
#ifndef ProductVersion
  #error "Use BuildInstaller.ps1 to supply the CMake product version and validate tests."
#endif
#define Publisher "Vocalis Audio"
#define ProjectRoot AddBackslash(SourcePath) + ".."
#ifndef BundleSource
  #error "Use BuildInstaller.ps1 to supply the validated bundle."
#endif
#define OutputName ProductName + "-" + ProductVersion + "-Setup"

[Setup]
; Permanent installer identity: NEVER regenerate for subsequent releases.
AppId={{745846CB-3E2F-4EB3-9D76-6B6B3469742A}
AppName={#ProductName}
AppVersion={#ProductVersion}
AppVerName={#ProductName} {#ProductVersion}
AppPublisher={#Publisher}
VersionInfoVersion={#ProductVersion}
DefaultDirName={commonpf64}\Vocalis
UsePreviousAppDir=no
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#ProjectRoot}\dist
OutputBaseFilename={#OutputName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableWelcomePage=no
DisableFinishedPage=no
InfoBeforeFile=Prerequisites.txt
LicenseFile={#ProjectRoot}\LICENSE.txt
Uninstallable=yes
UninstallDisplayName={#ProductName} {#ProductVersion}
UninstallFilesDir={app}
CloseApplications=yes
RestartApplications=no
; Signing integration point: configure an actual trusted SignTool in ISCC,
; then add SignTool=<configured-name> and SignedUninstaller=yes here.
; TODO: prepare approved BMP wizard artwork / ICO from Assets\VocalisLogo.png.
; Default Inno graphics are used until those files are available.

[Files]
; Copy CONTENTS into the one outer bundle; never nest Vocalis.vst3 twice.
Source: "{#BundleSource}\*"; DestDir: "{commoncf64}\VST3\Vocalis.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ProjectRoot}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#ProjectRoot}\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#ProjectRoot}\LICENSES\*"; DestDir: "{app}\LICENSES"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ProjectRoot}\docs\ReleaseChecklist.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#ProjectRoot}\docs\ReleaseCandidate.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#ProjectRoot}\docs\CurrentAudit.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#ProjectRoot}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion

[InstallDelete]
; Remove stale files ONLY inside this product's bundle on upgrade.
; Close DAWs before setup. This is not a transactional rollback of the old bundle.
Type: filesandordirs; Name: "{commoncf64}\VST3\Vocalis.vst3"

[UninstallDelete]
; Never remove the shared VST3 parent or any other plugin.
Type: filesandordirs; Name: "{commoncf64}\VST3\Vocalis.vst3"
; Installed docs are tracked by Inno; preserve any unrelated user-added files.
Type: dirifempty; Name: "{app}\docs"
Type: dirifempty; Name: "{app}\LICENSES"
Type: dirifempty; Name: "{app}"

[Run]
Filename: "{sys}\notepad.exe"; Parameters: """{app}\README.md"""; Description: "Open the Vocalis README"; Flags: postinstall unchecked nowait skipifsilent runasoriginaluser

[Messages]
WelcomeLabel2=This installs Vocalis {#ProductVersion} for Windows x64 VST3 hosts.%n%nClose FL Studio and all other audio hosts first.%n%nMicrosoft Visual C++ v14 x64 Redistributable is required. Setup does not download or install it.%n%nClick Next to review prerequisites.

