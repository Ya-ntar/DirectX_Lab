#include "Framework.h"
#include "FrameworkInternal.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <vector>
#include <wincodec.h>

namespace gfw {
    namespace {
        struct DecodedImage {
            UINT width = 0;
            UINT height = 0;
            std::vector<std::uint8_t> rgba;
        };

        static bool EndsWithTga(const std::wstring &filename) {
            size_t dot_pos = filename.find_last_of(L'.');
            if (dot_pos == std::wstring::npos) {
                return false;
            }
            std::wstring ext = filename.substr(dot_pos);
            std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) {
                if (c >= L'A' && c <= L'Z') return static_cast<wchar_t>(c - L'A' + L'a');
                return c;
            });
            return ext == L".tga";
        }

        static std::uint16_t ReadLe16(const std::uint8_t *ptr) {
            return static_cast<std::uint16_t>(ptr[0] | (static_cast<std::uint16_t>(ptr[1]) << 8u));
        }

        static bool LoadTgaImage(const std::wstring &filename, DecodedImage &out_image) {
            std::ifstream file(filename, std::ios::binary);
            if (!file.is_open()) {
                return false;
            }

            std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                            std::istreambuf_iterator<char>());
            if (bytes.size() < 18) {
                return false;
            }

            const std::uint8_t id_len = bytes[0];
            const std::uint8_t color_map_type = bytes[1];
            const std::uint8_t image_type = bytes[2];
            if (color_map_type != 0) {
                return false;
            }
            if (image_type != 2 && image_type != 10) { // uncompressed / RLE true-color
                return false;
            }

            const std::uint16_t width = ReadLe16(&bytes[12]);
            const std::uint16_t height = ReadLe16(&bytes[14]);
            const std::uint8_t bpp = bytes[16];
            const std::uint8_t image_desc = bytes[17];
            if (width == 0 || height == 0) {
                return false;
            }
            if (bpp != 24 && bpp != 32) {
                return false;
            }

            const size_t pixel_bytes = bpp / 8;
            size_t offset = 18u + static_cast<size_t>(id_len);
            if (offset >= bytes.size()) {
                return false;
            }

            const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
            std::vector<std::uint8_t> bgra(pixel_count * 4, 255);
            size_t out_i = 0;

            auto read_pixel = [&](size_t src_offset) {
                bgra[out_i + 0] = bytes[src_offset + 0];
                bgra[out_i + 1] = bytes[src_offset + 1];
                bgra[out_i + 2] = bytes[src_offset + 2];
                bgra[out_i + 3] = (pixel_bytes == 4) ? bytes[src_offset + 3] : 255;
            };

            if (image_type == 2) {
                const size_t need = pixel_count * pixel_bytes;
                if (offset + need > bytes.size()) {
                    return false;
                }
                for (size_t i = 0; i < pixel_count; ++i) {
                    read_pixel(offset);
                    offset += pixel_bytes;
                    out_i += 4;
                }
            } else {
                while (out_i / 4 < pixel_count) {
                    if (offset >= bytes.size()) {
                        return false;
                    }
                    const std::uint8_t packet = bytes[offset++];
                    const size_t count = static_cast<size_t>((packet & 0x7Fu) + 1u);
                    if (packet & 0x80u) {
                        if (offset + pixel_bytes > bytes.size()) {
                            return false;
                        }
                        for (size_t i = 0; i < count; ++i) {
                            if (out_i / 4 >= pixel_count) {
                                return false;
                            }
                            read_pixel(offset);
                            out_i += 4;
                        }
                        offset += pixel_bytes;
                    } else {
                        const size_t need = count * pixel_bytes;
                        if (offset + need > bytes.size()) {
                            return false;
                        }
                        for (size_t i = 0; i < count; ++i) {
                            if (out_i / 4 >= pixel_count) {
                                return false;
                            }
                            read_pixel(offset);
                            offset += pixel_bytes;
                            out_i += 4;
                        }
                    }
                }
            }

            std::vector<std::uint8_t> rgba(pixel_count * 4, 255);
            const bool top_origin = (image_desc & 0x20u) != 0;
            for (UINT y = 0; y < height; ++y) {
                const UINT src_y = top_origin ? y : (height - 1u - y);
                for (UINT x = 0; x < width; ++x) {
                    const size_t src = (static_cast<size_t>(src_y) * width + x) * 4u;
                    const size_t dst = (static_cast<size_t>(y) * width + x) * 4u;
                    rgba[dst + 0] = bgra[src + 2];
                    rgba[dst + 1] = bgra[src + 1];
                    rgba[dst + 2] = bgra[src + 0];
                    rgba[dst + 3] = bgra[src + 3];
                }
            }

            out_image.width = width;
            out_image.height = height;
            out_image.rgba = std::move(rgba);
            return true;
        }

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

        bool CreateUploadBuffer(ID3D12Device *device, UINT size, const void *data,
                               ComPtr<ID3D12Resource> &out_resource) {
            const D3D12_HEAP_PROPERTIES heap = detail::HeapProperties(D3D12_HEAP_TYPE_UPLOAD);
            const D3D12_RESOURCE_DESC desc = detail::BufferDesc(size);
            if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&out_resource))))
                return false;
            void *mapped = nullptr;
            if (FAILED(out_resource->Map(0, nullptr, &mapped)))
                return false;
            std::memcpy(mapped, data, size);
            out_resource->Unmap(0, nullptr);
            return true;
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
        buffers->vertex_count = mesh_data.vertex_count;  // Store vertex count for non-indexed draws

        const UINT vb_size = static_cast<UINT>(mesh_data.vertex_data.size());
        if (!CreateUploadBuffer(device_.Get(), vb_size, mesh_data.vertex_data.data(), buffers->vertex_buffer)) {
            std::wcerr << L"Failed to create vertex buffer!" << std::endl;
            return nullptr;
        }
        buffers->vertex_buffer_view.BufferLocation = buffers->vertex_buffer->GetGPUVirtualAddress();
        buffers->vertex_buffer_view.SizeInBytes = vb_size;
        buffers->vertex_buffer_view.StrideInBytes = mesh_data.vertex_stride;

        if (!mesh_data.indices.empty()) {
            const UINT ib_size = static_cast<UINT>(mesh_data.indices.size() * sizeof(std::uint32_t));
            if (!CreateUploadBuffer(device_.Get(), ib_size, mesh_data.indices.data(), buffers->index_buffer)) {
                std::wcerr << L"Failed to create index buffer!" << std::endl;
                return nullptr;
            }
            buffers->index_buffer_view.BufferLocation = buffers->index_buffer->GetGPUVirtualAddress();
            buffers->index_buffer_view.SizeInBytes = ib_size;
            buffers->index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
            buffers->index_count = static_cast<UINT>(mesh_data.indices.size());
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

    std::shared_ptr<Texture2D> Framework::CreateTextureFromFile(const std::wstring &filename) {
        if (!device_ || !srv_heap_) {
            return {};
        }

        const UINT heap_capacity = srv_heap_->GetDesc().NumDescriptors;
        if (next_srv_index_ >= heap_capacity) {
            return {};
        }

        UINT width = 0;
        UINT height = 0;
        std::vector<std::uint8_t> rgba_data;

        bool loaded = false;
        ComPtr<IWICImagingFactory> wic_factory;
        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(&wic_factory)))) {
            ComPtr<IWICBitmapDecoder> decoder;
            if (SUCCEEDED(wic_factory->CreateDecoderFromFilename(
                    filename.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder))) {
                ComPtr<IWICBitmapFrameDecode> frame;
                if (SUCCEEDED(decoder->GetFrame(0, &frame))) {
                    if (SUCCEEDED(frame->GetSize(&width, &height)) && width > 0 && height > 0) {
                        ComPtr<IWICFormatConverter> converter;
                        if (SUCCEEDED(wic_factory->CreateFormatConverter(&converter)) &&
                            SUCCEEDED(converter->Initialize(
                                    frame.Get(),
                                    GUID_WICPixelFormat32bppRGBA,
                                    WICBitmapDitherTypeNone,
                                    nullptr,
                                    0.0,
                                    WICBitmapPaletteTypeCustom))) {
                            const UINT src_row_pitch = width * 4;
                            rgba_data.resize(static_cast<size_t>(src_row_pitch) * height);
                            if (SUCCEEDED(converter->CopyPixels(nullptr, src_row_pitch,
                                                                static_cast<UINT>(rgba_data.size()),
                                                                rgba_data.data()))) {
                                loaded = true;
                            }
                        }
                    }
                }
            }
        }

        if (!loaded && EndsWithTga(filename)) {
            DecodedImage tga;
            if (LoadTgaImage(filename, tga)) {
                width = tga.width;
                height = tga.height;
                rgba_data = std::move(tga.rgba);
                loaded = true;
            }
        }

        if (!loaded || width == 0 || height == 0 || rgba_data.empty()) {
            return {};
        }

        const UINT src_row_pitch = width * 4;

        D3D12_RESOURCE_DESC tex_desc = {};
        tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        tex_desc.Alignment = 0;
        tex_desc.Width = width;
        tex_desc.Height = height;
        tex_desc.DepthOrArraySize = 1;
        tex_desc.MipLevels = 1;
        tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        tex_desc.SampleDesc.Count = 1;
        tex_desc.SampleDesc.Quality = 0;
        tex_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        tex_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        const D3D12_HEAP_PROPERTIES default_heap_props = detail::HeapProperties(D3D12_HEAP_TYPE_DEFAULT);
        ComPtr<ID3D12Resource> resource;
        if (FAILED(device_->CreateCommittedResource(
                &default_heap_props, D3D12_HEAP_FLAG_NONE, &tex_desc,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource)))) {
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
        if (FAILED(device_->CreateCommittedResource(
                &upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_desc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload)))) {
            return {};
        }

        void *mapped = nullptr;
        if (FAILED(upload->Map(0, nullptr, &mapped))) {
            return {};
        }
        std::memset(mapped, 0, static_cast<size_t>(upload_size));
        auto *dst_bytes = static_cast<std::uint8_t *>(mapped);
        for (UINT y = 0; y < height; ++y) {
            const auto *src_row = rgba_data.data() + static_cast<size_t>(y) * src_row_pitch;
            auto *dst_row = dst_bytes + static_cast<size_t>(y) * footprint.Footprint.RowPitch;
            std::memcpy(dst_row, src_row, src_row_pitch);
        }
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

