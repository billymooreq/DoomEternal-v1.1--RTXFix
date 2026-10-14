#pragma once

#include <vector>

namespace doomfix {

// Логирует модули, стоящие в цепочке ниже doomfix (ближе к драйверу), по адресам функций
// pfnNext* из связного списка VkLayer*Link, и проверяет, что RenderDoc среди них.
// Слои выше doomfix (ближе к приложению) отсюда не видны, поэтому отдельно проверяется,
// загружен ли renderdoc.dll в процесс вообще.
void LogLayersBelow(const char* stage, const std::vector<const void*>& nextFunctions);

}  // namespace doomfix
