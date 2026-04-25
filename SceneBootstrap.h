#pragma once

#include <vector>

#include "SceneConfig.h"
#include "AssetRepository.h"
#include "MaterialResolver.h"
#include "framework/FrameworkTypes.h"

namespace gfw {

std::vector<RenderObject> BootstrapSceneObjects(const SceneConfig &config,
                                                AssetRepository &asset_repository,
                                                MaterialResolver &material_resolver);

}  // namespace gfw

