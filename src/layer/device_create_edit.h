#pragma once

#include <vulkan/vulkan.h>

#include <string_view>
#include <vector>

namespace doomfix {

// Изменяемое представление VkDeviceCreateInfo на время одного вызова vkCreateDevice.
//
// Сама VkDeviceCreateInfo и список расширений копируются. Структуры цепочки pNext принадлежат
// приложению (или loader'у), их размер в общем случае неизвестен, поэтому при вырезании узла
// pNext предыдущей структуры временно правится на месте и восстанавливается в деструкторе.
// Объект должен жить, пока не вернётся вызов vkCreateDevice следующего звена.
class DeviceCreateEdit {
 public:
  explicit DeviceCreateEdit(const VkDeviceCreateInfo& original);
  ~DeviceCreateEdit();
  DeviceCreateEdit(const DeviceCreateEdit&) = delete;
  DeviceCreateEdit& operator=(const DeviceCreateEdit&) = delete;

  const std::vector<const char*>& Extensions() const { return extensions_; }
  bool HasExtension(std::string_view name) const;
  // Возвращает true, если расширение было в списке.
  bool RemoveExtension(std::string_view name);

  // Первая структура данного типа в текущей цепочке pNext или nullptr.
  const VkBaseInStructure* FindStruct(VkStructureType type) const;
  // Вырезает из цепочки все структуры данного типа. Возвращает число вырезанных.
  int RemoveStructs(VkStructureType type);

  // Итоговая структура для следующего звена. Действительна, пока жив объект.
  const VkDeviceCreateInfo* Result();

 private:
  struct Patch {
    VkBaseOutStructure* node;
    VkBaseOutStructure* originalNext;
  };

  VkDeviceCreateInfo info_;
  std::vector<const char*> extensions_;
  std::vector<Patch> patches_;
};

}  // namespace doomfix
