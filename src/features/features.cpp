#include "features/feature.h"

#include "common/config.h"

namespace doomfix {

std::unique_ptr<Feature> CreateHideDeviceExtensionsFeature(const Config& config);

std::vector<std::unique_ptr<Feature>> CreateFeatures(const Config& config) {
  std::vector<std::unique_ptr<Feature>> features;
  features.push_back(CreateHideDeviceExtensionsFeature(config));
  // Сюда добавятся фичи дампа и замены шейдеров.
  return features;
}

}  // namespace doomfix
