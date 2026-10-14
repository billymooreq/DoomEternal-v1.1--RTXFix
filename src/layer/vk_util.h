#pragma once

#include <vulkan/vulkan.h>

#include <string>

namespace doomfix {

// Ключ диспетчеризации: первое слово любого диспетчеризуемого хэндла — указатель на таблицу
// loader'а. Он общий у VkInstance и его VkPhysicalDevice, а также у VkDevice и его
// VkQueue/VkCommandBuffer.
template <typename Handle>
void* DispatchKey(Handle handle) {
  return *reinterpret_cast<void**>(handle);
}

std::string ApiVersionString(uint32_t version);

// "VK_STRUCTURE_TYPE_... (N)" для известных типов, иначе "sType N".
std::string DescribeStructureType(VkStructureType type);

// Маски стадий/доступа в виде "COMPUTE_SHADER|DRAW_INDIRECT". Принимают и 32-битные маски
// VkPipelineStageFlags/VkAccessFlags (их биты совпадают с младшими битами *2-версий).
std::string StageMaskString(VkPipelineStageFlags2 mask);
std::string AccessMaskString(VkAccessFlags2 mask);

}  // namespace doomfix
