# PostProcess core library (Phantom::PostProcess -- bloom/tonemap/SSAO/FXAA fullscreen Vulkan effects). Compile-only: needs Vulkan headers.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/PostProcess -B build/PostProcess -DCMAKE_BUILD_TYPE=Debug && cmake --build build/PostProcess
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake / cmake/PhantomVulkanApp.cmake;
# this file adds the module's test / app targets on top. See docs/standalone-repository-plan.md.

cmake_minimum_required(VERSION 3.20)

project(PostProcessStandalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    set(POSTPROCESS_WARN_FLAGS /W3)
    add_compile_options(/utf-8)
else()
    set(POSTPROCESS_WARN_FLAGS -Wall)
endif()

set(POSTPROCESS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()

# ---------------------------------------------------------------------------
# Vulkan headers discovery (compile-only -- no loader/link step needed)
# ---------------------------------------------------------------------------

set(POSTPROCESS_DEFAULT_VULKAN_INCLUDE_DIR "")
if(WIN32 AND DEFINED ENV{VULKAN_SDK})
    set(POSTPROCESS_DEFAULT_VULKAN_INCLUDE_DIR "$ENV{VULKAN_SDK}/Include")
endif()
set(VULKAN_INCLUDE_DIR "${POSTPROCESS_DEFAULT_VULKAN_INCLUDE_DIR}" CACHE PATH "Directory containing vulkan/vulkan.h (defaults to $ENV{VULKAN_SDK}/Include on Windows; on Linux, e.g. a Vulkan-Headers checkout's include/, or a system libvulkan-dev's /usr/include). Must NOT be an app-bundled copy (e.g. Blender's).")

if(VULKAN_INCLUDE_DIR)
    if(EXISTS "${VULKAN_INCLUDE_DIR}/vulkan/vulkan.h")
        set(POSTPROCESS_VULKAN_INCLUDE_DIR ${VULKAN_INCLUDE_DIR})
    endif()
else()
    find_path(POSTPROCESS_VULKAN_INCLUDE_DIR NAMES vulkan/vulkan.h)
endif()

if(POSTPROCESS_VULKAN_INCLUDE_DIR)
    message(STATUS "PostProcessCore: using Vulkan headers from ${POSTPROCESS_VULKAN_INCLUDE_DIR}")
else()
    message(WARNING "PostProcessCore: vulkan/vulkan.h not found -- PostProcessCore is skipped. Pass -DVULKAN_INCLUDE_DIR=/path/to/include to enable it (on Windows, check $ENV{VULKAN_SDK} is set).")
endif()

if(POSTPROCESS_VULKAN_INCLUDE_DIR)

# ---------------------------------------------------------------------------
# PostProcess (Phantom::PostProcess)
# ---------------------------------------------------------------------------

add_library(PostProcessCore STATIC
    ${POSTPROCESS_ROOT}/PostProcessChain.cpp
    ${POSTPROCESS_ROOT}/BloomEffect.cpp
    ${POSTPROCESS_ROOT}/ToneMappingEffect.cpp
    ${POSTPROCESS_ROOT}/SSAOEffect.cpp
    ${POSTPROCESS_ROOT}/FXAAEffect.cpp
    ${POSTPROCESS_ROOT}/Internal/FullscreenEffectPipeline.cpp
)
target_include_directories(PostProcessCore PRIVATE
    ${POSTPROCESS_VULKAN_INCLUDE_DIR}
    ${CGLIB_ROOT}/ThirdParty/glm-0.9.9.8
    ${CGLIB_ROOT}/VulkanGraphics
)
target_compile_options(PostProcessCore PRIVATE ${POSTPROCESS_WARN_FLAGS})

else()
    message(WARNING "PostProcessCore: Vulkan headers not found -- PostProcessCore is skipped. Pass -DVULKAN_INCLUDE_DIR=... to enable it.")
endif()
