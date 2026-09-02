# Linux (clang) + Windows (VS2026) build for GizmoCore (Phantom::Gizmo -- 3-axis
# translate/rotate/scale transform gizmo geometry + Vulkan sub-renderer).
#
# This is a standalone CMake build, separate from the MSBuild/.vcxproj-based
# Windows build (VkGizmo.vcxproj, listed as project "Gizmo" in Phantom2026.sln
# and producing VkGizmo.lib -- note the stray, solution-unreferenced duplicate
# Gizmo.vcxproj that also lives in this directory; it is not built by
# Phantom2026.sln and is left untouched here). Part of
# internal design notes Phase 3: Gizmo is only
# consumed by CGApp (Universe/CGStudio), which is out of this plan's scope, so
# unlike Phase 2's apps there is no runtime/--screenshot verification
# available here -- successful compilation is the completion criterion.
#
# Usage (Linux, from WSL -- avoid /mnt/c/... for speed on a Linux-native checkout):
#   cmake -S CGLib/Gizmo -B CGLib/Gizmo/build_linux \
#       -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
#       -DCMAKE_BUILD_TYPE=Debug \
#       -DVULKAN_INCLUDE_DIR=/path/to/Vulkan-Headers/include
#   cmake --build CGLib/Gizmo/build_linux -j
#
# GizmoCore only needs Vulkan/VkAppBase *headers* (IVkSubRenderer.h,
# VulkanBuffer.h/VulkanDescriptorPool.h/VulkanPipeline.h are declarations
# implemented elsewhere) -- it links against nothing here, so unlike the
# Phase 2 app CMakeLists there is no need to build VulkanGraphicsCore/
# VkAppBaseCore or discover a Vulkan/GLFW loader, just the SDK's headers.

cmake_minimum_required(VERSION 3.20)

project(GizmoStandalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    set(GIZMO_WARN_FLAGS /W3)
    add_compile_options(/utf-8)
else()
    set(GIZMO_WARN_FLAGS -Wall)
endif()

set(GIZMO_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(CGLIB_ROOT ${REPO_ROOT}/CGLib)

# ---------------------------------------------------------------------------
# Vulkan headers discovery (compile-only -- no loader/link step needed)
# ---------------------------------------------------------------------------

set(GIZMO_DEFAULT_VULKAN_INCLUDE_DIR "")
if(WIN32 AND DEFINED ENV{VULKAN_SDK})
    set(GIZMO_DEFAULT_VULKAN_INCLUDE_DIR "$ENV{VULKAN_SDK}/Include")
endif()
set(VULKAN_INCLUDE_DIR "${GIZMO_DEFAULT_VULKAN_INCLUDE_DIR}" CACHE PATH "Directory containing vulkan/vulkan.h (defaults to $ENV{VULKAN_SDK}/Include on Windows; on Linux, e.g. a Vulkan-Headers checkout's include/, or a system libvulkan-dev's /usr/include). Must NOT be an app-bundled copy (e.g. Blender's).")

if(VULKAN_INCLUDE_DIR)
    if(EXISTS "${VULKAN_INCLUDE_DIR}/vulkan/vulkan.h")
        set(GIZMO_VULKAN_INCLUDE_DIR ${VULKAN_INCLUDE_DIR})
    endif()
else()
    find_path(GIZMO_VULKAN_INCLUDE_DIR NAMES vulkan/vulkan.h)
endif()

if(GIZMO_VULKAN_INCLUDE_DIR)
    message(STATUS "GizmoCore: using Vulkan headers from ${GIZMO_VULKAN_INCLUDE_DIR}")
else()
    message(WARNING "GizmoCore: vulkan/vulkan.h not found -- GizmoCore is skipped. Pass -DVULKAN_INCLUDE_DIR=/path/to/include to enable it (on Windows, check $ENV{VULKAN_SDK} is set).")
endif()

if(GIZMO_VULKAN_INCLUDE_DIR)

# ---------------------------------------------------------------------------
# Gizmo (Phantom::Gizmo) -- vcxproj target name "Gizmo", RootNamespace/output VkGizmo
# ---------------------------------------------------------------------------

add_library(GizmoCore STATIC
    ${GIZMO_ROOT}/GizmoGeometry.cpp
    ${GIZMO_ROOT}/VkTransformGizmo.cpp
)
target_include_directories(GizmoCore PRIVATE
    ${GIZMO_VULKAN_INCLUDE_DIR}
    ${CGLIB_ROOT}/ThirdParty/glm-0.9.9.8
    ${CGLIB_ROOT}/VkAppBase
    ${CGLIB_ROOT}/VulkanGraphics
    ${CGLIB_ROOT}/ThirdParty/imgui
    ${CGLIB_ROOT}/ThirdParty
)
target_compile_options(GizmoCore PRIVATE ${GIZMO_WARN_FLAGS})

else()
    message(WARNING "GizmoCore: Vulkan headers not found -- GizmoCore is skipped. Pass -DVULKAN_INCLUDE_DIR=... to enable it.")
endif()
