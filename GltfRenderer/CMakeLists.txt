# Linux (clang) + Windows (VS2026) build for Phantom::Gltf/Phantom::Vrm
# (GltfRenderer core library), GltfRendererTest (GoogleTest), and GltfViewer
# (standalone ImGui + Vulkan glTF/VRM viewer).
#
# This is a standalone CMake build, separate from the MSBuild/.vcxproj-based
# Windows build (GltfRenderer/GltfRenderer.vcxproj, GltfRenderer/
# GltfRendererTest/GltfRendererTest.vcxproj, GltfViewer/GltfViewer.vcxproj,
# driven by Phantom2026.sln). GltfRendererCore/FileCore/AnimationCore/etc. are
# now defined once in ../../cmake/PhantomCoreLibs.cmake and
# ../../cmake/PhantomVulkanApp.cmake (Phase 4 §5.4 item 1 dedup,
# internal design notes) -- this file just calls
# them and adds GltfRendererTest/GltfViewer on top, so it also works
# standalone (`cmake -S CGLib/GltfRenderer -B ...`) exactly as before, and
# identically when add_subdirectory()'d from the root CMakeLists.txt.
#
# Usage (Linux, from WSL -- avoid /mnt/c/... for speed on a Linux-native checkout):
#   cmake -S CGLib/GltfRenderer -B CGLib/GltfRenderer/build_linux \
#       -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
#       -DCMAKE_BUILD_TYPE=Debug \
#       -DVULKAN_INCLUDE_DIR=/path/to/Vulkan-Headers/include \
#       -DVULKAN_LIBRARY=/path/to/libvulkan.so \
#       -DGLFW_LIBRARY=/path/to/libglfw.so
#   cmake --build CGLib/GltfRenderer/build_linux -j
#   ctest --test-dir CGLib/GltfRenderer/build_linux --output-on-failure
#
# NOTE on Vulkan being required even for GltfRendererTest (unlike File/
# Animation/Physics/PointCloud, whose *Test targets stay outside the Vulkan
# gate): GltfRendererCore's own source list includes Renderer/
# GltfSceneRenderer.cpp / ShadowMapPass.cpp / LightManager.cpp, which use
# CGLib::VKG (VulkanGraphics) types directly -- confirmed by
# GltfRendererTest.vcxproj itself linking vulkan-1.lib even though none of its
# *Test.cpp files touch rendering. So GltfRendererCore has no
# Vulkan-independent subset to split out; the whole target (and therefore its
# test binary) lives inside this file's Vulkan/GLFW discovery gate.

cmake_minimum_required(VERSION 3.20)

project(GltfRendererStandalone LANGUAGES CXX)

# Matches GltfRenderer.vcxproj/GltfViewer.vcxproj (<LanguageStandard>stdcpp20
# -- VkAppBase.cpp needs std::string_view::starts_with).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    set(PHANTOM_WARN_FLAGS /W3)
    add_compile_options(/utf-8)
else()
    set(PHANTOM_WARN_FLAGS -Wall)
endif()

set(GLTFR_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(CGLIB_ROOT ${REPO_ROOT}/CGLib)
set(GLTFVIEWER_ROOT ${CGLIB_ROOT}/GltfViewer)

list(APPEND CMAKE_MODULE_PATH ${REPO_ROOT}/cmake)
include(PhantomGTest)
include(PhantomCoreLibs)
include(PhantomVulkanApp)

phantom_find_gtest()

phantom_find_vulkan_headers()

if(PHANTOM_VULKAN_INCLUDE_DIR)

phantom_find_vulkan_loader()
phantom_find_glfw_loader()

if(PHANTOM_VULKAN_LIBRARY AND PHANTOM_GLFW_LIBRARY)

phantom_add_vkappbase_core()
phantom_add_gltfrenderer_core()

# --- GltfRendererTest (GoogleTest) ---

if(PHANTOM_GTEST_FOUND)
    enable_testing()

    add_executable(GltfRendererTest
        ${GLTFR_ROOT}/GltfRendererTest/ObjToGltfConverterTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/StlToGltfConverterTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/MmdAnimationBakerTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/GltfAnimationEvaluatorTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/SkeletonGltfConverterTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/MmdToGltfConverterTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/GLTFFileReaderMorphTargetTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/GLTFFileReaderNodeMatrixTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/VrmExtensionParserTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/VrmReaderTest.cpp
    )
    target_include_directories(GltfRendererTest PRIVATE
        ${GLTFR_ROOT}/GltfRendererTest
    )
    target_link_libraries(GltfRendererTest PRIVATE
        GltfRendererCore
        GTest::gtest
        GTest::gtest_main
        ${PHANTOM_VULKAN_LIBRARY}
    )
    target_compile_options(GltfRendererTest PRIVATE ${PHANTOM_WARN_FLAGS})

    include(GoogleTest)
    gtest_discover_tests(GltfRendererTest)
else()
    message(WARNING "GTest not found -- skipping GltfRendererTest target")
endif()

# ---------------------------------------------------------------------------
# GltfViewer (Phantom::Gltf app) -- standalone ImGui + Vulkan glTF/VRM viewer
# ---------------------------------------------------------------------------

add_executable(GltfViewer
    ${GLTFVIEWER_ROOT}/main.cpp
    ${GLTFVIEWER_ROOT}/CommandDispatcher.cpp
    ${GLTFVIEWER_ROOT}/App.cpp
    ${GLTFVIEWER_ROOT}/ControlPanel.cpp
)
target_include_directories(GltfViewer PRIVATE
    ${CGLIB_ROOT}/UIWidgets
)
target_link_libraries(GltfViewer PRIVATE
    GltfRendererCore
    VkAppBaseCore
    ${PHANTOM_VULKAN_LIBRARY}
    ${PHANTOM_GLFW_LIBRARY}
)
target_compile_options(GltfViewer PRIVATE ${PHANTOM_WARN_FLAGS})

# loadSPVRepo() resolves "shaders/*.spv" relative to the binary's own
# directory on Windows, and to CWD on Linux -- mirror Windows' PostBuildEvent
# xcopy so the binary finds its shaders either way.
add_custom_command(TARGET GltfViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${GLTFVIEWER_ROOT}/shaders
        $<TARGET_FILE_DIR:GltfViewer>/shaders
)

else()
    message(WARNING "GltfRenderer: Vulkan and/or GLFW loader not found -- GltfRendererCore/GltfRendererTest/GltfViewer are all skipped.")
endif()

else()
    message(WARNING "GltfRenderer: vulkan/vulkan.h not found -- GltfRendererCore/GltfRendererTest/GltfViewer are all skipped (they all need Vulkan, see file header). Pass -DVULKAN_INCLUDE_DIR=/path/to/include to enable them.")
endif()
