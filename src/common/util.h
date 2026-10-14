#pragma once

#include <string>
#include <string_view>

namespace doomfix {

std::string ToUtf8(std::wstring_view text);

// Значение переменной окружения или пустая строка, если её нет.
std::wstring GetEnv(const wchar_t* name);

// Полный путь к exe текущего процесса.
std::wstring ExecutablePath();

// Имя файла модуля (без каталога), которому принадлежит адрес; "<unknown>", если не найден.
std::wstring ModuleNameFromAddress(const void* address);

// Можно ли писать по адресу (страница закоммичена и имеет права на запись).
bool IsWritable(const void* address);

bool ContainsNoCase(std::wstring_view haystack, std::wstring_view needle);

}  // namespace doomfix
