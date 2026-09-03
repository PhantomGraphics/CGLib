# Space core library (Phantom::Space -- Octree / KDTree / BVH / SpaceHash / Z-order spatial queries), its GoogleTest suite, and the SpaceView Vulkan app.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/Space -B build/Space -DCMAKE_BUILD_TYPE=Debug && cmake --build build/Space
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake / cmake/PhantomVulkanApp.cmake;
# this file adds the module's test / app targets on top. See docs/standalone-repository-plan.md.

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

set(SPACE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
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

elseif(NOT DEFINED CGLIB_ENABLE_VULKAN OR CGLIB_ENABLE_VULKAN)
    message(WARNING "Space: vulkan/vulkan.h not found -- SpaceView is skipped. Pass -DVULKAN_INCLUDE_DIR=/path/to/include to enable it (on Windows, check $ENV{VULKAN_SDK} is set).")
endif()
