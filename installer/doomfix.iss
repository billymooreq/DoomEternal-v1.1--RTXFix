; doomfix installer (Inno Setup 6). Build it with scripts\build_installer.ps1, which passes:
;   /DAppVersion=0.3.1[-rc1]   version shown to the user and used in the file name
;   /DFileVersion=0.3.1        numeric part, for the version resource of setup.exe
;   /DBinDir=<folder>          ready doomfix_layer.dll, VK_LAYER_DOOMFIX.json and doomfix_launcher.exe
;   /DOutputDir=<folder>       where doomfix-<AppVersion>-setup.exe goes
;
; The files from BinDir are taken as they are, so they can be signed before this script runs.
; The artwork comes from installer\assets (generate.py, scripts\build_assets.ps1). /DUITest makes a test build
; for screenshots: it stops on the Installing page, installs only into a test folder (pass /DIR=...) and changes
; nothing else on the computer (no registry values, shortcuts or "Installed apps" entry). Never release one.
;
; Installs for the current user only, like scripts\install.ps1, and into the same folders, so an
; installation made with install.bat is replaced in place:
;   %LOCALAPPDATA%\doomfix\layer\    doomfix_layer.dll, VK_LAYER_DOOMFIX.json
;   %LOCALAPPDATA%\doomfix\loader\   vk_loader_settings.json (layer order)
;   %LOCALAPPDATA%\doomfix\          doomfix_launcher.exe, doomfix_game_path.txt, uninstaller, docs
;   %LOCALAPPDATA%\doomfix\logs\     written by the layer; left alone by the installer and the uninstaller
; Registry (HKCU only): the ImplicitLayers and LoaderSettings values of install.ps1 and the "Installed apps"
; entry. DOOMFIX_ENABLE is never stored for the user: only doomfix_launcher.exe sets it, for the game alone.
; The "Steam mode" of v0.3.1 (DOOMFIX_ENABLE=1 in HKCU\Environment, which turned the layer on in every Vulkan
; program, including games with anti-cheat) is removed by the installer and the uninstaller.

#ifndef AppVersion
  #error Pass /DAppVersion=x.y.z (scripts\build_installer.ps1 does that)
#endif
#ifndef FileVersion
  #define FileVersion AppVersion
#endif
#ifndef BinDir
  #error Pass /DBinDir=<folder with doomfix_layer.dll, VK_LAYER_DOOMFIX.json and doomfix_launcher.exe>
#endif
#ifndef OutputDir
  #define OutputDir AddBackslash(SourcePath) + "..\dist"
#endif
#define RootDir AddBackslash(SourcePath) + ".."
#define AssetsDir AddBackslash(SourcePath) + "assets"
#define RepoUrl "https://github.com/billymooreq/DoomEternal-v1.1--RTXFix"
#define ShortcutName "DOOM Eternal (doomfix)"

[Setup]
#ifndef UITest
; Never change AppId: it ties new versions to the existing installation.
AppId={{B3F23E39-9C8E-4D02-B62B-4A2E262905A6}
#else
; Screenshot builds only: a separate AppId and no uninstaller, so a real installation is never touched.
AppId={{0E5D3C1A-6F2B-4B7E-9C41-5A1E7E570000}
Uninstallable=no
CreateUninstallRegKey=no
UsePreviousAppDir=no
#endif
AppName=doomfix
AppVersion={#AppVersion}
AppVerName=doomfix {#AppVersion}
AppPublisher=billymooreq
AppPublisherURL={#RepoUrl}
AppSupportURL={#RepoUrl}/issues
AppUpdatesURL={#RepoUrl}/releases
AppCopyright=Copyright (c) 2026 billymooreq, MIT License
VersionInfoVersion={#FileVersion}
VersionInfoProductName=doomfix
VersionInfoDescription=doomfix Setup
VersionInfoCompany=billymooreq
; Current user only, no administrator rights: Windows does not load HKCU layers into elevated games.
PrivilegesRequired=lowest
#ifndef UITest
DefaultDirName={localappdata}\doomfix
#else
DefaultDirName={localappdata}\doomfix-uitest
#endif
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableWelcomePage=no
DisableReadyPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
; Look: the built-in dark style, fire on the whole window, the content on dark panels and drawn buttons (see the
; "Look" part of [Code]). The artwork is drawn by installer\assets\generate.py (scripts\build_assets.ps1).
WizardStyle=modern dark hidebevels
WizardBackColor=#1c1210
WizardImageFile=
WizardSmallImageFile=
SetupIconFile={#AssetsDir}\doomfix.ico
ShowLanguageDialog=auto
; Removing the old Steam mode variable changes the user environment: tell Explorer about it (Setup and the
; uninstaller both broadcast WM_SETTINGCHANGE).
ChangesEnvironment=yes
CloseApplications=yes
RestartApplications=no
UninstallDisplayName=doomfix (DOOM Eternal 1.1 fix)
UninstallDisplayIcon={app}\doomfix_launcher.exe
OutputDir={#OutputDir}
OutputBaseFilename=doomfix-{#AppVersion}-setup
Compression=lzma2/max
SolidCompression=yes

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[Messages]
en.WelcomeLabel2=This will install [name/ver] for your Windows user.%n%nClose DOOM Eternal before you continue.
ru.WelcomeLabel2=Будет установлен [name/ver] для вашего пользователя Windows.%n%nПеред продолжением закройте DOOM Eternal.

[CustomMessages]
; Text of the last page, set in CurPageChanged; the installation folder goes between the two lines.
en.FinishedInstalledTo=doomfix is installed to:
en.FinishedShortcut=Start the game with the "{#ShortcutName}" shortcut.
en.GamePageCaption=DOOM Eternal folder
en.GamePageDescription=Where is the game installed?
en.GamePageText=Setup looked for DOOM Eternal in your Steam libraries. If the folder below is empty or wrong, click Browse and choose the folder that contains DOOMEternalx64vk.exe.
en.GameNotFound=DOOMEternalx64vk.exe was not found in this folder:%n%1%n%nContinue anyway? The doomfix shortcut will ask for the game folder when you first start it.
en.GameNotChosen=No game folder is chosen.%n%nContinue anyway? The doomfix shortcut will ask for the game folder when you first start it.
en.ShortcutsGroup=Shortcuts:
en.TaskDesktopIcon=Create a desktop shortcut
en.LaunchGame=Start DOOM Eternal with doomfix
en.SetupElevated=Setup is running as administrator.%n%ndoomfix is installed for the current Windows user, and Windows does not load it into programs that run as administrator. If you chose "Run as administrator", it is better to close Setup and start it with a normal double-click.%n%nContinue anyway?
en.SetupUacOff=User Account Control (UAC) is turned off on this PC, so Setup and all other programs run as administrator.%n%nThat is fine: doomfix is installed for your Windows user as usual, and the doomfix shortcut starts the game without administrator rights, so the fix works. Pressing Play in Steam starts the game without the fix.
en.GameElevated=DOOM Eternal is running as administrator, so doomfix cannot work in it. Close the game and start it with the doomfix shortcut, not as administrator.
en.SteamModeRemoved=Steam mode from an older doomfix version was turned off. Launch the game with the doomfix shortcut. Restart Steam to apply.
en.CloseGame=DOOM Eternal is running. Close the game, then click Retry.
en.CloseGameAbort=DOOM Eternal is still running. Close the game and run Setup again.
en.CloseGameUninstall=DOOM Eternal is running. Close the game, then click Retry to uninstall doomfix.
en.ForeignLoaderSettings=Another Vulkan loader settings file is already registered (probably by Vulkan Configurator):%1%n%nThe loader uses only one such file, so doomfix leaves it alone and does not set the layer order. doomfix still works; only tools such as RenderDoc may end up above it. To change this, close Vulkan Configurator (or turn off its "Vulkan Loader Management") and run Setup again.
ru.FinishedInstalledTo=doomfix установлен в папку:
ru.FinishedShortcut=Запускайте игру ярлыком «{#ShortcutName}».
ru.GamePageCaption=Папка DOOM Eternal
ru.GamePageDescription=Где установлена игра?
ru.GamePageText=Установщик поискал DOOM Eternal в библиотеках Steam. Если поле ниже пустое или путь неверный, нажмите «Обзор» и выберите папку, в которой лежит DOOMEternalx64vk.exe.
ru.GameNotFound=В этой папке нет DOOMEternalx64vk.exe:%n%1%n%nВсё равно продолжить? Ярлык doomfix спросит папку игры при первом запуске.
ru.GameNotChosen=Папка игры не выбрана.%n%nВсё равно продолжить? Ярлык doomfix спросит папку игры при первом запуске.
ru.ShortcutsGroup=Ярлыки:
ru.TaskDesktopIcon=Создать ярлык на рабочем столе
ru.LaunchGame=Запустить DOOM Eternal с doomfix
ru.SetupElevated=Установщик запущен от имени администратора.%n%ndoomfix устанавливается для текущего пользователя Windows, а Windows не загружает его в программы, запущенные от имени администратора. Если вы выбрали «Запуск от имени администратора», лучше закрыть установщик и запустить его обычным двойным щелчком.%n%nВсё равно продолжить?
ru.SetupUacOff=На этом компьютере выключен контроль учётных записей (UAC), поэтому установщик и все остальные программы работают от имени администратора.%n%nЭто не помешает: doomfix устанавливается для вашего пользователя Windows как обычно, а ярлык doomfix запускает игру без прав администратора, поэтому исправление работает. Кнопка «Играть» в Steam запускает игру без исправления.
ru.GameElevated=DOOM Eternal запущен от имени администратора, поэтому doomfix в нём работать не может. Закройте игру и запустите её ярлыком doomfix, не от имени администратора.
ru.SteamModeRemoved=Режим Steam из старой версии doomfix выключен. Запускайте игру ярлыком doomfix. Перезапустите Steam, чтобы изменение вступило в силу.
ru.CloseGame=DOOM Eternal запущен. Закройте игру и нажмите «Повтор».
ru.CloseGameAbort=DOOM Eternal всё ещё запущен. Закройте игру и запустите установщик снова.
ru.CloseGameUninstall=DOOM Eternal запущен. Закройте игру и нажмите «Повтор», чтобы удалить doomfix.
ru.ForeignLoaderSettings=Уже зарегистрирован другой файл настроек загрузчика Vulkan (вероятно, от Vulkan Configurator):%1%n%nЗагрузчик использует только один такой файл, поэтому doomfix его не трогает и не задаёт порядок слоёв. doomfix всё равно работает; только инструменты вроде RenderDoc могут оказаться выше него. Чтобы это изменить, закройте Vulkan Configurator (или выключите в нём «Vulkan Loader Management») и запустите установщик снова.

[Tasks]
Name: "desktopicon"; Description: "{cm:TaskDesktopIcon}"; GroupDescription: "{cm:ShortcutsGroup}"

[InstallDelete]
; Left by install.ps1 when it was run on a build from source.
Type: files; Name: "{app}\layer\doomfix_layer.pdb"

[Files]
Source: "{#BinDir}\doomfix_layer.dll"; DestDir: "{app}\layer"; Flags: ignoreversion
Source: "{#BinDir}\VK_LAYER_DOOMFIX.json"; DestDir: "{app}\layer"; Flags: ignoreversion
Source: "{#BinDir}\doomfix_launcher.exe"; DestDir: "{app}"; Flags: ignoreversion
; The same launcher, unpacked to {tmp} before installing to check whether the game is running.
Source: "{#BinDir}\doomfix_launcher.exe"; Flags: dontcopy
Source: "{#RootDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RootDir}\README.ru.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RootDir}\PRIVACY.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RootDir}\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
; Wizard backgrounds for 100 to 200% DPI, loaded in [Code] (welcome/finish pages and the other pages differ).
Source: "{#AssetsDir}\back_welcome_*.png"; Flags: dontcopy
Source: "{#AssetsDir}\back_inner_*.png"; Flags: dontcopy
; Buttons, the path field and the heading lines, one set per background size (see generate.py).
Source: "{#AssetsDir}\ui\*.png"; Flags: dontcopy

#ifndef UITest
[Icons]
Name: "{autoprograms}\{#ShortcutName}"; Filename: "{app}\doomfix_launcher.exe"; WorkingDir: "{app}"; IconFilename: "{code:GetShortcutIcon}"; Comment: "{cm:LaunchGame}"
Name: "{autodesktop}\{#ShortcutName}"; Filename: "{app}\doomfix_launcher.exe"; WorkingDir: "{app}"; IconFilename: "{code:GetShortcutIcon}"; Comment: "{cm:LaunchGame}"; Tasks: desktopicon
#endif

[Run]
#ifndef UITest
Filename: "{app}\doomfix_launcher.exe"; Description: "{cm:LaunchGame}"; Flags: postinstall nowait skipifsilent
#else
; The same check box, but it never starts the game.
Filename: "{cmd}"; Parameters: "/c exit"; Description: "{cm:LaunchGame}"; Flags: postinstall nowait skipifsilent runhidden
#endif

[UninstallDelete]
; Everything doomfix wrote except logs\ (scripts\uninstall.ps1 keeps the logs too).
Type: filesandordirs; Name: "{app}\layer"
Type: filesandordirs; Name: "{app}\loader"
Type: files; Name: "{app}\doomfix_game_path.txt"

[Code]
const
  GameExe = 'DOOMEternalx64vk.exe';
  LayerManifest = 'VK_LAYER_DOOMFIX.json';
  LoaderSettingsFile = 'vk_loader_settings.json';
  ImplicitLayersKey = 'SOFTWARE\Khronos\Vulkan\ImplicitLayers';
  LoaderSettingsKey = 'SOFTWARE\Khronos\Vulkan\LoaderSettings';
  // The user variable of the old Steam mode (v0.3.1 enable_steam_mode.bat).
  EnvironmentKey = 'Environment';
  SteamModeVariable = 'DOOMFIX_ENABLE';
  GamePathFile = 'doomfix_game_path.txt';
  // Exit code flags of "doomfix_launcher.exe --check-processes" (src/launcher/launcher.cpp).
  GameRunning = 1;
  GameElevated = 2;
  SelfElevated = 4;
  UacOff = 8;

var
  GamePage: TInputDirWizardPage;
  FinishedLayoutDone, TasksLayoutDone: Boolean;
  WelcomeBack, InnerBack: TArrayOfGraphic;
  ShownBack: Integer;

// --- Helpers -----------------------------------------------------------------------------------

function IsGameDir(const Dir: String): Boolean;
begin
  Result := (Dir <> '') and FileExists(AddBackslash(Dir) + GameExe);
end;

function SameFileName(const A, B: String): Boolean;
begin
  Result := CompareText(A, B) = 0;
end;

function JsonEscape(const S: String): String;
begin
  Result := S;
  StringChangeEx(Result, '\', '\\', True);
  StringChangeEx(Result, '"', '\"', True);
end;

// Runs the launcher's process check; 0 if it cannot run.
function CheckProcesses(const Launcher: String): Integer;
var
  Code: Integer;
begin
  Result := 0;
  if FileExists(Launcher) and
     Exec(Launcher, '--check-processes', '', SW_SHOWNORMAL, ewWaitUntilTerminated, Code) and
     (Code >= 0) and (Code < 32) then
    Result := Code;
end;

function SetupProcessFlags: Integer;
begin
  Result := CheckProcesses(ExpandConstant('{tmp}\doomfix_launcher.exe'));
end;

// True if the old Steam mode variable was there and is now removed.
function RemoveOldSteamMode: Boolean;
begin
  Result := RegValueExists(HKCU, EnvironmentKey, SteamModeVariable) and
    RegDeleteValue(HKCU, EnvironmentKey, SteamModeVariable);
end;

// --- Game folder ---------------------------------------------------------------------------------

procedure AddSteamLibraries(const Root: String; var Libraries: TStringList);
var
  Vdf: AnsiString;
  Lines: TStringList;
  I, Q1, Q2: Integer;
  Line, Path: String;
begin
  if Root = '' then Exit;
  Libraries.Add(Root);
  // Entries look like:  "path"   "D:\\SteamLibrary". The file is UTF-8 without a BOM.
  if not LoadStringFromFile(AddBackslash(Root) + 'steamapps\libraryfolders.vdf', Vdf) then Exit;
  Lines := TStringList.Create;
  try
    Lines.Text := UTF8Decode(Vdf);
    for I := 0 to Lines.Count - 1 do begin
      Line := Trim(Lines[I]);
      if Pos('"path"', Line) <> 1 then Continue;
      Line := Trim(Copy(Line, 7, MaxInt));
      Q1 := Pos('"', Line);
      if Q1 <> 1 then Continue;
      Line := Copy(Line, 2, MaxInt);
      Q2 := Pos('"', Line);
      if Q2 = 0 then Continue;
      Path := Copy(Line, 1, Q2 - 1);
      StringChangeEx(Path, '\\', '\', True);
      Libraries.Add(Path);
    end;
  finally
    Lines.Free;
  end;
end;

function SteamRoot(const RootKey: Integer; const SubKey, Name: String): String;
begin
  if not RegQueryStringValue(RootKey, SubKey, Name, Result) then Result := '';
  StringChangeEx(Result, '/', '\', True);
end;

// The same search as scripts\launch.ps1: <library>\steamapps\common\DOOMEternal in every Steam library.
function FindGameInSteam: String;
var
  Libraries: TStringList;
  I: Integer;
begin
  Result := '';
  Libraries := TStringList.Create;
  try
    AddSteamLibraries(SteamRoot(HKCU, 'Software\Valve\Steam', 'SteamPath'), Libraries);
    AddSteamLibraries(SteamRoot(HKLM32, 'SOFTWARE\Valve\Steam', 'InstallPath'), Libraries);
    AddSteamLibraries(SteamRoot(HKLM64, 'SOFTWARE\Valve\Steam', 'InstallPath'), Libraries);
    for I := 0 to Libraries.Count - 1 do
      if IsGameDir(AddBackslash(Libraries[I]) + 'steamapps\common\DOOMEternal') then begin
        Result := AddBackslash(Libraries[I]) + 'steamapps\common\DOOMEternal';
        Exit;
      end;
  finally
    Libraries.Free;
  end;
end;

// /GAMEDIR=<folder> on the command line, then the folder saved by an earlier installation, then Steam.
function InitialGameDir: String;
var
  Lines: TArrayOfString;
begin
  Result := ExpandConstant('{param:GAMEDIR}');
  if Result <> '' then Exit;
  if LoadStringsFromFile(AddBackslash(WizardDirValue) + GamePathFile, Lines) and (GetArrayLength(Lines) > 0) and
     IsGameDir(Trim(Lines[0])) then begin
    Result := Trim(Lines[0]);
    Exit;
  end;
  Result := FindGameInSteam;
end;

function GameDir: String;
begin
  Result := RemoveBackslashUnlessRoot(Trim(GamePage.Values[0]));
end;

function GetShortcutIcon(Param: String): String;
begin
  if IsGameDir(GameDir) then
    Result := AddBackslash(GameDir) + GameExe
  else
    Result := ExpandConstant('{app}\doomfix_launcher.exe');
end;

// --- Layer registration (the same as scripts\install.ps1) ---------------------------------------

function InstalledManifest: String;
begin
  Result := ExpandConstant('{app}\layer\') + LayerManifest;
end;

function InstalledLoaderSettings: String;
begin
  Result := ExpandConstant('{app}\loader\') + LoaderSettingsFile;
end;

// Removes doomfix registrations from other folders, or all of them when Keep is empty.
procedure RemoveLayerRegistrations(const Keep: String);
var
  Names: TArrayOfString;
  I: Integer;
begin
  if not RegGetValueNames(HKCU, ImplicitLayersKey, Names) then Exit;
  for I := 0 to GetArrayLength(Names) - 1 do
    if SameFileName(ExtractFileName(Names[I]), LayerManifest) and not SameFileName(Names[I], Keep) then
      RegDeleteValue(HKCU, ImplicitLayersKey, Names[I]);
end;

procedure RegisterLayer;
begin
  RemoveLayerRegistrations(InstalledManifest);
  RegWriteDWordValue(HKCU, ImplicitLayersKey, InstalledManifest, 0);
end;

// Layer order: doomfix first (closest to the game), then everything else. See scripts\install.ps1.
procedure WriteLoaderSettings;
var
  Names, Json: TArrayOfString;
  I: Integer;
  Foreign: String;
begin
  Foreign := '';
  if RegGetValueNames(HKCU, LoaderSettingsKey, Names) then
    for I := 0 to GetArrayLength(Names) - 1 do
      if SameFileName(ExtractFileName(Names[I]), LoaderSettingsFile) and
         not SameFileName(Names[I], InstalledLoaderSettings) and FileExists(Names[I]) then
        Foreign := Foreign + #13#10 + '  ' + Names[I];
  if Foreign <> '' then begin
    SuppressibleMsgBox(FmtMessage(CustomMessage('ForeignLoaderSettings'), [Foreign]), mbInformation, MB_OK, IDOK);
    Exit;
  end;

  SetArrayLength(Json, 18);
  Json[0] := '{';
  Json[1] := '  "file_format_version": "1.0.1",';
  Json[2] := '  "settings_array": [';
  Json[3] := '    {';
  Json[4] := '      "layers": [';
  Json[5] := '        {';
  Json[6] := '          "control": "auto",';
  Json[7] := '          "name": "VK_LAYER_DOOMFIX",';
  Json[8] := '          "path": "' + JsonEscape(InstalledManifest) + '",';
  Json[9] := '          "treat_as_implicit_manifest": true';
  Json[10] := '        },';
  Json[11] := '        {';
  Json[12] := '          "control": "unordered_layer_location"';
  Json[13] := '        }';
  Json[14] := '      ]';
  Json[15] := '    }';
  Json[16] := '  ]';
  Json[17] := '}';
  ForceDirectories(ExtractFileDir(InstalledLoaderSettings));
  SaveStringsToUTF8FileWithoutBOM(InstalledLoaderSettings, Json, False);
  RegWriteDWordValue(HKCU, LoaderSettingsKey, InstalledLoaderSettings, 0);
end;

procedure SaveGameDir;
var
  Lines: TArrayOfString;
begin
  if not IsGameDir(GameDir) then Exit;
  SetArrayLength(Lines, 1);
  Lines[0] := GameDir;
  SaveStringsToUTF8File(ExpandConstant('{app}\') + GamePathFile, Lines, False);
end;

// --- Setup events ------------------------------------------------------------------------------

function InitializeSetup: Boolean;
var
  Flags: Integer;
begin
  Result := True;
  ExtractTemporaryFile('doomfix_launcher.exe');
  Flags := SetupProcessFlags;
  // With UAC off every program runs as administrator; the launcher lowers the game's rights itself.
  if ((Flags and SelfElevated) <> 0) and ((Flags and UacOff) <> 0) then
    SuppressibleMsgBox(CustomMessage('SetupUacOff'), mbInformation, MB_OK, IDOK)
  else if ((Flags and SelfElevated) <> 0) and
     (SuppressibleMsgBox(CustomMessage('SetupElevated'), mbConfirmation, MB_YESNO or MB_DEFBUTTON2, IDYES) <> IDYES) then begin
    Result := False;
    Exit;
  end;
  if (Flags and GameElevated) <> 0 then
    SuppressibleMsgBox(CustomMessage('GameElevated'), mbInformation, MB_OK, IDOK);
end;

// --- Look -------------------------------------------------------------------------------------------
// The approved design: fire on the whole window, the text on dark see-through panels with chamfered corners, and
// hexagonal buttons that light up red under the mouse. The panels are part of the background images
// (installer\assets\generate.py): the custom style draws transparent controls over the window background, so only
// there do they show behind every control. Labels and the page content are placed in units of the 497x360 design,
// scaled to the client area that the background images fill; buttons and the path field in pixels of the 596x432
// client area at 100% DPI.
//
// Buttons. Inno Setup has no buttons with a hover look, and no mouse events for script controls. So every button
// is drawn as a picture (TBitmapImage, one PNG per state from installer\assets\ui, the caption is written on it here)
// and a timer checks the mouse a few dozen times a second. The real buttons stay on the form with a size of 0x0:
// they keep doing everything a button does from the keyboard (Enter, Esc, Tab, Alt+letter), and Setup keeps
// changing their captions, visibility and state as usual; the pictures follow them. A click on a picture is sent
// to its real button as an ordinary button click (WM_COMMAND, BN_CLICKED).

const
  DesignW = 497;
  DesignH = 360;
  BackWelcome = 1;
  BackInner = 2;
  UiBaseW = 596;            // client width at 100% DPI
  WM_COMMAND = $0111;
  WM_QUERYUISTATE = $0129;
  UISF_HIDEACCEL = 2;
  VK_LBUTTON = 1;
  VK_TAB = 9;
  StNormal = 0;
  StHover = 1;
  StPressed = 2;
  StFocus = 3;
  StDisabled = 4;
  TextColor = $DFE2E9;      // #E9E2DF
  FieldTextColor = $E5E8EF; // #EFE8E5
  FieldColor = $0E0E12;     // #120E0E

type
  TPngList = array of TPngImage;
  TUiButton = record
    Real: TNewButton;
    Image: TBitmapImage;
    Kind: String;      // 'button' or 'browse': the file names in installer\assets\ui
    Caption: String;   // the caption of Real the faces were drawn with
    Accel: Boolean;    // whether the faces show the underlined access key
    State: Integer;    // the face shown, -1 = none yet
    Faces: TPngList;   // one per state
  end;

  // A check box of a TNewCheckListBox (the shortcut task, "Start the game"): the list stays as 0x0 for the
  // keyboard (Tab, Space), and a picture with a red tick and a label show the item.
  TUiCheck = record
    List: TNewCheckListBox;
    Index: Integer;
    Image: TBitmapImage;
    Text: TNewStaticText;
    Face: Integer;     // index in CheckFaces, -1 = none yet
  end;

var
  UiChecks: array of TUiCheck;
  CheckFaces: TPngList; // off: normal, hover, focus; on: normal, hover, focus
  UiButtons: array of TUiButton;
  UiSet: String;       // the background width the ui images were made for
  UiScale: Extended;   // client pixels per pixel at 100% DPI
  UiTimer: Longword;
  // The focus look of the pictures appears only after Tab on the current page: unlike the Windows setting for
  // normal buttons, a page never opens with a focused-looking button, even when it was opened with the keyboard.
  TabUsed: Boolean;
  WelcomeLine, FinishedLine: TBitmapImage;

function GetCursorPos(var Point: TPoint): BOOL;
  external 'GetCursorPos@user32.dll stdcall';
function ScreenToClient(Wnd: HWND; var Point: TPoint): BOOL;
  external 'ScreenToClient@user32.dll stdcall';
function WindowFromPoint(X, Y: Longint): HWND;
  external 'WindowFromPoint@user32.dll stdcall';
function GetAsyncKeyState(Key: Integer): Integer;
  external 'GetAsyncKeyState@user32.dll stdcall';
function SetTimer(Wnd: HWND; IDEvent, Elapse, TimerFunc: Longword): Longword;
  external 'SetTimer@user32.dll stdcall';
function KillTimer(Wnd: HWND; IDEvent: Longword): BOOL;
  external 'KillTimer@user32.dll stdcall';
function GetForegroundWindow: HWND;
  external 'GetForegroundWindow@user32.dll stdcall';

function DX(V: Integer): Integer;
begin
  Result := Round(V * WizardForm.ClientWidth / DesignW);
end;

function DY(V: Integer): Integer;
begin
  Result := Round(V * WizardForm.ClientHeight / DesignH);
end;

// Client coordinates of the pages (WelcomePage, InnerPage, FinishedPage) start at the outer notebook.
function PX(V: Integer): Integer;
begin
  Result := DX(V) - WizardForm.OuterNotebook.Left;
end;

function PY(V: Integer): Integer;
begin
  Result := DY(V) - WizardForm.OuterNotebook.Top;
end;

// Pixels at 100% DPI to client pixels.
function UX(V: Extended): Integer;
begin
  Result := Round(V * UiScale);
end;

// Capital letters in any language. AnsiUppercase goes through the ANSI code page (Cyrillic becomes question marks),
// Uppercase only handles a-z, and non-ASCII string literals in [Code] are not Unicode. The Windows API converts the
// string buffer in place.
function CharUpperBuff(Buf: String; Len: DWORD): DWORD;
  external 'CharUpperBuffW@user32.dll stdcall';

function UpperText(const S: String): String;
begin
  Result := S;
  if Result <> '' then
    CharUpperBuff(Result, Length(Result));
end;

procedure LoadBack(const Kind: String; var Images: TArrayOfGraphic);
var
  Sizes: TArrayOfString;
  I: Integer;
  Name: String;
begin
  Sizes := ['596x432', '796x576', '994x720', '1193x864', '1272x922'];
  SetLength(Images, GetArrayLength(Sizes));
  for I := 0 to GetArrayLength(Sizes) - 1 do begin
    Name := 'back_' + Kind + '_' + Sizes[I] + '.png';
    ExtractTemporaryFile(Name);
    Images[I] := TPngImage.Create;
    Images[I].LoadFromFile(ExpandConstant('{tmp}\') + Name);
  end;
end;

procedure ShowBack(Which: Integer);
begin
  if Which = ShownBack then Exit;
  ShownBack := Which;
  if Which = BackWelcome then
    WizardSetBackImage(WelcomeBack, True, True, 255)
  else
    WizardSetBackImage(InnerBack, True, True, 255);
end;

// The ui images made for the background width closest to the client width (WizardSetBackImage picks the
// background the same way and stretches it to the client area).
procedure ChooseUiSet;
var
  Widths: TArrayOfString;
  I, Best: Integer;
begin
  Widths := ['596', '796', '994', '1193', '1272'];
  Best := 0;
  for I := 1 to GetArrayLength(Widths) - 1 do
    if Abs(StrToInt(Widths[I]) - WizardForm.ClientWidth) < Abs(StrToInt(Widths[Best]) - WizardForm.ClientWidth) then
      Best := I;
  UiSet := Widths[Best];
  UiScale := WizardForm.ClientWidth / UiBaseW;
end;

function LoadUiPng(const Name: String): TPngImage;
var
  FileName: String;
begin
  FileName := ExpandConstant('{tmp}\') + Name + '_' + UiSet + '.png';
  if not FileExists(FileName) then
    ExtractTemporaryFile(ExtractFileName(FileName));
  Result := TPngImage.Create;
  Result.LoadFromFile(FileName);
end;

function UiImage(AParent: TWinControl; const Name: String; ALeft, ATop: Integer): TBitmapImage;
begin
  Result := TBitmapImage.Create(WizardForm);
  Result.Parent := AParent;
  Result.BackColor := clNone;  // transparent: the pointed ends and chamfers show the background
  Result.PngImage := LoadUiPng(Name);
  Result.SetBounds(ALeft, ATop, Result.PngImage.Width, Result.PngImage.Height);
end;

procedure PlaceOnPage(C: TControl; L, T, R, B: Integer);
begin
  C.Left := PX(L);
  C.Top := PY(T);
  C.Width := PX(R) - PX(L);
  C.Height := PY(B) - PY(T);
end;

// Body text: 13 px, warm white. Without seFont the style would keep its own text color.
procedure StyleText(C: TControl);
begin
  if C is TNewStaticText then begin
    TNewStaticText(C).Font.Height := -UX(13);
    TNewStaticText(C).Font.Color := TextColor;
  end else if C is TNewCheckListBox then begin
    TNewCheckListBox(C).Font.Height := -UX(13);
    TNewCheckListBox(C).Font.Color := TextColor;
  end else
    Exit;
  C.StyleElements := C.StyleElements - [seFont];
end;

procedure StyleAllText(Parent: TWinControl);
var
  I: Integer;
begin
  for I := 0 to Parent.ControlCount - 1 do begin
    StyleText(Parent.Controls[I]);
    if Parent.Controls[I] is TWinControl then
      StyleAllText(TWinControl(Parent.Controls[I]));
  end;
end;

// A bold white heading in capitals on one line above the line of the panel: the font gets smaller if the text
// does not fit. (Labels cannot space their letters, so unlike the buttons the heading has normal spacing.)
procedure SetHeading(L: TNewStaticText; const Caption: String; MaxWidth: Integer);
begin
  L.Caption := UpperText(Caption);
  L.Font.Style := [fsBold];
  L.Font.Color := clWhite;
  L.StyleElements := L.StyleElements - [seFont];
  L.Font.Height := -UX(16);
  L.WordWrap := False;
  L.AutoSize := True;
  while (L.Width > MaxWidth) and (L.Font.Height < -UX(11)) do
    L.Font.Height := L.Font.Height + 1;
  L.AutoSize := False;
  L.Width := MaxWidth;
end;

// Welcome and finish pages: the large logo in the background, a panel from (40,104) to (455,279) with the heading
// line at y = 144 (generate.py: WELCOME_PANEL, WELCOME_SEPARATOR_Y). The line is a picture: red on the welcome
// page, green on the finish page.
procedure PlaceWelcomeTitle(Title: TNewStaticText);
begin
  Title.Left := PX(56);
  Title.Top := PY(118);
  SetHeading(Title, Title.Caption, DX(439) - DX(56));
end;

procedure LayoutWelcomePage;
begin
  WizardForm.WizardBitmapImage.Visible := False;
  PlaceOnPage(WizardForm.WelcomeLabel1, 56, 118, 439, 140);
  PlaceWelcomeTitle(WizardForm.WelcomeLabel1);
  PlaceOnPage(WizardForm.WelcomeLabel2, 56, 154, 439, 270);
  WelcomeLine := UiImage(WizardForm.WelcomePage, 'line_red', PX(56), PY(144));
end;

// Other pages: the small logo in the background; title, description and content in the panel from (18,50) to
// (479,306) with the line at y = 102 (generate.py: INNER_PANEL, INNER_SEPARATOR_Y).
procedure LayoutInnerPages;
begin
  WizardForm.PageNameLabel.Parent := WizardForm.InnerPage;
  WizardForm.PageDescriptionLabel.Parent := WizardForm.InnerPage;
  WizardForm.MainPanel.Visible := False;
  PlaceOnPage(WizardForm.InnerNotebook, 34, 112, 463, 298);
  PlaceOnPage(WizardForm.PageNameLabel, 34, 62, 463, 82);
  PlaceOnPage(WizardForm.PageDescriptionLabel, 34, 82, 463, 100);
end;

// The text of a caption without the "<" and ">" arrows; Accel = the position of the access key in it (after "&"),
// or 0.
function ButtonText(const Caption: String; var Accel: Integer): String;
var
  I: Integer;
  S: String;
begin
  S := Trim(Caption);
  if (S <> '') and (S[1] = '<') then S := Trim(Copy(S, 2, MaxInt));
  if (S <> '') and (S[Length(S)] = '>') then S := Trim(Copy(S, 1, Length(S) - 1));
  Result := '';
  Accel := 0;
  I := 1;
  while I <= Length(S) do begin
    if (S[I] = '&') and (I < Length(S)) then begin
      I := I + 1;
      if (S[I] <> '&') and (Accel = 0) then Accel := Length(Result) + 1;
    end;
    Result := Result + S[I];
    I := I + 1;
  end;
  Result := UpperText(Result);
end;

function SpacedWidth(C: TCanvas; const Text: String; Spacing: Integer): Integer;
var
  I: Integer;
begin
  Result := 0;
  for I := 1 to Length(Text) do
    Result := Result + C.TextWidth(Text[I]) + Spacing;
  Result := Result - Spacing;
end;

// Writes the caption on a face: 13 px bold capitals with 1.5 px between the letters, centered. A caption that does
// not fit between the pointed ends (10 px each) gets less spacing first, then a smaller font.
procedure DrawCaption(Face: TPngImage; const Text: String; Accel: Integer; Color: TColor);
var
  C: TCanvas;
  I, X, Y, Total, Spacing, W, H, Room: Integer;
begin
  C := Face.Canvas;
  C.Brush.Style := bsClear;
  C.Font.Name := 'Segoe UI';
  C.Font.Style := [fsBold];
  C.Font.Height := -UX(13);
  C.Font.Color := Color;
  Spacing := UX(1.5);
  Room := Face.Width - 2 * UX(14);
  Total := SpacedWidth(C, Text, Spacing);
  while (Total > Room) and (Spacing > 0) do begin
    Spacing := Spacing - 1;
    Total := SpacedWidth(C, Text, Spacing);
  end;
  while (Total > Room) and (C.Font.Height < -UX(10)) do begin
    C.Font.Height := C.Font.Height + 1;
    Total := SpacedWidth(C, Text, Spacing);
  end;
  H := C.TextHeight(Text);
  X := (Face.Width - Total) div 2;
  Y := (Face.Height - H) div 2;
  for I := 1 to Length(Text) do begin
    W := C.TextWidth(Text[I]);
    C.TextOut(X, Y, Text[I]);
    if I = Accel then begin
      C.Pen.Color := Color;
      C.Pen.Width := 1;
      C.MoveTo(X, Y + H - UX(2));
      C.LineTo(X + W, Y + H - UX(2));
    end;
    X := X + W + Spacing;
  end;
end;

procedure DrawFaces(Index: Integer; ShowAccel: Boolean);
var
  States: TArrayOfString;
  Text: String;
  I, Accel: Integer;
  Color: TColor;
begin
  States := ['normal', 'hover', 'pressed', 'focus', 'disabled'];
  Text := ButtonText(UiButtons[Index].Real.Caption, Accel);
  if not ShowAccel then Accel := 0;
  for I := 0 to GetArrayLength(States) - 1 do begin
    if UiButtons[Index].Faces[I] <> nil then
      UiButtons[Index].Faces[I].Free;
    UiButtons[Index].Faces[I] := LoadUiPng(UiButtons[Index].Kind + '_' + States[I]);
    if (I = StHover) or (I = StPressed) then Color := clWhite else Color := TextColor;
    DrawCaption(UiButtons[Index].Faces[I], Text, Accel, Color);
  end;
  UiButtons[Index].Caption := UiButtons[Index].Real.Caption;
  UiButtons[Index].Accel := ShowAccel;
  UiButtons[Index].State := -1;
end;

procedure UiButtonClick(Sender: TObject);
var
  I: Integer;
  R: TNewButton;
begin
  for I := 0 to GetArrayLength(UiButtons) - 1 do
    if TObject(UiButtons[I].Image) = Sender then begin
      R := UiButtons[I].Real;
      if R.Visible and R.Enabled then
        SendMessage(R.Parent.Handle, WM_COMMAND, 0, R.Handle);  // BN_CLICKED from the real button
      Exit;
    end;
end;

// Replaces a real button with its picture at the given place.
procedure AddUiButton(Real: TNewButton; const Kind: String; ALeft, ATop: Integer);
var
  I: Integer;
begin
  I := GetArrayLength(UiButtons);
  SetLength(UiButtons, I + 1);
  UiButtons[I].Real := Real;
  UiButtons[I].Kind := Kind;
  SetLength(UiButtons[I].Faces, 5);
  UiButtons[I].Image := UiImage(Real.Parent, Kind + '_normal', ALeft, ATop);
  UiButtons[I].Image.OnClick := @UiButtonClick;
  UiButtons[I].Image.OnDblClick := @UiButtonClick;
  Real.SetBounds(ALeft, ATop, 0, 0);
  DrawFaces(I, False);
end;

procedure UpdateUiButton(Index: Integer; Mouse: TPoint; Over: HWND; Down: Boolean; UiState: Integer);
var
  R: TNewButton;
  Img: TBitmapImage;
  P: TPoint;
  Hot: Boolean;
  St: Integer;
begin
  R := UiButtons[Index].Real;
  Img := UiButtons[Index].Image;
  if (R.Width <> 0) or (R.Height <> 0) then
    R.SetBounds(Img.Left, Img.Top, 0, 0);
  if Img.Visible <> R.Visible then
    Img.Visible := R.Visible;
  if (R.Caption <> UiButtons[Index].Caption) or (((UiState and UISF_HIDEACCEL) = 0) <> UiButtons[Index].Accel) then
    DrawFaces(Index, (UiState and UISF_HIDEACCEL) = 0);
  if not R.Visible then Exit;

  if not R.Enabled then
    St := StDisabled
  else begin
    Hot := False;
    if Over = Img.Parent.Handle then begin
      P := Mouse;
      ScreenToClient(Over, P);
      Hot := (P.X >= Img.Left) and (P.X < Img.Left + Img.Width) and (P.Y >= Img.Top) and (P.Y < Img.Top + Img.Height);
    end;
    if Hot and Down then
      St := StPressed
    else if Hot then
      St := StHover
    else if R.Focused and TabUsed then
      St := StFocus
    else
      St := StNormal;
  end;
  if St <> UiButtons[Index].State then begin
    UiButtons[Index].State := St;
    Img.PngImage := UiButtons[Index].Faces[St];
  end;
end;

procedure LoadCheckFaces;
var
  Names: TArrayOfString;
  I: Integer;
begin
  Names := ['check_off_normal', 'check_off_hover', 'check_off_focus', 'check_on_normal', 'check_on_hover',
    'check_on_focus'];
  SetLength(CheckFaces, GetArrayLength(Names));
  for I := 0 to GetArrayLength(Names) - 1 do
    CheckFaces[I] := LoadUiPng(Names[I]);
end;

procedure UiCheckClick(Sender: TObject);
var
  I: Integer;
  L: TNewCheckListBox;
begin
  for I := 0 to GetArrayLength(UiChecks) - 1 do
    if (TObject(UiChecks[I].Image) = Sender) or (TObject(UiChecks[I].Text) = Sender) then begin
      L := UiChecks[I].List;
      if L.Enabled and L.ItemEnabled[UiChecks[I].Index] then begin
        L.Checked[UiChecks[I].Index] := not L.Checked[UiChecks[I].Index];
        L.ItemIndex := UiChecks[I].Index;
      end;
      Exit;
    end;
end;

// Shows the items of a check list box as pictures and labels in its place; GroupCaption marks the group item
// (shown as plain text). The list itself becomes 0x0 but keeps the keyboard.
procedure ReplaceCheckList(List: TNewCheckListBox; const GroupCaption: String);
var
  I, N, Y, Box, Row: Integer;
  L: TNewStaticText;
begin
  if List.Items.Count = 0 then Exit;
  Box := CheckFaces[0].Width;
  Row := UX(22);
  Y := List.Top;
  for I := 0 to List.Items.Count - 1 do begin
    L := TNewStaticText.Create(WizardForm);
    L.Parent := List.Parent;
    StyleText(L);
    L.Caption := List.ItemCaption[I];
    L.Top := Y + (Row - L.Height) div 2;
    if (GroupCaption <> '') and (List.ItemCaption[I] = GroupCaption) then
      L.Left := List.Left
    else begin
      N := GetArrayLength(UiChecks);
      SetLength(UiChecks, N + 1);
      UiChecks[N].List := List;
      UiChecks[N].Index := I;
      UiChecks[N].Face := -1;
      UiChecks[N].Text := L;
      UiChecks[N].Image := TBitmapImage.Create(WizardForm);
      UiChecks[N].Image.Parent := List.Parent;
      UiChecks[N].Image.BackColor := clNone;
      UiChecks[N].Image.SetBounds(List.Left, Y + (Row - Box) div 2, Box, Box);
      UiChecks[N].Image.OnClick := @UiCheckClick;
      UiChecks[N].Image.OnDblClick := @UiCheckClick;
      L.Left := List.Left + Box + UX(8);
      L.OnClick := @UiCheckClick;
      L.OnDblClick := @UiCheckClick;
      if List.ItemIndex < 0 then List.ItemIndex := I;
    end;
    Y := Y + Row;
  end;
  List.SetBounds(List.Left, List.Top, 0, 0);
end;

function Inside(C: TControl; P: TPoint): Boolean;
begin
  Result := (P.X >= C.Left) and (P.X < C.Left + C.Width) and (P.Y >= C.Top) and (P.Y < C.Top + C.Height);
end;

procedure UpdateUiCheck(Index: Integer; Mouse: TPoint; Over: HWND; UiState: Integer);
var
  L: TNewCheckListBox;
  Img: TBitmapImage;
  P: TPoint;
  Face: Integer;
begin
  L := UiChecks[Index].List;
  Img := UiChecks[Index].Image;
  Face := 0;
  if L.Checked[UiChecks[Index].Index] then Face := 3;
  P := Mouse;
  if Over = Img.Parent.Handle then
    ScreenToClient(Over, P);
  if (Over = Img.Parent.Handle) and (Inside(Img, P) or Inside(UiChecks[Index].Text, P)) then
    Face := Face + 1
  else if L.Focused and TabUsed then
    Face := Face + 2;
  if Face <> UiChecks[Index].Face then begin
    UiChecks[Index].Face := Face;
    Img.PngImage := CheckFaces[Face];
  end;
end;

procedure UiTick(Wnd, Msg, IDEvent, Time: Longword);
var
  I, UiState: Integer;
  Mouse: TPoint;
  Over: HWND;
  Down: Boolean;
begin
  try
    if not GetCursorPos(Mouse) then Exit;
    Over := WindowFromPoint(Mouse.X, Mouse.Y);
    Down := (GetAsyncKeyState(VK_LBUTTON) and $8000) <> 0;
    if ((GetAsyncKeyState(VK_TAB) and $8001) <> 0) and (GetForegroundWindow = WizardForm.Handle) then
      TabUsed := True;
    UiState := SendMessage(WizardForm.Handle, WM_QUERYUISTATE, 0, 0);
    for I := 0 to GetArrayLength(UiButtons) - 1 do
      UpdateUiButton(I, Mouse, Over, Down, UiState);
    for I := 0 to GetArrayLength(UiChecks) - 1 do
      UpdateUiCheck(I, Mouse, Over, UiState);
  except
    Log('UiTick: ' + GetExceptionMessage);
  end;
end;

// Buttons at the bottom right, 16 px from the edges, 8 px apart, Cancel 8 px further away. The page area ends
// above them, so that no page covers them.
procedure LayoutButtons;
var
  Probe: TPngImage;
  W, H, Top, CancelLeft, NextLeft: Integer;
begin
  Probe := LoadUiPng('button_normal');
  W := Probe.Width;
  H := Probe.Height;
  Probe.Free;
  Top := WizardForm.ClientHeight - UX(16) - H;
  CancelLeft := WizardForm.ClientWidth - UX(16) - W;
  NextLeft := CancelLeft - UX(16) - W;
  if WizardForm.OuterNotebook.Top + WizardForm.OuterNotebook.Height > Top - UX(4) then
    WizardForm.OuterNotebook.Height := Top - UX(4) - WizardForm.OuterNotebook.Top;
  AddUiButton(WizardForm.BackButton, 'button', NextLeft - UX(8) - W, Top);
  AddUiButton(WizardForm.NextButton, 'button', NextLeft, Top);
  AddUiButton(WizardForm.CancelButton, 'button', CancelLeft, Top);
end;

// The game folder page: the path in a dark chamfered field (a picture with a borderless edit inside), Browse at
// the right.
procedure LayoutGamePage;
var
  Frame: TBitmapImage;
  Top: Integer;
begin
  GamePage.SubCaptionLabel.AdjustHeight;
  Top := GamePage.SubCaptionLabel.Top + GamePage.SubCaptionLabel.Height + UX(12);
  Frame := UiImage(GamePage.Surface, 'edit', 0, Top);
  GamePage.Edits[0].BorderStyle := bsNone;
  GamePage.Edits[0].StyleElements := [];
  GamePage.Edits[0].Color := FieldColor;
  GamePage.Edits[0].Font.Color := FieldTextColor;
  GamePage.Edits[0].Font.Height := -UX(13);
  GamePage.Edits[0].Left := UX(12);
  GamePage.Edits[0].Width := Frame.Width - 2 * UX(12);
  GamePage.Edits[0].Top := Top + (Frame.Height - GamePage.Edits[0].Height) div 2;
  AddUiButton(GamePage.Buttons[0], 'browse', Frame.Width + UX(8), Top);
end;

procedure InitializeWizard;
begin
  GamePage := CreateInputDirPage(wpWelcome, CustomMessage('GamePageCaption'),
    CustomMessage('GamePageDescription'), CustomMessage('GamePageText'), False, '');
  GamePage.Add('');
  GamePage.Values[0] := InitialGameDir;

  ChooseUiSet;
  LoadCheckFaces;
  LoadBack('welcome', WelcomeBack);
  LoadBack('inner', InnerBack);
  StyleAllText(WizardForm);
  LayoutWelcomePage;
  LayoutInnerPages;
  LayoutButtons;
  LayoutGamePage;
  UiTimer := SetTimer(0, 0, 30, CreateCallback(@UiTick));
end;

procedure DeinitializeSetup;
begin
  if UiTimer <> 0 then
    KillTimer(0, UiTimer);
  UiTimer := 0;
end;

// The last page says where doomfix is installed, replacing the standard text (and its "Click Finish to exit Setup."
// line). The folder is shown in a read-only memo without a border: unlike a label, it wraps a long path without
// spaces at any character, and the path can be selected and copied. The "Start the game" check box goes below.
// Like the welcome page, everything sits in the panel under the large logo, with a green heading line.
procedure LayoutFinishedPage;
var
  LineHeight: Integer;
  Text: TNewStaticText;
  PathBox: TNewMemo;
  Shortcut: TNewStaticText;
begin
  WizardForm.WizardBitmapImage2.Visible := False;
  PlaceOnPage(WizardForm.FinishedHeadingLabel, 56, 118, 439, 140);
  PlaceWelcomeTitle(WizardForm.FinishedHeadingLabel);
  FinishedLine := UiImage(WizardForm.FinishedPage, 'line_green', PX(56), PY(144));

  Text := WizardForm.FinishedLabel;
  Text.Caption := CustomMessage('FinishedInstalledTo');
  PlaceOnPage(Text, 56, 154, 439, 170);
  WizardForm.AdjustLabelHeight(Text);
  LineHeight := Text.Height;

  PathBox := TNewMemo.Create(WizardForm);
  PathBox.Parent := Text.Parent;
  PathBox.Left := Text.Left;
  PathBox.Width := Text.Width;
  PathBox.Top := Text.Top + Text.Height + ScaleY(2);
  PathBox.Font.Assign(Text.Font);
  PathBox.StyleElements := PathBox.StyleElements - [seFont];
  PathBox.BorderStyle := bsNone;
  PathBox.ReadOnly := True;
  PathBox.WordWrap := True;
  PathBox.ScrollBars := ssNone;
  PathBox.TabStop := False;
  PathBox.Text := ExpandConstant('{app}');
  // EM_GETLINECOUNT: the number of lines after wrapping.
  PathBox.Height := SendMessage(PathBox.Handle, $00BA, 0, 0) * LineHeight + ScaleY(2);

  Shortcut := TNewStaticText.Create(WizardForm);
  Shortcut.Parent := Text.Parent;
  Shortcut.Left := Text.Left;
  Shortcut.Width := Text.Width;
  Shortcut.AutoSize := False;
  Shortcut.WordWrap := True;
  StyleText(Shortcut);
  Shortcut.Caption := CustomMessage('FinishedShortcut');
  Shortcut.Top := PathBox.Top + PathBox.Height + DY(8);
  WizardForm.AdjustLabelHeight(Shortcut);

  WizardForm.RunList.Left := Text.Left;
  WizardForm.RunList.Width := Text.Width;
  WizardForm.RunList.Top := Shortcut.Top + Shortcut.Height + DY(8);
  WizardForm.RunList.Height := UX(22);
  ReplaceCheckList(WizardForm.RunList, '');
end;

#ifdef UITest
procedure UITestPause;
var
  F: TSetupForm;
begin
  F := CreateCustomForm(ScaleX(200), ScaleY(60), False, False);
  try
    F.Caption := 'UITest pause';
    F.Position := poDesigned;
    F.Left := 0;
    F.Top := 0;
    F.ShowModal;
  finally
    F.Free;
  end;
end;
#endif

procedure CurPageChanged(CurPageID: Integer);
begin
  TabUsed := False;
  if (CurPageID = wpWelcome) or (CurPageID = wpFinished) then
    ShowBack(BackWelcome)
  else begin
    ShowBack(BackInner);
    SetHeading(WizardForm.PageNameLabel, WizardForm.PageNameLabel.Caption, DX(463) - DX(34));
  end;
  if (CurPageID = wpFinished) and not FinishedLayoutDone then begin
    FinishedLayoutDone := True;
    LayoutFinishedPage;
  end;
  // The task list is filled when its page is shown.
  if (CurPageID = wpSelectTasks) and not TasksLayoutDone then begin
    TasksLayoutDone := True;
    ReplaceCheckList(WizardForm.TasksList, CustomMessage('ShortcutsGroup'));
  end;
#ifdef UITest
  // Test builds only (/DUITest): stop on the Installing page until the screenshot script closes this small form.
  if CurPageID = wpInstalling then
    UITestPause;
#endif
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = GamePage.ID) and not IsGameDir(GameDir) then begin
    if GameDir = '' then
      Result := SuppressibleMsgBox(CustomMessage('GameNotChosen'), mbConfirmation,
        MB_YESNO or MB_DEFBUTTON2, IDYES) = IDYES
    else
      Result := SuppressibleMsgBox(FmtMessage(CustomMessage('GameNotFound'), [GameDir]), mbConfirmation,
        MB_YESNO or MB_DEFBUTTON2, IDYES) = IDYES;
  end;
end;

// The game keeps doomfix_layer.dll open; it cannot be replaced while the game runs.
function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  while (SetupProcessFlags and GameRunning) <> 0 do
    if SuppressibleMsgBox(CustomMessage('CloseGame'), mbError, MB_RETRYCANCEL, IDCANCEL) <> IDRETRY then begin
      Result := CustomMessage('CloseGameAbort');
      Exit;
    end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep <> ssPostInstall then Exit;
#ifdef UITest
  Exit;  // test builds change nothing outside the test folder
#endif
  RegisterLayer;
  WriteLoaderSettings;
  SaveGameDir;
  if RemoveOldSteamMode then
    SuppressibleMsgBox(CustomMessage('SteamModeRemoved'), mbInformation, MB_OK, IDOK);
  // A shortcut from an earlier installation stays unless it is removed here.
  if not WizardIsTaskSelected('desktopicon') then
    DeleteFile(ExpandConstant('{autodesktop}\{#ShortcutName}.lnk'));
end;

// --- Uninstall events (the same as scripts\uninstall.ps1) ---------------------------------------

function InitializeUninstall: Boolean;
begin
  Result := True;
  while (CheckProcesses(ExpandConstant('{app}\doomfix_launcher.exe')) and GameRunning) <> 0 do
    if SuppressibleMsgBox(CustomMessage('CloseGameUninstall'), mbError, MB_RETRYCANCEL, IDCANCEL) <> IDRETRY then begin
      Result := False;
      Exit;
    end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep <> usUninstall then Exit;
  RemoveLayerRegistrations('');
  if RegValueExists(HKCU, LoaderSettingsKey, InstalledLoaderSettings) then
    RegDeleteValue(HKCU, LoaderSettingsKey, InstalledLoaderSettings);
  RemoveOldSteamMode;
end;
