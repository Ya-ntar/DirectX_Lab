cmake_minimum_required(VERSION 3.20)
project(DX12Test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(DX12Test main.cpp
        Keys.h
        InputDevice.h
        InputDevice.cpp
        Window.h
        Window.cpp
        Delegates.h
        Delegates.cpp
        Exports.h
        Timer.h
        Timer.cpp
        Framework.h
        Framework.cpp)

target_compile_definitions(DX12Test PRIVATE GAMEFRAMEWORK_STATIC)

target_link_libraries(DX12Test PRIVATE
        d3d12.lib
        dxgi.lib
        dxguid.lib
)
