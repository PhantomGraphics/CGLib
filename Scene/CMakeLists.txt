# Cross-platform CMake build for the Scene core library (Crystal::Scene) and
# its GoogleTest suite (SceneTest).
#
# This is a standalone CMake build, separate from the MSBuild/.vcxproj-based
# Windows build (Scene/Scene/Scene.vcxproj + Scene/SceneTest/SceneTest.vcxproj,
# driven by Phantom2026.sln). Part of
# docs/todo/PLAN_crossplatform_non_cgapp_build.md Phase 1 -- follows the same
# pattern as ../../Physics/CMakeLists.txt / ../../PointCloud/CMakeLists.txt.
# SceneCore itself is now defined once in ../../cmake/CrystalCoreLibs.cmake
# (Phase 4 §5.4 item 1 dedup) -- this file just calls it and adds SceneTest
# on top, so it also works standalone (`cmake -S CGLib/Scene -B ...`)
# exactly as before, and identically when add_subdirectory()'d from the root
# CMakeLists.txt.
#
# Usage (Linux, from WSL -- avoid /mnt/c/... for speed on a Linux-native checkout):
#   cmake -S CGLib/Scene -B CGLib/Scene/build_linux \
#       -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
#       -DCMAKE_BUILD_TYPE=Debug
#   cmake --build CGLib/Scene/build_linux -j
#   ctest --test-dir CGLib/Scene/build_linux --output-on-failure
#
# Prerequisite: libgtest-dev (provides libgtest.a/libgtest_main.a + the GTest
# CMake package config under /usr/lib/x86_64-linux-gnu/cmake/GTest).
#
# NOTE on Scene's on-disk source list (ParticleSystemIdPresenter.cpp etc. NOT
# part of SceneCore): see cmake/CrystalCoreLibs.cmake's crystal_add_scene_core().

cmake_minimum_required(VERSION 3.20)

project(SceneCoreLinux LANGUAGES CXX)

# Matches Scene.vcxproj/SceneTest.vcxproj: neither overrides
# <LanguageStandard>, so both build under BuildConfig.props' repo-wide C++17
# default.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(CGLIB_ROOT ${REPO_ROOT}/CGLib)
set(SCENE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    add_compile_options(/utf-8)
    set(CRYSTAL_WARN_FLAGS /W3)
else()
    set(CRYSTAL_WARN_FLAGS -Wall)
endif()

list(APPEND CMAKE_MODULE_PATH ${REPO_ROOT}/cmake)
include(PhantomGTest)
include(PhantomCoreLibs)

crystal_add_scene_core()

# --- SceneTest (GoogleTest) ---

crystal_find_gtest()

if(CRYSTAL_GTEST_FOUND)
    enable_testing()

    add_executable(SceneTest
        ${SCENE_ROOT}/SceneTest/SceneBaseTest.cpp
        ${SCENE_ROOT}/SceneTest/SceneGroupTest.cpp
    )
    target_include_directories(SceneTest PRIVATE
        ${REPO_ROOT}
        ${SCENE_ROOT}/SceneTest
    )
    target_link_libraries(SceneTest PRIVATE
        SceneCore
        GTest::gtest
        GTest::gtest_main
    )
    target_compile_options(SceneTest PRIVATE ${CRYSTAL_WARN_FLAGS})

    include(GoogleTest)
    gtest_discover_tests(SceneTest)
else()
    message(WARNING "GTest not found -- skipping SceneTest target")
endif()
