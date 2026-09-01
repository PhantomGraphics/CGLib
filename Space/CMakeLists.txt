# Linux (clang) + Windows (VS2026) build for Phantom::Space (Space core library),
# SpaceTest (GoogleTest), and SpaceView (standalone ImGui + Vulkan viewer).
#
# This is a standalone CMake build, separate from the MSBuild/.vcxproj-based
# Windows build (Space.vcxproj/SpaceTest.vcxproj/SpaceView.vcxproj, driven by
# Phantom2026.sln, which remains the build the rest of the repo's
# ProjectReference graph uses -- untouched by this file).
#
# SpaceCore/VkAppBaseCore/VkRendererCore/etc. are now defined once in
# ../../cmake/PhantomCoreLibs.cmake and ../../cmake/PhantomVulkanApp.cmake
# (Phase 4 §5.4 item 1 dedup, docs/todo/PLAN_crossplatform_non_cgapp_build.md)
# -- this file just calls them and adds SpaceTest/SpaceView on top, so it
# also works standalone (`cmake -S CGLib/Space -B ...`) exactly as before,
# and identically when add_subdirectory()'d from the root CMakeLists.txt.
#
# Usage (Linux, from WSL -- avoid /mnt/c/... for speed on a Linux-native checkout):
#   cmake -S CGLib/Space -B CGLib/Space/build_linux \
#       -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
#       -DCMAKE_BUILD_TYPE=Debug \
#       -DVULKAN_INCLUDE_DIR=/path/to/Vulkan-Headers/include \
#       -DVULKAN_LIBRARY=/path/to/libvulkan.so \
#       -DGLFW_LIBRARY=/path/to/libglfw.so
#   cmake --build CGLib/Space/build_linux -j
#   ctest --test-dir CGLib/Space/build_linux --output-on-failure
#
# Space/SpaceTest build regardless of Vulkan availability (Space itself has no
# Vulkan dependency -- only CGLib/Math). SpaceView additionally needs Vulkan
# headers + a Vulkan loader + a GLFW loader to link; it is skipped with a
# warning (not a hard error) if any of those aren't found.

cmake_minimum_required(VERSION 3.20)

project(SpaceStandalone LANGUAGES CXX)

# C++20, not the repo-wide default C++17 (see root CLAUDE.md) -- Graphics.vcxproj/
# VulkanGraphics.vcxproj/UIWidgets.vcxproj/SpaceView.vcxproj already override
# <LanguageStandard> to stdcpp20 on Windows (Space/SpaceTest/Math stay at the
# BuildConfig.props default), and VkAppBase.cpp uses std::string_view::starts_with
# (C++20). One standard for the whole standalone build is simplest and matches
# the superset actually needed.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    set(PHANTOM_WARN_FLAGS /W3)
    # Matches BuildConfig.props' `/utf-8 %(AdditionalOptions)` applied repo-wide --
    # several sources have Japanese comments not valid in the legacy MSVC source
    # code page, so this is required, not cosmetic.
    add_compile_options(/utf-8)
else()
    set(PHANTOM_WARN_FLAGS -Wall)
endif()

set(SPACE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(CGLIB_ROOT ${REPO_ROOT}/CGLib)

list(APPEND CMAKE_MODULE_PATH ${REPO_ROOT}/cmake)
include(PhantomGTest)
include(PhantomCoreLibs)
include(PhantomVulkanApp)

phantom_add_space_core()

# --- SpaceTest (GoogleTest) ---

phantom_find_gtest()

if(PHANTOM_GTEST_FOUND)
    enable_testing()

    file(GLOB SPACETEST_SOURCES ${SPACE_ROOT}/SpaceTest/*.cpp)
    list(FILTER SPACETEST_SOURCES EXCLUDE REGEX "pch\\.cpp$")

    add_executable(SpaceTest ${SPACETEST_SOURCES})
    target_include_directories(SpaceTest PRIVATE ${REPO_ROOT} ${SPACE_ROOT}/SpaceTest)
    target_link_libraries(SpaceTest PRIVATE SpaceCore GTest::gtest GTest::gtest_main)
    target_compile_options(SpaceTest PRIVATE ${PHANTOM_WARN_FLAGS})

    include(GoogleTest)
    gtest_discover_tests(SpaceTest)
endif()

# ---------------------------------------------------------------------------
# SpaceView (VkSpaceApp : VkAppBase) -- standalone ImGui + Vulkan viewer
# ---------------------------------------------------------------------------

phantom_find_vulkan_headers()

if(PHANTOM_VULKAN_INCLUDE_DIR)

phantom_find_vulkan_loader()
phantom_find_glfw_loader()

if(PHANTOM_VULKAN_LIBRARY AND PHANTOM_GLFW_LIBRARY)

phantom_add_vkappbase_core()
phantom_add_vkrenderer_core()

add_executable(SpaceView
    ${SPACE_ROOT}/SpaceView/CompactSpaceHashPanel.cpp
    ${SPACE_ROOT}/SpaceView/KDTreePanel.cpp
    ${SPACE_ROOT}/SpaceView/main.cpp
    ${SPACE_ROOT}/SpaceView/OctreePanel.cpp
    ${SPACE_ROOT}/SpaceView/SignedDistancePanel.cpp
    ${SPACE_ROOT}/SpaceView/SpaceHashPanel.cpp
    ${SPACE_ROOT}/SpaceView/SpaceMenuPanel.cpp
    ${SPACE_ROOT}/SpaceView/VkSpaceApp.cpp
    ${SPACE_ROOT}/SpaceView/CommandDispatcher.cpp
    ${SPACE_ROOT}/SpaceView/Renderer.cpp
    ${SPACE_ROOT}/SpaceView/World.cpp
)
target_include_directories(SpaceView PRIVATE
    ${REPO_ROOT}
    ${CGLIB_ROOT}/UIWidgets
    ${CGLIB_ROOT}/Renderer/VkRenderer
)
target_link_libraries(SpaceView PRIVATE
    SpaceCore
    VkAppBaseCore
    VkRendererCore
    ${PHANTOM_VULKAN_LIBRARY}
    ${PHANTOM_GLFW_LIBRARY}
)
target_compile_options(SpaceView PRIVATE ${PHANTOM_WARN_FLAGS})

# loadSPVRepo() (VulkanSPVResolver.h) resolves "shaders/*.spv" relative to the
# running binary's directory on Windows, and to the current working directory
# on other platforms (no portable "own module path" API used there) -- mirror
# Windows' PostBuildEvent xcopy so the binary finds its shaders either way.
add_custom_command(TARGET SpaceView POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SPACE_ROOT}/SpaceView/shaders
        $<TARGET_FILE_DIR:SpaceView>/shaders
)

else()
    message(WARNING "Space: Vulkan and/or GLFW loader not found -- SpaceView is skipped (Space/SpaceTest still build). Pass -DVULKAN_LIBRARY=... -DGLFW_LIBRARY=... to enable it.")
endif()

else()
    message(WARNING "Space: vulkan/vulkan.h not found -- SpaceView is skipped. Pass -DVULKAN_INCLUDE_DIR=/path/to/include to enable it (on Windows, check $ENV{VULKAN_SDK} is set).")
endif()
