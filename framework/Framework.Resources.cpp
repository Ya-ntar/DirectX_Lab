#include "Framework.h"
#include "FrameworkInternal.h"

#include <algorithm>
#include <iterator>

namespace gfw {
    namespace {
        std::uint32_t PackRGBA8(const DirectX::XMFLOAT4 &color) {
            auto clamp01 = [](float v) {
                return std::max(0.0f, std::min(1.0f, v));
            };

            const std::uint32_t r = static_cast<std::uint32_t>(clamp01(color.x) * 255.0f + 0.5f);
            const std::uint32_t g = static_cast<std::uint32_t>(clamp01(color.y) * 255.0f + 0.5f);
            const std::uint32_t b = static_cast<std::uint32_t>(clamp01(color.z) * 255.0f + 0.5f);
            const std::uint32_t a = static_cast<std::uint32_t>(clamp01(color.w) * 255.0f + 0.5f);

            return (a << 24u) | (b << 16u) | (g << 8u) | r;
        }
    }

    bool Framework::CreateDepthResources() {
        D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = {};
        dsv_heap_desc.NumDescriptors = 1;
        dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        if (FAILED(device_->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&dsv_heap_)))) {
            std::wcerr << L"Failed to create DSV Descriptor Heap!" << std::endl;
            return false;
        }

        D3D12_RESOURCE_DESC depth_desc = {};
        depth_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depth_desc.Alignment = 0;
        depth_desc.Width = window_->GetWidth();
        depth_desc.Height = window_->GetHeight();
        depth_desc.DepthOrArraySize = 1;
        depth_desc.MipLevels = 1;
        depth_desc.Format = DXGI_FORMAT_D32_FLOAT;
        depth_desc.SampleDesc.Count = 1;
        depth_desc.SampleDesc.Quality = 0;
        depth_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depth_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clear_value = {};
        clear_value.Format = DXGI_FORMAT_D32_FLOAT;
        clear_value.DepthStencil.Depth = 1.0f;
        clear_value.DepthStencil.Stencil = 0;

        const D3D12_HEAP_PROPERTIES heap_props = detail::HeapProperties(D3D12_HEAP_TYPE_DEFAULT);
        if (FAILED(device_->CreateCommittedResource(
                &heap_props,
                D3D12_HEAP_FLAG_NONE,
                &depth_desc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &clear_value,
                IID_PPV_ARGS(&depth_stencil_)))) {
            std::wcerr << L"Failed to create Depth Stencil resource!" << std::endl;
            return false;
        }

        D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
        dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
        dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
        device_->CreateDepthStencilView(depth_stencil_.Get(), &dsv_desc,
                                        dsv_heap_->GetCPUDescriptorHandleForHeapStart());

        return true;
    }

    bool Framework::CreateConstantBuffer() {
        const UINT cb_size = detail::Align256(sizeof(SceneConstants));

        const D3D12_HEAP_PROPERTIES heap_props = detail::HeapProperties(D3D12_HEAP_TYPE_UPLOAD);
        const D3D12_RESOURCE_DESC cb_desc = detail::BufferDesc(cb_size);
        if (FAILED(device_->CreateCommittedResource(
                &heap_props,
                D3D12_HEAP_FLAG_NONE,
                &cb_desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&constant_buffer_)))) {
            std::wcerr << L"Failed to create constant buffer!" << std::endl;
            return false;
        }

        void *mapped = nullptr;
        if (FAILED(constant_buffer_->Map(0, nullptr, &mapped))) {
            std::wcerr << L"Failed to map constant buffer!" << std::endl;
            return false;
        }

        constant_buffer_mapped_ = static_cast<std::uint8_t *>(mapped);
        std::memset(constant_buffer_mapped_, 0, cb_size);
        return true;
    }

    std::unique_ptr<MeshBuffers> Framework::CreateMeshBuffers(const MeshData &mesh_data) {
        if (mesh_data.vertex_data.empty()) {
            std::wcerr << L"CreateMeshBuffers: Empty vertex data!" << std::endl;
            return nullptr;
        }

        auto buffers = std::make_unique<MeshBuffers>();
        buffers->topology = mesh_data.topology;

        const UINT vb_size = static_cast<UINT>(mesh_data.vertex_data.size());
        const D3D12_HEAP_PROPERTIES heap_props = detail::HeapProperties(D3D12_HEAP_TYPE_UPLOAD);
        const D3D12_RESOURCE_DESC vb_desc = detail::BufferDesc(vb_size);

        if (FAILED(device_->CreateCommittedResource(
                &heap_props,
                D3D12_HEAP_FLAG_NONE,
                &vb_desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&buffers->vertex_buffer)))) {
            std::wcerr << L"Failed to create vertex buffer!" << std::endl;
            return nullptr;
        }

        void *vb_mapped = nullptr;
        if (FAILED(buffers->vertex_buffer->Map(0, nullptr, &vb_mapped))) {
            std::wcerr << L"Failed to map vertex buffer!" << std::endl;
            return nullptr;
        }
        std::memcpy(vb_mapped, mesh_data.vertex_data.data(), vb_size);
        buffers->vertex_buffer->Unmap(0, nullptr);

        buffers->vertex_buffer_view.BufferLocation = buffers->vertex_buffer->GetGPUVirtualAddress();
        buffers->vertex_buffer_view.SizeInBytes = vb_size;
        buffers->vertex_buffer_view.StrideInBytes = mesh_data.vertex_stride;

        if (!mesh_data.indices.empty()) {
            const UINT ib_size = static_cast<UINT>(mesh_data.indices.size() * sizeof(std::uint16_t));
            const D3D12_RESOURCE_DESC ib_desc = detail::BufferDesc(ib_size);

            if (FAILED(device_->CreateCommittedResource(
                    &heap_props,
                    D3D12_HEAP_FLAG_NONE,
                    &ib_desc,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(&buffers->index_buffer)))) {
                std::wcerr << L"Failed to create index buffer!" << std::endl;
                return nullptr;
            }

            void *ib_mapped = nullptr;
            if (FAILED(buffers->index_buffer->Map(0, nullptr, &ib_mapped))) {
                std::wcerr << L"Failed to map index buffer!" << std::endl;
                return nullptr;
            }
            std::memcpy(ib_mapped, mesh_data.indices.data(), ib_size);
            buffers->index_buffer->Unmap(0, nullptr);

            buffers->index_buffer_view.BufferLocation = buffers->index_buffer->GetGPUVirtualAddress();
            buffers->index_buffer_view.SizeInBytes = ib_size;
            buffers->index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
            buffers->index_count = static_cast<UINT>(mesh_data.indices.size());
        } else {
            buffers->index_count = mesh_data.vertex_count;
        }

        return buffers;
    }

    bool Framework::CreateSrvHeap(UINT descriptor_count) {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = descriptor_count;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        desc.NodeMask = 0;

        if (FAILED(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srv_heap_)))) {
            std::wcerr << L"Failed to create SRV descriptor heap!" << std::endl;
            return false;
        }

        return true;
    }

    std::shared_ptr<Texture2D> Framework::CreateSolidTexture(std::uint32_t rgba8) {
        if (!device_ || !srv_heap_) {
            return {};
        }

        const UINT heap_capacity = srv_heap_->GetDesc().NumDescriptors;
        if (next_srv_index_ >= heap_capacity) {
            return {};
        }

        D3D12_RESOURCE_DESC tex_desc = {};
        tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        tex_desc.Alignment = 0;
        tex_desc.Width = 1;
        tex_desc.Height = 1;
        tex_desc.DepthOrArraySize = 1;
        tex_desc.MipLevels = 1;
        tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        tex_desc.SampleDesc.Count = 1;
        tex_desc.SampleDesc.Quality = 0;
        tex_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        tex_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        const D3D12_HEAP_PROPERTIES default_heap_props = detail::HeapProperties(D3D12_HEAP_TYPE_DEFAULT);

        ComPtr<ID3D12Resource> resource;
        HRESULT hr = device_->CreateCommittedResource(
                &default_heap_props,
                D3D12_HEAP_FLAG_NONE,
                &tex_desc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&resource));
        if (FAILED(hr)) {
            std::wcerr << L"Failed to create texture resource! hr=0x" << std::hex << hr << std::dec << std::endl;
            return {};
        }

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
        UINT num_rows = 0;
        UINT64 row_size_in_bytes = 0;
        UINT64 upload_size = 0;
        device_->GetCopyableFootprints(&tex_desc, 0, 1, 0, &footprint, &num_rows, &row_size_in_bytes, &upload_size);

        const D3D12_HEAP_PROPERTIES upload_heap_props = detail::HeapProperties(D3D12_HEAP_TYPE_UPLOAD);
        const D3D12_RESOURCE_DESC upload_desc = detail::BufferDesc(upload_size);

        ComPtr<ID3D12Resource> upload;
        hr = device_->CreateCommittedResource(
                &upload_heap_props,
                D3D12_HEAP_FLAG_NONE,
                &upload_desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&upload));
        if (FAILED(hr)) {
            std::wcerr << L"Failed to create texture upload buffer! hr=0x" << std::hex << hr << std::dec << std::endl;
            return {};
        }

        void *mapped = nullptr;
        hr = upload->Map(0, nullptr, &mapped);
        if (FAILED(hr)) {
            std::wcerr << L"Failed to map texture upload buffer! hr=0x" << std::hex << hr << std::dec << std::endl;
            return {};
        }
        std::memset(mapped, 0, static_cast<size_t>(upload_size));
        std::memcpy(mapped, &rgba8, sizeof(rgba8));
        upload->Unmap(0, nullptr);

        if (FAILED(command_allocator_[frame_index_]->Reset())) {
            return {};
        }

        if (FAILED(command_list_->Reset(command_allocator_[frame_index_].Get(), nullptr))) {
            return {};
        }

        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = resource.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = upload.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = footprint;

        command_list_->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        const auto barrier = detail::TransitionBarrier(
                resource.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        command_list_->ResourceBarrier(1, &barrier);

        if (FAILED(command_list_->Close())) {
            return {};
        }

        ID3D12CommandList *lists[] = {command_list_.Get()};
        command_queue_->ExecuteCommandLists(static_cast<UINT>(std::size(lists)), lists);
        WaitForPreviousFrame();

        const UINT descriptor_index = next_srv_index_++;

        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = srv_heap_->GetCPUDescriptorHandleForHeapStart();
        cpu_handle.ptr += static_cast<SIZE_T>(descriptor_index) * srv_descriptor_size_;

        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = srv_heap_->GetGPUDescriptorHandleForHeapStart();
        gpu_handle.ptr += static_cast<UINT64>(descriptor_index) * srv_descriptor_size_;

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MostDetailedMip = 0;
        srv_desc.Texture2D.MipLevels = 1;
        srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

        device_->CreateShaderResourceView(resource.Get(), &srv_desc, cpu_handle);

        auto texture = std::make_shared<Texture2D>();
        texture->resource = resource;
        texture->srv_gpu = gpu_handle;
        textures_.push_back(texture);
        return texture;
    }

    std::shared_ptr<Texture2D> Framework::CreateSolidTexture(const DirectX::XMFLOAT4 &color) {
        return CreateSolidTexture(PackRGBA8(color));
    }
}

