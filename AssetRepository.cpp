#include "AssetRepository.h"

#include "CubeMesh.h"
#include "MeshLoader.h"
#include "framework/Framework.h"

namespace gfw {

AssetRepository::AssetRepository(Framework &framework) : framework_(framework) {
}

std::vector<LoadedSubmesh> AssetRepository::ResolveSubmeshes(const SceneObjectConfig &object_config) {
    if (!object_config.obj_path.empty()) {
        return LoadModelSubmeshes(object_config);
    }
    return LoadCubeSubmeshes();
}

std::vector<LoadedSubmesh> AssetRepository::LoadModelSubmeshes(const SceneObjectConfig &object_config) {
    const std::wstring mesh_key = object_config.obj_path + L"|" + object_config.mtl_path;
    const auto cached_it = model_cache_.find(mesh_key);
    if (cached_it != model_cache_.end()) {
        return cached_it->second;
    }

    ObjModelData model = MeshLoader::LoadObjModel(object_config.obj_path, object_config.mtl_path);
    std::vector<LoadedSubmesh> loaded_submeshes;

    for (const auto &submesh : model.submeshes) {
        if (submesh.mesh.vertex_count == 0 || submesh.mesh.indices.empty()) {
            continue;
        }

        std::unique_ptr<MeshBuffers> buffers = framework_.CreateMeshBuffers(submesh.mesh);
        if (!buffers) {
            continue;
        }

        const MeshBuffers *mesh_ptr = buffers.get();
        mesh_storage_.push_back(std::move(buffers));
        loaded_submeshes.push_back(LoadedSubmesh{
            .mesh = mesh_ptr,
            .texture_path = submesh.diffuse_texture_path,
            .albedo = submesh.albedo,
        });
    }

    model_cache_[mesh_key] = loaded_submeshes;
    return loaded_submeshes;
}

std::vector<LoadedSubmesh> AssetRepository::LoadCubeSubmeshes() {
    if (!cube_mesh_) {
        MeshData cube_data = CubeMesh::CreateUnit().ToMeshData();
        std::unique_ptr<MeshBuffers> cube_buffers = framework_.CreateMeshBuffers(cube_data);
        if (!cube_buffers) {
            return {};
        }

        cube_mesh_ = cube_buffers.get();
        mesh_storage_.push_back(std::move(cube_buffers));
    }

    return {
        LoadedSubmesh{
            .mesh = cube_mesh_,
            .texture_path = L"",
            .albedo = {1.0f, 1.0f, 1.0f, 1.0f},
        },
    };
}

}  // namespace gfw

