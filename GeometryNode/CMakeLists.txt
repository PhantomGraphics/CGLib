# GeometryNode core library (Phantom::GeometryNode -- CPU-only node-graph geometry evaluation) and its GoogleTest suite.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/GeometryNode -B build/GeometryNode -DCMAKE_BUILD_TYPE=Debug && cmake --build build/GeometryNode
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake / cmake/PhantomVulkanApp.cmake;
# this file adds the module's test target on top. See docs/standalone-repository-plan.md
# and docs/todo/PLAN_geometry_node.md.

cmake_minimum_required(VERSION 3.20)

project(GeometryNodeStandalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(GEOMETRYNODE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
include(PhantomGTest)
include(PhantomCoreLibs)

phantom_add_geometrynode_core()

# --- GeometryNodeTest (GoogleTest) ---

phantom_find_gtest()

if(PHANTOM_GTEST_FOUND)
    enable_testing()

    add_executable(GeometryNodeTest
        ${GEOMETRYNODE_ROOT}/GeometryNodeTest/GeometryOpsTest.cpp
        ${GEOMETRYNODE_ROOT}/GeometryNodeTest/GraphEvaluatorTest.cpp
        ${GEOMETRYNODE_ROOT}/GeometryNodeTest/GraphJsonTest.cpp
        ${GEOMETRYNODE_ROOT}/GeometryNodeTest/FieldTest.cpp
    )
    target_include_directories(GeometryNodeTest PRIVATE
        ${REPO_ROOT}
        ${GEOMETRYNODE_ROOT}/GeometryNodeTest
    )
    target_link_libraries(GeometryNodeTest PRIVATE
        GeometryNodeCore
        GTest::gtest
        GTest::gtest_main
    )
    target_compile_options(GeometryNodeTest PRIVATE ${PHANTOM_WARN_FLAGS})

    include(GoogleTest)
    gtest_discover_tests(GeometryNodeTest)
else()
    message(STATUS "GTest unavailable or tests disabled -- skipping GeometryNodeTest target")
endif()
