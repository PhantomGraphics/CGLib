# Linux (clang) + Windows (VS2026) build for ParticlesCore (Phantom::Particles
# -- GPU particle emitter/pool/simulator + Vulkan billboard renderer).
#
# This is a standalone CMake build, separate from the MSBuild/.vcxproj-based
# Windows build (Particles.vcxproj, driven by Phantom2026.sln, untouched by
# this file). Part of docs/todo/PLAN_crossplatform_non_cgapp_build.md Phase 3:
# Particles is only consumed by CGApp (Universe/CGStudio), which is out of
# this plan's scope, so unlike Phase 2's apps there is no runtime
# verification available here -- successful compilation is the completion
# criterion.
#
# Usage (Linux, from WSL -- avoid /mnt/c/... for speed on a Linux-native checkout):
#   cmake -S CGLib/Particles -B CGLib/Particles/build_linux \
#       -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
#       -DCMAKE_BUILD_TYPE=Debug \
#       -DVULKAN_INCLUDE_DIR=/path/to/Vulkan-Headers/include
#   cmake --build CGLib/Particles/build_linux -j
#
# ParticlesCore only needs Vulkan *headers* (VulkanContext.h/VulkanBuffer.h/
# VulkanCommandPool.h/VulkanDescriptorPool.h/VulkanPipeline.h/
# VulkanComputePipeline.h are declarations implemented elsewhere, and are
# pulled in via the sources' own "../../CGLib/VulkanGraphics/..." relative
# includes, so no CGLib include dir is needed for those) -- it links against
# nothing here, so unlike the Phase 2 app CMakeLists there is no loader
# discovery/link step.

cmake_minimum_required(VERSION 3.20)

project(ParticlesStandalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    set(PARTICLES_WARN_FLAGS /W3)
    add_compile_options(/utf-8)
else()
    set(PARTICLES_WARN_FLAGS -Wall)
endif()

set(PARTICLES_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(CGLIB_ROOT ${REPO_ROOT}/CGLib)

# ---------------------------------------------------------------------------
# Vulkan headers discovery (compile-only -- no loader/link step needed)
# ---------------------------------------------------------------------------

set(PARTICLES_DEFAULT_VULKAN_INCLUDE_DIR "")
if(WIN32 AND DEFINED ENV{VULKAN_SDK})
    set(PARTICLES_DEFAULT_VULKAN_INCLUDE_DIR "$ENV{VULKAN_SDK}/Include")
endif()
set(VULKAN_INCLUDE_DIR "${PARTICLES_DEFAULT_VULKAN_INCLUDE_DIR}" CACHE PATH "Directory containing vulkan/vulkan.h (defaults to $ENV{VULKAN_SDK}/Include on Windows; on Linux, e.g. a Vulkan-Headers checkout's include/, or a system libvulkan-dev's /usr/include). Must NOT be an app-bundled copy (e.g. Blender's).")

if(VULKAN_INCLUDE_DIR)
    if(EXISTS "${VULKAN_INCLUDE_DIR}/vulkan/vulkan.h")
        set(PARTICLES_VULKAN_INCLUDE_DIR ${VULKAN_INCLUDE_DIR})
    endif()
else()
    find_path(PARTICLES_VULKAN_INCLUDE_DIR NAMES vulkan/vulkan.h)
endif()

if(PARTICLES_VULKAN_INCLUDE_DIR)
    message(STATUS "ParticlesCore: using Vulkan headers from ${PARTICLES_VULKAN_INCLUDE_DIR}")
else()
    message(WARNING "ParticlesCore: vulkan/vulkan.h not found -- ParticlesCore is skipped. Pass -DVULKAN_INCLUDE_DIR=/path/to/include to enable it (on Windows, check $ENV{VULKAN_SDK} is set).")
endif()

if(PARTICLES_VULKAN_INCLUDE_DIR)

# ---------------------------------------------------------------------------
# Particles (Phantom::Particles)
# ---------------------------------------------------------------------------

add_library(ParticlesCore STATIC
    ${PARTICLES_ROOT}/ParticleEmitter.cpp
    ${PARTICLES_ROOT}/ParticlePool.cpp
    ${PARTICLES_ROOT}/ParticleSimulator.cpp
    ${PARTICLES_ROOT}/ParticleBillboardRenderer.cpp
)
target_include_directories(ParticlesCore PRIVATE
    ${PARTICLES_VULKAN_INCLUDE_DIR}
    ${CGLIB_ROOT}/ThirdParty/glm-0.9.9.8
    ${CGLIB_ROOT}/VulkanGraphics
)
target_compile_options(ParticlesCore PRIVATE ${PARTICLES_WARN_FLAGS})

else()
    message(WARNING "ParticlesCore: Vulkan headers not found -- ParticlesCore is skipped. Pass -DVULKAN_INCLUDE_DIR=... to enable it.")
endif()
