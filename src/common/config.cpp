#include "common/config.h"

#include <cwctype>

#include "common/util.h"

namespace doomfix {
namespace {

std::wstring Lower(std::wstring s) {
  for (wchar_t& c : s) c = static_cast<wchar_t>(std::towlower(c));
  return s;
}

std::wstring Trim(const std::wstring& s) {
  const size_t begin = s.find_first_not_of(L" \t");
  if (begin == std::wstring::npos) return {};
  const size_t end = s.find_last_not_of(L" \t");
  return s.substr(begin, end - begin + 1);
}

std::vector<std::string> SplitList(const std::wstring& list) {
  std::vector<std::string> out;
  size_t pos = 0;
  while (pos <= list.size()) {
    size_t next = list.find_first_of(L",;", pos);
    if (next == std::wstring::npos) next = list.size();
    const std::wstring item = Trim(list.substr(pos, next - pos));
    if (!item.empty()) out.push_back(ToUtf8(item));
    pos = next + 1;
  }
  return out;
}

log::Level ParseLevel(const std::wstring& value) {
  const std::wstring v = Lower(value);
  if (v == L"debug") return log::Level::Debug;
  if (v == L"warn" || v == L"warning") return log::Level::Warn;
  if (v == L"error") return log::Level::Error;
  return log::Level::Info;
}

Config Load() {
  Config c;
  c.enabled = GetEnv(L"DOOMFIX_ENABLE") == L"1";
  c.logLevel = ParseLevel(GetEnv(L"DOOMFIX_LOG_LEVEL"));

  const std::wstring dataDir = GetEnv(L"DOOMFIX_DATA_DIR");
  if (!dataDir.empty()) {
    c.dataDir = dataDir;
  } else {
    std::wstring base = GetEnv(L"LOCALAPPDATA");
    if (base.empty()) base = GetEnv(L"TEMP");
    c.dataDir = std::filesystem::path(base) / L"doomfix";
  }

  const std::wstring logDir = GetEnv(L"DOOMFIX_LOG_DIR");
  c.logDir = logDir.empty() ? c.dataDir / L"logs" : std::filesystem::path(logDir);

  c.extraHiddenDeviceExtensions = SplitList(GetEnv(L"DOOMFIX_HIDE_DEVICE_EXTENSIONS"));

  const std::wstring indirectFix = Lower(Trim(GetEnv(L"DOOMFIX_INDIRECT_FIX")));
  c.indirectFixRaw = ToUtf8(indirectFix);
  if (indirectFix == L"off" || indirectFix == L"0" || indirectFix == L"false") {
    c.indirectFixMode = IndirectFixMode::Off;
  } else if (indirectFix == L"aggressive" || indirectFix == L"full") {
    c.indirectFixMode = IndirectFixMode::Aggressive;
  } else {
    c.indirectFixMode = IndirectFixMode::On;  // "", "on", "1" и неизвестные значения
  }
  return c;
}

}  // namespace

const Config& GetConfig() {
  static const Config config = Load();
  return config;
}

}  // namespace doomfix
