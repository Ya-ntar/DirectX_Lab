#pragma once

#include <vector>

#include "AppConfig.h"
#include "AssetRepository.h"
#include "MaterialResolver.h"
#include "framework/FrameworkTypes.h"

namespace gfw {

std::vector<RenderObject> BootstrapSceneObjects(const AppConfig &config,
                                                AssetRepository &asset_repository,
                                                MaterialResolver &material_resolver);

}  // namespace gfw

