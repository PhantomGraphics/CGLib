# Terrain core library (Phantom::Terrain -- CPU-only, deterministic height-field
# terrain mesh generation from a fBm noise recipe) and its GoogleTest suite.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/Terrain -B build/Terrain -DCMAKE_BUILD_TYPE=Debug && cmake --build build/Terrain
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake / cmake/PhantomVulkanApp.cmake;
# this file adds the module's test target on top. See docs/standalone-repository-plan.md
# and docs/todo/PLAN_cgstudio_terrain_generation.md.

cmake_minimum_required(VERSION 3.20)

project(TerrainStandalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(TERRAIN_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
include(PhantomGTest)
include(PhantomCoreLibs)

phantom_add_terrain_core()

# --- TerrainTest (GoogleTest) ---

phantom_find_gtest()

if(PHANTOM_GTEST_FOUND)
    enable_testing()

    add_executable(TerrainTest
        ${TERRAIN_ROOT}/TerrainTest/TerrainGeneratorTest.cpp
    )
    target_include_directories(TerrainTest PRIVATE
        ${REPO_ROOT}
        ${TERRAIN_ROOT}/TerrainTest
    )
    target_link_libraries(TerrainTest PRIVATE
        TerrainCore
        GTest::gtest
        GTest::gtest_main
    )
    target_compile_options(TerrainTest PRIVATE ${PHANTOM_WARN_FLAGS})

    include(GoogleTest)
    gtest_discover_tests(TerrainTest)
else()
    message(WARNING "GTest not found -- skipping TerrainTest target")
endif()
