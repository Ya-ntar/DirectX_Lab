#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <memory>
#include "Exports.h"
#include "../MeshData.h"

using Microsoft::WRL::ComPtr;

namespace gfw {

struct Texture2D {
    ComPtr<ID3D12Resource> resource;
    D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu = {};
};

struct RenderObject {
    const MeshBuffers *mesh = nullptr;
    DirectX::XMFLOAT4X4 world = {};
    DirectX::XMFLOAT4 albedo = {0.85f, 0.25f, 0.25f, 1.0f};
    DirectX::XMFLOAT4 uv_params = {1.0f, 1.0f, 0.15f, -0.10f};
    DirectX::XMFLOAT4 effect_params = {0.0f, 0.0f, 0.0f, 0.0f};
    std::shared_ptr<Texture2D> texture = {};
    // Optional textures for advanced shading (can be null).
    std::shared_ptr<Texture2D> normal_texture = {};
    std::shared_ptr<Texture2D> displacement_texture = {};

    // x=min tess, y=max tess, z=near distance, w=far distance
    DirectX::XMFLOAT4 tess_params = {1.0f, 8.0f, 2.0f, 25.0f};
    float displacement_scale = 0.08f;
    float _pad0[3] = {0.0f, 0.0f, 0.0f};

    RenderObject() {
        DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixIdentity());
    }


    void DisableUVAnimation() {
        uv_params.z = 0.0f;
        uv_params.w = 0.0f;
    }
};
}
