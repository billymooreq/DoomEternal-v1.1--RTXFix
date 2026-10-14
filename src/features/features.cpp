#include "features/feature.h"

#include "common/config.h"

namespace doomfix {

std::vector<std::unique_ptr<Feature>> CreateFeatures(const Config& /*config*/) {
  std::vector<std::unique_ptr<Feature>> features;
  // Сюда добавятся фичи дампа и замены шейдеров.
  return features;
}

}  // namespace doomfix
