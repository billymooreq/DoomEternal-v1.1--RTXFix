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

};

// Создаёт все включённые фичи в порядке, в котором они получают события.
std::vector<std::unique_ptr<Feature>> CreateFeatures(const Config& config);

}  // namespace doomfix
