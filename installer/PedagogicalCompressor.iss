; ============================================================================
;  PedagogicalCompressor.iss — Instalador Windows (InnoSetup 6)
;
;  Genera PedagogicalCompressor-Setup.exe que instala:
;    • VST3       → C:\Program Files\Common Files\VST3\  (estándar VST3)
;    • CLAP       → C:\Program Files\Common Files\CLAP\  (estándar CLAP)
;    • Standalone → C:\Program Files\PedagogicalCompressor\ + menú Inicio
;    • Desinstalador en "Agregar o quitar programas"
;
;  Compilar (local o CI):
;    iscc installer/PedagogicalCompressor.iss
;  El ejecutable queda en installer/Output/PedagogicalCompressor-Setup.exe
;
;  Los binarios se toman de build\PedagogicalCompressor_artefacts\Release\
;  (o de la ruta que se pase con /DBuildDir=...)
; ============================================================================

#ifndef BuildDir
#define BuildDir "build\PedagogicalCompressor_artefacts\Release"
#endif

[Setup]
AppId={{8F4B2C1D-9E7A-4C3B-B6D5-PEDACOMP001}
AppName=Pedagogical Compressor
AppVersion=1.2.4
AppVerName=Pedagogical Compressor 1.2.4
AppPublisher=PedagogicalAudio
AppPublisherURL=https://github.com/AlexHincapie77/AlexHincapie77_Compresi-n_Proyecto-Pedag-gico
DefaultDirName={autopf}\PedagogicalCompressor
DefaultGroupName=Pedagogical Compressor
UninstallDisplayIcon={app}\PedagogicalCompressor.exe
OutputDir=installer\Output
OutputBaseFilename=PedagogicalCompressor-Setup
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Standalone
Source: "{#BuildDir}\Standalone\Pedagogical Compressor.exe"; DestDir: "{app}"; DestName: "PedagogicalCompressor.exe"; Flags: ignoreversion
; VST3 (carpeta bundle completa)
Source: "{#BuildDir}\VST3\Pedagogical Compressor.vst3\*"; DestDir: "{commoncf64}\VST3\Pedagogical Compressor.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
; CLAP
Source: "{#BuildDir}\CLAP\Pedagogical Compressor.clap"; DestDir: "{commoncf64}\CLAP"; Flags: ignoreversion

[Icons]
Name: "{group}\Pedagogical Compressor"; Filename: "{app}\PedagogicalCompressor.exe"
Name: "{group}\{cm:UninstallProgram,Pedagogical Compressor}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Pedagogical Compressor"; Filename: "{app}\PedagogicalCompressor.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\PedagogicalCompressor.exe"; Description: "{cm:LaunchProgram,Pedagogical Compressor}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Al desinstalar, quitar también los plugins
Type: filesandordirs; Name: "{commoncf64}\VST3\Pedagogical Compressor.vst3"
Type: filesandordirs; Name: "{commoncf64}\CLAP\Pedagogical Compressor.clap"
