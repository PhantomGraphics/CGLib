# MeshEdit core library (Phantom::MeshEdit -- CPU-only polygon mesh editing) and its GoogleTest suite.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/MeshEdit -B build/MeshEdit -DCMAKE_BUILD_TYPE=Debug && cmake --build build/MeshEdit
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake; this file adds the module's test target on top.

cmake_minimum_required(VERSION 3.20)

project(MeshEditStandalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(MESHEDIT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
include(PhantomGTest)
include(PhantomCoreLibs)

phantom_add_meshedit_core()

# --- MeshEditTest (GoogleTest) ---

phantom_find_gtest()

if(PHANTOM_GTEST_FOUND)
    enable_testing()

    cglib_add_test(MeshEditTest
        SOURCES
            ${MESHEDIT_ROOT}/MeshEditTest/MeshEditPickingTest.cpp
            ${MESHEDIT_ROOT}/MeshEditTest/MeshEditStateTest.cpp
            ${MESHEDIT_ROOT}/MeshEditTest/EditableMeshTest.cpp
            ${MESHEDIT_ROOT}/MeshEditTest/MeshEditOperationsTest.cpp
        INCLUDES ${MESHEDIT_ROOT}/MeshEditTest
        LINK MeshEditCore
    )
else()
    message(STATUS "GTest unavailable or tests disabled -- skipping MeshEditTest target")
endif()
