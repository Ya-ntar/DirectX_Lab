#include "DeviceManager.h"
#include <iostream>

namespace gfw {

bool DeviceManager::Initialize() {
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory_));
    if (FAILED(hr)) {
        std::cerr << "Failed to create DXGI factory!" << std::endl;
        return false;
    }

    // Find a hardware adapter.
    Microsoft::WRL::ComPtr<IDXGIAdapter1> hardwareAdapter;
    hardwareAdapter = GetHardwareAdapter(factory_.Get());
    if (hardwareAdapter.Get() == nullptr) {
        std::cerr << "Failed to find hardware adapter!" << std::endl;
        return false;
    }

    hr = D3D12CreateDevice(
        hardwareAdapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device_));
    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D12 device!" << std::endl;
        return false;
    }

    return true;
}

Microsoft::WRL::ComPtr<IDXGIAdapter1> DeviceManager::GetHardwareAdapter(IDXGIFactory1* pFactory) {
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    Microsoft::WRL::ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6)))) {
        for (
            UINT adapterIndex = 0;
            SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                adapterIndex,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(&adapter)));
            ++adapterIndex) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see if the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(),
                    D3D_FEATURE_LEVEL_11_0,
                    _uuidof(ID3D12Device),
                    nullptr))) {
                break;
            }
        }
    }

    if (adapter.Get() == nullptr) {
        for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see if the adapter supports Direct3D 12.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(),
                    D3D_FEATURE_LEVEL_11_0,
                    _uuidof(ID3D12Device),
                    nullptr))) {
                break;
            }
        }
    }

    return adapter;
}

} // namespace gfw