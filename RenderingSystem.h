#pragma once

#include <array>
#include <vector>

#include "GBuffer.h"
#include "SceneLighting.h"
#include "framework/Framework.h"

namespace gfw {

class RenderingSystem {
public:
    static constexpr UINT kMaxPointLights = 16;
    static constexpr UINT kMaxSpotLights = 8;

    bool Initialize(Framework *framework, UINT width, UINT height);
    void Shutdown();

    void SetDirectionalLight(const DirectionalLight &light);
    void SetPointLights(const std::vector<PointLight> &lights);
    void SetSpotLights(const std::vector<SpotLight> &lights);

    void Render(const std::vector<RenderObject> &objects, float total_time);

private:
    struct GeometryCB {
        DirectX::XMFLOAT4X4 world = {};
        DirectX::XMFLOAT4X4 view = {};
        DirectX::XMFLOAT4X4 proj = {};
        DirectX::XMFLOAT4 albedo = {1.0f, 1.0f, 1.0f, 1.0f};
    };

    struct PointLightGpu {
        DirectX::XMFLOAT4 pos_range = {};
        DirectX::XMFLOAT4 color_intensity = {};
    };

    struct SpotLightGpu {
        DirectX::XMFLOAT4 pos_range = {};
        DirectX::XMFLOAT4 dir_angle_cos = {};
        DirectX::XMFLOAT4 color_intensity = {};
    };

    struct LightingCB {
        DirectX::XMFLOAT4 dir_light_dir = {};
        DirectX::XMFLOAT4 dir_light_color_intensity = {};
        DirectX::XMFLOAT4 ambient_color = {};
        std::array<PointLightGpu, kMaxPointLights> point_lights = {};
        std::array<SpotLightGpu, kMaxSpotLights> spot_lights = {};
        UINT point_count = 0;
        UINT spot_count = 0;
        DirectX::XMFLOAT2 _pad = {};
    };

    bool CreateGeometryPipeline();
    bool CreateLightingPipeline();
    bool CreateConstantBuffers();

    void GeometryPass(const std::vector<RenderObject> &objects);
    void LightingPass();

    Framework *framework_ = nullptr;
    GBuffer gbuffer_ = {};
    DirectionalLight directional_light_ = {};
    std::vector<PointLight> point_lights_ = {};
    std::vector<SpotLight> spot_lights_ = {};

    Microsoft::WRL::ComPtr<ID3D12RootSignature> geometry_root_sig_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> geometry_pso_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> lighting_root_sig_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> lighting_pso_;
    Microsoft::WRL::ComPtr<ID3D12Resource> geometry_cb_;
    Microsoft::WRL::ComPtr<ID3D12Resource> lighting_cb_;
    std::uint8_t *geometry_cb_mapped_ = nullptr;
    std::uint8_t *lighting_cb_mapped_ = nullptr;
    std::shared_ptr<Texture2D> fallback_white_ = {};
};
}
