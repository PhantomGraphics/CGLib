# PBR core library (Phantom::PBR -- CPU-only physically based shading parts shared by the renderers) and its GoogleTest suite.
# Phase 1: the light description (Light.h) and the viewport's lighting uniform block (LightBlock.h). Later phases add shadow
# matrices, conversions to the other renderers' light types and shared GLSL.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/PBR -B build/PBR -DCMAKE_BUILD_TYPE=Debug && cmake --build build/PBR
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake; this file adds the module's test target on top.

cmake_minimum_required(VERSION 3.20)

project(PBRStandalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(PBR_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
include(PhantomGTest)
include(PhantomCoreLibs)

phantom_add_pbr_core()

# --- PBRTest (GoogleTest) ---

phantom_find_gtest()

if(PHANTOM_GTEST_FOUND)
    enable_testing()

    cglib_add_test(PBRTest
        SOURCES
            ${PBR_ROOT}/PBRTest/LightBlockTest.cpp
            ${PBR_ROOT}/PBRTest/GltfLightTest.cpp
        INCLUDES ${PBR_ROOT}/PBRTest
        LINK PBRCore
    )
else()
    message(STATUS "GTest unavailable or tests disabled -- skipping PBRTest target")
endif()
