#pragma once

#include <sal.h>

#include <filesystem>

namespace doomfix::log {

enum class Level { Debug = 0, Info = 1, Warn = 2, Error = 3 };

// Открывает файл лога <dir>\<exe>_<дата-время>_<pid>.log. Повторные вызовы ничего не делают.
// Пока лог не открыт, Write() молча отбрасывает сообщения.
bool Open(const std::filesystem::path& dir, Level minLevel);

std::filesystem::path CurrentPath();

// Будет ли записано сообщение этого уровня (чтобы не готовить дорогие строки впустую).
bool Enabled(Level level);

void Write(Level level, _Printf_format_string_ const char* format, ...);

}  // namespace doomfix::log

#define DF_LOG_DEBUG(...) ::doomfix::log::Write(::doomfix::log::Level::Debug, __VA_ARGS__)
#define DF_LOG_INFO(...) ::doomfix::log::Write(::doomfix::log::Level::Info, __VA_ARGS__)
#define DF_LOG_WARN(...) ::doomfix::log::Write(::doomfix::log::Level::Warn, __VA_ARGS__)
#define DF_LOG_ERROR(...) ::doomfix::log::Write(::doomfix::log::Level::Error, __VA_ARGS__)
