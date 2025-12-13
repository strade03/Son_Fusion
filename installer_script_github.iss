; Script adapté pour GitHub Actions
#define MyAppName "Son Fusion"
#define MyAppVersion "2.1"
#define MyAppPublisher "Stéphane Verdier"
#define MySite "https://estrade03.forge.apps.education.fr/"
#define MyAppExeName "son_fusion.exe"
; Le dossier généré par le script YAML
#define SourceFolder "deploy" 

[Setup]
; ID Unique (Gardé le tien pour la compatibilité des mises à jour)
AppId={{680F2644-FAD6-4F6C-A28C-889CB9F836E2}

; Informations de base
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MySite}
AppSupportURL={#MySite}
AppUpdatesURL={#MySite}

; Installation
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
; Pas besoin de droits admin pour installer (installé dans AppData utilisateur)
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

; Sortie (Le fichier .exe sera créé à la racine du runner)
OutputDir=.
OutputBaseFilename=SonFusion_Setup_Windows

; Icône (Doit être présente dans ton dépôt git sous icones/fusionner.ico)
SetupIconFile=icones/fusionner.ico

; Compression (LZMA2 est bien meilleur que ZIP pour un installeur)
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

; Métadonnées du fichier exe
VersionInfoVersion={#MyAppVersion}
VersionInfoDescription=Application de fusion de fichiers audio
VersionInfoCopyright=© 2025 {#MyAppPublisher}
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoOriginalFileName={#MyAppExeName}

; Pages de l'assistant
DisableWelcomePage=no
DisableDirPage=no
DisableProgramGroupPage=yes
AllowNoIcons=yes

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; C'est ici que la magie opère : on prend tout ce que le YAML a mis dans 'deploy'
Source: "{#SourceFolder}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Comment: "Fusion de fichiers audio"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; Comment: "Fusion de fichiers audio"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: files; Name: "{app}\*.*"
Type: dirifempty; Name: "{app}"