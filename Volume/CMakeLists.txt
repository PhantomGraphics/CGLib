# Volume core library (Phantom::Volume -- level sets, marching cubes, sparse volume trees), its GoogleTest suite, and the VolumeRenderer + VolumeView Vulkan app.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/Volume -B build/Volume -DCMAKE_BUILD_TYPE=Debug && cmake --build build/Volume
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake / cmake/PhantomVulkanApp.cmake;
# this file adds the module's test / app targets on top. See docs/standalone-repository-plan.md.

cmake_minimum_required(VERSION 3.20)

project(VolumeCoreLinux LANGUAGES CXX)

# Matches Volume.vcxproj/VolumeTest.vcxproj: neither overrides
# <LanguageStandard>, so both build under BuildConfig.props' repo-wide C++17
# default (VolumeRenderer/VolumeView below are promoted to C++20 per-target,
# see cmake/PhantomVulkanApp.cmake).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(VOLUME_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
include(PhantomGTest)
include(PhantomCoreLibs)
include(PhantomVulkanApp)

phantom_add_volume_core()

# --- VolumeTest (GoogleTest) ---

phantom_find_gtest()

if(PHANTOM_GTEST_FOUND)
    enable_testing()

    add_executable(VolumeTest
        ${VOLUME_ROOT}/VolumeTest/LevelSetTest.cpp
        ${VOLUME_ROOT}/VolumeTest/VdbIOTest.cpp
        ${VOLUME_ROOT}/VolumeTest/MCSurfaceBuilderTest.cpp
        ${VOLUME_ROOT}/VolumeTest/SparseVolumeTreeTest.cpp
        ${VOLUME_ROOT}/VolumeTest/SurfaceVoxelizerTest.cpp
        ${VOLUME_ROOT}/VolumeTest/VolumeTest.cpp
    )
    target_include_directories(VolumeTest PRIVATE
        ${REPO_ROOT}
        ${VOLUME_ROOT}/VolumeTest
    )
    target_link_libraries(VolumeTest PRIVATE
        VolumeCore
        GTest::gtest
        GTest::gtest_main
    )
    target_compile_options(VolumeTest PRIVATE ${PHANTOM_WARN_FLAGS})

    include(GoogleTest)
    gtest_discover_tests(VolumeTest)
else()
    message(WARNING "GTest not found -- skipping VolumeTest target")
endif()

# ---------------------------------------------------------------------------
# VolumeRenderer (Phantom::VKR PBVR pipeline) + VolumeView (VkVolumeView) --
# standalone ImGui + Vulkan volume viewer. Skipped with a warning (not a hard
# error) if Vulkan/GLFW aren't found, so Volume/VolumeTest above still build.
#
# VkVolumeView.vcxproj additionally references File.vcxproj, Numerics.vcxproj,
# Scene.vcxproj and Renderer/VkRenderer.vcxproj (confirmed by reading the
# .vcxproj's <ProjectReference> list directly, not assumed).
# ---------------------------------------------------------------------------

phantom_find_vulkan_headers()

if(PHANTOM_VULKAN_INCLUDE_DIR)

phantom_find_vulkan_loader()
phantom_find_glfw_loader()

if(PHANTOM_VULKAN_LIBRARY AND PHANTOM_GLFW_LIBRARY)

phantom_add_numerics_core()
phantom_add_scene_core()
phantom_add_file_core()
phantom_add_vkappbase_core()
phantom_add_vkrenderer_core()
phantom_add_volumerenderer_core()

add_executable(VolumeView
    ${VOLUME_ROOT}/VolumeView/main.cpp
    ${VOLUME_ROOT}/VolumeView/DVCreateBoxView.cpp
    ${VOLUME_ROOT}/VolumeView/DVFromSparseView.cpp
    ${VOLUME_ROOT}/VolumeView/DVMarchingCubesView.cpp
    ${VOLUME_ROOT}/VolumeView/CommandDispatcher.cpp
    ${VOLUME_ROOT}/VolumeView/MCMeshView.cpp
    ${VOLUME_ROOT}/VolumeView/SVBoxView.cpp
    ${VOLUME_ROOT}/VolumeView/SVCombineView.cpp
    ${VOLUME_ROOT}/VolumeView/SVMeshView.cpp
    ${VOLUME_ROOT}/VolumeView/SVParticleView.cpp
    ${VOLUME_ROOT}/VolumeView/SVResampleView.cpp
    ${VOLUME_ROOT}/VolumeView/SVSphereView.cpp
    ${VOLUME_ROOT}/VolumeView/DenseVolumeRenderer.cpp
    ${VOLUME_ROOT}/VolumeView/LinePipeline.cpp
    ${VOLUME_ROOT}/VolumeView/MeshOverlayRenderer.cpp
    ${VOLUME_ROOT}/VolumeView/SparseVolumeRenderer.cpp
    ${VOLUME_ROOT}/VolumeView/VectorFieldRenderer.cpp
    ${VOLUME_ROOT}/VolumeView/VolumePipeline.cpp
    ${VOLUME_ROOT}/VolumeView/VolumeViewApp.cpp
    ${VOLUME_ROOT}/VolumeView/MenuPanel.cpp
    ${VOLUME_ROOT}/VolumeView/SceneListPanel.cpp
    ${VOLUME_ROOT}/VolumeView/World.cpp
)
target_include_directories(VolumeView PRIVATE
    ${REPO_ROOT}
    ${CGLIB_ROOT}/UIWidgets
    ${CGLIB_ROOT}/Renderer/VkRenderer
    ${VOLUME_ROOT}/VolumeRenderer
)
target_link_libraries(VolumeView PRIVATE
    VolumeCore
    VolumeRenderer
    FileCore
    NumericsCore
    SceneCore
    VkAppBaseCore
    VkRendererCore
    ${PHANTOM_VULKAN_LIBRARY}
    ${PHANTOM_GLFW_LIBRARY}
)
target_compile_options(VolumeView PRIVATE ${PHANTOM_WARN_FLAGS})
target_compile_features(VolumeView PRIVATE cxx_std_20)

# loadSPVRepo() (VulkanSPVResolver.h) resolves "shaders/*.spv" relative to the
# running binary's directory on Windows, and to the current working directory
# on other platforms -- mirror Windows' PostBuildEvent xcopy so the binary
# finds its shaders either way.
add_custom_command(TARGET VolumeView POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${VOLUME_ROOT}/VolumeView/shaders
        $<TARGET_FILE_DIR:VolumeView>/shaders
)
add_custom_command(TARGET VolumeView POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${VOLUME_ROOT}/VolumeView/scenarios
        $<TARGET_FILE_DIR:VolumeView>/scenarios
)

else()
    message(WARNING "Volume: Vulkan and/or GLFW loader not found -- VolumeView is skipped (Volume/VolumeTest still build). Pass -DVULKAN_LIBRARY=... -DGLFW_LIBRARY=... to enable it.")
endif()

elseif(NOT DEFINED CGLIB_ENABLE_VULKAN OR CGLIB_ENABLE_VULKAN)
    message(WARNING "Volume: vulkan/vulkan.h not found -- VolumeView is skipped. Pass -DVULKAN_INCLUDE_DIR=/path/to/include to enable it (on Windows, check $ENV{VULKAN_SDK} is set).")
endif()
