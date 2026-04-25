#pragma once

#include "MeshData.h"
#include <string>
#include <vector>
#include <DirectXMath.h>

namespace gfw {
    struct ObjSubmeshData {
        MeshData mesh;
        std::wstring material_name;
        std::wstring diffuse_texture_path;
        DirectX::XMFLOAT4 albedo = {1.0f, 1.0f, 1.0f, 1.0f};
    };

    struct ObjModelData {
        std::vector<ObjSubmeshData> submeshes;
    };

    class MeshLoader {
    public:
        static ObjModelData LoadObjModel(const std::wstring &obj_filename, const std::wstring &mtl_filename = L"");
        static MeshData LoadObj(const std::wstring& filename);
    };
}
