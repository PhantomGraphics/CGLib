# Vulkan visual effects: GPU particles and image-space post-processing.
# Standalone: cmake -S Effects -B build/Effects -DCMAKE_BUILD_TYPE=Debug
cmake_minimum_required(VERSION 3.20)
project(EffectsStandalone LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
if(NOT DEFINED CGLIB_ROOT)
    get_filename_component(CGLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
endif()
list(APPEND CMAKE_MODULE_PATH "${CGLIB_ROOT}/cmake")
include(CGLibCommon)
include(PhantomVulkanApp)
phantom_find_vulkan_headers()
if(NOT PHANTOM_VULKAN_INCLUDE_DIR)
    phantom_gpu_target_skipped("EffectsCore: Vulkan headers unavailable")
    return()
endif()
phantom_add_vulkangraphics_core()

add_library(EffectsCore STATIC
    Particles/ParticleEmitter.cpp
    Particles/ParticlePool.cpp
    Particles/ParticleSimulator.cpp
    Particles/ParticleBillboardRenderer.cpp
    PostProcess/PostProcessChain.cpp
    PostProcess/BloomEffect.cpp
    PostProcess/ToneMappingEffect.cpp
    PostProcess/SSAOEffect.cpp
    PostProcess/FXAAEffect.cpp
    PostProcess/Internal/FullscreenEffectPipeline.cpp
)
target_include_directories(EffectsCore PUBLIC "${REPO_ROOT}" "${CGLIB_ROOT}/ThirdParty/glm")
target_link_libraries(EffectsCore PUBLIC VulkanGraphicsCore)
target_compile_features(EffectsCore PUBLIC cxx_std_20)
target_compile_options(EffectsCore PRIVATE ${PHANTOM_WARN_FLAGS})
if(NOT TARGET CGLib::Effects)
    add_library(CGLib::Effects ALIAS EffectsCore)
endif()
