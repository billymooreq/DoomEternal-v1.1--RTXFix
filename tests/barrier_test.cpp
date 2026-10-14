// Проверка фичи indirect-barrier-fix через настоящий loader, без игры.
//
// Создаёт устройство и записывает в командный буфер барьеры с масками из захвата DOOM Eternal
// (EID 15778/15818/15828) и граничные случаи; ничего не отправляет на GPU. Что слой передал
// драйверу, видно в его логе (запускать с DOOMFIX_ENABLE=1 DOOMFIX_LOG_LEVEL=debug).
//
//   build\bin\doomfix_barrier_test.exe           все случаи по одному разу
//   build\bin\doomfix_barrier_test.exe --soak    плюс 4 потока ~12 с перезаписывают командные
//                                                буферы (периодический отчёт, многопоточность)

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace {

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

}  // namespace

int main(int argc, char** argv) {
  const bool soak = argc > 1 && std::strcmp(argv[1], "--soak") == 0;
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
  std::printf("device: %s, api %u.%u\n", props.deviceName, VK_API_VERSION_MAJOR(props.apiVersion),
              VK_API_VERSION_MINOR(props.apiVersion));

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
  std::printf("vkCmdPipelineBarrier2KHR (extension not enabled) -> %p (expected null)\n",
              reinterpret_cast<void*>(gdpa(device, "vkCmdPipelineBarrier2KHR")));
  std::printf("vkCmdPipelineBarrier2 -> %p, vkCmdWaitEvents2 -> %p\n",
              reinterpret_cast<void*>(CmdPipelineBarrier2), reinterpret_cast<void*>(CmdWaitEvents2));

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
  CHECK(BeginCommandBuffer(cb, &begin));

  const VkAccessFlags gameAccess =
      VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
  const VkMemoryBarrier gameMem{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, gameAccess, gameAccess};

  // 1. Барьер из захвата (EID 15778): должен получить INDIRECT_COMMAND_READ и DRAW_INDIRECT в src.
  std::printf("case 1: game barrier 15778\n");
  CmdPipelineBarrier(cb,
                     VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT |
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                     0, 1, &gameMem, 0, nullptr, 0, nullptr);

  // 2. Без DRAW_INDIRECT (как EID 15768): не трогается.
  std::printf("case 2: compute->compute, must pass through\n");
  CmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                     0, 1, &gameMem, 0, nullptr, 0, nullptr);

  // 3. Только исполнение, без барьеров памяти: добавляется глобальный VkMemoryBarrier.
  std::printf("case 3: execution-only barrier\n");
  CmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                     0, 0, nullptr, 0, nullptr, 0, nullptr);

  // 4. Buffer-барьер без нужного доступа, memory-барьер уже правильный: копируется только buffer.
  std::printf("case 4: buffer barrier missing access\n");
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

  // 5. Уже правильный барьер (нет COMPUTE в src, доступ есть): "already correct".
  std::printf("case 5: already correct\n");
  const VkMemoryBarrier xferMem{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT};
  CmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 1,
                     &xferMem, 0, nullptr, 0, nullptr);

  // 6. vkCmdWaitEvents: доступ добавляется, srcStageMask остаётся равным stageMask из SetEvent.
  std::printf("case 6: vkCmdWaitEvents\n");
  CmdSetEvent(cb, event, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  CmdWaitEvents(cb, 1, &event, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 1, &gameMem, 0, nullptr, 0, nullptr);

  if (CmdPipelineBarrier2 && CmdWaitEvents2 && CmdSetEvent2) {
    // 7. synchronization2: тот же барьер, что в случае 1.
    std::printf("case 7: vkCmdPipelineBarrier2\n");
    VkMemoryBarrier2 m2{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    m2.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    m2.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    m2.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    m2.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.memoryBarrierCount = 1;
    dep.pMemoryBarriers = &m2;
    CmdPipelineBarrier2(cb, &dep);

    // 8. vkCmdWaitEvents2 с DRAW_INDIRECT: только предупреждение в логе, без правки.
    std::printf("case 8: vkCmdWaitEvents2 (log only)\n");
    CmdSetEvent2(cb, event, &dep);
    CmdWaitEvents2(cb, 1, &event, &dep);
  }

  CHECK(EndCommandBuffer(cb));
  std::printf("recorded OK\n");

  if (soak) {
    auto ResetCommandPool =
        reinterpret_cast<PFN_vkResetCommandPool>(gdpa(device, "vkResetCommandPool"));
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
    std::printf("soak: %llu command buffers, %llu barriers with DRAW_INDIRECT expected\n",
                static_cast<unsigned long long>(recorded.load()),
                static_cast<unsigned long long>(recorded.load() * 3));
  }

  DestroyEvent(device, event, nullptr);
  DestroyBuffer(device, args, nullptr);
  DestroyCommandPool(device, pool, nullptr);
  DestroyDevice(device, nullptr);
  Load<PFN_vkDestroyInstance>(instance, "vkDestroyInstance")(instance, nullptr);
  return 0;
}
