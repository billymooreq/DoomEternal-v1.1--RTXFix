// doomfix_launcher.exe: запускает DOOM Eternal со слоем doomfix (ярлыки установщика).
//
// Делает то же, что scripts/launch.ps1, но сам, без PowerShell и консольных окон: читает папку игры
// из doomfix_game_path.txt рядом с собой (её записывает установщик) и запускает игру из её папки с
// DOOMFIX_ENABLE=1, SteamAppId=782330 (игра не перезапускает себя через Steam и не теряет переменные)
// и DOOMFIX_INDIRECT_FIX. Эти переменные получает только процесс игры: ни переменные пользователя или
// системы, ни собственное окружение лаунчер не меняет.
//
//   doomfix_launcher.exe [--mode on|strict|off|aggressive] [--choose-game] [--dry-run]
//   doomfix_launcher.exe --check-processes
//
// --dry-run: всё проверить, но игру не запускать и окон не показывать; код возврата - ExitCode.
// --check-processes: для установщика и деинсталлятора; код возврата - сумма флагов ProcessFlag.

#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <tlhelp32.h>

#include <cwchar>
#include <cwctype>
#include <string>
#include <string_view>
#include <vector>

#include "common/util.h"

namespace doomfix {
namespace {

constexpr wchar_t kGameExe[] = L"DOOMEternalx64vk.exe";
constexpr wchar_t kSteamExe[] = L"steam.exe";  // только чтобы напомнить запустить Steam
constexpr wchar_t kSteamAppId[] = L"782330";
constexpr wchar_t kLayerManifest[] = L"VK_LAYER_DOOMFIX.json";
constexpr wchar_t kImplicitLayersKey[] = L"SOFTWARE\\Khronos\\Vulkan\\ImplicitLayers";
constexpr wchar_t kConfigFileName[] = L"doomfix_game_path.txt";
constexpr wchar_t kTitle[] = L"doomfix";

enum ExitCode : int {
  kOk = 0,
  kCancelled = 1,
  kBadArguments = 2,
  kGameNotFound = 3,
  kLayerNotInstalled = 4,
  kStartFailed = 5,
};

enum ProcessFlag : int {
  kGameRunning = 1,
  kGameElevated = 2,
  kSelfElevated = 4,
};

// --- Тексты (английский и русский по языку интерфейса Windows) -------------------------------

struct Text {
  const wchar_t* en;
  const wchar_t* ru;
};

bool IsRussianUi() { return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN; }

std::wstring Tr(const Text& text) { return IsRussianUi() ? text.ru : text.en; }

constexpr Text kMsgElevated = {
    L"This launcher is running as administrator. Windows does not load the doomfix layer into programs "
    L"started as administrator, so the fix will NOT work.\n\n"
    L"Close this message and start the doomfix shortcut with a normal double-click, not \"Run as "
    L"administrator\".\n\nStart the game anyway, without the fix?",
    L"Лаунчер запущен от имени администратора. Windows не загружает слой doomfix в программы, "
    L"запущенные от имени администратора, поэтому исправление работать НЕ будет.\n\n"
    L"Закройте это сообщение и запустите ярлык doomfix обычным двойным щелчком, а не «Запуск от "
    L"имени администратора».\n\nВсё равно запустить игру без исправления?"};
constexpr Text kMsgNotInstalled = {
    L"The doomfix layer is not installed for this Windows user. Run the doomfix installer again.",
    L"Слой doomfix не установлен для этого пользователя Windows. Запустите установщик doomfix ещё раз."};
constexpr Text kMsgChooseGame = {
    L"The DOOM Eternal folder is not set, or it no longer contains DOOMEternalx64vk.exe.\n\n"
    L"Choose the game folder (the one that contains DOOMEternalx64vk.exe).",
    L"Папка DOOM Eternal не задана или в ней больше нет DOOMEternalx64vk.exe.\n\n"
    L"Выберите папку игры (ту, в которой лежит DOOMEternalx64vk.exe)."};
constexpr Text kMsgPickerTitle = {L"Choose the DOOM Eternal folder", L"Выберите папку DOOM Eternal"};
constexpr Text kMsgWrongFolder = {L"DOOMEternalx64vk.exe was not found in this folder:\n",
                                  L"В этой папке нет DOOMEternalx64vk.exe:\n"};
constexpr Text kMsgSteamNotRunning = {
    L"Steam is not running. Start Steam first, then press OK to start the game.",
    L"Steam не запущен. Сначала запустите Steam, затем нажмите OK, чтобы запустить игру."};
constexpr Text kMsgStartFailed = {L"Could not start the game:\n", L"Не удалось запустить игру:\n"};
constexpr Text kMsgGameNeedsAdmin = {
    L"Windows is set to start DOOMEternalx64vk.exe as administrator (\"Run this program as an "
    L"administrator\" on the Compatibility tab of its properties). Windows does not load doomfix into "
    L"programs that run as administrator.\n\nUntick that box, then start the doomfix shortcut again.",
    L"В Windows для DOOMEternalx64vk.exe включён запуск от имени администратора («Запускать эту "
    L"программу от имени администратора» на вкладке «Совместимость» в его свойствах). Windows не "
    L"загружает doomfix в программы, запущенные от имени администратора.\n\nСнимите эту галочку и "
    L"снова запустите ярлык doomfix."};
constexpr Text kMsgUsage = {
    L"Usage: doomfix_launcher.exe [--mode on|strict|off|aggressive] [--choose-game]",
    L"Использование: doomfix_launcher.exe [--mode on|strict|off|aggressive] [--choose-game]"};

int Message(const std::wstring& text, UINT flags) {
  return MessageBoxW(nullptr, text.c_str(), kTitle, flags | MB_SETFOREGROUND);
}

std::wstring ErrorText(DWORD error) {
  wchar_t* buffer = nullptr;
  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                 nullptr, error, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
  std::wstring text = buffer ? buffer : L"";
  LocalFree(buffer);
  while (!text.empty() && std::iswspace(text.back())) text.pop_back();
  return text + L" (" + std::to_wstring(error) + L")";
}

// --- Файлы и папки ---------------------------------------------------------------------------

bool EqualsNoCase(std::wstring_view a, std::wstring_view b) {
  return a.size() == b.size() && ContainsNoCase(a, b);
}

std::wstring_view FileName(std::wstring_view path) {
  const size_t slash = path.find_last_of(L"\\/");
  return slash == std::wstring_view::npos ? path : path.substr(slash + 1);
}

std::wstring ParentDir(std::wstring_view path) {
  const size_t slash = path.find_last_of(L"\\/");
  return std::wstring(slash == std::wstring_view::npos ? std::wstring_view() : path.substr(0, slash));
}

bool IsFile(const std::wstring& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring GameExePath(const std::wstring& dir) {
  if (dir.empty()) return {};
  return dir + (dir.back() == L'\\' ? L"" : L"\\") + kGameExe;
}

bool IsGameDir(const std::wstring& dir) { return !dir.empty() && IsFile(GameExePath(dir)); }

std::wstring ConfigPath() { return ParentDir(ExecutablePath()) + L"\\" + kConfigFileName; }

// Файл в UTF-8 (с BOM или без, как пишут установщик и Set-Content); пробелы и кавычки по краям
// отбрасываются.
std::wstring ReadGameDir() {
  HANDLE file = CreateFileW(ConfigPath().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return {};
  std::string bytes(4096, '\0');
  DWORD read = 0;
  const BOOL ok = ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
  CloseHandle(file);
  if (!ok) return {};
  bytes.resize(read);
  if (bytes.rfind("\xEF\xBB\xBF", 0) == 0) bytes.erase(0, 3);
  const int size = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()),
                                       nullptr, 0);
  std::wstring text(static_cast<size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), text.data(), size);
  const size_t newline = text.find_first_of(L"\r\n");
  if (newline != std::wstring::npos) text.resize(newline);
  const size_t first = text.find_first_not_of(L" \t\"");
  const size_t last = text.find_last_not_of(L" \t\"");
  return first == std::wstring::npos ? std::wstring() : text.substr(first, last - first + 1);
}

void SaveGameDir(const std::wstring& dir) {
  const std::string text = "\xEF\xBB\xBF" + ToUtf8(dir) + "\r\n";
  HANDLE file = CreateFileW(ConfigPath().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return;  // не страшно: в следующий раз спросим снова
  DWORD written = 0;
  WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
  CloseHandle(file);
}

// Диалог выбора папки; пустая строка, если пользователь отменил.
std::wstring PickFolder() {
  std::wstring result;
  IFileOpenDialog* dialog = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&dialog)))) {
    return result;
  }
  DWORD options = 0;
  dialog->GetOptions(&options);
  dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
  dialog->SetTitle(Tr(kMsgPickerTitle).c_str());
  IShellItem* item = nullptr;
  if (SUCCEEDED(dialog->Show(nullptr)) && SUCCEEDED(dialog->GetResult(&item))) {
    wchar_t* path = nullptr;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
      result = path;
      CoTaskMemFree(path);
    }
    item->Release();
  }
  dialog->Release();
  return result;
}

std::wstring AskGameDir() {
  if (Message(Tr(kMsgChooseGame), MB_OKCANCEL | MB_ICONINFORMATION) != IDOK) return {};
  for (;;) {
    const std::wstring dir = PickFolder();
    if (dir.empty()) return {};
    if (IsGameDir(dir)) return dir;
    if (Message(Tr(kMsgWrongFolder) + dir, MB_RETRYCANCEL | MB_ICONWARNING) != IDRETRY) return {};
  }
}

// Зарегистрирован ли слой для текущего пользователя (как проверяет launch.ps1).
bool IsLayerInstalled() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kImplicitLayersKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
    return false;
  }
  bool found = false;
  std::vector<wchar_t> name(16384);  // предел длины имени значения в реестре
  for (DWORD i = 0; !found; ++i) {
    DWORD length = static_cast<DWORD>(name.size());
    if (RegEnumValueW(key, i, name.data(), &length, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
      break;
    }
    const std::wstring path(name.data(), length);
    found = EqualsNoCase(FileName(path), kLayerManifest) && IsFile(path);
  }
  RegCloseKey(key);
  return found;
}

// --- Процессы и права администратора -------------------------------------------------------

// Загрузчик Vulkan не читает слои из HKCU в процессах с уровнем целостности High и выше
// (loader_windows.c, is_high_integrity), поэтому проверяем именно уровень целостности.
bool IsHighIntegrity(HANDLE process, bool* known) {
  *known = false;
  HANDLE token = nullptr;
  if (!OpenProcessToken(process, TOKEN_QUERY, &token)) return false;
  DWORD size = 0;
  GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &size);
  std::vector<BYTE> buffer(size);
  bool high = false;
  if (size && GetTokenInformation(token, TokenIntegrityLevel, buffer.data(), size, &size)) {
    const auto* label = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(buffer.data());
    const DWORD count = *GetSidSubAuthorityCount(label->Label.Sid);
    high = *GetSidSubAuthority(label->Label.Sid, count - 1) >= SECURITY_MANDATORY_HIGH_RID;
    *known = true;
  }
  CloseHandle(token);
  return high;
}

bool IsSelfElevated() {
  bool known = false;
  return IsHighIntegrity(GetCurrentProcess(), &known);
}

std::vector<DWORD> FindProcesses(const wchar_t* exeName) {
  std::vector<DWORD> pids;
  DWORD session = 0;
  ProcessIdToSessionId(GetCurrentProcessId(), &session);
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return pids;
  PROCESSENTRY32W entry = {};
  entry.dwSize = sizeof(entry);
  for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry)) {
    DWORD processSession = 0;
    if (EqualsNoCase(entry.szExeFile, exeName) &&
        ProcessIdToSessionId(entry.th32ProcessID, &processSession) && processSession == session) {
      pids.push_back(entry.th32ProcessID);
    }
  }
  CloseHandle(snapshot);
  return pids;
}

// Запущен ли процесс от имени администратора. Если обычный процесс не может прочитать маркер
// доступа, значит, у того процесса уровень целостности выше нашего.
bool AnyElevated(const std::vector<DWORD>& pids) {
  const bool selfElevated = IsSelfElevated();
  for (DWORD pid : pids) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
      if (!selfElevated && GetLastError() == ERROR_ACCESS_DENIED) return true;
      continue;
    }
    bool known = false;
    const bool high = IsHighIntegrity(process, &known);
    const DWORD error = GetLastError();
    CloseHandle(process);
    if (high || (!known && !selfElevated && error == ERROR_ACCESS_DENIED)) return true;
  }
  return false;
}

int CheckProcesses() {
  int flags = 0;
  const std::vector<DWORD> game = FindProcesses(kGameExe);
  if (!game.empty()) flags |= kGameRunning;
  if (AnyElevated(game)) flags |= kGameElevated;
  if (IsSelfElevated()) flags |= kSelfElevated;
  return flags;
}

// --- Запуск ----------------------------------------------------------------------------------

bool IsValidMode(std::wstring_view mode) {
  return mode == L"on" || mode == L"strict" || mode == L"off" || mode == L"aggressive";
}

// Блок окружения для процесса игры: окружение лаунчера, в котором переменные doomfix заменены
// нужными значениями.
std::vector<wchar_t> GameEnvironment(const std::wstring& mode) {
  struct Variable {
    const wchar_t* name;
    std::wstring value;
  };
  const Variable variables[] = {
      {L"DOOMFIX_ENABLE", L"1"}, {L"SteamAppId", kSteamAppId}, {L"DOOMFIX_INDIRECT_FIX", mode}};

  std::vector<wchar_t> block;
  wchar_t* strings = GetEnvironmentStringsW();
  for (const wchar_t* entry = strings; entry && *entry; entry += std::wcslen(entry) + 1) {
    const std::wstring_view text(entry);
    // Имя до первого '=' не в начале строки: служебные "=C:=C:\..." начинаются с '='.
    const std::wstring_view name = text.substr(0, text.find(L'=', 1));
    bool overridden = false;
    for (const Variable& v : variables) overridden = overridden || EqualsNoCase(name, v.name);
    if (overridden) continue;
    block.insert(block.end(), text.begin(), text.end());
    block.push_back(L'\0');
  }
  FreeEnvironmentStringsW(strings);
  for (const Variable& v : variables) {
    const std::wstring entry = std::wstring(v.name) + L"=" + v.value;
    block.insert(block.end(), entry.begin(), entry.end());
    block.push_back(L'\0');
  }
  block.push_back(L'\0');
  return block;
}

int Run(int argc, wchar_t** argv) {
  std::wstring mode = L"on";
  bool chooseGame = false;
  bool dryRun = false;
  for (int i = 1; i < argc; ++i) {
    const std::wstring_view arg = argv[i];
    if (arg == L"--check-processes") return CheckProcesses();
    if (arg == L"--choose-game") {
      chooseGame = true;
    } else if (arg == L"--dry-run") {
      dryRun = true;
    } else if (arg == L"--mode" && i + 1 < argc && IsValidMode(argv[i + 1])) {
      mode = argv[++i];
    } else if (arg.rfind(L"--mode=", 0) == 0 && IsValidMode(arg.substr(7))) {
      mode = arg.substr(7);
    } else {
      if (!dryRun) Message(Tr(kMsgUsage), MB_OK | MB_ICONERROR);
      return kBadArguments;
    }
  }

  if (!dryRun && IsSelfElevated() &&
      Message(Tr(kMsgElevated), MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES) {
    return kCancelled;
  }

  std::wstring gameDir = chooseGame ? std::wstring() : ReadGameDir();
  if (!IsGameDir(gameDir)) {
    if (dryRun) return kGameNotFound;
    gameDir = AskGameDir();
    if (gameDir.empty()) return kCancelled;
    SaveGameDir(gameDir);
  }

  if (!IsLayerInstalled()) {
    if (!dryRun) Message(Tr(kMsgNotInstalled), MB_OK | MB_ICONERROR);
    return kLayerNotInstalled;
  }
  if (dryRun) return kOk;

  if (FindProcesses(kSteamExe).empty() &&
      Message(Tr(kMsgSteamNotRunning), MB_OKCANCEL | MB_ICONINFORMATION) != IDOK) {
    return kCancelled;
  }

  const std::wstring exe = GameExePath(gameDir);
  std::wstring commandLine = L"\"" + exe + L"\"";
  std::vector<wchar_t> environment = GameEnvironment(mode);
  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION info = {};
  if (!CreateProcessW(exe.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
                      CREATE_UNICODE_ENVIRONMENT, environment.data(), gameDir.c_str(), &startup,
                      &info)) {
    const DWORD error = GetLastError();
    // Совместимость «Запускать от имени администратора» у exe игры: CreateProcess так не умеет,
    // а повышенный процесс всё равно не загрузил бы слой из HKCU.
    if (error == ERROR_ELEVATION_REQUIRED) {
      Message(Tr(kMsgGameNeedsAdmin), MB_OK | MB_ICONERROR);
    } else {
      Message(Tr(kMsgStartFailed) + exe + L"\n\n" + ErrorText(error), MB_OK | MB_ICONERROR);
    }
    return kStartFailed;
  }
  CloseHandle(info.hThread);
  CloseHandle(info.hProcess);
  return kOk;
}

}  // namespace
}  // namespace doomfix

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  const int code = argv ? doomfix::Run(argc, argv) : doomfix::kBadArguments;
  LocalFree(argv);
  CoUninitialize();
  return code;
}
