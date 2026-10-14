#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace doomfix {

// Функции следующего звена цепочки, которые вызывает слой. Чтобы добавить функцию, допишите
// X(Имя): поле в таблице и его загрузка появятся автоматически.
#define DOOMFIX_INSTANCE_FUNCS(X)         \
  X(DestroyInstance)                      \
  X(EnumerateDeviceExtensionProperties)   \
  X(GetPhysicalDeviceProperties)

// Сюда добавятся CreateShaderModule, CreateGraphicsPipelines, CreateComputePipelines и т.д.
// для дампа и замены шейдеров.
#define DOOMFIX_DEVICE_FUNCS(X) \
  X(DestroyDevice)

struct InstanceDispatch {
  PFN_vkGetInstanceProcAddr GetInstanceProcAddr = nullptr;
#define DOOMFIX_DECLARE(name) PFN_vk##name name = nullptr;
  DOOMFIX_INSTANCE_FUNCS(DOOMFIX_DECLARE)
#undef DOOMFIX_DECLARE

  void Load(VkInstance instance, PFN_vkGetInstanceProcAddr gipa) {
    GetInstanceProcAddr = gipa;
#define DOOMFIX_LOAD(name) name = reinterpret_cast<PFN_vk##name>(gipa(instance, "vk" #name));
    DOOMFIX_INSTANCE_FUNCS(DOOMFIX_LOAD)
#undef DOOMFIX_LOAD
  }
};

struct DeviceDispatch {
  PFN_vkGetDeviceProcAddr GetDeviceProcAddr = nullptr;
#define DOOMFIX_DECLARE(name) PFN_vk##name name = nullptr;
  DOOMFIX_DEVICE_FUNCS(DOOMFIX_DECLARE)
#undef DOOMFIX_DECLARE

  void Load(VkDevice device, PFN_vkGetDeviceProcAddr gdpa) {
    GetDeviceProcAddr = gdpa;
#define DOOMFIX_LOAD(name) name = reinterpret_cast<PFN_vk##name>(gdpa(device, "vk" #name));
    DOOMFIX_DEVICE_FUNCS(DOOMFIX_LOAD)
#undef DOOMFIX_LOAD
  }
};

struct InstanceData {
  VkInstance handle = VK_NULL_HANDLE;
  InstanceDispatch vk;

  std::mutex mutex;  // защищает поля ниже
  std::set<VkPhysicalDevice> extensionListLogged;
};

struct DeviceData {
  VkDevice handle = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  InstanceData* instance = nullptr;
  DeviceDispatch vk;
  std::string deviceName;
  std::vector<std::string> enabledExtensions;  // то, что реально ушло вниз по цепочке
};

// Потокобезопасная таблица "ключ диспетчеризации -> данные".
template <typename T>
class HandleMap {
 public:
  T* Insert(void* key, std::unique_ptr<T> value) {
    std::unique_lock lock(mutex_);
    auto& slot = map_[key];
    slot = std::move(value);
    return slot.get();
  }

  T* Find(void* key) const {
    std::shared_lock lock(mutex_);
    auto it = map_.find(key);
    return it == map_.end() ? nullptr : it->second.get();
  }

  std::unique_ptr<T> Remove(void* key) {
    std::unique_lock lock(mutex_);
    auto it = map_.find(key);
    if (it == map_.end()) return nullptr;
    std::unique_ptr<T> value = std::move(it->second);
    map_.erase(it);
    return value;
  }

 private:
  mutable std::shared_mutex mutex_;
  std::unordered_map<void*, std::unique_ptr<T>> map_;
};

}  // namespace doomfix
