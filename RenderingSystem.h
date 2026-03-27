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

    // GBuffer visualization modes
    enum class GBufferDebugMode {
        None = -1,
        Position = 0,
        Normal = 1,
        Albedo = 2,
        Depth = 3
    };

    // Rendering modes
    enum class RenderMode {
        Solid = 0,
        Wireframe = 1
    };

    bool Initialize(Framework *framework, UINT width, UINT height);
    void Shutdown();

    void SetDirectionalLight(const DirectionalLight &light);
    void SetPointLights(const std::vector<PointLight> &lights);
    void SetSpotLights(const std::vector<SpotLight> &lights);

    void Render(const std::vector<RenderObject> &objects, float total_time);

    // Tessellation control
    void SetTessellationEnabled(bool enabled) { tessellation_enabled_ = enabled; }
    void SetTessellationParams(float min_level, float max_level) {
        tessellation_min_ = min_level;
        tessellation_max_ = max_level;
    }
    void SetDisplacementScale(float scale) { displacement_scale_ = scale; }
    /// Extra height from normal map in tess domain (edges where tangent-space z is below 1).
    void SetNormalDisplacementScale(float scale) { normal_displacement_scale_ = scale; }
    bool IsTessellationEnabled() const { return tessellation_enabled_; }

    // GBuffer visualization
    void SetGBufferDebugMode(GBufferDebugMode mode) { gbuffer_debug_mode_ = mode; }
    GBufferDebugMode GetGBufferDebugMode() const { return gbuffer_debug_mode_; }

    // Render mode control
    void SetRenderMode(RenderMode mode) { render_mode_ = mode; }
    RenderMode GetRenderMode() const { return render_mode_; }
    void ToggleRenderMode() { render_mode_ = (render_mode_ == RenderMode::Solid) ? RenderMode::Wireframe : RenderMode::Solid; }

private:
    struct GeometryCB {
        DirectX::XMFLOAT4X4 world = {};
        DirectX::XMFLOAT4X4 view = {};
        DirectX::XMFLOAT4X4 proj = {};
        DirectX::XMFLOAT4 albedo = {1.0f, 1.0f, 1.0f, 1.0f};
        DirectX::XMFLOAT4 tess_params = {1.0f, 16.0f, 0.0f, 0.0f};
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
    bool CreateGeometryTessPipeline();
    bool CreateLightingPipeline();
    bool CreateGBufferDebugPipeline();
    bool CreateConstantBuffers();

    void GeometryPass(const std::vector<RenderObject> &objects);
    void LightingPass();
    void GBufferDebugPass();

    Framework *framework_ = nullptr;
    GBuffer gbuffer_ = {};
    DirectionalLight directional_light_ = {};
    std::vector<PointLight> point_lights_ = {};
    std::vector<SpotLight> spot_lights_ = {};

    Microsoft::WRL::ComPtr<ID3D12RootSignature> geometry_root_sig_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> geometry_pso_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> geometry_pso_wireframe_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> geometry_tess_root_sig_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> geometry_tess_pso_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> geometry_tess_pso_wireframe_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> lighting_root_sig_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> lighting_pso_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> gbuffer_debug_root_sig_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gbuffer_debug_pso_;
    Microsoft::WRL::ComPtr<ID3D12Resource> geometry_cb_;
    Microsoft::WRL::ComPtr<ID3D12Resource> lighting_cb_;
    Microsoft::WRL::ComPtr<ID3D12Resource> gbuffer_debug_cb_;
    std::uint8_t *geometry_cb_mapped_ = nullptr;
    std::uint8_t *lighting_cb_mapped_ = nullptr;
    std::uint8_t *gbuffer_debug_cb_mapped_ = nullptr;
    std::shared_ptr<Texture2D> fallback_white_ = {};

    struct GBufferDebugCB {
        INT mode = -1;
        DirectX::XMFLOAT3 _pad = {};
    };

    bool tessellation_enabled_ = true;
    float tessellation_min_ = 2.0f;
    float tessellation_max_ = 8.0f;
    float displacement_scale_ = 0.1f;
    float normal_displacement_scale_ = 0.0f;
    GBufferDebugMode gbuffer_debug_mode_ = GBufferDebugMode::None;
    RenderMode render_mode_ = RenderMode::Solid;
};
}
