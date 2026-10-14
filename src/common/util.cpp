#include "common/util.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <vector>

namespace doomfix {

std::string ToUtf8(std::wstring_view text) {
  if (text.empty()) return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                       nullptr, 0, nullptr, nullptr);
  std::string out(static_cast<size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size,
                      nullptr, nullptr);
  return out;
}

std::wstring GetEnv(const wchar_t* name) {
  const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
  if (size == 0) return {};
  std::wstring value(size, L'\0');
  const DWORD written = GetEnvironmentVariableW(name, value.data(), size);
  value.resize(written);
  return value;
}

std::wstring ExecutablePath() {
  std::vector<wchar_t> buffer(MAX_PATH);
  for (;;) {
    const DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (len == 0) return {};
    if (len < buffer.size()) return std::wstring(buffer.data(), len);
    buffer.resize(buffer.size() * 2);
  }
}

std::wstring ModuleNameFromAddress(const void* address) {
  HMODULE module = nullptr;
  if (!address ||
      !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          static_cast<LPCWSTR>(address), &module)) {
    return L"<unknown>";
  }
  wchar_t path[MAX_PATH] = {};
  const DWORD len = GetModuleFileNameW(module, path, MAX_PATH);
  std::wstring_view full(path, len);
  const size_t slash = full.find_last_of(L"\\/");
  return std::wstring(slash == std::wstring_view::npos ? full : full.substr(slash + 1));
}

bool IsWritable(const void* address) {
  MEMORY_BASIC_INFORMATION mbi = {};
  if (VirtualQuery(address, &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
  if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
  constexpr DWORD kWritable =
      PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
  return (mbi.Protect & kWritable) != 0;
}

bool ContainsNoCase(std::wstring_view haystack, std::wstring_view needle) {
  auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                        [](wchar_t a, wchar_t b) { return std::towlower(a) == std::towlower(b); });
  return it != haystack.end();
}

}  // namespace doomfix
