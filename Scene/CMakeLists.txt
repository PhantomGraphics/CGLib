# Scene core library (Phantom::Scene -- particle-system / triangle-mesh / wireframe scene graph) and its GoogleTest suite.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/Scene -B build/Scene -DCMAKE_BUILD_TYPE=Debug && cmake --build build/Scene
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake / cmake/PhantomVulkanApp.cmake;
# this file adds the module's test / app targets on top. See docs/standalone-repository-plan.md.

cmake_minimum_required(VERSION 3.20)

project(SceneCoreLinux LANGUAGES CXX)

# Matches Scene.vcxproj/SceneTest.vcxproj: neither overrides
# <LanguageStandard>, so both build under BuildConfig.props' repo-wide C++17
# default.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SCENE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
include(PhantomGTest)
include(PhantomCoreLibs)

phantom_add_scene_core()

# --- SceneTest (GoogleTest) ---

phantom_find_gtest()

if(PHANTOM_GTEST_FOUND)
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
    target_compile_options(SceneTest PRIVATE ${PHANTOM_WARN_FLAGS})

    include(GoogleTest)
    gtest_discover_tests(SceneTest)
else()
    message(WARNING "GTest not found -- skipping SceneTest target")
endif()
