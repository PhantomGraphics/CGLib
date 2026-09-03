# Numerics core library (Phantom::Numerics -- 2x2/3x3 SVD + eigen-decomposition, Eigen-backed) and its GoogleTest suite.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/Numerics -B build/Numerics -DCMAKE_BUILD_TYPE=Debug && cmake --build build/Numerics
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake / cmake/PhantomVulkanApp.cmake;
# this file adds the module's test / app targets on top. See docs/standalone-repository-plan.md.

cmake_minimum_required(VERSION 3.20)

project(NumericsCoreLinux LANGUAGES CXX)

# Matches Numerics.vcxproj/NumericsTest.vcxproj: neither overrides
# <LanguageStandard>, so both build under BuildConfig.props' repo-wide C++17
# default (unlike File/RayTracer/PhysicsTest, which explicitly set stdcpp20).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NUMERICS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
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
