
#pragma once

#include <DirectXMath.h>

namespace gfw {

    struct SceneConstants {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 proj;
        DirectX::XMFLOAT4 light_dir_shininess;
        DirectX::XMFLOAT4 camera_pos;
        DirectX::XMFLOAT4 light_color;
        DirectX::XMFLOAT4 ambient_color;
        DirectX::XMFLOAT4 albedo;
    };
}