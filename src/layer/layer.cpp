// Ядро слоя: согласование интерфейса с loader'ом, vkGet*ProcAddr, построение цепочки вызовов,
// учёт VkInstance/VkDevice и раздача событий фичам (src/features).

#include <vulkan/vk_layer.h>

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "common/config.h"
#include "common/log.h"
#include "common/util.h"
#include "features/feature.h"
#include "layer/chain_diag.h"
#include "layer/device_create_edit.h"
#include "layer/dispatch.h"
#include "layer/vk_util.h"

extern "C" {
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL DoomFix_GetInstanceProcAddr(VkInstance instance,
                                                                     const char* pName);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL DoomFix_GetDeviceProcAddr(VkDevice device,
                                                                   const char* pName);
}

namespace doomfix {
namespace {

std::once_flag g_initOnce;
std::vector<std::unique_ptr<Feature>> g_features;
HandleMap<InstanceData> g_instances;
HandleMap<DeviceData> g_devices;

void EnsureInitialized() {
  std::call_once(g_initOnce, [] {
    const Config& config = GetConfig();
    // Без DOOMFIX_ENABLE=1 (например, если слой включили принудительно) работаем прозрачно.
    if (!config.enabled) return;
    log::Open(config.logDir, config.logLevel);
    DF_LOG_INFO("doomfix %s (%s) loaded into %s, pid %lu", DOOMFIX_VERSION, DOOMFIX_LAYER_NAME,
                ToUtf8(ExecutablePath()).c_str(), GetCurrentProcessId());
    g_features = CreateFeatures(config);
    for (const auto& feature : g_features) DF_LOG_INFO("feature: %s", feature->Name());
  });
}

bool IsDeviceExtensionHidden(std::string_view name) {
  return std::any_of(g_features.begin(), g_features.end(),
                     [&](const auto& f) { return f->HidesDeviceExtension(name); });
}

// --- Логирование ----------------------------------------------------------------------------

void LogNameList(const char* title, uint32_t count, const char* const* names,
                 const DeviceCreateEdit* edit = nullptr) {
  DF_LOG_INFO("  %s (%u):", title, count);
  for (uint32_t i = 0; i < count; ++i) {
    const bool removed = edit && !edit->HasExtension(names[i]);
    DF_LOG_INFO("    %s%s", names[i], removed ? "   <-- REMOVED" : "");
  }
}

void LogPNextChain(const char* title, const void* pNext) {
  std::string chain;
  for (auto* s = static_cast<const VkBaseInStructure*>(pNext); s; s = s->pNext) {
    if (s->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO) continue;  // служебная loader'а
    if (!chain.empty()) chain += ", ";
    chain += DescribeStructureType(s->sType);
  }
  DF_LOG_INFO("  %s: %s", title, chain.empty() ? "(empty)" : chain.c_str());
}

// --- Цепочка слоёв ------------------------------------------------------------------------

template <typename CreateInfo, typename LinkInfo>
LinkInfo* FindLinkInfo(const CreateInfo* createInfo, VkStructureType type) {
  for (auto* s = static_cast<const VkBaseInStructure*>(createInfo->pNext); s; s = s->pNext) {
    if (s->sType != type) continue;
    auto* info = reinterpret_cast<LinkInfo*>(const_cast<VkBaseInStructure*>(s));
    if (info->function == VK_LAYER_LINK_INFO) return info;
  }
  return nullptr;
}

// --- Instance -----------------------------------------------------------------------------

VKAPI_ATTR VkResult VKAPI_CALL CreateInstance(const VkInstanceCreateInfo* pCreateInfo,
                                              const VkAllocationCallbacks* pAllocator,
                                              VkInstance* pInstance) {
  EnsureInitialized();

  auto* chain = FindLinkInfo<VkInstanceCreateInfo, VkLayerInstanceCreateInfo>(
      pCreateInfo, VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO);
  if (!chain || !chain->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;

  const PFN_vkGetInstanceProcAddr nextGipa = chain->u.pLayerInfo->pfnNextGetInstanceProcAddr;
  auto nextCreate =
      reinterpret_cast<PFN_vkCreateInstance>(nextGipa(VK_NULL_HANDLE, "vkCreateInstance"));
  if (!nextCreate) return VK_ERROR_INITIALIZATION_FAILED;

  if (const VkApplicationInfo* app = pCreateInfo->pApplicationInfo) {
    DF_LOG_INFO("vkCreateInstance: app=\"%s\" v%u, engine=\"%s\" v%u, apiVersion=%s",
                app->pApplicationName ? app->pApplicationName : "", app->applicationVersion,
                app->pEngineName ? app->pEngineName : "", app->engineVersion,
                ApiVersionString(app->apiVersion).c_str());
  } else {
    DF_LOG_INFO("vkCreateInstance: no VkApplicationInfo");
  }
  LogNameList("instance extensions requested by app", pCreateInfo->enabledExtensionCount,
              pCreateInfo->ppEnabledExtensionNames);
  LogNameList("instance layers requested by app", pCreateInfo->enabledLayerCount,
              pCreateInfo->ppEnabledLayerNames);

  std::vector<const void*> below;
  for (const VkLayerInstanceLink* link = chain->u.pLayerInfo; link; link = link->pNext) {
    below.push_back(reinterpret_cast<const void*>(link->pfnNextGetInstanceProcAddr));
  }
  LogLayersBelow("vkCreateInstance", below);

  chain->u.pLayerInfo = chain->u.pLayerInfo->pNext;
  const VkResult result = nextCreate(pCreateInfo, pAllocator, pInstance);
  DF_LOG_INFO("vkCreateInstance -> %d", static_cast<int>(result));
  if (result != VK_SUCCESS) return result;

  auto data = std::make_unique<InstanceData>();
  data->handle = *pInstance;
  data->vk.Load(*pInstance, nextGipa);
  g_instances.Insert(DispatchKey(*pInstance), std::move(data));
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL DestroyInstance(VkInstance instance,
                                           const VkAllocationCallbacks* pAllocator) {
  if (!instance) return;
  std::unique_ptr<InstanceData> data = g_instances.Remove(DispatchKey(instance));
  if (!data) return;
  DF_LOG_INFO("vkDestroyInstance");
  data->vk.DestroyInstance(instance, pAllocator);
}

std::string PhysicalDeviceName(InstanceData& instance, VkPhysicalDevice physicalDevice) {
  VkPhysicalDeviceProperties props = {};
  instance.vk.GetPhysicalDeviceProperties(physicalDevice, &props);
  return props.deviceName;
}

VKAPI_ATTR VkResult VKAPI_CALL EnumerateDeviceExtensionProperties(
    VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount,
    VkExtensionProperties* pProperties) {
  // Собственных расширений у слоя нет.
  if (pLayerName && std::strcmp(pLayerName, DOOMFIX_LAYER_NAME) == 0) {
    *pPropertyCount = 0;
    return VK_SUCCESS;
  }

  InstanceData* instance = g_instances.Find(DispatchKey(physicalDevice));
  if (!instance) return VK_ERROR_INITIALIZATION_FAILED;
  const auto next = instance->vk.EnumerateDeviceExtensionProperties;
  if (pLayerName) return next(physicalDevice, pLayerName, pPropertyCount, pProperties);

  // Полный список от нижних звеньев (двухфазный запрос, повтор при VK_INCOMPLETE).
  std::vector<VkExtensionProperties> all;
  VkResult result;
  do {
    uint32_t count = 0;
    result = next(physicalDevice, nullptr, &count, nullptr);
    if (result != VK_SUCCESS) return result;
    all.resize(count);
    result = next(physicalDevice, nullptr, &count, all.data());
    all.resize(count);
  } while (result == VK_INCOMPLETE);
  if (result != VK_SUCCESS) return result;

  std::vector<VkExtensionProperties> visible;
  std::string hidden;
  visible.reserve(all.size());
  for (const VkExtensionProperties& ext : all) {
    if (IsDeviceExtensionHidden(ext.extensionName)) {
      if (!hidden.empty()) hidden += ", ";
      hidden += ext.extensionName;
    } else {
      visible.push_back(ext);
    }
  }

  bool firstTime;
  {
    std::lock_guard lock(instance->mutex);
    firstTime = instance->extensionListLogged.insert(physicalDevice).second;
  }
  if (firstTime) {
    DF_LOG_INFO("vkEnumerateDeviceExtensionProperties [%s]: %zu reported below, %zu visible "
                "to app, hidden: %s",
                PhysicalDeviceName(*instance, physicalDevice).c_str(), all.size(), visible.size(),
                hidden.empty() ? "(none - not exposed by driver)" : hidden.c_str());
  }

  if (!pProperties) {
    *pPropertyCount = static_cast<uint32_t>(visible.size());
    return VK_SUCCESS;
  }
  const uint32_t copied = std::min(*pPropertyCount, static_cast<uint32_t>(visible.size()));
  std::copy_n(visible.begin(), copied, pProperties);
  *pPropertyCount = copied;
  return copied < visible.size() ? VK_INCOMPLETE : VK_SUCCESS;
}

// --- Device -------------------------------------------------------------------------------

VKAPI_ATTR VkResult VKAPI_CALL CreateDevice(VkPhysicalDevice physicalDevice,
                                            const VkDeviceCreateInfo* pCreateInfo,
                                            const VkAllocationCallbacks* pAllocator,
                                            VkDevice* pDevice) {
  InstanceData* instance = g_instances.Find(DispatchKey(physicalDevice));
  auto* chain = FindLinkInfo<VkDeviceCreateInfo, VkLayerDeviceCreateInfo>(
      pCreateInfo, VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO);
  if (!instance || !chain || !chain->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;

  const PFN_vkGetInstanceProcAddr nextGipa = chain->u.pLayerInfo->pfnNextGetInstanceProcAddr;
  const PFN_vkGetDeviceProcAddr nextGdpa = chain->u.pLayerInfo->pfnNextGetDeviceProcAddr;
  auto nextCreate =
      reinterpret_cast<PFN_vkCreateDevice>(nextGipa(instance->handle, "vkCreateDevice"));
  if (!nextCreate) return VK_ERROR_INITIALIZATION_FAILED;

  const std::string deviceName = PhysicalDeviceName(*instance, physicalDevice);
  DF_LOG_INFO("vkCreateDevice [%s]", deviceName.c_str());

  std::vector<const void*> below;
  for (const VkLayerDeviceLink* link = chain->u.pLayerInfo; link; link = link->pNext) {
    below.push_back(reinterpret_cast<const void*>(link->pfnNextGetDeviceProcAddr));
  }
  LogLayersBelow("vkCreateDevice", below);
  LogPNextChain("pNext chain from app", pCreateInfo->pNext);

  VkResult result;
  std::vector<std::string> enabledExtensions;
  {
    // edit восстанавливает правки чужих структур в деструкторе — строго после вызова вниз.
    DeviceCreateEdit edit(*pCreateInfo);
    for (const auto& feature : g_features) feature->OnCreateDevice(physicalDevice, edit);

    LogNameList("device extensions requested by app", pCreateInfo->enabledExtensionCount,
                pCreateInfo->ppEnabledExtensionNames, &edit);
    const VkDeviceCreateInfo* finalInfo = edit.Result();
    LogPNextChain("pNext chain passed down", finalInfo->pNext);
    DF_LOG_INFO("  passing %u extensions down the chain", finalInfo->enabledExtensionCount);

    for (const char* ext : edit.Extensions()) enabledExtensions.emplace_back(ext);

    chain->u.pLayerInfo = chain->u.pLayerInfo->pNext;
    result = nextCreate(physicalDevice, finalInfo, pAllocator, pDevice);
  }
  DF_LOG_INFO("vkCreateDevice -> %d", static_cast<int>(result));
  if (result != VK_SUCCESS) return result;

  auto data = std::make_unique<DeviceData>();
  data->handle = *pDevice;
  data->physicalDevice = physicalDevice;
  data->instance = instance;
  data->deviceName = deviceName;
  data->enabledExtensions = std::move(enabledExtensions);
  data->vk.Load(*pDevice, nextGdpa);
  DeviceData* device = g_devices.Insert(DispatchKey(*pDevice), std::move(data));
  for (const auto& feature : g_features) feature->OnDeviceCreated(*device);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL DestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) {
  if (!device) return;
  std::unique_ptr<DeviceData> data = g_devices.Remove(DispatchKey(device));
  if (!data) return;
  DF_LOG_INFO("vkDestroyDevice [%s]", data->deviceName.c_str());
  for (const auto& feature : g_features) feature->OnDestroyDevice(*data);
  data->vk.DestroyDevice(device, pAllocator);
}

// --- Таблицы перехвата ----------------------------------------------------------------------

struct NamedHook {
  const char* name;
  PFN_vkVoidFunction function;
};

#define DOOMFIX_HOOK(name) \
  { "vk" #name, reinterpret_cast<PFN_vkVoidFunction>(&name) }

const NamedHook kInstanceHooks[] = {
    {"vkGetInstanceProcAddr", reinterpret_cast<PFN_vkVoidFunction>(&DoomFix_GetInstanceProcAddr)},
    DOOMFIX_HOOK(CreateInstance),
    DOOMFIX_HOOK(DestroyInstance),
    DOOMFIX_HOOK(EnumerateDeviceExtensionProperties),
    DOOMFIX_HOOK(CreateDevice),
};

const NamedHook kDeviceHooks[] = {
    {"vkGetDeviceProcAddr", reinterpret_cast<PFN_vkVoidFunction>(&DoomFix_GetDeviceProcAddr)},
    DOOMFIX_HOOK(DestroyDevice),
};

#undef DOOMFIX_HOOK

template <size_t N>
PFN_vkVoidFunction FindHook(const NamedHook (&hooks)[N], const char* name) {
  for (const NamedHook& hook : hooks) {
    if (std::strcmp(hook.name, name) == 0) return hook.function;
  }
  return nullptr;
}

}  // namespace
}  // namespace doomfix

// --- Экспортируемые точки входа ----------------------------------------------------------------

extern "C" {

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL DoomFix_GetInstanceProcAddr(VkInstance instance,
                                                                     const char* pName) {
  using namespace doomfix;
  if (!pName) return nullptr;
  if (auto fn = FindHook(kInstanceHooks, pName)) return fn;
  // По спецификации vkGetInstanceProcAddr обязан отдавать и функции уровня устройства.
  if (auto fn = FindHook(kDeviceHooks, pName)) return fn;
  if (!instance) return nullptr;
  InstanceData* data = g_instances.Find(DispatchKey(instance));
  return data ? data->vk.GetInstanceProcAddr(instance, pName) : nullptr;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL DoomFix_GetDeviceProcAddr(VkDevice device,
                                                                   const char* pName) {
  using namespace doomfix;
  if (!pName || !device) return nullptr;
  if (auto fn = FindHook(kDeviceHooks, pName)) return fn;
  DeviceData* data = g_devices.Find(DispatchKey(device));
  return data ? data->vk.GetDeviceProcAddr(device, pName) : nullptr;
}

VKAPI_ATTR VkResult VKAPI_CALL
DoomFix_NegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* pVersionStruct) {
  if (!pVersionStruct || pVersionStruct->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT ||
      pVersionStruct->loaderLayerInterfaceVersion < 2) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  pVersionStruct->loaderLayerInterfaceVersion = 2;
  pVersionStruct->pfnGetInstanceProcAddr = DoomFix_GetInstanceProcAddr;
  pVersionStruct->pfnGetDeviceProcAddr = DoomFix_GetDeviceProcAddr;
  pVersionStruct->pfnGetPhysicalDeviceProcAddr = nullptr;
  doomfix::EnsureInitialized();
  return VK_SUCCESS;
}

}  // extern "C"
