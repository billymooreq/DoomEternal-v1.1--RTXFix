# doomfix — Vulkan-слой для DOOM Eternal

Implicit Vulkan layer `VK_LAYER_DOOMFIX`. В нём две фичи:

1. **indirect-barrier-fix** исправляет барьеры перед indirect-командами. Из-за этой ошибки игры
   на NVIDIA оружие от первого лица выходит полностью чёрным, а мир местами слишком тёмный.
   Подробности ниже, в разделе [«Исправление барьеров indirect-команд»](#исправление-барьеров-indirect-команд),
   разбор захвата — в [analysis/report.md](analysis/report.md).
2. **hide-device-extensions** скрывает от приложения расширение
   `VK_AMD_memory_overallocation_behavior`:
   - убирает его из `vkEnumerateDeviceExtensionProperties`;
   - в `vkCreateDevice` удаляет его из `ppEnabledExtensionNames` и вырезает
     `VkDeviceMemoryOverallocationCreateInfoAMD` из цепочки `pNext`;
   - пишет в лог, что запросила игра и что было удалено.

Слой стоит в цепочке ближе к приложению, чем RenderDoc. Поэтому RenderDoc не видит скрытого
расширения, а в захват попадают уже исправленные барьеры.

Файлы игры слой не трогает.

## Что установить

| Компонент | Нужен? | Как поставить |
|---|---|---|
| Visual Studio 2022 Build Tools, нагрузка «Desktop development with C++» | **да** | см. команду ниже |
| CMake ≥ 3.22 и Ninja | **да**, но входят в Build Tools (компонент «C++ CMake tools for Windows») | — |
| Git | да (чтобы скачать Vulkan-Headers) | уже установлен |
| Vulkan SDK | нет, по желанию | `winget install KhronosGroup.VulkanSDK` |

```bash
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

`--includeRecommended` добавляет MSVC x64, Windows SDK и «C++ CMake tools» (CMake + Ninja).
Установщик попросит права администратора, объём около 5–7 ГБ.

Без Vulkan SDK CMake сам скачает заголовки [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers)
(тег `v1.4.341`, как у системного loader'а). Слою нужны только заголовки, `vulkan-1.lib` не нужна.
Если будете ставить SDK, учтите: **Vulkan Configurator (vkconfig)** пишет собственный файл
loader settings, а он конфликтует с упорядочиванием слоёв doomfix (см. ниже).

## Сборка

```bash
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
```

Скрипт находит Visual Studio через `vswhere`, поднимает x64 dev-окружение и собирает проект
CMake + Ninja в `build\`. Результат лежит в `build\bin\`: `doomfix_layer.dll`, `VK_LAYER_DOOMFIX.json`
и `.pdb`, а также тестовая программа `doomfix_barrier_test.exe` (её можно отключить через
`-DDOOMFIX_BUILD_TESTS=OFF`). По умолчанию собирается `RelWithDebInfo`, другую конфигурацию задаёт `-Config Release`.

Вручную из «x64 Native Tools Command Prompt»:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build
```

## Установка

```bash
powershell -ExecutionPolicy Bypass -File scripts\install.ps1
```

Права администратора не нужны. Скрипт:

1. копирует DLL и манифест в `%LOCALAPPDATA%\doomfix\layer\` (пересобранный DLL нужно
   переустановить, пока игра закрыта);
2. регистрирует манифест в `HKCU\SOFTWARE\Khronos\Vulkan\ImplicitLayers` (значение DWORD 0);
3. настраивает порядок слоёв (подробности в следующем разделе).

Удаление: `scripts\uninstall.ps1`. Логи при этом остаются.

### Почему нужен порядок слоёв и как он задаётся

Loader собирает implicit-слои сначала из `HKLM`, затем из `HKCU`, и первый найденный слой
оказывается ближе всего к приложению. RenderDoc зарегистрирован в `HKLM`, поэтому одна регистрация
в `HKCU` поставила бы doomfix **ниже** RenderDoc, и RenderDoc видел бы исходный `vkCreateDevice`.

Порядок задаётся штатным механизмом loader'а (1.3.2xx и новее, в системе стоит 1.4.341) — файлом
**loader settings**. `install.ps1` создаёт `%LOCALAPPDATA%\doomfix\loader\vk_loader_settings.json`
и регистрирует его в `HKCU\SOFTWARE\Khronos\Vulkan\LoaderSettings`:

```json
{ "file_format_version": "1.0.1",
  "settings_array": [ { "layers": [
      { "control": "auto", "name": "VK_LAYER_DOOMFIX", "path": "...\\VK_LAYER_DOOMFIX.json",
        "treat_as_implicit_manifest": true },
      { "control": "unordered_layer_location" } ] } ] }
```

- `control: auto` — слой включается по обычным правилам, то есть только при `DOOMFIX_ENABLE=1`.
- `unordered_layer_location` — все прочие слои (RenderDoc, Steam overlay, OBS) идут после doomfix
  в своём обычном порядке. Без этой записи loader отключил бы все остальные слои.
- Правило глобальное, но для приложений без `DOOMFIX_ENABLE=1` ничего не меняется. Ограничить его
  одним exe можно параметром `-GameExe "C:\Steam\steamapps\common\DOOMEternal\DOOMEternalx64vk.exe"`.
  Loader сравнивает этот путь с путём процесса с учётом регистра, поэтому вариант по умолчанию надёжнее.
- Если файл настроек уже создал vkconfig, `install.ps1` его не трогает и выводит предупреждение:
  loader использует только один такой файл.

## Запуск

Слой активен, только если у процесса игры задано `DOOMFIX_ENABLE=1`. Аварийно выключить его можно
через `DOOMFIX_DISABLE=1`.

**Через RenderDoc.** Launch Application → Executable Path:
`C:\Steam\steamapps\common\DOOMEternal\DOOMEternalx64vk.exe`, затем в Environment Variables
добавьте `DOOMFIX_ENABLE` = `1`. Если игра при запуске перезапускается через Steam и RenderDoc
теряет процесс, попробуйте добавить ещё `SteamAppId` = `782330`.

**Из Steam.** Задайте переменную пользователя и перезапустите Steam, чтобы он её унаследовал:

```bash
setx DOOMFIX_ENABLE 1
```

Убрать её потом можно так:

```bash
reg delete HKCU\Environment /v DOOMFIX_ENABLE /f
```

### Дополнительные переменные

| Переменная | Значение |
|---|---|
| `DOOMFIX_LOG_LEVEL` | `debug` / `info` (по умолчанию) / `warn` / `error` |
| `DOOMFIX_DATA_DIR` | корень данных, по умолчанию `%LOCALAPPDATA%\doomfix` |
| `DOOMFIX_LOG_DIR` | каталог логов, по умолчанию `<DATA_DIR>\logs` |
| `DOOMFIX_HIDE_DEVICE_EXTENSIONS` | дополнительные расширения для скрытия через запятую (без вырезания pNext) |
| `DOOMFIX_INDIRECT_FIX` | `on` (по умолчанию) / `off` / `aggressive`, см. следующий раздел |

## Исправление барьеров indirect-команд

**В чём ошибка игры.** Кластерный биннинг источников света, проб окружения и декалей
(compute, около EID 15761–15883 в захвате) записывает аргументы для `vkCmdDispatchIndirect`
compute-шейдером. Затем он ставит `vkCmdPipelineBarrier` с `dstStageMask = DRAW_INDIRECT`,
но без `VK_ACCESS_INDIRECT_COMMAND_READ_BIT` в `dstAccessMask`.

На NVIDIA indirect-вызов в таком случае читает устаревшие (нулевые) аргументы. Биннинг
ничего не делает, и в кластерах остаётся только солнце: локальных источников, проб и декалей нет.
На AMD indirect-аргументы читаются через тот же кэш, в который пишут шейдеры, поэтому там
ошибка не проявляется.

**Что делает фича** (только с вызовами, у которых в `dstStageMask` есть `DRAW_INDIRECT`):

| Режим `DOOMFIX_INDIRECT_FIX` | Действие |
|---|---|
| `on` (по умолчанию) | В `dstAccessMask` всех memory- и buffer-барьеров добавляется `INDIRECT_COMMAND_READ`. Если таких барьеров нет, добавляется глобальный `VkMemoryBarrier` (запись стадий src → `INDIRECT_COMMAND_READ`; для compute это `SHADER_WRITE`). WAR-случай: если в `srcStageMask` есть `COMPUTE_SHADER`, туда же добавляется `DRAW_INDIRECT`, чтобы перезапись аргументов ждала их чтения предыдущими indirect-вызовами. |
| `aggressive` | Такой барьер превращается в полный: `ALL_COMMANDS → ALL_COMMANDS`, `MEMORY_WRITE → MEMORY_READ\|MEMORY_WRITE` (у image-барьеров меняется только доступ, layout не трогается). Это запасной вариант, если точечного исправления не хватит. |
| `off` | Ничего не меняется. Барьеры только считаются, и в лог пишется, что было бы исправлено. Этот режим нужен для A/B-сравнения. |

Подробности:

- Вызовы без `DRAW_INDIRECT` передаются дальше как есть, без копирования структур. Копируется
  только тот массив барьеров, который действительно меняется (в thread_local-буфер потока записи).
- Перехватываются `vkCmdPipelineBarrier`, `vkCmdPipelineBarrier2`, `vkCmdPipelineBarrier2KHR`,
  `vkCmdWaitEvents`, `vkCmdWaitEvents2` и `vkCmdWaitEvents2KHR`. Функции, которых у устройства нет,
  слой из `vkGetDeviceProcAddr` не отдаёт.
- По захвату DOOM Eternal 1.1 игра использует **только `vkCmdPipelineBarrier`**: 79 вызовов за кадр,
  из них 3 барьера с `DRAW_INDIRECT`. Остальные функции она не вызывает, `VK_KHR_synchronization2`
  не включён. Если игра всё же вызовет какую-то из них, в логе появится строка `first call of ...`.
- У `vkCmdWaitEvents` `srcStageMask` обязан совпадать со `stageMask` из `vkCmdSetEvent`, поэтому
  там исправляется только вторая половина барьера: доступ и, в режиме `aggressive`, стадии
  назначения. `vkCmdWaitEvents2` только считается: его зависимость должна соответствовать
  `vkCmdSetEvent2`. Если там встретится `DRAW_INDIRECT`, в лог пишется `WARN`.
- Исправление работает при **записи** командного буфера. DOOM Eternal перезаписывает буферы каждый
  кадр (`ONE_TIME_SUBMIT`). Буфер, записанный один раз заранее, тоже исправляется, если запись
  прошла при активном слое. Режим читается один раз при запуске, поэтому после смены переменной
  игру нужно перезапустить.

**A/B-сравнение.** Запустите игру с `DOOMFIX_INDIRECT_FIX=off`, затем без переменной (или с `on`)
в том же месте и сравните оружие. Для захвата в RenderDoc переменную задают там же, где
`DOOMFIX_ENABLE`. В исправленном захвате у кластеров должно быть больше одного источника, а список
проб — непустым: это проверяет `analysis/scripts/s11_indirect.py`.

**Лог.** Первое исправление для каждой функции пишется подробно, затем раз в 5 секунд выводится
счётчик. Подробности по каждому барьеру пишутся только при `DOOMFIX_LOG_LEVEL=debug`.

```
INFO    indirect-barrier-fix: mode=on (DOOMFIX_INDIRECT_FIX="")
INFO  indirect-barrier-fix [on]: intercepted at record time; available on device: vkCmdPipelineBarrier=yes, vkCmdPipelineBarrier2=no, ...
INFO  indirect-barrier-fix: first call of vkCmdPipelineBarrier
INFO  indirect-barrier-fix [on]: PATCHED vkCmdPipelineBarrier in command buffer 000002CD804070D0
    before: srcStage=VERTEX_SHADER|FRAGMENT_SHADER|COMPUTE_SHADER dstStage=TOP_OF_PIPE|DRAW_INDIRECT|COMPUTE_SHADER
      memory[0]: UNIFORM_READ|SHADER_READ|SHADER_WRITE -> UNIFORM_READ|SHADER_READ|SHADER_WRITE
    after:  srcStage=DRAW_INDIRECT|VERTEX_SHADER|FRAGMENT_SHADER|COMPUTE_SHADER dstStage=TOP_OF_PIPE|DRAW_INDIRECT|COMPUTE_SHADER
      memory[0]: UNIFORM_READ|SHADER_READ|SHADER_WRITE -> INDIRECT_COMMAND_READ|UNIFORM_READ|SHADER_READ|SHADER_WRITE
INFO  indirect-barrier-fix [on]: barriers with DRAW_INDIRECT dst 19916 (+19916), patched 19914 (+19914), already correct 1; calls: vkCmdPipelineBarrier=39822
INFO  indirect-barrier-fix [on] final: barriers with DRAW_INDIRECT dst 48244 (+8215), patched 48242 (+8215), ...
```

**Проверка без игры.** `doomfix_barrier_test.exe` создаёт устройство и записывает барьеры с масками
из захвата и граничные случаи: пустой барьер, buffer-барьер, уже корректный барьер,
`vkCmdWaitEvents`, `vkCmdPipelineBarrier2`, `vkCmdWaitEvents2`. На GPU он ничего не отправляет.
С `--soak` четыре потока ещё около 12 секунд перезаписывают командные буферы, чтобы проверить
счётчики и периодический отчёт. Результат смотрите в логе `doomfix_barrier_test_*.log`.

```bash
cmd /c "set DOOMFIX_ENABLE=1&& set DOOMFIX_LOG_LEVEL=debug&& build\bin\doomfix_barrier_test.exe"
```

## Лог

`%LOCALAPPDATA%\doomfix\logs\DOOMEternalx64vk_<дата-время>_<pid>.log` — отдельный файл на каждый
запуск. Пример того, что в нём должно быть:

```
INFO  doomfix 0.2.0 (VK_LAYER_DOOMFIX) loaded into C:\Steam\...\DOOMEternalx64vk.exe, pid 1234
INFO  vkCreateInstance: app="DOOMEternal" ..., apiVersion=1.1.0
INFO  vkCreateInstance: chain below doomfix: renderdoc.dll -> ... -> vulkan-1.dll
INFO  vkCreateInstance: RenderDoc is below doomfix: it will not see hidden extensions
INFO  vkEnumerateDeviceExtensionProperties [NVIDIA GeForce RTX 4070]: N reported below, N-1 visible to app, hidden: VK_AMD_memory_overallocation_behavior
INFO  vkCreateDevice [NVIDIA GeForce RTX 4070]
INFO    pNext chain from app: ..., VK_STRUCTURE_TYPE_DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD (1000189000)
INFO    app passed VkDeviceMemoryOverallocationCreateInfoAMD: overallocationBehavior=ALLOWED (1)
INFO    REMOVED extension VK_AMD_memory_overallocation_behavior from ppEnabledExtensionNames
INFO    REMOVED 1 x VK_STRUCTURE_TYPE_DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD (1000189000) from pNext chain
INFO    device extensions requested by app (K):
INFO      VK_KHR_swapchain
INFO      VK_AMD_memory_overallocation_behavior   <-- REMOVED
INFO    passing K-1 extensions down the chain
```

Строка `chain below doomfix` показывает модули всех звеньев ниже doomfix: адреса `pfnNext*` из цепочки
loader'а переведены в имена DLL. Так проверяется требование «doomfix выше RenderDoc». Если RenderDoc
загружен, но в этой строке его нет, в лог пишется `WARN` с подсказкой перезапустить `install.ps1`.

## Структура проекта

```
CMakeLists.txt
layer/VK_LAYER_DOOMFIX.json.in       манифест слоя (имя DLL подставляет CMake)
scripts/build.ps1|install.ps1|uninstall.ps1
src/doomfix_layer.def                экспорт точек входа
src/common/                          config (env), log (файл), util (WinAPI)
src/layer/layer.cpp                  ядро: negotiate, vkGet*ProcAddr, Create/Destroy Instance/Device,
                                     перехватчики, таблицы kInstanceHooks/kDeviceHooks
src/layer/dispatch.h                 таблицы функций следующего звена (X-macro), учёт хэндлов
src/layer/device_create_edit.*       безопасная правка VkDeviceCreateInfo (расширения, pNext)
src/layer/chain_diag.*               диагностика порядка слоёв
src/features/feature.h               интерфейс фичи (события от ядра)
src/features/features.cpp            список включённых фич
src/features/hide_device_extensions.cpp   скрытие VK_AMD_memory_overallocation_behavior
src/features/indirect_barrier_fix.cpp     исправление барьеров перед indirect-командами
tests/barrier_test.cpp               проверка indirect-barrier-fix через loader (без игры)
analysis/                            разбор захвата RenderDoc: report.md, скрипты, шейдеры
```

Ядро только перехватывает вызовы и раздаёт события фичам, вся логика живёт в `src/features/`.
Помимо создания устройства, фичи получают события записи барьеров (`OnCmdPipelineBarrier`,
`OnCmdWaitEvents`, `OnCmdPipelineBarrier2`, `OnCmdWaitEvents2`) и могут подменить аргументы до
передачи вниз.

### Как добавить дамп и замену шейдеров

1. В `DOOMFIX_DEVICE_FUNCS` (`src/layer/dispatch.h`) добавить `X(CreateShaderModule)`, а также
   `X(CreateGraphicsPipelines)` / `X(CreateComputePipelines)`, если они нужны.
2. В `src/features/feature.h` добавить событие, например
   `virtual void OnCreateShaderModule(DeviceData&, VkShaderModuleCreateInfo& info, std::vector<uint32_t>& replacement)`.
3. В `src/layer/layer.cpp` написать перехватчик `CreateShaderModule`: скопировать create info,
   прогнать через фичи, вызвать `device->vk.CreateShaderModule`. Затем внести его в `kDeviceHooks`.
4. Создать `src/features/shader_dump.cpp` / `shader_replace.cpp` (каталог `GetConfig().dataDir / "shaders"`)
   и зарегистрировать их в `src/features/features.cpp`, а файлы добавить в `CMakeLists.txt`.
