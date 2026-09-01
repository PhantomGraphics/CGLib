# Cross-platform CMake build for the Numerics core library (Phantom::Numerics)
# and its GoogleTest suite (NumericsTest).
#
# This is a standalone CMake build, separate from the MSBuild/.vcxproj-based
# Windows build (Numerics/Numerics/Numerics.vcxproj + Numerics/NumericsTest/
# NumericsTest.vcxproj, driven by Phantom2026.sln). Part of
# docs/todo/PLAN_crossplatform_non_cgapp_build.md Phase 1 -- follows the same
# pattern as ../../Physics/CMakeLists.txt / ../../PointCloud/CMakeLists.txt.
# NumericsCore itself is now defined once in ../../cmake/PhantomCoreLibs.cmake
# (Phase 4 §5.4 item 1 dedup) -- this file just calls it and adds NumericsTest
# on top, so it also works standalone (`cmake -S CGLib/Numerics -B ...`)
# exactly as before, and identically when add_subdirectory()'d from the root
# CMakeLists.txt.
#
# Usage (Linux, from WSL -- avoid /mnt/c/... for speed on a Linux-native checkout):
#   cmake -S CGLib/Numerics -B CGLib/Numerics/build_linux \
#       -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
#       -DCMAKE_BUILD_TYPE=Debug
#   cmake --build CGLib/Numerics/build_linux -j
#   ctest --test-dir CGLib/Numerics/build_linux --output-on-failure
#
# Prerequisite: libgtest-dev (provides libgtest.a/libgtest_main.a + the GTest
# CMake package config under /usr/lib/x86_64-linux-gnu/cmake/GTest).
#
# Dependencies: CGLib/Math (header-only usage -- Numerics.vcxproj/NumericsTest.
# vcxproj have no ProjectReference to Math.vcxproj on Windows either, so
# NumericsCore itself doesn't link MathCore) and Eigen 3.4.0, vendored under
# ThirdParty/eigen-3.4.0 and referenced via a relative #include ("../ThirdParty/
# eigen-3.4.0/Eigen/Eigen") in Converter.h -- no extra include dir needed for it.

cmake_minimum_required(VERSION 3.20)

project(NumericsCoreLinux LANGUAGES CXX)

# Matches Numerics.vcxproj/NumericsTest.vcxproj: neither overrides
# <LanguageStandard>, so both build under BuildConfig.props' repo-wide C++17
# default (unlike File/RayTracer/PhysicsTest, which explicitly set stdcpp20).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(CGLIB_ROOT ${REPO_ROOT}/CGLib)
set(NUMERICS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    add_compile_options(/utf-8)
    set(PHANTOM_WARN_FLAGS /W3)
else()
    set(PHANTOM_WARN_FLAGS -Wall)
endif()

list(APPEND CMAKE_MODULE_PATH ${REPO_ROOT}/cmake)
include(PhantomGTest)
include(PhantomCoreLibs)

phantom_add_numerics_core()

# --- NumericsTest (GoogleTest) ---

phantom_find_gtest()

if(PHANTOM_GTEST_FOUND)
    enable_testing()

    add_executable(NumericsTest
        ${NUMERICS_ROOT}/NumericsTest/ConverterTest.cpp
        ${NUMERICS_ROOT}/NumericsTest/SVD2dTest.cpp
        ${NUMERICS_ROOT}/NumericsTest/SVD3dTest.cpp
        ${NUMERICS_ROOT}/NumericsTest/test.cpp
    )
    target_include_directories(NumericsTest PRIVATE
        ${REPO_ROOT}
        ${NUMERICS_ROOT}/NumericsTest
    )
    target_link_libraries(NumericsTest PRIVATE
        NumericsCore
        GTest::gtest
        GTest::gtest_main
    )
    target_compile_options(NumericsTest PRIVATE ${PHANTOM_WARN_FLAGS})

    include(GoogleTest)
    gtest_discover_tests(NumericsTest)
else()
    message(WARNING "GTest not found -- skipping NumericsTest target")
endif()
