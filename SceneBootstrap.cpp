#include "SceneBootstrap.h"

#include <iostream>

namespace gfw {
namespace {

DirectX::XMFLOAT4X4 MakeWorldMatrix(const DirectX::XMFLOAT3 &position, const DirectX::XMFLOAT3 &scale) {
    const DirectX::XMMATRIX world =
        DirectX::XMMatrixScaling(scale.x, scale.y, scale.z) *
        DirectX::XMMatrixTranslation(position.x, position.y, position.z);
    DirectX::XMFLOAT4X4 out = {};
    DirectX::XMStoreFloat4x4(&out, world);
    return out;
}

}  // namespace

std::vector<RenderObject> BootstrapSceneObjects(const AppConfig &config,
                                                AssetRepository &asset_repository,
                                                MaterialResolver &material_resolver) {
    std::vector<RenderObject> objects;

    for (const SceneObjectConfig &object_config : config.objects) {
        std::vector<LoadedSubmesh> submeshes = asset_repository.ResolveSubmeshes(object_config);
        if (submeshes.empty()) {
            std::wcerr << L"Skipped object '" << object_config.name << L"': failed to create mesh." << std::endl;
            continue;
        }

        for (const LoadedSubmesh &submesh : submeshes) {
            RenderObject render_object;
            render_object.mesh = submesh.mesh;
            render_object.world = MakeWorldMatrix(object_config.position, object_config.scale);
            render_object.uv_params = {2.0f, 2.0f, 0.08f, -0.05f};
            material_resolver.ApplyMaterial(object_config, submesh, render_object);
            objects.push_back(std::move(render_object));
        }
    }

    return objects;
}

}  // namespace gfw

