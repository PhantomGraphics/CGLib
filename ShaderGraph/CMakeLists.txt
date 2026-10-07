# ShaderGraph core library (Phantom::ShaderGraph -- CPU-only shader graph) and its GoogleTest suite.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/ShaderGraph -B build/ShaderGraph -DCMAKE_BUILD_TYPE=Debug && cmake --build build/ShaderGraph
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake / cmake/PhantomVulkanApp.cmake;
# this file adds the module's test target on top. See docs/standalone-repository-plan.md
# and docs/todo/PLAN_phantomstudio_shader_graph.md.

cmake_minimum_required(VERSION 3.20)

project(ShaderGraphStandalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(SHADERGRAPH_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
include(PhantomGTest)
include(PhantomCoreLibs)

phantom_add_shadergraph_core()

# --- ShaderGraphTest (GoogleTest) ---

phantom_find_gtest()

if(PHANTOM_GTEST_FOUND)
    enable_testing()

    cglib_add_test(ShaderGraphTest
        SOURCES
            ${SHADERGRAPH_ROOT}/ShaderGraphTest/ShaderGraphTest.cpp
        
        INCLUDES ${SHADERGRAPH_ROOT}/ShaderGraphTest
        LINK ShaderGraphCore
    )
else()
    message(STATUS "GTest unavailable or tests disabled -- skipping ShaderGraphTest target")
endif()
