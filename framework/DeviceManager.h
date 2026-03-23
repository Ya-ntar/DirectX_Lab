#pragma once

#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include "FrameworkInternal.h"

namespace gfw {

class DeviceManager {
public:
    DeviceManager() = default;
    ~DeviceManager() = default;

    // Initialize the device manager (creates the DXGI factory and D3D12 device)
    bool Initialize();

    // Get the DXGI factory
    Microsoft::WRL::ComPtr<IDXGIFactory4> GetFactory() const { return factory_; }

    // Get the D3D12 device
    Microsoft::WRL::ComPtr<ID3D12Device> GetDevice() const { return device_; }

    // Check if the device is valid
    bool IsValid() const { return device_ != nullptr; }

private:
    static Microsoft::WRL::ComPtr<IDXGIAdapter1> GetHardwareAdapter(IDXGIFactory1* pFactory);
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
};

} // namespace gfw