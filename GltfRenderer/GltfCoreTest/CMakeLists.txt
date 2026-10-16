# GltfCore (Phantom::Gltf -- CPU-only glTF evaluation: accessors, node transforms, animation / morph evaluation, the character
# pose and its CPU bake) and its GoogleTest suite. No Vulkan dependency, so this builds and runs in CPU-only configurations;
# the Vulkan renderer (GltfRendererCore) lives in the parent directory and links this library.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/GltfRenderer/GltfCoreTest -B build/GltfCore -DCMAKE_BUILD_TYPE=Debug && cmake --build build/GltfCore
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.

cmake_minimum_required(VERSION 3.20)

project(GltfCoreStandalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(GLTFCORETEST_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
endif()
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
include(PhantomGTest)
include(PhantomCoreLibs)

phantom_add_gltf_core()

phantom_find_gtest()

if(PHANTOM_GTEST_FOUND)
    enable_testing()

    cglib_add_test(GltfCoreTest
        SOURCES
            ${GLTFCORETEST_ROOT}/CharacterPoseTest.cpp
            ${GLTFCORETEST_ROOT}/CharacterPoseBakeTest.cpp
        INCLUDES ${GLTFCORETEST_ROOT}
        LINK GltfCore
    )
else()
    message(STATUS "GTest unavailable or tests disabled -- skipping GltfCoreTest target")
endif()
