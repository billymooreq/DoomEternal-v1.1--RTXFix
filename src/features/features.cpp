#include "features/feature.h"

#include "common/config.h"

namespace doomfix {

std::unique_ptr<Feature> CreateHideDeviceExtensionsFeature(const Config& config);
std::unique_ptr<Feature> CreateIndirectBarrierFixFeature(const Config& config);

std::vector<std::unique_ptr<Feature>> CreateFeatures(const Config& config) {
  std::vector<std::unique_ptr<Feature>> features;
  features.push_back(CreateHideDeviceExtensionsFeature(config));
  // Создаётся и при DOOMFIX_INDIRECT_FIX=off: тогда только считает барьеры (A/B-сравнение).
  features.push_back(CreateIndirectBarrierFixFeature(config));
  // Сюда добавятся фичи дампа и замены шейдеров.
  return features;
}

}  // namespace doomfix
