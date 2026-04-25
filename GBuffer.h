#pragma once

#include <array>
#include <d3d12.h>
#include <wrl/client.h>

namespace gfw {
class GBuffer {
public:
    static constexpr UINT kTargetCount = 3;

    bool Initialize(ID3D12Device *device, UINT width, UINT height);
    void Shutdown();

    void TransitionToRenderTargets(ID3D12GraphicsCommandList *cmd) const;
    void TransitionToShaderResources(ID3D12GraphicsCommandList *cmd) const;
    void Clear(ID3D12GraphicsCommandList *cmd) const;

    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetRtv(UINT index) const;
    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(UINT index) const;
    [[nodiscard]] ID3D12DescriptorHeap *GetSrvHeap() const { return srv_heap_.Get(); }

private:
    struct Target {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    };

    ID3D12Device *device_ = nullptr;
    UINT width_ = 0;
    UINT height_ = 0;
    UINT rtv_stride_ = 0;
    UINT srv_stride_ = 0;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_heap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srv_heap_;
    std::array<Target, kTargetCount> targets_ = {};
};
}
