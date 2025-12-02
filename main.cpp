#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <iostream>

int main() {
    ID3D12Device* device = nullptr;

    HRESULT hr = D3D12CreateDevice(
        nullptr,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device)
    );

    if (SUCCEEDED(hr)) {
        std::cout << "DX12 device created!" << std::endl;
        device->Release();
    } else {
        std::cout << "Failed: " << std::hex << hr << std::endl;
    }


    return 0;
}
