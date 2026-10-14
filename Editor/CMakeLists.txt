# Shared editing module. CPU types/geometry are always available;
# the Vulkan backend is optional and independent of WPF and ImGui.
cmake_minimum_required(VERSION 3.20)
project(EditorStandalone LANGUAGES CXX)

if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
endif()
include(${CGLIB_ROOT}/cmake/CGLibCommon.cmake)
include(${CGLIB_ROOT}/cmake/PhantomCoreLibs.cmake)

add_library(EditorCore STATIC
    Gizmo/GizmoGeometry.cpp
    Selection/SelectionGeometry.cpp
    Commands/Tokenizer.cpp
    Commands/Script.cpp
    Commands/Registry.cpp
    Commands/References.cpp
    Input/Input.cpp
    Input/Bindings.cpp
    Input/InputResolver.cpp)
target_compile_features(EditorCore PUBLIC cxx_std_20)
target_include_directories(EditorCore PUBLIC ${REPO_ROOT} ${CMAKE_CURRENT_SOURCE_DIR}/Gizmo)
phantom_add_math_core()
# Response exposes nlohmann::json; header-only, no binary JSON library required.
target_include_directories(EditorCore PUBLIC ${CGLIB_ROOT}/ThirdParty/nlohmann)
target_link_libraries(EditorCore PUBLIC MathCore)
target_compile_options(EditorCore PRIVATE ${PHANTOM_WARN_FLAGS})
add_library(CGLib::Editor ALIAS EditorCore)

include(${CGLIB_ROOT}/cmake/PhantomGTest.cmake)
phantom_find_gtest()
if(PHANTOM_GTEST_FOUND)
    enable_testing()
    cglib_add_test(EditorTest
        SOURCES EditorTest/InputTest.cpp EditorTest/InputResolverTest.cpp
            EditorTest/CommandTest.cpp EditorTest/SelectionSetTest.cpp
            EditorTest/CommandConsoleModelTest.cpp
            EditorTest/MultiSelectionGizmoTest.cpp EditorTest/SelectionGeometryTest.cpp
            EditorTest/UndoStackTest.cpp
        LINK EditorCore)
endif()

if(NOT DEFINED CGLIB_ENABLE_VULKAN)
    find_package(Vulkan QUIET)
    option(CGLIB_ENABLE_VULKAN "Build the Editor Vulkan backend" ${Vulkan_FOUND})
endif()
if(CGLIB_ENABLE_VULKAN)
    include(${CGLIB_ROOT}/cmake/PhantomVulkanApp.cmake)
    phantom_find_vulkan_headers()
    if(NOT PHANTOM_VULKAN_INCLUDE_DIR)
        message(FATAL_ERROR "Editor Vulkan backend requires Vulkan headers")
    endif()
    add_library(EditorVulkanCore STATIC Gizmo/VkTransformGizmo.cpp)
    target_compile_features(EditorVulkanCore PUBLIC cxx_std_20)
    target_include_directories(EditorVulkanCore PUBLIC
        ${PHANTOM_VULKAN_INCLUDE_DIR}
        ${CGLIB_ROOT}/VkAppBase
        ${CGLIB_ROOT}/VulkanGraphics
        ${CGLIB_ROOT}/ThirdParty/glm
        ${CGLIB_ROOT}/ThirdParty)
    phantom_add_vulkangraphics_core()
    target_link_libraries(EditorVulkanCore PUBLIC EditorCore VulkanGraphicsCore)
    target_compile_options(EditorVulkanCore PRIVATE ${PHANTOM_WARN_FLAGS})
    add_library(CGLib::EditorVulkan ALIAS EditorVulkanCore)
endif()
