// Проверка фичи indirect-barrier-fix через настоящий loader, без игры.
//
// Тест запускает сам себя (--child) по разу для каждого режима DOOMFIX_INDIRECT_FIX (on, strict,
// aggressive, off) с DOOMFIX_ENABLE=1, DOOMFIX_LOG_LEVEL=debug и отдельным DOOMFIX_LOG_DIR.
// Дочерний процесс создаёт устройство и записывает в командный буфер барьеры с масками из захвата
// DOOM Eternal (EID 15778/15818/15828) и граничные случаи; ничего не отправляет на GPU. Родитель
// читает лог слоя и сравнивает, что слой передал драйверу («after:»), с таблицей ожиданий ниже.
// Проверяется установленный слой (install.bat), а не DLL из build\bin.
// Заодно печатает, видно ли приложению VK_AMD_memory_overallocation_behavior: по умолчанию да,
// с DOOMFIX_HIDE_OVERALLOCATION=1 — нет (если драйвер вообще его предоставляет).
//
//   build\bin\doomfix_barrier_test.exe           все режимы, код выхода 0 = всё совпало
//   build\bin\doomfix_barrier_test.exe --soak    плюс прогон в режиме on, где 4 потока ~12 с
//                                                перезаписывают командные буферы (счётчики,
//                                                периодический отчёт, многопоточность)

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>
#include <thread>
#include <vector>

namespace {

namespace fs = std::filesystem;

PFN_vkGetInstanceProcAddr gipa;

#define CHECK(x)                                                     \
  do {                                                               \
    VkResult r_ = (x);                                               \
    if (r_ != VK_SUCCESS) {                                          \
      std::printf("FAILED %s -> %d (line %d)\n", #x, r_, __LINE__); \
      std::exit(1);                                                  \
    }                                                                \
  } while (0)

template <typename T>
T Load(VkInstance instance, const char* name) {
  return reinterpret_cast<T>(gipa(instance, name));
}

// --- Дочерний процесс: записывает барьеры --------------------------------------------------------

// soakCountFile: если не пусто, вместо обычных случаев идёт soak, а число записанных
// командных буферов пишется в этот файл.
int RunChild(const fs::path& soakCountFile) {
  const bool soak = !soakCountFile.empty();
  HMODULE loader = LoadLibraryW(L"vulkan-1.dll");
  if (!loader) {
    std::printf("vulkan-1.dll not found\n");
    return 1;
  }
  gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(loader, "vkGetInstanceProcAddr"));

  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "doomfix_barrier_test";
  app.apiVersion = VK_API_VERSION_1_3;
  VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ici.pApplicationInfo = &app;
  VkInstance instance;
  CHECK(Load<PFN_vkCreateInstance>(nullptr, "vkCreateInstance")(&ici, nullptr, &instance));

  uint32_t count = 0;
  auto enumPd = Load<PFN_vkEnumeratePhysicalDevices>(instance, "vkEnumeratePhysicalDevices");
  enumPd(instance, &count, nullptr);
  std::vector<VkPhysicalDevice> pds(count);
  enumPd(instance, &count, pds.data());
  VkPhysicalDevice pd = pds.at(0);
  VkPhysicalDeviceProperties props;
  Load<PFN_vkGetPhysicalDeviceProperties>(instance, "vkGetPhysicalDeviceProperties")(pd, &props);
  std::printf("  device: %s, api %u.%u\n", props.deviceName, VK_API_VERSION_MAJOR(props.apiVersion),
              VK_API_VERSION_MINOR(props.apiVersion));

  // hide-device-extensions: VK_AMD_memory_overallocation_behavior must be visible by default and hidden
  // only with DOOMFIX_HIDE_OVERALLOCATION=1 (if the driver exposes it at all).
  auto enumExt = Load<PFN_vkEnumerateDeviceExtensionProperties>(instance, "vkEnumerateDeviceExtensionProperties");
  uint32_t extCount = 0;
  CHECK(enumExt(pd, nullptr, &extCount, nullptr));
  std::vector<VkExtensionProperties> exts(extCount);
  CHECK(enumExt(pd, nullptr, &extCount, exts.data()));
  bool overallocation = false;
  for (const VkExtensionProperties& e : exts) {
    overallocation |= std::strcmp(e.extensionName, VK_AMD_MEMORY_OVERALLOCATION_BEHAVIOR_EXTENSION_NAME) == 0;
  }
  std::printf("  %s visible to the app: %s\n", VK_AMD_MEMORY_OVERALLOCATION_BEHAVIOR_EXTENSION_NAME,
              overallocation ? "yes" : "no");

  // Vulkan 1.3 с synchronization2, чтобы проверить и vkCmdPipelineBarrier2 / vkCmdWaitEvents2.
  VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  f13.synchronization2 = VK_TRUE;
  const float prio = 1.0f;
  VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  qci.queueFamilyIndex = 0;
  qci.queueCount = 1;
  qci.pQueuePriorities = &prio;
  VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  dci.pNext = &f13;
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &qci;
  VkDevice device;
  CHECK(Load<PFN_vkCreateDevice>(instance, "vkCreateDevice")(pd, &dci, nullptr, &device));

  auto gdpa = Load<PFN_vkGetDeviceProcAddr>(instance, "vkGetDeviceProcAddr");
#define DEV(name) auto name = reinterpret_cast<PFN_vk##name>(gdpa(device, "vk" #name))
  DEV(CreateCommandPool);
  DEV(AllocateCommandBuffers);
  DEV(BeginCommandBuffer);
  DEV(EndCommandBuffer);
  DEV(ResetCommandPool);
  DEV(CmdPipelineBarrier);
  DEV(CmdPipelineBarrier2);
  DEV(CmdWaitEvents);
  DEV(CmdWaitEvents2);
  DEV(CmdSetEvent);
  DEV(CmdSetEvent2);
  DEV(CreateEvent);
  DEV(CreateBuffer);
  DEV(DestroyBuffer);
  DEV(DestroyEvent);
  DEV(DestroyCommandPool);
  DEV(DestroyDevice);
#undef DEV
  if (gdpa(device, "vkCmdPipelineBarrier2KHR")) {
    std::printf("  FAIL: vkCmdPipelineBarrier2KHR is not null although the extension is not enabled\n");
    return 1;
  }
  if (!CmdPipelineBarrier2 || !CmdWaitEvents2 || !CmdSetEvent2) {
    std::printf("  FAIL: the device has no synchronization2 entry points\n");
    return 1;
  }

  VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pci.queueFamilyIndex = 0;
  VkCommandPool pool;
  CHECK(CreateCommandPool(device, &pci, nullptr, &pool));
  VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cai.commandPool = pool;
  cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cai.commandBufferCount = 1;
  VkCommandBuffer cb;
  CHECK(AllocateCommandBuffers(device, &cai, &cb));

  VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bci.size = 224;
  bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  VkBuffer args;
  CHECK(CreateBuffer(device, &bci, nullptr, &args));
  VkEventCreateInfo eci{VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};
  VkEvent event;
  CHECK(CreateEvent(device, &eci, nullptr, &event));

  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  const VkAccessFlags gameAccess =
      VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
  const VkMemoryBarrier gameMem{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, gameAccess, gameAccess};

  if (!soak) {
    // Номера случаев совпадают с таблицей ожиданий в родителе (kExpected).
    CHECK(BeginCommandBuffer(cb, &begin));

    // 1. Барьер из захвата (EID 15778).
    CmdPipelineBarrier(cb,
                       VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT |
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       0, 1, &gameMem, 0, nullptr, 0, nullptr);

    // 2. Без DRAW_INDIRECT (как EID 15768): не трогается ни в одном режиме.
    CmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       0, 1, &gameMem, 0, nullptr, 0, nullptr);

    // 3. Только исполнение, без барьеров памяти: добавляется глобальный VkMemoryBarrier.
    CmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                       0, 0, nullptr, 0, nullptr, 0, nullptr);

    // 4. Buffer-барьер без нужного доступа, memory-барьер уже правильный: копируется только buffer.
    const VkMemoryBarrier okMem{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT,
                                VK_ACCESS_INDIRECT_COMMAND_READ_BIT};
    VkBufferMemoryBarrier bufBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    bufBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bufBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    bufBarrier.srcQueueFamilyIndex = bufBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufBarrier.buffer = args;
    bufBarrier.size = VK_WHOLE_SIZE;
    CmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 1,
                       &okMem, 1, &bufBarrier, 0, nullptr);

    // 5. Полностью правильный барьер (нет COMPUTE в src, доступ есть).
    const VkMemoryBarrier xferMem{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                  VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT};
    CmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 1,
                       &xferMem, 0, nullptr, 0, nullptr);

    // 6. Доступ уже есть, но COMPUTE в src без DRAW_INDIRECT (как барьеры актуальной версии игры):
    //    on оставляет как есть, strict добавляет WAR-стадию.
    CmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                       0, 1, &okMem, 0, nullptr, 0, nullptr);

    // 7. vkCmdWaitEvents: доступ добавляется, srcStageMask остаётся равным stageMask из SetEvent.
    CmdSetEvent(cb, event, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    CmdWaitEvents(cb, 1, &event, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                  VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 1, &gameMem, 0, nullptr, 0, nullptr);

    // 8. synchronization2: тот же барьер, что в случае 1.
    VkMemoryBarrier2 m2{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    m2.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    m2.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    m2.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    m2.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.memoryBarrierCount = 1;
    dep.pMemoryBarriers = &m2;
    CmdPipelineBarrier2(cb, &dep);

    // 9. vkCmdWaitEvents2 с DRAW_INDIRECT: только предупреждение в логе, без правки.
    CmdSetEvent2(cb, event, &dep);
    CmdWaitEvents2(cb, 1, &event, &dep);

    CHECK(EndCommandBuffer(cb));
  } else {
    std::atomic<uint64_t> recorded{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
      threads.emplace_back([&] {
        VkCommandPool p;
        CHECK(CreateCommandPool(device, &pci, nullptr, &p));
        VkCommandBufferAllocateInfo a = cai;
        a.commandPool = p;
        VkCommandBuffer c;
        CHECK(AllocateCommandBuffers(device, &a, &c));
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(12);
        while (std::chrono::steady_clock::now() < end) {
          CHECK(ResetCommandPool(device, p, 0));
          CHECK(BeginCommandBuffer(c, &begin));
          for (int i = 0; i < 3; ++i) {  // три барьера биннинга на кадр, как в игре
            CmdPipelineBarrier(c, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT |
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               0, 1, &gameMem, 0, nullptr, 0, nullptr);
            CmdPipelineBarrier(c, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &gameMem, 0, nullptr,
                               0, nullptr);
          }
          CHECK(EndCommandBuffer(c));
          recorded.fetch_add(1, std::memory_order_relaxed);
          std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        DestroyCommandPool(device, p, nullptr);
      });
    }
    for (auto& t : threads) t.join();
    std::ofstream(soakCountFile) << recorded.load();
  }

  DestroyEvent(device, event, nullptr);
  DestroyBuffer(device, args, nullptr);
  DestroyCommandPool(device, pool, nullptr);
  DestroyDevice(device, nullptr);  // слой пишет итоговый отчёт ("final")
  Load<PFN_vkDestroyInstance>(instance, "vkDestroyInstance")(instance, nullptr);
  return 0;
}

// --- Ожидания ------------------------------------------------------------------------------------
// after: что слой передал драйверу, в формате лога слоя; строки продолжения склеены через " | ",
// адрес буфера заменён на <buf>. Случаи без записи в логе (слой их не трогал) в таблицу не входят.

struct Expected {
  int testCase;
  const char* api;
  const char* after;
};

#define GAME_ACCESS "UNIFORM_READ|SHADER_READ|SHADER_WRITE"
#define GAME_ACCESS_FIXED "INDIRECT_COMMAND_READ|" GAME_ACCESS
#define GAME_SRC "VERTEX_SHADER|FRAGMENT_SHADER|COMPUTE_SHADER"
#define GAME_DST "TOP_OF_PIPE|DRAW_INDIRECT|COMPUTE_SHADER"
#define FULL_V1 "srcStage=ALL_COMMANDS dstStage=ALL_COMMANDS | memory[0]: MEMORY_WRITE -> MEMORY_READ|MEMORY_WRITE"
#define BUF "buffer[0] <buf> +0/WHOLE: "

// on: только INDIRECT_COMMAND_READ в dstAccessMask, srcStageMask как у игры.
const Expected kOn[] = {
    {1, "vkCmdPipelineBarrier", "srcStage=" GAME_SRC " dstStage=" GAME_DST " | memory[0]: " GAME_ACCESS " -> " GAME_ACCESS_FIXED},
    {3, "vkCmdPipelineBarrier", "srcStage=COMPUTE_SHADER dstStage=DRAW_INDIRECT | memory[0]: SHADER_WRITE -> INDIRECT_COMMAND_READ"},
    {4, "vkCmdPipelineBarrier", "srcStage=TRANSFER dstStage=DRAW_INDIRECT | memory[0]: SHADER_WRITE -> INDIRECT_COMMAND_READ | " BUF "SHADER_WRITE -> INDIRECT_COMMAND_READ|SHADER_READ"},
    {7, "vkCmdWaitEvents", "srcStage=COMPUTE_SHADER dstStage=DRAW_INDIRECT | memory[0]: " GAME_ACCESS " -> " GAME_ACCESS_FIXED},
    {8, "vkCmdPipelineBarrier2", "dependencyFlags=0 | memory[0]: COMPUTE_SHADER / SHADER_WRITE -> DRAW_INDIRECT|COMPUTE_SHADER / INDIRECT_COMMAND_READ|SHADER_READ"},
};

// strict: то же плюс DRAW_INDIRECT в srcStageMask при COMPUTE_SHADER (кроме vkCmdWaitEvents).
const Expected kStrict[] = {
    {1, "vkCmdPipelineBarrier", "srcStage=DRAW_INDIRECT|" GAME_SRC " dstStage=" GAME_DST " | memory[0]: " GAME_ACCESS " -> " GAME_ACCESS_FIXED},
    {3, "vkCmdPipelineBarrier", "srcStage=DRAW_INDIRECT|COMPUTE_SHADER dstStage=DRAW_INDIRECT | memory[0]: SHADER_WRITE -> INDIRECT_COMMAND_READ"},
    {4, "vkCmdPipelineBarrier", "srcStage=TRANSFER dstStage=DRAW_INDIRECT | memory[0]: SHADER_WRITE -> INDIRECT_COMMAND_READ | " BUF "SHADER_WRITE -> INDIRECT_COMMAND_READ|SHADER_READ"},
    {6, "vkCmdPipelineBarrier", "srcStage=DRAW_INDIRECT|COMPUTE_SHADER dstStage=DRAW_INDIRECT | memory[0]: SHADER_WRITE -> INDIRECT_COMMAND_READ"},
    {7, "vkCmdWaitEvents", "srcStage=COMPUTE_SHADER dstStage=DRAW_INDIRECT | memory[0]: " GAME_ACCESS " -> " GAME_ACCESS_FIXED},
    {8, "vkCmdPipelineBarrier2", "dependencyFlags=0 | memory[0]: DRAW_INDIRECT|COMPUTE_SHADER / SHADER_WRITE -> DRAW_INDIRECT|COMPUTE_SHADER / INDIRECT_COMMAND_READ|SHADER_READ"},
};

// aggressive: каждый барьер с DRAW_INDIRECT в dst становится полным.
const Expected kAggressive[] = {
    {1, "vkCmdPipelineBarrier", FULL_V1},
    {3, "vkCmdPipelineBarrier", FULL_V1},
    {4, "vkCmdPipelineBarrier", FULL_V1 " | " BUF "MEMORY_WRITE -> MEMORY_READ|MEMORY_WRITE"},
    {5, "vkCmdPipelineBarrier", FULL_V1},
    {6, "vkCmdPipelineBarrier", FULL_V1},
    {7, "vkCmdWaitEvents", "srcStage=COMPUTE_SHADER dstStage=ALL_COMMANDS | memory[0]: " GAME_ACCESS " -> MEMORY_READ|MEMORY_WRITE"},
    {8, "vkCmdPipelineBarrier2", "dependencyFlags=0 | memory[0]: ALL_COMMANDS / MEMORY_WRITE -> ALL_COMMANDS / MEMORY_READ|MEMORY_WRITE"},
};

// off: те же барьеры, что правит on, только отмечаются ("WOULD PATCH") и уходят без изменений.
const Expected kOff[] = {
    {1, "vkCmdPipelineBarrier", "srcStage=" GAME_SRC " dstStage=" GAME_DST " | memory[0]: " GAME_ACCESS " -> " GAME_ACCESS},
    {3, "vkCmdPipelineBarrier", "srcStage=COMPUTE_SHADER dstStage=DRAW_INDIRECT"},
    {4, "vkCmdPipelineBarrier", "srcStage=TRANSFER dstStage=DRAW_INDIRECT | memory[0]: SHADER_WRITE -> INDIRECT_COMMAND_READ | " BUF "SHADER_WRITE -> SHADER_READ"},
    {7, "vkCmdWaitEvents", "srcStage=COMPUTE_SHADER dstStage=DRAW_INDIRECT | memory[0]: " GAME_ACCESS " -> " GAME_ACCESS},
    {8, "vkCmdPipelineBarrier2", "dependencyFlags=0 | memory[0]: COMPUTE_SHADER / SHADER_WRITE -> DRAW_INDIRECT|COMPUTE_SHADER / SHADER_READ"},
};

struct ModeRun {
  const char* mode;
  const Expected* expected;
  size_t count;
};

const ModeRun kModes[] = {
    {"on", kOn, std::size(kOn)},
    {"strict", kStrict, std::size(kStrict)},
    {"aggressive", kAggressive, std::size(kAggressive)},
    {"off", kOff, std::size(kOff)},
};

// --- Родитель: запуск дочерних процессов и разбор лога ---------------------------------------------

struct Patch {
  std::string mode, verb, api, after;
};

int g_failures = 0;

void Fail(const std::string& message) {
  std::printf("  FAIL: %s\n", message.c_str());
  ++g_failures;
}

bool RunSelf(const fs::path& logDir, const char* mode, const char* logLevel, const wchar_t* args) {
  fs::create_directories(logDir);
  SetEnvironmentVariableW(L"DOOMFIX_ENABLE", L"1");
  SetEnvironmentVariableW(L"DOOMFIX_LOG_DIR", logDir.c_str());
  SetEnvironmentVariableA("DOOMFIX_INDIRECT_FIX", mode);
  SetEnvironmentVariableA("DOOMFIX_LOG_LEVEL", logLevel);

  std::fflush(stdout);  // дочерний процесс пишет в ту же консоль
  wchar_t exe[MAX_PATH];
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  std::wstring cmd = L"\"" + std::wstring(exe) + L"\" " + args;
  STARTUPINFOW si{sizeof(si)};
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(exe, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
    Fail("cannot start a child process, error " + std::to_string(GetLastError()));
    return false;
  }
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  if (code != 0) {
    Fail("child process exited with code " + std::to_string(code));
    return false;
  }
  return true;
}

// Читает единственный лог слоя в каталоге.
bool ReadLog(const fs::path& logDir, std::vector<std::string>& lines) {
  std::vector<fs::path> logs;
  for (const auto& e : fs::directory_iterator(logDir)) {
    if (e.path().extension() == L".log") logs.push_back(e.path());
  }
  if (logs.size() != 1) {
    Fail("expected one layer log in " + logDir.string() + ", found " + std::to_string(logs.size()) +
         " (is the layer installed? run install.bat)");
    return false;
  }
  std::ifstream in(logs[0]);
  for (std::string line; std::getline(in, line);) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    lines.push_back(line);
  }
  return true;
}

std::vector<Patch> ParsePatches(const std::vector<std::string>& lines) {
  static const std::regex header(R"(indirect-barrier-fix \[(\w+)\]: (PATCHED|WOULD PATCH) .*?(vk\w+) in command buffer)");
  static const std::regex bufferHandle(R"((buffer\[\d+\]) \S+ )");
  std::vector<Patch> out;
  for (size_t i = 0; i < lines.size(); ++i) {
    std::smatch m;
    if (!std::regex_search(lines[i], m, header)) continue;
    Patch p{m[1], m[2], m[3], {}};
    size_t j = i + 1;
    while (j < lines.size() && lines[j].rfind("    after:  ", 0) != 0) ++j;
    if (j == lines.size()) break;
    p.after = lines[j].substr(12);
    for (++j; j < lines.size() && lines[j].rfind("      ", 0) == 0; ++j) {
      p.after += " | " + lines[j].substr(6);
    }
    p.after = std::regex_replace(p.after, bufferHandle, "$1 <buf> ");
    out.push_back(p);
    i = j - 1;
  }
  return out;
}

void CheckMode(const ModeRun& run, const fs::path& logDir) {
  std::vector<std::string> lines;
  if (!ReadLog(logDir, lines)) return;
  const std::vector<Patch> patches = ParsePatches(lines);
  const std::string verb = std::strcmp(run.mode, "off") == 0 ? "WOULD PATCH" : "PATCHED";
  for (size_t i = 0; i < run.count || i < patches.size(); ++i) {
    if (i >= run.count) {
      Fail("unexpected patch of " + patches[i].api + ": " + patches[i].after);
      continue;
    }
    const Expected& e = run.expected[i];
    const std::string where = "case " + std::to_string(e.testCase) + " (" + e.api + ")";
    if (i >= patches.size()) {
      Fail(where + ": the layer did not touch it");
      continue;
    }
    const Patch& p = patches[i];
    if (p.mode != run.mode || p.verb != verb || p.api != e.api) {
      Fail(where + ": got [" + p.mode + "] " + p.verb + " " + p.api);
    } else if (p.after != e.after) {
      Fail(where + ":\n        expected: " + e.after + "\n        got:      " + p.after);
    } else {
      std::printf("  ok   %s\n", where.c_str());
    }
  }
  bool warned = false;
  for (const std::string& line : lines) {
    warned |= line.find("vkCmdWaitEvents2 with DRAW_INDIRECT in dstStageMask is NOT patched") !=
              std::string::npos;
  }
  if (warned) {
    std::printf("  ok   case 9 (vkCmdWaitEvents2 left unchanged, warning logged)\n");
  } else {
    Fail("case 9: no warning about vkCmdWaitEvents2");
  }
}

// Итоговый отчёт слоя должен насчитать ровно 3 барьера на каждый записанный командный буфер.
void CheckSoak(const fs::path& logDir) {
  uint64_t recorded = 0;
  std::ifstream(logDir / "soak_count.txt") >> recorded;
  std::vector<std::string> lines;
  if (!ReadLog(logDir, lines)) return;
  static const std::regex finalReport(R"(\[on\] final: barriers with DRAW_INDIRECT dst (\d+) .*patched (\d+) )");
  for (const std::string& line : lines) {
    std::smatch m;
    if (!std::regex_search(line, m, finalReport)) continue;
    const uint64_t indirect = std::stoull(m[1]), patched = std::stoull(m[2]);
    if (recorded > 0 && indirect == recorded * 3 && patched == recorded * 3) {
      std::printf("  ok   %llu command buffers, %llu barriers patched\n",
                  static_cast<unsigned long long>(recorded), static_cast<unsigned long long>(patched));
    } else {
      Fail("recorded " + std::to_string(recorded) + " command buffers (expected " +
           std::to_string(recorded * 3) + " barriers), the layer counted " + m[1].str() +
           ", patched " + m[2].str());
    }
    return;
  }
  Fail("no final report in the soak log");
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc > 1 && std::wcscmp(argv[1], L"--child") == 0) return RunChild({});
  if (argc > 2 && std::wcscmp(argv[1], L"--child-soak") == 0) return RunChild(argv[2]);
  const bool soak = argc > 1 && std::wcscmp(argv[1], L"--soak") == 0;

  wchar_t temp[MAX_PATH];
  GetTempPathW(MAX_PATH, temp);
  const fs::path root =
      fs::path(temp) / L"doomfix_barrier_test" / std::to_wstring(GetCurrentProcessId());

  for (const ModeRun& run : kModes) {
    std::printf("mode %s\n", run.mode);
    const fs::path dir = root / run.mode;
    if (RunSelf(dir, run.mode, "debug", L"--child")) CheckMode(run, dir);
  }
  if (soak) {
    std::printf("soak (mode on, 4 threads, ~12 s)\n");
    const fs::path dir = root / L"soak";
    const std::wstring args = L"--child-soak \"" + (dir / L"soak_count.txt").wstring() + L"\"";
    if (RunSelf(dir, "on", "info", args.c_str())) CheckSoak(dir);
  }

  if (g_failures) {
    std::printf("\n%d check(s) FAILED. Layer logs: %ls\n", g_failures, root.c_str());
    return 1;
  }
  std::error_code ec;
  fs::remove_all(root, ec);
  std::printf("\nALL PASSED\n");
  return 0;
}
