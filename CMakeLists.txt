cmake_minimum_required(VERSION 3.20)
project(DX12Test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(DX12Test main.cpp)

# Линкуем библиотеки Windows SDK (MSVC сам знает их путь)
target_link_libraries(DX12Test PRIVATE
        d3d12.lib
        dxgi.lib
        dxguid.lib
)
