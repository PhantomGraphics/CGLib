# Linux (clang) + Windows (VS2026) build for Phantom::VKR (VkRenderer, the
# shared line/point/triangle/tex/skybox Vulkan renderer library) and
# VkRendererView (standalone ImGui + Vulkan viewer that exercises it).
#
# This is a standalone CMake build, separate from the MSBuild/.vcxproj-based
# Windows build (VkRenderer/VkRenderer.vcxproj, VkRendererView/
# VkRendererView.vcxproj, driven by Phantom2026.sln, which remains the build
# the rest of the repo's ProjectReference graph uses -- untouched by this
# file). VkRendererCore/VkAppBaseCore/etc. are now defined once in
# ../../cmake/PhantomCoreLibs.cmake and ../../cmake/PhantomVulkanApp.cmake
# (Phase 4 §5.4 item 1 dedup, docs/todo/PLAN_crossplatform_non_cgapp_build.md)
# -- this file just calls them and adds VkRendererView on top, so it also
# works standalone (`cmake -S CGLib/Renderer -B ...`) exactly as before,
# and identically when add_subdirectory()'d from the root CMakeLists.txt.
#
# Usage (Linux, from WSL -- avoid /mnt/c/... for speed on a Linux-native checkout):
#   cmake -S CGLib/Renderer -B CGLib/Renderer/build_linux \
#       -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
#       -DCMAKE_BUILD_TYPE=Debug \
#       -DVULKAN_INCLUDE_DIR=/path/to/Vulkan-Headers/include \
#       -DVULKAN_LIBRARY=/path/to/libvulkan.so \
#       -DGLFW_LIBRARY=/path/to/libglfw.so
#   cmake --build CGLib/Renderer/build_linux -j
#
# VkRendererView has no dedicated GoogleTest project (VkRendererView.vcxproj
# has no corresponding *Test project in Phantom2026.sln), so unlike Space/
# PointCloud there is no ctest target here -- build success + the runtime
# --screenshot check are the only verification available.
#
# VkRendererView is skipped with a warning (not a hard error) if Vulkan/GLFW
# aren't found, same as SpaceView/PointCloudView.

cmake_minimum_required(VERSION 3.20)

project(VkRendererViewStandalone LANGUAGES CXX)

# C++20 -- VkRenderer.vcxproj/VkRendererView.vcxproj both override
# <LanguageStandard> to stdcpp20 (VkAppBase.cpp needs std::string_view::
# starts_with, a C++20 feature), matching CGLib/Space/CMakeLists.txt's finding.
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

set(RENDERER_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(CGLIB_ROOT ${REPO_ROOT}/CGLib)

list(APPEND CMAKE_MODULE_PATH ${REPO_ROOT}/cmake)
include(PhantomCoreLibs)
include(PhantomVulkanApp)

phantom_find_vulkan_headers()

if(PHANTOM_VULKAN_INCLUDE_DIR)

phantom_find_vulkan_loader()
phantom_find_glfw_loader()

if(PHANTOM_VULKAN_LIBRARY AND PHANTOM_GLFW_LIBRARY)

phantom_add_vkappbase_core()
phantom_add_vkrenderer_core()

# ---------------------------------------------------------------------------
# VkRendererView -- standalone ImGui + Vulkan viewer exercising VkRenderer
# ---------------------------------------------------------------------------

add_executable(VkRendererView
    ${RENDERER_ROOT}/VkRendererView/main.cpp
    ${RENDERER_ROOT}/VkRendererView/VkRendererApp.cpp
    ${RENDERER_ROOT}/VkRendererView/VkRendererSubRenderer.cpp
)
target_include_directories(VkRendererView PRIVATE
    ${REPO_ROOT}
    ${CGLIB_ROOT}/UIWidgets
    ${RENDERER_ROOT}/VkRenderer
)
target_link_libraries(VkRendererView PRIVATE
    VkRendererCore
    VkAppBaseCore
    ${PHANTOM_VULKAN_LIBRARY}
    ${PHANTOM_GLFW_LIBRARY}
)
target_compile_options(VkRendererView PRIVATE ${PHANTOM_WARN_FLAGS})

# loadSPVRepo() (VulkanSPVResolver.h) resolves "shaders/*.spv" relative to the
# running binary's directory on Windows, and to the current working directory
# on other platforms -- mirror VkRendererView.vcxproj's PostBuildEvent xcopy
# (which pulls from VkRenderer/Shaders/, not VkRendererView/shaders/ -- the
# .vcxproj's own shaders/*.vert|frag are unused leftovers, VkRenderer/Shaders/
# is the actual source of truth) so the binary finds its shaders either way.
add_custom_command(TARGET VkRendererView POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${RENDERER_ROOT}/VkRenderer/Shaders
        $<TARGET_FILE_DIR:VkRendererView>/shaders
)

else()
    message(WARNING "VkRendererView: Vulkan and/or GLFW loader not found -- VkRendererView is skipped. Pass -DVULKAN_LIBRARY=... -DGLFW_LIBRARY=... to enable it.")
endif()

else()
    message(WARNING "VkRendererView: vulkan/vulkan.h not found -- VkRendererView is skipped. Pass -DVULKAN_INCLUDE_DIR=/path/to/include to enable it (on Windows, check $ENV{VULKAN_SDK} is set).")
endif()
