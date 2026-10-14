#include "layer/vk_util.h"

namespace doomfix {
namespace {

// Небольшой справочник для лога. Неизвестные типы выводятся числом — их легко найти
// в vulkan_core.h.
const char* StructureTypeName(VkStructureType type) {
#define DF_CASE(x) \
  case x:          \
    return #x;
  switch (type) {
    DF_CASE(VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO)
    DF_CASE(VK_STRUCTURE_TYPE_DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD)
    DF_CASE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2)
    DF_CASE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES)
    DF_CASE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES)
    DF_CASE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES)
    DF_CASE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES)
    DF_CASE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES)
    DF_CASE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES)
    DF_CASE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES)
    DF_CASE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES)
    DF_CASE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES)
    DF_CASE(VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV)
    default:
      return nullptr;
  }
#undef DF_CASE
}

}  // namespace

std::string ApiVersionString(uint32_t version) {
  return std::to_string(VK_API_VERSION_MAJOR(version)) + "." +
         std::to_string(VK_API_VERSION_MINOR(version)) + "." +
         std::to_string(VK_API_VERSION_PATCH(version));
}

std::string DescribeStructureType(VkStructureType type) {
  const std::string number = std::to_string(static_cast<int>(type));
  if (const char* name = StructureTypeName(type)) return std::string(name) + " (" + number + ")";
  return "sType " + number;
}

}  // namespace doomfix
