// Исправляет барьеры перед indirect-командами (vkCmdDispatchIndirect / vkCmdDraw*Indirect).
//
// DOOM Eternal пишет аргументы indirect-dispatch'ей кластерного биннинга источников, проб и
// декалей compute-шейдером, а в барьере с dstStageMask = DRAW_INDIRECT не указывает
// VK_ACCESS_INDIRECT_COMMAND_READ_BIT. На NVIDIA indirect-вызов тогда читает устаревшие (нулевые)
// аргументы, списки источников и проб остаются пустыми, и оружие от первого лица выходит чёрным
// (analysis/report.md, раздел 6). Фича правит такие барьеры при записи командного буфера:
//
//   on (по умолчанию)  dstAccessMask |= INDIRECT_COMMAND_READ у memory/buffer-барьеров; если их
//                      нет — добавляется глобальный VkMemoryBarrier (запись → INDIRECT_COMMAND_READ);
//                      WAR: при COMPUTE_SHADER в srcStageMask добавляется DRAW_INDIRECT;
//   aggressive         такой барьер превращается в полный ALL_COMMANDS → ALL_COMMANDS,
//                      MEMORY_WRITE → MEMORY_READ|MEMORY_WRITE;
//   off                ничего не меняется, барьеры только считаются (для A/B-сравнения).
//
// Барьеры без DRAW_INDIRECT в dstStageMask передаются дальше как есть, без копирования.
// vkCmdWaitEvents2 только считается: его зависимость должна соответствовать vkCmdSetEvent2.

#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "common/config.h"
#include "common/log.h"
#include "features/feature.h"
#include "layer/dispatch.h"
#include "layer/vk_util.h"

namespace doomfix {
namespace {

constexpr VkPipelineStageFlags2 kIndirectStage = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
constexpr VkPipelineStageFlags2 kComputeStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
constexpr VkPipelineStageFlags2 kAllCommands = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
constexpr VkAccessFlags2 kIndirectRead = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
constexpr VkAccessFlags2 kFullRead = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
constexpr VkAccessFlags2 kFullWrite = VK_ACCESS_2_MEMORY_WRITE_BIT;
constexpr auto kReportInterval = std::chrono::seconds(5);

// Биты VkPipelineStageFlags/VkAccessFlags совпадают с младшими битами *2-версий, поэтому
// константы выше годятся и для 32-битных масок (после приведения).
template <typename T>
constexpr T Low(uint64_t v) {
  return static_cast<T>(v);
}

bool CoversIndirectRead(VkAccessFlags2 access) {
  return (access & (kIndirectRead | VK_ACCESS_2_MEMORY_READ_BIT)) != 0;
}

// Типы записи, допустимые для данных стадий первой области синхронизации.
VkAccessFlags2 WritesForStages(VkPipelineStageFlags2 stages) {
  if (stages & kAllCommands) return kFullWrite;
  VkAccessFlags2 access = 0;
  constexpr VkPipelineStageFlags2 kShaders =
      VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT |
      VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT |
      VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT |
      VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT;
  constexpr VkPipelineStageFlags2 kTransfer =
      VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT | VK_PIPELINE_STAGE_2_COPY_BIT |
      VK_PIPELINE_STAGE_2_RESOLVE_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT;
  if (stages & kShaders) access |= VK_ACCESS_2_SHADER_WRITE_BIT;
  if (stages & kTransfer) access |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
  if (stages & VK_PIPELINE_STAGE_2_HOST_BIT) access |= VK_ACCESS_2_HOST_WRITE_BIT;
  return access;
}

enum Api {
  kPipelineBarrier,
  kPipelineBarrier2,
  kPipelineBarrier2KHR,
  kWaitEvents,
  kWaitEvents2,
  kWaitEvents2KHR,
  kApiCount
};
const char* const kApiNames[kApiCount] = {"vkCmdPipelineBarrier", "vkCmdPipelineBarrier2",
                                          "vkCmdPipelineBarrier2KHR", "vkCmdWaitEvents",
                                          "vkCmdWaitEvents2", "vkCmdWaitEvents2KHR"};

Api Barrier2Api(const char* entryPoint) {
  return std::string_view(entryPoint).back() == 'R' ? kPipelineBarrier2KHR : kPipelineBarrier2;
}
Api WaitEvents2Api(const char* entryPoint) {
  return std::string_view(entryPoint).back() == 'R' ? kWaitEvents2KHR : kWaitEvents2;
}

const char* ModeName(IndirectFixMode mode) {
  switch (mode) {
    case IndirectFixMode::Off: return "off";
    case IndirectFixMode::On: return "on";
    case IndirectFixMode::Aggressive: return "aggressive";
  }
  return "?";
}

// --- Описание барьеров для лога ---------------------------------------------------------------

std::string Hex(const void* p) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%p", p);
  return buf;
}

std::string DescribeV1(const BarrierSet& b) {
  std::string s = "srcStage=" + StageMaskString(b.srcStageMask) +
                  " dstStage=" + StageMaskString(b.dstStageMask);
  for (uint32_t i = 0; i < b.memoryBarrierCount; ++i) {
    const VkMemoryBarrier& m = b.pMemoryBarriers[i];
    s += "\n      memory[" + std::to_string(i) + "]: " + AccessMaskString(m.srcAccessMask) + " -> " +
         AccessMaskString(m.dstAccessMask);
  }
  for (uint32_t i = 0; i < b.bufferMemoryBarrierCount; ++i) {
    const VkBufferMemoryBarrier& m = b.pBufferMemoryBarriers[i];
    s += "\n      buffer[" + std::to_string(i) + "] " + Hex(m.buffer) + " +" +
         std::to_string(m.offset) + "/" +
         (m.size == VK_WHOLE_SIZE ? std::string("WHOLE") : std::to_string(m.size)) + ": " +
         AccessMaskString(m.srcAccessMask) + " -> " + AccessMaskString(m.dstAccessMask);
  }
  for (uint32_t i = 0; i < b.imageMemoryBarrierCount; ++i) {
    const VkImageMemoryBarrier& m = b.pImageMemoryBarriers[i];
    s += "\n      image[" + std::to_string(i) + "] " + Hex(m.image) + " layout " +
         std::to_string(m.oldLayout) + "->" + std::to_string(m.newLayout) + ": " +
         AccessMaskString(m.srcAccessMask) + " -> " + AccessMaskString(m.dstAccessMask);
  }
  return s;
}

template <typename B>
void AppendV2(std::string& s, const char* kind, uint32_t count, const B* barriers) {
  for (uint32_t i = 0; i < count; ++i) {
    const B& m = barriers[i];
    s += "\n      " + std::string(kind) + "[" + std::to_string(i) + "]: " +
         StageMaskString(m.srcStageMask) + " / " + AccessMaskString(m.srcAccessMask) + " -> " +
         StageMaskString(m.dstStageMask) + " / " + AccessMaskString(m.dstAccessMask);
  }
}

std::string DescribeV2(const VkDependencyInfo& d) {
  std::string s = "dependencyFlags=" + std::to_string(d.dependencyFlags);
  AppendV2(s, "memory", d.memoryBarrierCount, d.pMemoryBarriers);
  AppendV2(s, "buffer", d.bufferMemoryBarrierCount, d.pBufferMemoryBarriers);
  AppendV2(s, "image", d.imageMemoryBarrierCount, d.pImageMemoryBarriers);
  return s;
}

// --- Хранилище копий ---------------------------------------------------------------------------
// Копии живут до следующего вызова на том же потоке; ядро передаёт их вниз сразу после фичи.

struct ScratchV1 {
  std::vector<VkMemoryBarrier> mem;
  std::vector<VkBufferMemoryBarrier> buf;
  std::vector<VkImageMemoryBarrier> img;
};

struct ScratchV2 {
  VkDependencyInfo info;
  std::vector<VkMemoryBarrier2> mem;
  std::vector<VkBufferMemoryBarrier2> buf;
  std::vector<VkImageMemoryBarrier2> img;
};

thread_local ScratchV1 t_v1;
thread_local ScratchV2 t_v2;

// --- Барьеры vkCmdPipelineBarrier / vkCmdWaitEvents --------------------------------------------
// У vkCmdWaitEvents srcStageMask обязан совпадать со stageMask из vkCmdSetEvent, поэтому для
// событий первая область синхронизации не меняется.

bool NeedsPatchV1(const BarrierSet& b, bool isWaitEvents) {
  if (!isWaitEvents && (b.srcStageMask & kComputeStage) && !(b.srcStageMask & kIndirectStage)) {
    return true;  // WAR
  }
  if (b.memoryBarrierCount == 0 && b.bufferMemoryBarrierCount == 0) return true;
  for (uint32_t i = 0; i < b.memoryBarrierCount; ++i) {
    if (!CoversIndirectRead(b.pMemoryBarriers[i].dstAccessMask)) return true;
  }
  for (uint32_t i = 0; i < b.bufferMemoryBarrierCount; ++i) {
    if (!CoversIndirectRead(b.pBufferMemoryBarriers[i].dstAccessMask)) return true;
  }
  return false;
}

void ApplyOnV1(BarrierSet& b, bool isWaitEvents) {
  if (!isWaitEvents && (b.srcStageMask & kComputeStage)) {
    b.srcStageMask |= Low<VkPipelineStageFlags>(kIndirectStage);
  }
  if (b.memoryBarrierCount == 0 && b.bufferMemoryBarrierCount == 0) {
    t_v1.mem.assign(1, VkMemoryBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                       Low<VkAccessFlags>(WritesForStages(b.srcStageMask)),
                                       Low<VkAccessFlags>(kIndirectRead)});
    b.memoryBarrierCount = 1;
    b.pMemoryBarriers = t_v1.mem.data();
    return;
  }
  bool memFix = false, bufFix = false;
  for (uint32_t i = 0; i < b.memoryBarrierCount; ++i) {
    memFix |= !CoversIndirectRead(b.pMemoryBarriers[i].dstAccessMask);
  }
  for (uint32_t i = 0; i < b.bufferMemoryBarrierCount; ++i) {
    bufFix |= !CoversIndirectRead(b.pBufferMemoryBarriers[i].dstAccessMask);
  }
  if (memFix) {
    t_v1.mem.assign(b.pMemoryBarriers, b.pMemoryBarriers + b.memoryBarrierCount);
    for (auto& m : t_v1.mem) m.dstAccessMask |= Low<VkAccessFlags>(kIndirectRead);
    b.pMemoryBarriers = t_v1.mem.data();
  }
  if (bufFix) {
    t_v1.buf.assign(b.pBufferMemoryBarriers, b.pBufferMemoryBarriers + b.bufferMemoryBarrierCount);
    for (auto& m : t_v1.buf) m.dstAccessMask |= Low<VkAccessFlags>(kIndirectRead);
    b.pBufferMemoryBarriers = t_v1.buf.data();
  }
}

void ApplyAggressiveV1(BarrierSet& b, bool isWaitEvents) {
  // Для событий src-маски остаются как есть (см. выше), расширяется только вторая область.
  const auto full = Low<VkAccessFlags>(kFullRead);
  if (!isWaitEvents) b.srcStageMask = Low<VkPipelineStageFlags>(kAllCommands);
  b.dstStageMask = Low<VkPipelineStageFlags>(kAllCommands);
  auto srcAccess = [&](VkAccessFlags original) {
    return isWaitEvents ? original : Low<VkAccessFlags>(kFullWrite);
  };
  if (b.memoryBarrierCount == 0) {
    const VkAccessFlags src = isWaitEvents ? Low<VkAccessFlags>(WritesForStages(b.srcStageMask))
                                           : Low<VkAccessFlags>(kFullWrite);
    t_v1.mem.assign(1, VkMemoryBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, src, full});
  } else {
    t_v1.mem.assign(b.pMemoryBarriers, b.pMemoryBarriers + b.memoryBarrierCount);
    for (auto& m : t_v1.mem) {
      m.srcAccessMask = srcAccess(m.srcAccessMask);
      m.dstAccessMask = full;
    }
  }
  b.memoryBarrierCount = static_cast<uint32_t>(t_v1.mem.size());
  b.pMemoryBarriers = t_v1.mem.data();
  if (b.bufferMemoryBarrierCount) {
    t_v1.buf.assign(b.pBufferMemoryBarriers, b.pBufferMemoryBarriers + b.bufferMemoryBarrierCount);
    for (auto& m : t_v1.buf) {
      m.srcAccessMask = srcAccess(m.srcAccessMask);
      m.dstAccessMask = full;
    }
    b.pBufferMemoryBarriers = t_v1.buf.data();
  }
  if (b.imageMemoryBarrierCount) {
    t_v1.img.assign(b.pImageMemoryBarriers, b.pImageMemoryBarriers + b.imageMemoryBarrierCount);
    for (auto& m : t_v1.img) {
      m.srcAccessMask = srcAccess(m.srcAccessMask);
      m.dstAccessMask = full;
    }
    b.pImageMemoryBarriers = t_v1.img.data();
  }
}

// --- Барьеры synchronization2 ------------------------------------------------------------------

template <typename B>
bool AnyIndirectDst(uint32_t count, const B* barriers) {
  for (uint32_t i = 0; i < count; ++i) {
    if (barriers[i].dstStageMask & kIndirectStage) return true;
  }
  return false;
}

bool HasIndirectDst(const VkDependencyInfo& d) {
  return AnyIndirectDst(d.memoryBarrierCount, d.pMemoryBarriers) ||
         AnyIndirectDst(d.bufferMemoryBarrierCount, d.pBufferMemoryBarriers) ||
         AnyIndirectDst(d.imageMemoryBarrierCount, d.pImageMemoryBarriers);
}

template <typename B>
bool NeedsWar(const B& m) {
  return (m.dstStageMask & kIndirectStage) && (m.srcStageMask & kComputeStage) &&
         !(m.srcStageMask & kIndirectStage);
}

template <typename B>
bool NeedsAccess(const B& m) {
  return (m.dstStageMask & kIndirectStage) && !CoversIndirectRead(m.dstAccessMask);
}

template <typename B>
bool AnyNeeds(uint32_t count, const B* barriers, bool access) {
  for (uint32_t i = 0; i < count; ++i) {
    if (NeedsWar(barriers[i]) || (access && NeedsAccess(barriers[i]))) return true;
  }
  return false;
}

// Глобальный барьер нужен, если DRAW_INDIRECT есть только у image-барьеров.
bool NeedsGlobalV2(const VkDependencyInfo& d) {
  return !AnyIndirectDst(d.memoryBarrierCount, d.pMemoryBarriers) &&
         !AnyIndirectDst(d.bufferMemoryBarrierCount, d.pBufferMemoryBarriers) &&
         AnyIndirectDst(d.imageMemoryBarrierCount, d.pImageMemoryBarriers);
}

bool NeedsPatchV2(const VkDependencyInfo& d) {
  return NeedsGlobalV2(d) || AnyNeeds(d.memoryBarrierCount, d.pMemoryBarriers, true) ||
         AnyNeeds(d.bufferMemoryBarrierCount, d.pBufferMemoryBarriers, true) ||
         AnyNeeds(d.imageMemoryBarrierCount, d.pImageMemoryBarriers, false);
}

template <typename B>
void FixV2(B& m, bool access) {
  if (!(m.dstStageMask & kIndirectStage)) return;
  if (m.srcStageMask & kComputeStage) m.srcStageMask |= kIndirectStage;
  if (access && !CoversIndirectRead(m.dstAccessMask)) m.dstAccessMask |= kIndirectRead;
}

const VkDependencyInfo* ApplyOnV2(const VkDependencyInfo& d) {
  ScratchV2& t = t_v2;
  t.info = d;
  const bool global = NeedsGlobalV2(d);
  if (global || AnyNeeds(d.memoryBarrierCount, d.pMemoryBarriers, true)) {
    t.mem.assign(d.pMemoryBarriers, d.pMemoryBarriers + d.memoryBarrierCount);
    for (auto& m : t.mem) FixV2(m, true);
  }
  if (global) {
    VkPipelineStageFlags2 src = 0;
    for (uint32_t i = 0; i < d.imageMemoryBarrierCount; ++i) {
      if (d.pImageMemoryBarriers[i].dstStageMask & kIndirectStage) {
        src |= d.pImageMemoryBarriers[i].srcStageMask;
      }
    }
    if (src & kComputeStage) src |= kIndirectStage;
    t.mem.push_back(VkMemoryBarrier2{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2, nullptr, src,
                                     WritesForStages(src), kIndirectStage, kIndirectRead});
  }
  if (global || AnyNeeds(d.memoryBarrierCount, d.pMemoryBarriers, true)) {
    t.info.memoryBarrierCount = static_cast<uint32_t>(t.mem.size());
    t.info.pMemoryBarriers = t.mem.data();
  }
  if (AnyNeeds(d.bufferMemoryBarrierCount, d.pBufferMemoryBarriers, true)) {
    t.buf.assign(d.pBufferMemoryBarriers, d.pBufferMemoryBarriers + d.bufferMemoryBarrierCount);
    for (auto& m : t.buf) FixV2(m, true);
    t.info.pBufferMemoryBarriers = t.buf.data();
  }
  if (AnyNeeds(d.imageMemoryBarrierCount, d.pImageMemoryBarriers, false)) {
    t.img.assign(d.pImageMemoryBarriers, d.pImageMemoryBarriers + d.imageMemoryBarrierCount);
    for (auto& m : t.img) FixV2(m, false);
    t.info.pImageMemoryBarriers = t.img.data();
  }
  return &t.info;
}

template <typename B>
void MakeFull(B& m) {
  m.srcStageMask = kAllCommands;
  m.dstStageMask = kAllCommands;
  m.srcAccessMask = kFullWrite;
  m.dstAccessMask = kFullRead;
}

const VkDependencyInfo* ApplyAggressiveV2(const VkDependencyInfo& d) {
  ScratchV2& t = t_v2;
  t.info = d;
  t.mem.assign(d.pMemoryBarriers, d.pMemoryBarriers + d.memoryBarrierCount);
  if (t.mem.empty()) t.mem.push_back(VkMemoryBarrier2{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2});
  for (auto& m : t.mem) MakeFull(m);
  t.info.memoryBarrierCount = static_cast<uint32_t>(t.mem.size());
  t.info.pMemoryBarriers = t.mem.data();
  if (d.bufferMemoryBarrierCount) {
    t.buf.assign(d.pBufferMemoryBarriers, d.pBufferMemoryBarriers + d.bufferMemoryBarrierCount);
    for (auto& m : t.buf) MakeFull(m);
    t.info.pBufferMemoryBarriers = t.buf.data();
  }
  if (d.imageMemoryBarrierCount) {
    t.img.assign(d.pImageMemoryBarriers, d.pImageMemoryBarriers + d.imageMemoryBarrierCount);
    for (auto& m : t.img) MakeFull(m);
    t.info.pImageMemoryBarriers = t.img.data();
  }
  return &t.info;
}

// --- Фича ----------------------------------------------------------------------------------------

class IndirectBarrierFix final : public Feature {
 public:
  explicit IndirectBarrierFix(const Config& config)
      : mode_(config.indirectFixMode), lastReport_(std::chrono::steady_clock::now()) {
    DF_LOG_INFO("  indirect-barrier-fix: mode=%s (DOOMFIX_INDIRECT_FIX=\"%s\")", ModeName(mode_),
                config.indirectFixRaw.c_str());
  }

  const char* Name() const override { return "indirect-barrier-fix"; }

  void OnDeviceCreated(DeviceData& device) override {
    const DeviceDispatch& vk = device.vk;
    const bool present[kApiCount] = {
        vk.CmdPipelineBarrier != nullptr, vk.CmdPipelineBarrier2 != nullptr,
        vk.CmdPipelineBarrier2KHR != nullptr, vk.CmdWaitEvents != nullptr,
        vk.CmdWaitEvents2 != nullptr, vk.CmdWaitEvents2KHR != nullptr};
    std::string list;
    for (int i = 0; i < kApiCount; ++i) {
      if (!list.empty()) list += ", ";
      list += std::string(kApiNames[i]) + (present[i] ? "=yes" : "=no");
    }
    DF_LOG_INFO("indirect-barrier-fix [%s]: intercepted at record time; available on device: %s",
                ModeName(mode_), list.c_str());
  }

  void OnDestroyDevice(DeviceData&) override { Report(true); }

  void OnCmdPipelineBarrier(DeviceData&, VkCommandBuffer cb, VkDependencyFlags,
                            BarrierSet& b) override {
    HandleV1(kPipelineBarrier, cb, b, false);
  }

  void OnCmdWaitEvents(DeviceData&, VkCommandBuffer cb, uint32_t, const VkEvent*,
                       BarrierSet& b) override {
    HandleV1(kWaitEvents, cb, b, true);
  }

  void OnCmdPipelineBarrier2(DeviceData&, VkCommandBuffer cb, const char* entryPoint,
                             const VkDependencyInfo*& info) override {
    const Api api = Barrier2Api(entryPoint);
    calls_[api].fetch_add(1, std::memory_order_relaxed);
    if (FirstCall(api)) DF_LOG_INFO("indirect-barrier-fix: first call of %s", entryPoint);
    if (!info || !HasIndirectDst(*info)) return;
    indirect_[api].fetch_add(1, std::memory_order_relaxed);
    const bool needs = mode_ == IndirectFixMode::Aggressive || NeedsPatchV2(*info);
    if (!needs) {
      alreadyOk_.fetch_add(1, std::memory_order_relaxed);
    } else {
      const bool first = FirstPatch(api);
      const bool detail = first || log::Enabled(log::Level::Debug);
      const std::string before = detail ? DescribeV2(*info) : std::string();
      if (mode_ != IndirectFixMode::Off) {
        info = mode_ == IndirectFixMode::Aggressive ? ApplyAggressiveV2(*info) : ApplyOnV2(*info);
      }
      patched_[api].fetch_add(1, std::memory_order_relaxed);
      if (detail) LogPatch(api, cb, first, before, DescribeV2(*info), false);
    }
    MaybeReport();
  }

  void OnCmdWaitEvents2(DeviceData&, VkCommandBuffer, const char* entryPoint, uint32_t eventCount,
                        const VkEvent*, const VkDependencyInfo*& infos) override {
    const Api api = WaitEvents2Api(entryPoint);
    calls_[api].fetch_add(1, std::memory_order_relaxed);
    if (FirstCall(api)) DF_LOG_INFO("indirect-barrier-fix: first call of %s", entryPoint);
    if (!infos) return;
    for (uint32_t i = 0; i < eventCount; ++i) {
      if (!HasIndirectDst(infos[i])) continue;
      indirect_[api].fetch_add(1, std::memory_order_relaxed);
      if (!warnedWaitEvents2_.exchange(true)) {
        DF_LOG_WARN("indirect-barrier-fix: %s with DRAW_INDIRECT in dstStageMask is NOT patched "
                    "(dependency must match vkCmdSetEvent2): %s",
                    entryPoint, DescribeV2(infos[i]).c_str());
      }
    }
    MaybeReport();
  }

 private:
  void HandleV1(Api api, VkCommandBuffer cb, BarrierSet& b, bool isWaitEvents) {
    calls_[api].fetch_add(1, std::memory_order_relaxed);
    if (FirstCall(api)) DF_LOG_INFO("indirect-barrier-fix: first call of %s", kApiNames[api]);
    if (!(b.dstStageMask & Low<VkPipelineStageFlags>(kIndirectStage))) return;
    indirect_[api].fetch_add(1, std::memory_order_relaxed);
    const bool needs = mode_ == IndirectFixMode::Aggressive || NeedsPatchV1(b, isWaitEvents);
    if (!needs) {
      alreadyOk_.fetch_add(1, std::memory_order_relaxed);
    } else {
      const bool first = FirstPatch(api);
      const bool detail = first || log::Enabled(log::Level::Debug);
      const std::string before = detail ? DescribeV1(b) : std::string();
      if (mode_ == IndirectFixMode::On) ApplyOnV1(b, isWaitEvents);
      if (mode_ == IndirectFixMode::Aggressive) ApplyAggressiveV1(b, isWaitEvents);
      patched_[api].fetch_add(1, std::memory_order_relaxed);
      if (detail) LogPatch(api, cb, first, before, DescribeV1(b), isWaitEvents);
    }
    MaybeReport();
  }

  bool FirstCall(Api api) {
    return !firstCall_[api].load(std::memory_order_relaxed) && !firstCall_[api].exchange(true);
  }

  bool FirstPatch(Api api) {
    return !firstPatch_[api].load(std::memory_order_relaxed) && !firstPatch_[api].exchange(true);
  }

  // Первое срабатывание для каждой функции — в INFO, остальные — только при DOOMFIX_LOG_LEVEL=debug.
  void LogPatch(Api api, VkCommandBuffer cb, bool first, const std::string& before,
                const std::string& after, bool isWaitEvents) {
    const log::Level level = first ? log::Level::Info : log::Level::Debug;
    const char* verb = mode_ == IndirectFixMode::Off ? "WOULD PATCH (mode=off, unchanged)" : "PATCHED";
    log::Write(level, "indirect-barrier-fix [%s]: %s %s in command buffer %s%s\n    before: %s\n    after:  %s",
               ModeName(mode_), verb, kApiNames[api], Hex(cb).c_str(),
               isWaitEvents ? " (srcStageMask kept: must match vkCmdSetEvent)" : "", before.c_str(),
               after.c_str());
  }

  void MaybeReport() {
    const auto now = std::chrono::steady_clock::now();
    auto last = lastReport_.load(std::memory_order_relaxed);
    if (now - last < kReportInterval) return;
    if (!lastReport_.compare_exchange_strong(last, now)) return;  // отчёт пишет один поток
    Report(false);
  }

  void Report(bool final) {
    std::lock_guard lock(reportMutex_);
    uint64_t indirect = 0, patched = 0, calls[kApiCount];
    for (int i = 0; i < kApiCount; ++i) {
      indirect += indirect_[i].load(std::memory_order_relaxed);
      patched += patched_[i].load(std::memory_order_relaxed);
      calls[i] = calls_[i].load(std::memory_order_relaxed);
    }
    const uint64_t ok = alreadyOk_.load(std::memory_order_relaxed);
    if (!final && indirect == reportedIndirect_) return;
    std::string perApi;
    for (int i = 0; i < kApiCount; ++i) {
      if (!calls[i]) continue;
      if (!perApi.empty()) perApi += ", ";
      perApi += std::string(kApiNames[i]) + "=" + std::to_string(calls[i]);
    }
    DF_LOG_INFO("indirect-barrier-fix [%s]%s: barriers with DRAW_INDIRECT dst %" PRIu64 " (+%" PRIu64
                "), %s %" PRIu64 " (+%" PRIu64 "), already correct %" PRIu64 "; calls: %s",
                ModeName(mode_), final ? " final" : "", indirect, indirect - reportedIndirect_,
                mode_ == IndirectFixMode::Off ? "would patch" : "patched", patched,
                patched - reportedPatched_, ok, perApi.empty() ? "none" : perApi.c_str());
    reportedIndirect_ = indirect;
    reportedPatched_ = patched;
  }

  const IndirectFixMode mode_;
  std::atomic<uint64_t> calls_[kApiCount] = {};
  std::atomic<uint64_t> indirect_[kApiCount] = {};
  std::atomic<uint64_t> patched_[kApiCount] = {};
  std::atomic<uint64_t> alreadyOk_{0};
  std::atomic<bool> firstCall_[kApiCount] = {};
  std::atomic<bool> firstPatch_[kApiCount] = {};
  std::atomic<bool> warnedWaitEvents2_{false};
  std::atomic<std::chrono::steady_clock::time_point> lastReport_;
  std::mutex reportMutex_;
  uint64_t reportedIndirect_ = 0;  // под reportMutex_
  uint64_t reportedPatched_ = 0;
};

}  // namespace

std::unique_ptr<Feature> CreateIndirectBarrierFixFeature(const Config& config) {
  return std::make_unique<IndirectBarrierFix>(config);
}

}  // namespace doomfix
