; doomfix installer (Inno Setup 6). Build it with scripts\build_installer.ps1, which passes:
;   /DAppVersion=0.3.1[-rc1]   version shown to the user and used in the file name
;   /DFileVersion=0.3.1        numeric part, for the version resource of setup.exe
;   /DBinDir=<folder>          ready doomfix_layer.dll, VK_LAYER_DOOMFIX.json and doomfix_launcher.exe
;   /DOutputDir=<folder>       where doomfix-<AppVersion>-setup.exe goes
;
; The files from BinDir are taken as they are, so they can be signed before this script runs.
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
#define RepoUrl "https://github.com/billymooreq/DoomEternal-v1.1--RTXFix"
#define ShortcutName "DOOM Eternal (doomfix)"

[Setup]
; Never change AppId: it ties new versions to the existing installation.
AppId={{B3F23E39-9C8E-4D02-B62B-4A2E262905A6}
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
DefaultDirName={localappdata}\doomfix
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableWelcomePage=no
DisableReadyPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
WizardStyle=modern
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
en.WelcomeLabel2=This will install [name/ver] for your Windows user.%n%ndoomfix fixes DOOM Eternal 1.1 on current NVIDIA drivers: the first-person weapon is no longer black in shadow, and local lights, decals and volumetric light are back. Game files are not changed, and no administrator rights are needed.%n%nClose DOOM Eternal before you continue.
ru.WelcomeLabel2=Будет установлен [name/ver] для вашего пользователя Windows.%n%ndoomfix исправляет DOOM Eternal 1.1 на современных драйверах NVIDIA: оружие в руках больше не чёрное в тени, возвращаются локальные источники света, декали и объёмный свет. Файлы игры не изменяются, права администратора не нужны.%n%nПеред продолжением закройте DOOM Eternal.

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

[Icons]
Name: "{autoprograms}\{#ShortcutName}"; Filename: "{app}\doomfix_launcher.exe"; WorkingDir: "{app}"; IconFilename: "{code:GetShortcutIcon}"; Comment: "{cm:LaunchGame}"
Name: "{autodesktop}\{#ShortcutName}"; Filename: "{app}\doomfix_launcher.exe"; WorkingDir: "{app}"; IconFilename: "{code:GetShortcutIcon}"; Comment: "{cm:LaunchGame}"; Tasks: desktopicon

[Run]
Filename: "{app}\doomfix_launcher.exe"; Description: "{cm:LaunchGame}"; Flags: postinstall nowait skipifsilent

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

var
  GamePage: TInputDirWizardPage;
  FinishedLayoutDone: Boolean;

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
  if ((Flags and SelfElevated) <> 0) and
     (SuppressibleMsgBox(CustomMessage('SetupElevated'), mbConfirmation, MB_YESNO or MB_DEFBUTTON2, IDYES) <> IDYES) then begin
    Result := False;
    Exit;
  end;
  if (Flags and GameElevated) <> 0 then
    SuppressibleMsgBox(CustomMessage('GameElevated'), mbInformation, MB_OK, IDOK);
end;

procedure InitializeWizard;
begin
  GamePage := CreateInputDirPage(wpWelcome, CustomMessage('GamePageCaption'),
    CustomMessage('GamePageDescription'), CustomMessage('GamePageText'), False, '');
  GamePage.Add('');
  GamePage.Values[0] := InitialGameDir;
end;

// The last page says where doomfix is installed, replacing the standard text (and its "Click Finish to exit Setup."
// line). The folder is shown in a read-only memo without a border: unlike a label, it wraps a long path without
// spaces at any character, and the path can be selected and copied. The "Start the game" check box goes below.
procedure CurPageChanged(CurPageID: Integer);
var
  Text: TNewStaticText;
  PathBox: TNewMemo;
  Shortcut: TNewStaticText;
  LineHeight: Integer;
begin
  if (CurPageID <> wpFinished) or FinishedLayoutDone then Exit;
  FinishedLayoutDone := True;
  Text := WizardForm.FinishedLabel;
  Text.Caption := CustomMessage('FinishedInstalledTo');
  WizardForm.AdjustLabelHeight(Text);
  LineHeight := Text.Height;

  PathBox := TNewMemo.Create(WizardForm);
  PathBox.Parent := Text.Parent;
  PathBox.Left := Text.Left;
  PathBox.Width := Text.Width;
  PathBox.Top := Text.Top + Text.Height + ScaleY(2);
  PathBox.Font.Assign(Text.Font);
  PathBox.Color := clWindow;
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
  Shortcut.Caption := CustomMessage('FinishedShortcut');
  Shortcut.Top := PathBox.Top + PathBox.Height + ScaleY(12);
  WizardForm.AdjustLabelHeight(Shortcut);

  WizardForm.RunList.Top := Shortcut.Top + Shortcut.Height + ScaleY(16);
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
