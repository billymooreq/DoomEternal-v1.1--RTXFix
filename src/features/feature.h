#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <string_view>
#include <vector>

namespace doomfix {

struct Config;
struct DeviceData;
class DeviceCreateEdit;

// Фича — независимый модуль поведения слоя. Ядро (layer/layer.cpp) перехватывает вызовы
// Vulkan и раздаёт их фичам через виртуальные методы ниже; фича переопределяет только нужные.
//
// Чтобы добавить фичу:
//   1. Унаследоваться от Feature в новом файле src/features/<имя>.cpp.
//   2. Зарегистрировать фабрику в src/features/features.cpp.
// Чтобы фичи могли реагировать на новый вызов Vulkan (например, vkCreateShaderModule
// для дампа/замены шейдеров):
//   1. Добавить функцию в DOOMFIX_DEVICE_FUNCS (layer/dispatch.h).
//   2. Написать перехватчик в layer/layer.cpp и внести его в kDeviceHooks.
//   3. Добавить сюда виртуальный метод-событие и вызывать его из перехватчика.
// Барьеры vkCmdPipelineBarrier / vkCmdWaitEvents (общая часть аргументов). Фича может заменить
// значения и указатели; массивы, на которые она перенаправит указатели, должны жить до
// возврата из перехватчика (удобно держать их в thread_local-хранилище фичи).
struct BarrierSet {
  VkPipelineStageFlags srcStageMask;
  VkPipelineStageFlags dstStageMask;
  uint32_t memoryBarrierCount;
  const VkMemoryBarrier* pMemoryBarriers;
  uint32_t bufferMemoryBarrierCount;
  const VkBufferMemoryBarrier* pBufferMemoryBarriers;
  uint32_t imageMemoryBarrierCount;
  const VkImageMemoryBarrier* pImageMemoryBarriers;
};

class Feature {
 public:
  virtual ~Feature() = default;

  virtual const char* Name() const = 0;

  // Скрыть расширение устройства из vkEnumerateDeviceExtensionProperties.
  virtual bool HidesDeviceExtension(std::string_view /*name*/) const { return false; }

  // Изменить параметры vkCreateDevice до передачи следующему звену цепочки.
  virtual void OnCreateDevice(VkPhysicalDevice /*physicalDevice*/, DeviceCreateEdit& /*edit*/) {}

  // Устройство создано и зарегистрировано в слое.
  virtual void OnDeviceCreated(DeviceData& /*device*/) {}

  // Вызывается перед vkDestroyDevice.
  virtual void OnDestroyDevice(DeviceData& /*device*/) {}

  // Запись барьеров в командный буфер (вызываются на потоке записи, часто — держите их дешёвыми).
  // entryPoint — имя перехваченной функции ("vkCmdPipelineBarrier2KHR" и т.п.).
  virtual void OnCmdPipelineBarrier(DeviceData& /*device*/, VkCommandBuffer /*cb*/,
                                    VkDependencyFlags /*dependencyFlags*/, BarrierSet& /*barriers*/) {}
  virtual void OnCmdWaitEvents(DeviceData& /*device*/, VkCommandBuffer /*cb*/, uint32_t /*eventCount*/,
                               const VkEvent* /*pEvents*/, BarrierSet& /*barriers*/) {}
  virtual void OnCmdPipelineBarrier2(DeviceData& /*device*/, VkCommandBuffer /*cb*/,
                                     const char* /*entryPoint*/,
                                     const VkDependencyInfo*& /*dependencyInfo*/) {}
  virtual void OnCmdWaitEvents2(DeviceData& /*device*/, VkCommandBuffer /*cb*/,
                                const char* /*entryPoint*/, uint32_t /*eventCount*/,
                                const VkEvent* /*pEvents*/,
                                const VkDependencyInfo*& /*pDependencyInfos*/) {}
};

// Создаёт все включённые фичи в порядке, в котором они получают события.
std::vector<std::unique_ptr<Feature>> CreateFeatures(const Config& config);

}  // namespace doomfix
