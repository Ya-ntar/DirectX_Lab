#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <DirectXMath.h>

#include "AppConfig.h"
#include "MeshData.h"

namespace gfw {

class Framework;

struct LoadedSubmesh {
    const MeshBuffers *mesh = nullptr;
    std::wstring texture_path;
    DirectX::XMFLOAT4 albedo = {1.0f, 1.0f, 1.0f, 1.0f};
};

class AssetRepository {
public:
    explicit AssetRepository(Framework &framework);

    std::vector<LoadedSubmesh> ResolveSubmeshes(const SceneObjectConfig &object_config);

private:
    std::vector<LoadedSubmesh> LoadModelSubmeshes(const SceneObjectConfig &object_config);
    std::vector<LoadedSubmesh> LoadCubeSubmeshes();

    Framework &framework_;
    std::vector<std::unique_ptr<MeshBuffers>> mesh_storage_;
    std::unordered_map<std::wstring, std::vector<LoadedSubmesh>> model_cache_;
    const MeshBuffers *cube_mesh_ = nullptr;
};

}  // namespace gfw

