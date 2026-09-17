# SceneRuntime core library (Phantom::SceneRuntime -- Vulkan/ImGui-independent scene node
# hierarchy) and its GoogleTest suite.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/SceneRuntime -B build/SceneRuntime -DCMAKE_BUILD_TYPE=Debug && cmake --build build/SceneRuntime
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake / cmake/PhantomVulkanApp.cmake;
# this file adds the module's test target on top. See docs/standalone-repository-plan.md.

cmake_minimum_required(VERSION 3.20)

project(SceneRuntimeStandalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(SCENERUNTIME_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
include(PhantomGTest)
include(PhantomCoreLibs)

phantom_add_scene_runtime_core()

# --- SceneRuntimeTest (GoogleTest) ---

phantom_find_gtest()

if(PHANTOM_GTEST_FOUND)
    enable_testing()

    add_executable(SceneRuntimeTest
        ${SCENERUNTIME_ROOT}/SceneRuntimeTest/TransformTest.cpp
        ${SCENERUNTIME_ROOT}/SceneRuntimeTest/SceneGraphTest.cpp
    )
    target_link_libraries(SceneRuntimeTest PRIVATE
        SceneRuntimeCore
        GTest::gtest
        GTest::gtest_main
    )
    target_compile_options(SceneRuntimeTest PRIVATE ${PHANTOM_WARN_FLAGS})

    include(GoogleTest)
    gtest_discover_tests(SceneRuntimeTest)
else()
    message(WARNING "GTest not found -- skipping SceneRuntimeTest target")
endif()
