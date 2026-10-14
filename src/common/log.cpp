#include "common/log.h"

#include <windows.h>

#include <share.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

#include "common/util.h"

namespace doomfix::log {
namespace {

std::mutex g_mutex;
FILE* g_file = nullptr;
std::filesystem::path g_path;
std::atomic<int> g_minLevel{static_cast<int>(Level::Info)};
std::atomic<bool> g_opened{false};

const char* LevelTag(Level level) {
  switch (level) {
    case Level::Debug: return "DEBUG";
    case Level::Info: return "INFO ";
    case Level::Warn: return "WARN ";
    case Level::Error: return "ERROR";
  }
  return "?????";
}

std::string FormatV(const char* format, va_list args) {
  va_list copy;
  va_copy(copy, args);
  const int size = std::vsnprintf(nullptr, 0, format, copy);
  va_end(copy);
  if (size <= 0) return {};
  std::string out(static_cast<size_t>(size) + 1, '\0');
  std::vsnprintf(out.data(), out.size(), format, args);
  out.resize(static_cast<size_t>(size));
  return out;
}

}  // namespace

bool Open(const std::filesystem::path& dir, Level minLevel) {
  std::lock_guard lock(g_mutex);
  if (g_file) return true;
  g_minLevel = static_cast<int>(minLevel);

  std::error_code ec;
  std::filesystem::create_directories(dir, ec);

  SYSTEMTIME t;
  GetLocalTime(&t);
  wchar_t name[160];
  swprintf_s(name, L"%ls_%04u%02u%02u-%02u%02u%02u_%lu.log",
             std::filesystem::path(ExecutablePath()).stem().c_str(), t.wYear, t.wMonth, t.wDay,
             t.wHour, t.wMinute, t.wSecond, GetCurrentProcessId());
  g_path = dir / name;
  // _SH_DENYWR: лог можно читать, пока игра запущена.
  g_file = _wfsopen(g_path.c_str(), L"wb", _SH_DENYWR);
  g_opened = g_file != nullptr;
  return g_file != nullptr;
}

std::filesystem::path CurrentPath() {
  std::lock_guard lock(g_mutex);
  return g_path;
}

bool Enabled(Level level) {
  return static_cast<int>(level) >= g_minLevel.load(std::memory_order_relaxed) &&
         g_opened.load(std::memory_order_relaxed);
}

void Write(Level level, const char* format, ...) {
  if (static_cast<int>(level) < g_minLevel.load(std::memory_order_relaxed)) return;

  va_list args;
  va_start(args, format);
  const std::string message = FormatV(format, args);
  va_end(args);

  SYSTEMTIME t;
  GetLocalTime(&t);
  char prefix[64];
  std::snprintf(prefix, sizeof(prefix), "%02u:%02u:%02u.%03u [%5lu] %s ", t.wHour, t.wMinute,
                t.wSecond, t.wMilliseconds, GetCurrentThreadId(), LevelTag(level));

  std::lock_guard lock(g_mutex);
  if (!g_file) return;
  std::fputs(prefix, g_file);
  std::fputs(message.c_str(), g_file);
  std::fputc('\n', g_file);
  std::fflush(g_file);
}

}  // namespace doomfix::log
