// Скрывает расширения устройства от приложения: убирает их из
// vkEnumerateDeviceExtensionProperties, из ppEnabledExtensionNames и вырезает связанные с ними
// структуры из цепочки pNext в vkCreateDevice.

#include <string>
#include <utility>

#include "common/config.h"
#include "common/log.h"
#include "features/feature.h"
#include "layer/device_create_edit.h"
#include "layer/vk_util.h"

namespace doomfix {
namespace {

struct HiddenExtension {
  std::string name;
  // Структуры pNext, допустимые только при включённом расширении.
  std::vector<VkStructureType> structs;
};

std::vector<HiddenExtension> BuiltInRules() {
  return {
      {VK_AMD_MEMORY_OVERALLOCATION_BEHAVIOR_EXTENSION_NAME,
       {VK_STRUCTURE_TYPE_DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD}},
  };
}

const char* OverallocationBehaviorName(VkMemoryOverallocationBehaviorAMD behavior) {
  switch (behavior) {
    case VK_MEMORY_OVERALLOCATION_BEHAVIOR_DEFAULT_AMD: return "DEFAULT";
    case VK_MEMORY_OVERALLOCATION_BEHAVIOR_ALLOWED_AMD: return "ALLOWED";
    case VK_MEMORY_OVERALLOCATION_BEHAVIOR_DISALLOWED_AMD: return "DISALLOWED";
    default: return "?";
  }
}

void LogStructDetails(const VkBaseInStructure* s) {
  if (s->sType == VK_STRUCTURE_TYPE_DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD) {
    const auto* info = reinterpret_cast<const VkDeviceMemoryOverallocationCreateInfoAMD*>(s);
    DF_LOG_INFO("  app passed VkDeviceMemoryOverallocationCreateInfoAMD: overallocationBehavior=%s (%d)",
                OverallocationBehaviorName(info->overallocationBehavior),
                static_cast<int>(info->overallocationBehavior));
  }
}

class HideDeviceExtensions final : public Feature {
 public:
  explicit HideDeviceExtensions(std::vector<HiddenExtension> rules) : rules_(std::move(rules)) {
    for (const auto& rule : rules_) DF_LOG_INFO("  will hide device extension %s", rule.name.c_str());
  }

  const char* Name() const override { return "hide-device-extensions"; }

  bool HidesDeviceExtension(std::string_view name) const override {
    for (const auto& rule : rules_) {
      if (rule.name == name) return true;
    }
    return false;
  }

  void OnCreateDevice(VkPhysicalDevice, DeviceCreateEdit& edit) override {
    for (const auto& rule : rules_) {
      if (edit.RemoveExtension(rule.name)) {
        DF_LOG_INFO("  REMOVED extension %s from ppEnabledExtensionNames", rule.name.c_str());
      }
      for (VkStructureType type : rule.structs) {
        if (const VkBaseInStructure* s = edit.FindStruct(type)) LogStructDetails(s);
        if (const int n = edit.RemoveStructs(type)) {
          DF_LOG_INFO("  REMOVED %d x %s from pNext chain", n, DescribeStructureType(type).c_str());
        }
      }
    }
  }

 private:
  std::vector<HiddenExtension> rules_;
};

}  // namespace

std::unique_ptr<Feature> CreateHideDeviceExtensionsFeature(const Config& config) {
  std::vector<HiddenExtension> rules = BuiltInRules();
  for (const std::string& name : config.extraHiddenDeviceExtensions) {
    rules.push_back({name, {}});
  }
  return std::make_unique<HideDeviceExtensions>(std::move(rules));
}

}  // namespace doomfix
