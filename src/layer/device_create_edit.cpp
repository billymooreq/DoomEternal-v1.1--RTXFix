#include "layer/device_create_edit.h"

#include <algorithm>

#include "common/log.h"
#include "common/util.h"
#include "layer/vk_util.h"

namespace doomfix {

DeviceCreateEdit::DeviceCreateEdit(const VkDeviceCreateInfo& original)
    : info_(original),
      extensions_(original.ppEnabledExtensionNames,
                  original.ppEnabledExtensionNames + original.enabledExtensionCount) {}

DeviceCreateEdit::~DeviceCreateEdit() {
  for (auto it = patches_.rbegin(); it != patches_.rend(); ++it) {
    it->node->pNext = it->originalNext;
  }
}

bool DeviceCreateEdit::HasExtension(std::string_view name) const {
  return std::any_of(extensions_.begin(), extensions_.end(),
                     [&](const char* ext) { return name == ext; });
}

bool DeviceCreateEdit::RemoveExtension(std::string_view name) {
  const auto end = std::remove_if(extensions_.begin(), extensions_.end(),
                                  [&](const char* ext) { return name == ext; });
  const bool found = end != extensions_.end();
  extensions_.erase(end, extensions_.end());
  return found;
}

const VkBaseInStructure* DeviceCreateEdit::FindStruct(VkStructureType type) const {
  for (auto* s = static_cast<const VkBaseInStructure*>(info_.pNext); s; s = s->pNext) {
    if (s->sType == type) return s;
  }
  return nullptr;
}

int DeviceCreateEdit::RemoveStructs(VkStructureType type) {
  auto* const head = reinterpret_cast<VkBaseOutStructure*>(&info_);
  int removed = 0;
  VkBaseOutStructure* prev = head;
  while (VkBaseOutStructure* cur = prev->pNext) {
    if (cur->sType != type) {
      prev = cur;
      continue;
    }
    if (prev != head) {
      if (!IsWritable(&prev->pNext)) {
        DF_LOG_ERROR("cannot unlink %s: preceding %s is in read-only memory",
                     DescribeStructureType(type).c_str(),
                     DescribeStructureType(prev->sType).c_str());
        prev = cur;
        continue;
      }
      DF_LOG_DEBUG("temporarily patching pNext of %s (restored after vkCreateDevice)",
                   DescribeStructureType(prev->sType).c_str());
      patches_.push_back({prev, prev->pNext});
    }
    prev->pNext = cur->pNext;
    ++removed;
  }
  return removed;
}

const VkDeviceCreateInfo* DeviceCreateEdit::Result() {
  info_.enabledExtensionCount = static_cast<uint32_t>(extensions_.size());
  info_.ppEnabledExtensionNames = extensions_.empty() ? nullptr : extensions_.data();
  return &info_;
}

}  // namespace doomfix
