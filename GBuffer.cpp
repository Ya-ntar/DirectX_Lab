#include "GBuffer.h"

#include "framework/FrameworkInternal.h"

namespace gfw {
bool GBuffer::Initialize(ID3D12Device *device, UINT width, UINT height) {
    if (!device || width == 0 || height == 0) {
        return false;
    }

    device_ = device;
    width_ = width;
    height_ = height;

    targets_[0].format = DXGI_FORMAT_R16G16B16A16_FLOAT; // world position
    targets_[1].format = DXGI_FORMAT_R16G16B16A16_FLOAT; // world normal
    targets_[2].format = DXGI_FORMAT_R8G8B8A8_UNORM;     // albedo

    D3D12_DESCRIPTOR_HEAP_DESC rtv_desc = {};
    rtv_desc.NumDescriptors = kTargetCount;
    rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(device_->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&rtv_heap_)))) {
        return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC srv_desc = {};
    srv_desc.NumDescriptors = kTargetCount;
    srv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device_->CreateDescriptorHeap(&srv_desc, IID_PPV_ARGS(&srv_heap_)))) {
        return false;
    }

    rtv_stride_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    srv_stride_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    for (UINT i = 0; i < kTargetCount; ++i) {
        D3D12_RESOURCE_DESC tex_desc = {};
        tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        tex_desc.Width = width_;
        tex_desc.Height = height_;
        tex_desc.DepthOrArraySize = 1;
        tex_desc.MipLevels = 1;
        tex_desc.Format = targets_[i].format;
        tex_desc.SampleDesc.Count = 1;
        tex_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        tex_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clear_value = {};
        clear_value.Format = targets_[i].format;
        clear_value.Color[0] = 0.0f;
        clear_value.Color[1] = 0.0f;
        clear_value.Color[2] = 0.0f;
        clear_value.Color[3] = 0.0f;

        const D3D12_HEAP_PROPERTIES heap_props = detail::HeapProperties(D3D12_HEAP_TYPE_DEFAULT);
        if (FAILED(device_->CreateCommittedResource(
                &heap_props,
                D3D12_HEAP_FLAG_NONE,
                &tex_desc,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                &clear_value,
                IID_PPV_ARGS(&targets_[i].resource)))) {
            return false;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetRtv(i);
        device_->CreateRenderTargetView(targets_[i].resource.Get(), nullptr, rtv);

        D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Format = targets_[i].format;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Texture2D.MipLevels = 1;
        D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu = srv_heap_->GetCPUDescriptorHandleForHeapStart();
        srv_cpu.ptr += static_cast<SIZE_T>(i) * srv_stride_;
        device_->CreateShaderResourceView(targets_[i].resource.Get(), &srv, srv_cpu);
    }

    return true;
}

void GBuffer::Shutdown() {
    for (auto &target : targets_) {
        target.resource.Reset();
    }
    srv_heap_.Reset();
    rtv_heap_.Reset();
    device_ = nullptr;
    width_ = 0;
    height_ = 0;
    rtv_stride_ = 0;
    srv_stride_ = 0;
}

void GBuffer::TransitionToRenderTargets(ID3D12GraphicsCommandList *cmd) const {
    D3D12_RESOURCE_BARRIER barriers[kTargetCount] = {};
    for (UINT i = 0; i < kTargetCount; ++i) {
        barriers[i] = detail::TransitionBarrier(
            targets_[i].resource.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
    cmd->ResourceBarrier(kTargetCount, barriers);
}

void GBuffer::TransitionToShaderResources(ID3D12GraphicsCommandList *cmd) const {
    D3D12_RESOURCE_BARRIER barriers[kTargetCount] = {};
    for (UINT i = 0; i < kTargetCount; ++i) {
        barriers[i] = detail::TransitionBarrier(
            targets_[i].resource.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    cmd->ResourceBarrier(kTargetCount, barriers);
}

void GBuffer::Clear(ID3D12GraphicsCommandList *cmd) const {
    const float clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (UINT i = 0; i < kTargetCount; ++i) {
        cmd->ClearRenderTargetView(GetRtv(i), clear_color, 0, nullptr);
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE GBuffer::GetRtv(UINT index) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * rtv_stride_;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE GBuffer::GetSrv(UINT index) const {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = srv_heap_->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(index) * srv_stride_;
    return handle;
}
}
