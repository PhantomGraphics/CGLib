# Graphics core library (Phantom::Graphics -- camera, colors, images, image file I/O) and its GoogleTest suite.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/Graphics -B build/Graphics -DCMAKE_BUILD_TYPE=Debug && cmake --build build/Graphics
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake; this file adds the module's test target.

cmake_minimum_required(VERSION 3.20)

project(GraphicsCoreLinux LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GRAPHICS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
include(PhantomGTest)
include(PhantomCoreLibs)

phantom_add_graphics_core()

# --- GraphicsTest (GoogleTest) ---

phantom_find_gtest()

if(PHANTOM_GTEST_FOUND)
    enable_testing()

    file(GLOB GRAPHICSTEST_SOURCES CONFIGURE_DEPENDS ${GRAPHICS_ROOT}/GraphicsTest/*.cpp)
    list(FILTER GRAPHICSTEST_SOURCES EXCLUDE REGEX "pch\.cpp$")

    cglib_add_test(GraphicsTest
        SOURCES ${GRAPHICSTEST_SOURCES}
        INCLUDES ${GRAPHICS_ROOT}/GraphicsTest
        LINK GraphicsCore MathCore
    )
else()
    message(STATUS "GTest unavailable or tests disabled -- skipping GraphicsTest target")
endif()
