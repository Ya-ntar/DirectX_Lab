cmake_minimum_required(VERSION 3.20)
project(DX12Test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

if (NOT WIN32)
    message(FATAL_ERROR "This project requires Windows (DirectX 12).")
endif ()

if (NOT (MSVC OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")))
    message(FATAL_ERROR "DirectX 12 requires MSVC or clang-cl toolchain because it fucking sucks. Settings -> Toolchains -> select Visual Studio.")
endif ()

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    if (MSVC)
        add_compile_options(/arch:SSE2)
    else ()
        add_compile_options(-msse2)
    endif ()
endif ()

add_executable(DX12Test main.cpp
        framework/Keys.h
        framework/InputDevice.h
        framework/InputDevice.cpp
        framework/Window.h
        framework/Window.cpp
        framework/Delegates.h
        framework/Delegates.cpp
        framework/Exports.h
        framework/Timer.h
        framework/Timer.cpp
        framework/Framework.h
        framework/FrameworkInternal.h
        framework/Framework.Init.cpp
        framework/Framework.Frame.cpp
        framework/Framework.Pipeline.cpp
        framework/Framework.Resources.cpp
        framework/Framework.Render.cpp
        CubeMesh.h
        CubeMesh.cpp
        MeshData.h
        MeshLoader.h
        MeshLoader.cpp
        framework/Constants.h)

target_compile_definitions(DX12Test PRIVATE GAMEFRAMEWORK_STATIC)

target_link_libraries(DX12Test PRIVATE
        d3d12.lib
        d3dcompiler.lib
        dxgi.lib
        dxguid.lib
)
