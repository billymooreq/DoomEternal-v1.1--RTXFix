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
//
// Если сам лаунчер работает от имени администратора (уровень целостности High: выключен UAC, «Запуск
// от имени администратора», запуск из повышенной программы), игра запускается с копией его маркера,
// пониженной до Medium: в повышенный процесс загрузчик Vulkan слой из HKCU не загрузит.
//
// Каждый запуск (кроме --check-processes) пишет лог в ту же папку, что и слой
// (%LOCALAPPDATA%\doomfix\logs): права процесса, UAC, родительский процесс, флаги совместимости -
// чтобы по логу было видно, почему игра запустилась бы от имени администратора.

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <string>
#include <string_view>
#include <vector>

#include "common/config.h"
#include "common/log.h"
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
  kUacOff = 8,  // EnableLUA = 0: у администратора все программы работают от имени администратора
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
// EnableLUA = 0: у администратора все процессы с полным маркером, «обычный» запуск не поможет.
constexpr Text kMsgElevatedUacOff = {
    L"User Account Control (UAC) is turned off on this PC, so every program you start runs as "
    L"administrator, the game too. Windows does not load the doomfix layer into programs that run as "
    L"administrator, so the fix will NOT work.\n\n"
    L"To use doomfix, turn UAC back on: in the registry key HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\"
    L"Windows\\CurrentVersion\\Policies\\Systemset EnableLUA to 1 (or turn on the policy \"User "
    L"Account Control: Run all administrators in Admin Approval Mode\"), then restart Windows.\n\n"
    L"Start the game anyway, without the fix?",
    L"Контроль учётных записей (UAC) выключен, поэтому все программы, в том числе игра, работают от "
    L"имени администратора. Windows не загружает слой doomfix в программы, работающие от имени "
    L"администратора, поэтому исправление работать НЕ будет.\n\n"
    L"Чтобы doomfix работал, включите UAC: в разделе реестра HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\"
    L"Windows\\CurrentVersion\\Policies\\Systemзадайте EnableLUA = 1 (или включите политику "
    L"«Контроль учетных записей: все администраторы работают в режиме одобрения администратором»), "
    L"затем перезагрузите Windows.\n\nВсё равно запустить игру без исправления?"};
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
// %1 - полный путь к exe игры. Обычно не показывается: лаунчер перекрывает эту галочку (RunAsInvoker).
constexpr Text kMsgGameNeedsAdmin = {
    L"Windows is set to start the game as administrator, and doomfix could not override this. Windows "
    L"does not load doomfix into programs that run as administrator, so the game was not started.\n\n"
    L"Game: %1\n\n"
    L"To fix it: in File Explorer, right-click this file, choose Properties, open the Compatibility tab "
    L"and untick \"Run this program as an administrator\". If the box is greyed out or still ticked, "
    L"press \"Change settings for all users\" on the same tab and untick it there as well. Then start "
    L"the doomfix shortcut again.",
    L"Windows настроена запускать игру от имени администратора, и doomfix не смог это обойти. Windows "
    L"не загружает doomfix в программы, работающие от имени администратора, поэтому игра не "
    L"запущена.\n\n"
    L"Игра: %1\n\n"
    L"Как исправить: в Проводнике щёлкните этот файл правой кнопкой, выберите «Свойства», откройте "
    L"вкладку «Совместимость» и снимите галочку «Запускать эту программу от имени администратора». "
    L"Если галочка серая или осталась, нажмите на той же вкладке «Изменить параметры для всех "
    L"пользователей» и снимите её и там. Затем снова запустите ярлык doomfix."};
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

// Манифест слоя, зарегистрированный в root (как проверяет launch.ps1); пусто, если его нет.
std::wstring FindLayer(HKEY root) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(root, kImplicitLayersKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return {};
  std::wstring found;
  std::vector<wchar_t> name(16384);  // предел длины имени значения в реестре
  for (DWORD i = 0; found.empty(); ++i) {
    DWORD length = static_cast<DWORD>(name.size());
    if (RegEnumValueW(key, i, name.data(), &length, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
      break;
    }
    const std::wstring path(name.data(), length);
    if (EqualsNoCase(FileName(path), kLayerManifest) && IsFile(path)) found = path;
  }
  RegCloseKey(key);
  return found;
}

// Зарегистрирован ли слой для текущего пользователя.
bool IsLayerInstalled() { return !FindLayer(HKEY_CURRENT_USER).empty(); }

// --- Процессы и права администратора -------------------------------------------------------

// Уровень целостности процесса (SECURITY_MANDATORY_*_RID); *known = false, если его не прочитать.
DWORD IntegrityLevel(HANDLE process, bool* known) {
  *known = false;
  HANDLE token = nullptr;
  if (!OpenProcessToken(process, TOKEN_QUERY, &token)) return 0;
  DWORD size = 0;
  GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &size);
  std::vector<BYTE> buffer(size);
  DWORD level = 0;
  if (size && GetTokenInformation(token, TokenIntegrityLevel, buffer.data(), size, &size)) {
    const auto* label = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(buffer.data());
    const DWORD count = *GetSidSubAuthorityCount(label->Label.Sid);
    level = *GetSidSubAuthority(label->Label.Sid, count - 1);
    *known = true;
  }
  CloseHandle(token);
  return level;
}

// Загрузчик Vulkan не читает слои и файл настроек из HKCU в процессах с уровнем целостности High и
// выше (loader_environment.c, is_high_integrity: integrity_level >= SECURITY_MANDATORY_HIGH_RID),
// поэтому проверяем ровно это же. Если уровень не прочитать, загрузчик считает процесс обычным.
bool IsHighIntegrity(HANDLE process, bool* known) {
  return IntegrityLevel(process, known) >= SECURITY_MANDATORY_HIGH_RID;
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

// --- Почему лаунчер работает от имени администратора (лог и текст предупреждения) ------------

constexpr wchar_t kUacPolicyKey[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System";
constexpr wchar_t kCompatLayersKey[] =
    L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers";

enum class ElevationCause {
  kNone,             // не High: загрузчик читает HKCU, всё в порядке
  kUacOff,           // EnableLUA = 0: у администратора все процессы High
  kBuiltinAdmin,     // встроенная учётная запись «Администратор» без режима одобрения
  kElevatedParent,   // запущен из программы, которая сама работает от имени администратора
  kCompatFlag,       // RUNASADMIN в AppCompatFlags для лаунчера (вкладка «Совместимость»)
  kShortcutRunAs,    // «Запуск от имени администратора» в дополнительных свойствах ярлыка
  kOther,            // «Запуск от имени администратора» из меню или причина не определена
};

struct ParentProcess {
  DWORD pid = 0;
  std::wstring name;  // имя exe; пусто, если родитель уже завершился
  DWORD integrity = 0;
  bool integrityKnown = false;
};

struct ElevationInfo {
  DWORD integrity = 0;
  bool integrityKnown = false;
  TOKEN_ELEVATION_TYPE elevationType = TokenElevationTypeDefault;
  bool elevationTypeKnown = false;
  DWORD enableLua = 1;  // нет значения - UAC включён
  bool enableLuaSet = false;
  bool builtinAdmin = false;
  bool localSystem = false;
  ParentProcess parent;
  std::wstring compatHkcu;  // AppCompatFlags\Layers для doomfix_launcher.exe
  std::wstring compatHklm;
  std::wstring compatLayerEnv;  // __COMPAT_LAYER
  std::wstring shortcut;        // ярлык, которым запущен лаунчер, если Windows его сообщила
  bool shortcutRunAs = false;
  ElevationCause cause = ElevationCause::kNone;
};

const char* IntegrityName(DWORD level) {
  if (level < SECURITY_MANDATORY_LOW_RID) return "Untrusted";
  if (level < SECURITY_MANDATORY_MEDIUM_RID) return "Low";
  if (level < SECURITY_MANDATORY_MEDIUM_PLUS_RID) return "Medium";
  if (level < SECURITY_MANDATORY_HIGH_RID) return "Medium Plus";
  if (level < SECURITY_MANDATORY_SYSTEM_RID) return "High";
  if (level < SECURITY_MANDATORY_PROTECTED_PROCESS_RID) return "System";
  return "Protected";
}

std::string IntegrityText(DWORD level, bool known) {
  if (!known) return "unknown";
  char text[48];
  std::snprintf(text, sizeof(text), "%s (0x%lX)", IntegrityName(level), level);
  return text;
}

const char* ElevationTypeName(TOKEN_ELEVATION_TYPE type, bool known) {
  if (!known) return "unknown";
  switch (type) {
    case TokenElevationTypeDefault: return "Default (no split token)";
    case TokenElevationTypeFull: return "Full (elevated)";
    case TokenElevationTypeLimited: return "Limited (not elevated)";
  }
  return "?";
}

const char* CauseName(ElevationCause cause) {
  switch (cause) {
    case ElevationCause::kNone: return "none, the loader reads HKCU";
    case ElevationCause::kUacOff: return "UAC is turned off (EnableLUA=0)";
    case ElevationCause::kBuiltinAdmin: return "built-in Administrator account";
    case ElevationCause::kElevatedParent: return "started from a program that runs as administrator";
    case ElevationCause::kCompatFlag: return "RUNASADMIN compatibility flag on doomfix_launcher.exe";
    case ElevationCause::kShortcutRunAs: return "the shortcut is set to \"Run as administrator\"";
    case ElevationCause::kOther: return "\"Run as administrator\" or unknown";
  }
  return "?";
}

std::vector<BYTE> TokenInformation(TOKEN_INFORMATION_CLASS type) {
  std::vector<BYTE> buffer;
  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return buffer;
  DWORD size = 0;
  GetTokenInformation(token, type, nullptr, 0, &size);
  buffer.resize(size);
  if (!size || !GetTokenInformation(token, type, buffer.data(), size, &size)) buffer.clear();
  CloseHandle(token);
  return buffer;
}

bool ReadDword(HKEY root, const wchar_t* key, const wchar_t* name, DWORD* value) {
  DWORD size = sizeof(*value);
  return RegGetValueW(root, key, name, RRF_RT_REG_DWORD, nullptr, value, &size) == ERROR_SUCCESS;
}

std::wstring ReadString(HKEY root, const wchar_t* key, const wchar_t* name) {
  DWORD size = 0;
  if (RegGetValueW(root, key, name, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS || !size) {
    return {};
  }
  std::wstring value(size / sizeof(wchar_t), L'\0');
  if (RegGetValueW(root, key, name, RRF_RT_REG_SZ, nullptr, value.data(), &size) != ERROR_SUCCESS) {
    return {};
  }
  value.resize(wcsnlen(value.c_str(), value.size()));
  return value;
}

// Флаги совместимости exe (значение с полным путём к exe, например "~ RUNASADMIN").
std::wstring CompatLayers(HKEY root, const std::wstring& exe) {
  return ReadString(root, kCompatLayersKey, exe.c_str());
}

ULONGLONG CreationTime(HANDLE process) {
  FILETIME created = {}, exited = {}, kernel = {}, user = {};
  if (!GetProcessTimes(process, &created, &exited, &kernel, &user)) return 0;
  return (static_cast<ULONGLONG>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
}

ParentProcess FindParent() {
  ParentProcess parent;
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return parent;
  std::vector<PROCESSENTRY32W> entries;
  PROCESSENTRY32W entry = {};
  entry.dwSize = sizeof(entry);
  for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry)) {
    entries.push_back(entry);
  }
  CloseHandle(snapshot);
  for (const auto& e : entries) {
    if (e.th32ProcessID == GetCurrentProcessId()) parent.pid = e.th32ParentProcessID;
  }
  for (const auto& e : entries) {
    if (parent.pid && e.th32ProcessID == parent.pid) parent.name = e.szExeFile;
  }
  if (parent.name.empty()) return parent;
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, parent.pid);
  if (!process) return parent;
  // Родитель мог завершиться, а его PID - достаться другому процессу, запущенному позже нас.
  const ULONGLONG parentCreated = CreationTime(process);
  if (parentCreated && parentCreated > CreationTime(GetCurrentProcess())) {
    parent.name.clear();
  } else {
    parent.integrity = IntegrityLevel(process, &parent.integrityKnown);
  }
  CloseHandle(process);
  return parent;
}

// Explorer передаёт путь ярлыка, которым запущена программа, в STARTUPINFO.
std::wstring StartedFromShortcut() {
  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  GetStartupInfoW(&startup);
  if ((startup.dwFlags & STARTF_TITLEISLINKNAME) && startup.lpTitle) return startup.lpTitle;
  return {};
}

// Отмечен ли у ярлыка «Дополнительно... → Запуск от имени администратора».
bool ShortcutRunsAsAdmin(const std::wstring& path) {
  bool runAs = false;
  IShellLinkW* link = nullptr;
  if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))) {
    return false;
  }
  IPersistFile* file = nullptr;
  if (SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&file)))) {
    IShellLinkDataList* data = nullptr;
    if (SUCCEEDED(file->Load(path.c_str(), STGM_READ)) &&
        SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&data)))) {
      DWORD flags = 0;
      runAs = SUCCEEDED(data->GetFlags(&flags)) && (flags & SLDF_RUNAS_USER);
      data->Release();
    }
    file->Release();
  }
  link->Release();
  return runAs;
}

ElevationCause Classify(const ElevationInfo& e) {
  if (!e.integrityKnown || e.integrity < SECURITY_MANDATORY_HIGH_RID) return ElevationCause::kNone;
  if (!e.elevationTypeKnown || e.elevationType != TokenElevationTypeFull) {
    // Маркер не «повышен», а сразу полный: UAC не разделяет права этого пользователя.
    if (e.enableLuaSet && e.enableLua == 0) return ElevationCause::kUacOff;
    if (e.builtinAdmin) return ElevationCause::kBuiltinAdmin;
    return ElevationCause::kOther;
  }
  // Через окно UAC Windows делает родителем того, кто просил повышения (обычно explorer.exe с
  // уровнем Medium), поэтому родитель с уровнем High - это повышенная программа, запустившая нас.
  if (!e.parent.name.empty() && e.parent.integrityKnown &&
      e.parent.integrity >= SECURITY_MANDATORY_HIGH_RID) {
    return ElevationCause::kElevatedParent;
  }
  if (ContainsNoCase(e.compatHkcu, L"RUNASADMIN") || ContainsNoCase(e.compatHklm, L"RUNASADMIN")) {
    return ElevationCause::kCompatFlag;
  }
  if (e.shortcutRunAs) return ElevationCause::kShortcutRunAs;
  return ElevationCause::kOther;
}

ElevationInfo CollectElevationInfo() {
  ElevationInfo e;
  e.integrity = IntegrityLevel(GetCurrentProcess(), &e.integrityKnown);
  const std::vector<BYTE> type = TokenInformation(TokenElevationType);
  if (type.size() >= sizeof(TOKEN_ELEVATION_TYPE)) {
    e.elevationType = *reinterpret_cast<const TOKEN_ELEVATION_TYPE*>(type.data());
    e.elevationTypeKnown = true;
  }
  e.enableLuaSet = ReadDword(HKEY_LOCAL_MACHINE, kUacPolicyKey, L"EnableLUA", &e.enableLua);
  const std::vector<BYTE> user = TokenInformation(TokenUser);
  if (!user.empty()) {
    PSID sid = reinterpret_cast<const TOKEN_USER*>(user.data())->User.Sid;
    const DWORD count = *GetSidSubAuthorityCount(sid);
    // S-1-5-21-<домен или компьютер>-500
    e.builtinAdmin = count >= 2 && *GetSidSubAuthority(sid, 0) == SECURITY_NT_NON_UNIQUE &&
                     *GetSidSubAuthority(sid, count - 1) == DOMAIN_USER_RID_ADMIN;
    e.localSystem = IsWellKnownSid(sid, WinLocalSystemSid);
  }
  e.parent = FindParent();
  const std::wstring self = ExecutablePath();
  e.compatHkcu = CompatLayers(HKEY_CURRENT_USER, self);
  e.compatHklm = CompatLayers(HKEY_LOCAL_MACHINE, self);
  e.compatLayerEnv = GetEnv(L"__COMPAT_LAYER");
  e.shortcut = StartedFromShortcut();
  e.shortcutRunAs = !e.shortcut.empty() && ShortcutRunsAsAdmin(e.shortcut);
  e.cause = Classify(e);
  return e;
}

std::string OrNone(const std::wstring& text) { return text.empty() ? "(none)" : ToUtf8(text); }

void LogElevationInfo(const ElevationInfo& e) {
  DWORD consent = 0, filterAdmin = 0;
  const bool consentSet =
      ReadDword(HKEY_LOCAL_MACHINE, kUacPolicyKey, L"ConsentPromptBehaviorAdmin", &consent);
  const bool filterSet =
      ReadDword(HKEY_LOCAL_MACHINE, kUacPolicyKey, L"FilterAdministratorToken", &filterAdmin);
  DF_LOG_INFO("Token: integrity %s, elevation type %s",
              IntegrityText(e.integrity, e.integrityKnown).c_str(),
              ElevationTypeName(e.elevationType, e.elevationTypeKnown));
  DF_LOG_INFO("UAC: EnableLUA=%s, ConsentPromptBehaviorAdmin=%s, FilterAdministratorToken=%s",
              e.enableLuaSet ? std::to_string(e.enableLua).c_str() : "(not set, UAC on)",
              consentSet ? std::to_string(consent).c_str() : "(not set)",
              filterSet ? std::to_string(filterAdmin).c_str() : "(not set)");
  DF_LOG_INFO("Account: built-in Administrator %s, LocalSystem %s", e.builtinAdmin ? "yes" : "no",
              e.localSystem ? "yes" : "no");
  if (e.parent.name.empty()) {
    DF_LOG_INFO("Parent process: pid %lu, already exited", e.parent.pid);
  } else {
    DF_LOG_INFO("Parent process: %s, pid %lu, integrity %s", ToUtf8(e.parent.name).c_str(),
                e.parent.pid, IntegrityText(e.parent.integrity, e.parent.integrityKnown).c_str());
  }
  DF_LOG_INFO("Shortcut: %s%s", OrNone(e.shortcut).c_str(),
              e.shortcut.empty()   ? ""
              : e.shortcutRunAs    ? ", \"Run as administrator\" ticked"
                                   : ", \"Run as administrator\" not ticked");
  DF_LOG_INFO("Compatibility flags of doomfix_launcher.exe: HKCU %s, HKLM %s; __COMPAT_LAYER %s",
              OrNone(e.compatHkcu).c_str(), OrNone(e.compatHklm).c_str(),
              OrNone(e.compatLayerEnv).c_str());
  DF_LOG_INFO("Elevated (loader ignores HKCU layers): %s; cause: %s",
              e.cause == ElevationCause::kNone ? "no" : "YES", CauseName(e.cause));
}

// Отдельный текст только для выключенного UAC: там «обычный» запуск ярлыка не поможет. Остальные
// причины видны в логе (CauseName), а совет в общем тексте для них подходит.
std::wstring ElevationMessage(const ElevationInfo& e) {
  return Tr(e.cause == ElevationCause::kUacOff ? kMsgElevatedUacOff : kMsgElevated);
}

bool IsUacOff() {
  DWORD enableLua = 1;
  return ReadDword(HKEY_LOCAL_MACHINE, kUacPolicyKey, L"EnableLUA", &enableLua) && enableLua == 0;
}

int CheckProcesses() {
  int flags = 0;
  const std::vector<DWORD> game = FindProcesses(kGameExe);
  if (!game.empty()) flags |= kGameRunning;
  if (AnyElevated(game)) flags |= kGameElevated;
  if (IsSelfElevated()) flags |= kSelfElevated;
  if (IsUacOff()) flags |= kUacOff;
  return flags;
}

// --- Запуск игры с пониженным уровнем целостности ------------------------------------------

class ScopedHandle {
 public:
  explicit ScopedHandle(HANDLE handle = nullptr) : handle_(handle) {}
  ~ScopedHandle() {
    if (handle_) CloseHandle(handle_);
  }
  ScopedHandle(const ScopedHandle&) = delete;
  ScopedHandle& operator=(const ScopedHandle&) = delete;
  HANDLE get() const { return handle_; }

 private:
  HANDLE handle_;
};

// Копия маркера лаунчера с уровнем целостности Medium. Игра с ним остаётся тем же пользователем с
// теми же группами, но загрузчик Vulkan считает её обычным процессом (IsHighIntegrity) и читает слой
// из HKCU. Права при этом только понижаются; так же Microsoft запускает процессы с уровнем Low
// («Designing Applications to Run at a Low Integrity Level»). nullptr и *error, если не вышло.
HANDLE MediumIntegrityToken(DWORD* error) {
  *error = ERROR_SUCCESS;
  HANDLE self = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(),
                        TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT | TOKEN_ASSIGN_PRIMARY, &self)) {
    *error = GetLastError();
    return nullptr;
  }
  HANDLE token = nullptr;
  const BOOL duplicated = DuplicateTokenEx(self, 0, nullptr, SecurityImpersonation, TokenPrimary, &token);
  if (!duplicated) *error = GetLastError();
  CloseHandle(self);
  if (!duplicated) return nullptr;

  SID_IDENTIFIER_AUTHORITY authority = SECURITY_MANDATORY_LABEL_AUTHORITY;
  PSID sid = nullptr;
  BOOL ok = AllocateAndInitializeSid(&authority, 1, SECURITY_MANDATORY_MEDIUM_RID, 0, 0, 0, 0, 0, 0, 0, &sid);
  if (ok) {
    TOKEN_MANDATORY_LABEL label = {};
    label.Label.Attributes = SE_GROUP_INTEGRITY;
    label.Label.Sid = sid;
    ok = SetTokenInformation(token, TokenIntegrityLevel, &label,
                             static_cast<DWORD>(sizeof(label) + GetLengthSid(sid)));
  }
  if (!ok) *error = GetLastError();
  if (sid) FreeSid(sid);
  if (!ok) {
    CloseHandle(token);
    return nullptr;
  }
  return token;
}

// Предупреждение «исправление не будет работать»; true - пользователь всё равно запускает игру.
bool ConfirmStartWithoutFix(const ElevationInfo& elevation) {
  if (Message(ElevationMessage(elevation), MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES) {
    DF_LOG_INFO("Cancelled at the administrator warning");
    return false;
  }
  DF_LOG_WARN("Starting the game without the fix (the user chose to)");
  return true;
}

std::string WindowsVersion() {
  OSVERSIONINFOW info = {};
  info.dwOSVersionInfoSize = sizeof(info);
  using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
  const auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
      GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
  if (!rtlGetVersion || rtlGetVersion(&info) != 0) return "unknown";
  return std::to_string(info.dwMajorVersion) + "." + std::to_string(info.dwMinorVersion) + "." +
         std::to_string(info.dwBuildNumber);
}

// --- Запуск ----------------------------------------------------------------------------------

bool IsValidMode(std::wstring_view mode) {
  return mode == L"on" || mode == L"strict" || mode == L"off" || mode == L"aggressive";
}

// Блок окружения для процесса игры: окружение лаунчера, в котором переменные doomfix заменены
// нужными значениями.
constexpr wchar_t kCompatLayerVariable[] = L"__COMPAT_LAYER";

// Слои совместимости, связанные с повышением прав. Windows ставит их в __COMPAT_LAYER процесса,
// запущенного от имени администратора (например, «Installer»), и дочерний процесс унаследовал бы их.
bool IsElevationCompatLayer(std::wstring_view layer) {
  for (const wchar_t* name :
       {L"RunAsAdmin", L"RunAsHighest", L"Installer", L"ElevateCreateProcess", L"RequireAdministrator"}) {
    if (EqualsNoCase(layer, name)) return true;
  }
  return false;
}

// __COMPAT_LAYER для игры: слои лаунчера без слоёв повышения прав, плюс RunAsInvoker. RunAsInvoker не
// даёт Windows повысить игру, даже если у её exe стоит «Запускать от имени администратора» (RUNASADMIN в
// AppCompatFlags\Layers в HKCU или HKLM; проверено - CreateProcess тогда не возвращает
// ERROR_ELEVATION_REQUIRED, игра получает Medium). Остальные флаги совместимости exe из реестра Windows
// по-прежнему применяет. Значение - слова через пробел, в начале могут быть пометки «~», «$», «#».
std::wstring GameCompatLayer(std::wstring_view value) {
  std::wstring markers, layers;
  bool runAsInvoker = false;
  size_t pos = 0;
  while (pos < value.size()) {
    const size_t begin = value.find_first_not_of(L" \t", pos);
    if (begin == std::wstring_view::npos) break;
    size_t end = value.find_first_of(L" \t", begin);
    if (end == std::wstring_view::npos) end = value.size();
    const std::wstring_view word = value.substr(begin, end - begin);
    pos = end;
    const bool marker = word.find_first_not_of(L"~$#") == std::wstring_view::npos;
    if (!marker && IsElevationCompatLayer(word)) continue;
    runAsInvoker = runAsInvoker || (!marker && EqualsNoCase(word, L"RunAsInvoker"));
    std::wstring& list = marker ? markers : layers;
    if (!list.empty()) list += L' ';
    list += word;
  }
  if (layers.empty()) return L"RunAsInvoker";  // пометки без слоёв не нужны
  if (!runAsInvoker) layers += L" RunAsInvoker";
  return markers.empty() ? layers : markers + L' ' + layers;
}

std::vector<wchar_t> GameEnvironment(const std::wstring& mode) {
  struct Variable {
    const wchar_t* name;
    std::wstring value;
  };
  std::vector<Variable> variables = {
      {L"DOOMFIX_ENABLE", L"1"}, {L"SteamAppId", kSteamAppId}, {L"DOOMFIX_INDIRECT_FIX", mode}};
  const std::wstring compatLayer = GetEnv(kCompatLayerVariable);
  const std::wstring gameCompatLayer = GameCompatLayer(compatLayer);
  variables.push_back({kCompatLayerVariable, gameCompatLayer});
  DF_LOG_INFO("__COMPAT_LAYER of the launcher: %s; passed to the game: \"%s\"", OrNone(compatLayer).c_str(),
              ToUtf8(gameCompatLayer).c_str());

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

  const Config& config = GetConfig();
  log::Open(config.logDir, config.logLevel);
  DF_LOG_INFO("doomfix_launcher %s, Windows %s, mode %s%s%s", DOOMFIX_VERSION, WindowsVersion().c_str(),
              ToUtf8(mode).c_str(), chooseGame ? ", --choose-game" : "", dryRun ? ", --dry-run" : "");
  DF_LOG_INFO("Launcher: %s", ToUtf8(ExecutablePath()).c_str());

  // Повышенный процесс игры загрузчик Vulkan не пустит к слою из HKCU (см. IsHighIntegrity), поэтому
  // из повышенного лаунчера игра запускается с маркером, пониженным до Medium. Предупреждение - только
  // если понизить не удалось.
  const ElevationInfo elevation = CollectElevationInfo();
  LogElevationInfo(elevation);
  const bool elevated = elevation.cause != ElevationCause::kNone;
  DWORD tokenError = ERROR_SUCCESS;
  const ScopedHandle gameToken(elevated ? MediumIntegrityToken(&tokenError) : nullptr);
  if (elevated && gameToken.get()) {
    DF_LOG_INFO("The game will be started with the integrity level lowered to Medium");
  } else if (elevated) {
    DF_LOG_ERROR("Could not lower the integrity level for the game: %s",
                 ToUtf8(ErrorText(tokenError)).c_str());
    if (!dryRun && !ConfirmStartWithoutFix(elevation)) return kCancelled;
  }

  std::wstring gameDir = chooseGame ? std::wstring() : ReadGameDir();
  if (!IsGameDir(gameDir)) {
    DF_LOG_INFO("Game folder not set or invalid: %s", OrNone(gameDir).c_str());
    if (dryRun) return kGameNotFound;
    gameDir = AskGameDir();
    if (gameDir.empty()) {
      DF_LOG_INFO("Cancelled at the game folder dialog");
      return kCancelled;
    }
    SaveGameDir(gameDir);
  }
  const std::wstring exe = GameExePath(gameDir);
  DF_LOG_INFO("Game: %s", ToUtf8(exe).c_str());
  const std::wstring gameCompatHkcu = CompatLayers(HKEY_CURRENT_USER, exe);
  const std::wstring gameCompatHklm = CompatLayers(HKEY_LOCAL_MACHINE, exe);
  DF_LOG_INFO("Compatibility flags of %ls: HKCU %s, HKLM %s", kGameExe, OrNone(gameCompatHkcu).c_str(),
              OrNone(gameCompatHklm).c_str());
  if (ContainsNoCase(gameCompatHkcu, L"RUNASADMIN") || ContainsNoCase(gameCompatHklm, L"RUNASADMIN")) {
    DF_LOG_INFO("The game is set to run as administrator (RUNASADMIN); RunAsInvoker in __COMPAT_LAYER "
                "overrides it");
  }

  DF_LOG_INFO("Layer registered: HKCU %s, HKLM %s", OrNone(FindLayer(HKEY_CURRENT_USER)).c_str(),
              OrNone(FindLayer(HKEY_LOCAL_MACHINE)).c_str());
  if (!IsLayerInstalled()) {
    DF_LOG_ERROR("The layer is not installed for this user");
    if (!dryRun) Message(Tr(kMsgNotInstalled), MB_OK | MB_ICONERROR);
    return kLayerNotInstalled;
  }
  if (dryRun) return kOk;

  const bool steamRunning = !FindProcesses(kSteamExe).empty();
  DF_LOG_INFO("Steam running: %s", steamRunning ? "yes" : "no");
  if (!steamRunning && Message(Tr(kMsgSteamNotRunning), MB_OKCANCEL | MB_ICONINFORMATION) != IDOK) {
    DF_LOG_INFO("Cancelled at the Steam reminder");
    return kCancelled;
  }

  std::wstring commandLine = L"\"" + exe + L"\"";
  std::vector<wchar_t> environment = GameEnvironment(mode);
  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION info = {};
  BOOL started = FALSE;
  DWORD error = ERROR_SUCCESS;
  if (gameToken.get()) {
    started = CreateProcessAsUserW(gameToken.get(), exe.c_str(), commandLine.data(), nullptr, nullptr,
                                   FALSE, CREATE_UNICODE_ENVIRONMENT, environment.data(),
                                   gameDir.c_str(), &startup, &info);
    if (!started) {
      error = GetLastError();
      DF_LOG_ERROR("CreateProcessAsUser with the Medium token failed: %s", ToUtf8(ErrorText(error)).c_str());
      // Без пониженного маркера игра будет работать без исправления: как и раньше, спрашиваем.
      if (error != ERROR_ELEVATION_REQUIRED) {
        if (!ConfirmStartWithoutFix(elevation)) return kCancelled;
        error = ERROR_SUCCESS;
      }
    }
  }
  if (!started && error == ERROR_SUCCESS) {
    started = CreateProcessW(exe.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
                             CREATE_UNICODE_ENVIRONMENT, environment.data(), gameDir.c_str(), &startup,
                             &info);
    if (!started) {
      error = GetLastError();
      DF_LOG_ERROR("CreateProcess failed: %s", ToUtf8(ErrorText(error)).c_str());
    }
  }
  if (!started) {
    // Совместимость «Запускать от имени администратора» у exe игры: CreateProcess так не умеет,
    // а повышенный процесс всё равно не загрузил бы слой из HKCU.
    if (error == ERROR_ELEVATION_REQUIRED) {
      std::wstring text = Tr(kMsgGameNeedsAdmin);
      text.replace(text.find(L"%1"), 2, exe);
      Message(text, MB_OK | MB_ICONERROR);
    } else {
      Message(Tr(kMsgStartFailed) + exe + L"\n\n" + ErrorText(error), MB_OK | MB_ICONERROR);
    }
    return kStartFailed;
  }
  bool known = false;
  const DWORD integrity = IntegrityLevel(info.hProcess, &known);
  DF_LOG_INFO("Game started: pid %lu, integrity %s%s", info.dwProcessId,
              IntegrityText(integrity, known).c_str(),
              known && integrity >= SECURITY_MANDATORY_HIGH_RID ? " - the fix will NOT load" : "");
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
