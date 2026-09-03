# VkRenderer core library (Phantom::VKR -- shared line/point/triangle/tex/skybox Vulkan renderers) and the VkRendererView app.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/Renderer -B build/Renderer -DCMAKE_BUILD_TYPE=Debug && cmake --build build/Renderer
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake / cmake/PhantomVulkanApp.cmake;
# this file adds the module's test / app targets on top. See docs/standalone-repository-plan.md.

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

set(RENDERER_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
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
