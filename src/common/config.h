#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "common/log.h"

namespace doomfix {

// Настройки читаются один раз из переменных окружения процесса.
//
//   DOOMFIX_ENABLE=1                 включает слой (его же проверяет loader по манифесту)
//   DOOMFIX_LOG_LEVEL=debug|info|warn|error            (по умолчанию info)
//   DOOMFIX_DATA_DIR=<путь>          корень данных      (по умолчанию %LOCALAPPDATA%\doomfix)
//   DOOMFIX_LOG_DIR=<путь>           каталог логов      (по умолчанию <DATA_DIR>\logs)
//   DOOMFIX_HIDE_DEVICE_EXTENSIONS=A,B   дополнительно скрыть эти расширения устройства
struct Config {
  bool enabled = false;
  log::Level logLevel = log::Level::Info;
  std::filesystem::path dataDir;
  std::filesystem::path logDir;
  std::vector<std::string> extraHiddenDeviceExtensions;
};

const Config& GetConfig();

}  // namespace doomfix
