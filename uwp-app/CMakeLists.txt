# TLOU.Uwp: the packaged app (Xbox Developer Mode / Windows) that hosts gow3-runtime.dll.
# Configure for UWP with the Visual Studio generator:
#   cmake -S uwp-app -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore
#         -DCMAKE_SYSTEM_VERSION=10.0 -DGOW3_UWP_INCLUDE=<port>/gow3_pc/uwp -DGOW3_RUNTIME_DIR=<dlls>
cmake_minimum_required(VERSION 3.24)
project(TLOUUwp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GOW3_UWP_INCLUDE "" CACHE PATH "gow3_pc/uwp of the patched port (gow3_uwp_host.h)")
set(GOW3_RUNTIME_DIR "" CACHE PATH "Folder with gow3-runtime.dll and every DLL it loads")
if (NOT EXISTS "${GOW3_UWP_INCLUDE}/gow3_uwp_host.h")
    message(FATAL_ERROR "GOW3_UWP_INCLUDE must point at gow3_pc/uwp of the patched port")
endif()

file(GLOB ASSETS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.png)
set(RUNTIME_FILES "")
if (GOW3_RUNTIME_DIR)
    file(GLOB RUNTIME_FILES ${GOW3_RUNTIME_DIR}/*.dll ${GOW3_RUNTIME_DIR}/*.json ${GOW3_RUNTIME_DIR}/*.txt)
endif()
if (NOT RUNTIME_FILES)
    message(WARNING "No runtime DLLs in GOW3_RUNTIME_DIR: the app will build, but Play will fail")
endif()

add_executable(TLOUUwp WIN32
    src/App.cpp src/Host.cpp src/Stage.cpp src/Receiver.cpp src/pch.h src/Host.h src/Stage.h src/Receiver.h
    ${GOW3_UWP_INCLUDE}/gow3_uwp_host.h
    Package.appxmanifest ${ASSETS} ${RUNTIME_FILES})
target_include_directories(TLOUUwp PRIVATE src ${GOW3_UWP_INCLUDE})
target_compile_definitions(TLOUUwp PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(TLOUUwp PRIVATE /EHsc /bigobj /permissive- /Zc:__cplusplus /utf-8 /W3)
target_link_libraries(TLOUUwp PRIVATE WindowsApp.lib d3d11.lib dxgi.lib d2d1.lib dwrite.lib xaudio2.lib)
# The PS4 address range (64 GiB - 1 TiB) must stay free for the game: no high-entropy ASLR.
target_link_options(TLOUUwp PRIVATE /HIGHENTROPYVA:NO)

set_property(SOURCE ${ASSETS} PROPERTY VS_DEPLOYMENT_CONTENT 1)
set_property(SOURCE ${ASSETS} PROPERTY VS_DEPLOYMENT_LOCATION "Assets")
if (RUNTIME_FILES)
    set_property(SOURCE ${RUNTIME_FILES} PROPERTY VS_DEPLOYMENT_CONTENT 1)
    set_property(SOURCE ${RUNTIME_FILES} PROPERTY VS_TOOL_OVERRIDE "None")
endif()
# The signing certificate (its subject must be the manifest's Publisher, CN=TLOU-UWP).
set(TLOU_CERT "" CACHE FILEPATH "Signing certificate (.pfx)")
if (TLOU_CERT)
    target_sources(TLOUUwp PRIVATE ${TLOU_CERT})
endif()
set_target_properties(TLOUUwp PROPERTIES
    VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "10.0.19041.0"
    VS_GLOBAL_AppxPackageSigningEnabled "true")
