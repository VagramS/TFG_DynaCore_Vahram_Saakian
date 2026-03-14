; ============================================================
; DynaCore Windows Installer (Inno Setup 6+)
;
; Before building this installer:
; 1. Build DynaCore in Visual Studio (Release x64)
; 2. Place the built VST3 bundle in installer\win-artefacts\DynaCore.vst3\
; 3. Run Inno Setup Compiler on this .iss file
;
; Output: DynaCore-1.0.0-Windows-Setup.exe
; ============================================================

#define MyAppName      "DynaCore"
#define MyAppVersion   "1.0.0"
#define MyAppPublisher "Vahram Saakian — UCM"
#define MyAppURL       "https://github.com/VagramS/TFG_DynaCore_Vahram_Saakian"

[Setup]
AppId={{E7A3B2C1-4D5F-6A78-9B0C-D1E2F3A4B5C6}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputBaseFilename={#MyAppName}-{#MyAppVersion}-Windows-Setup
Compression=lzma2/ultra
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
LicenseFile=
UninstallDisplayIcon={app}\{#MyAppName}.ico
WizardStyle=modern
SetupIconFile=

[Types]
Name: "full"; Description: "Full installation (VST3)"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 Plugin"; Types: full custom

[Files]
; VST3 — install to system VST3 folder
Source: "win-artefacts\DynaCore.vst3\*"; DestDir: "{commoncf}\VST3\DynaCore.vst3"; \
  Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Messages]
WelcomeLabel1=Welcome to the {#MyAppName} Setup Wizard
WelcomeLabel2=This will install {#MyAppName} {#MyAppVersion} on your computer.%n%n{#MyAppName} is a multi-effect dynamics and modulation audio plugin.%n%nFormats included: VST3

[Code]
// Show post-install message
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssDone then
  begin
    MsgBox('{#MyAppName} {#MyAppVersion} has been installed successfully!' + #13#10 +
           #13#10 +
           'VST3: ' + ExpandConstant('{commoncf}') + '\VST3\DynaCore.vst3' + #13#10 +
           #13#10 +
           'Please rescan your plugins in your DAW.',
           mbInformation, MB_OK);
  end;
end;
