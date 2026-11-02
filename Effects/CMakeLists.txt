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
    WaterSurface/WaterSurfaceSimulator.cpp
    WaterSurface/WaterSurfaceRenderer.cpp
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

include(PhantomGTest)
phantom_find_gtest()
cglib_add_test(WaterSurfaceTest SOURCES WaterSurface/WaterSurfaceTest.cpp)
if(TARGET WaterSurfaceTest)
    target_compile_features(WaterSurfaceTest PRIVATE cxx_std_20)
endif()
phantom_find_vulkan_loader()
phantom_find_glfw_loader()
if(PHANTOM_VULKAN_LIBRARY AND PHANTOM_GLFW_LIBRARY)
    cglib_add_test(WaterSurfaceGpuTest SOURCES WaterSurface/WaterSurfaceGpuTest.cpp
        INCLUDES "${CGLIB_ROOT}/ThirdParty/glfw-3.3.8/include"
        LINK EffectsCore ${PHANTOM_VULKAN_LIBRARY} ${PHANTOM_GLFW_LIBRARY})
    if(TARGET WaterSurfaceGpuTest)
        target_compile_definitions(WaterSurfaceGpuTest PRIVATE
            WATER_SHADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/WaterSurface/shaders/water_update.comp.spv")
    endif()
endif()
if((NOT DEFINED CGLIB_BUILD_VIEWERS OR CGLIB_BUILD_VIEWERS) AND PHANTOM_VULKAN_LIBRARY AND PHANTOM_GLFW_LIBRARY)
    phantom_add_vkappbase_core()
    add_executable(WaterSurfaceView WaterSurface/WaterSurfaceView.cpp
        "${CGLIB_ROOT}/VkAppBase/ScenarioRunner/ScenarioRunner.cpp")
    target_link_libraries(WaterSurfaceView PRIVATE EffectsCore VkAppBaseCore ${PHANTOM_VULKAN_LIBRARY} ${PHANTOM_GLFW_LIBRARY})
    target_include_directories(WaterSurfaceView PRIVATE "${CGLIB_ROOT}/ThirdParty")
    add_custom_command(TARGET WaterSurfaceView POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/WaterSurface/shaders" "$<TARGET_FILE_DIR:WaterSurfaceView>/shaders/water")
endif()
