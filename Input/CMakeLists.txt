# Input core library (Phantom::Input -- per-frame GLFW keyboard/gamepad polling). Compile-only: needs GLFW headers, links nothing here.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/Input -B build/Input -DCMAKE_BUILD_TYPE=Debug && cmake --build build/Input
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake / cmake/PhantomVulkanApp.cmake;
# this file adds the module's test / app targets on top. See docs/standalone-repository-plan.md.

cmake_minimum_required(VERSION 3.20)

project(InputStandalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    set(INPUT_WARN_FLAGS /W3)
    add_compile_options(/utf-8)
else()
    set(INPUT_WARN_FLAGS -Wall)
endif()

set(INPUT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()

# ---------------------------------------------------------------------------
# GLFW headers discovery (compile-only -- no loader/link step needed)
# ---------------------------------------------------------------------------

set(INPUT_DEFAULT_GLFW_INCLUDE_DIR "${CGLIB_ROOT}/ThirdParty/glfw-3.3.8/include")
set(GLFW_INCLUDE_DIR "${INPUT_DEFAULT_GLFW_INCLUDE_DIR}" CACHE PATH "Directory containing GLFW/glfw3.h (defaults to the repo-vendored CGLib/ThirdParty/glfw-3.3.8/include, present on both Windows and Linux checkouts; on Linux a system libglfw3-dev's /usr/include also works).")

if(GLFW_INCLUDE_DIR AND EXISTS "${GLFW_INCLUDE_DIR}/GLFW/glfw3.h")
    set(INPUT_GLFW_INCLUDE_DIR ${GLFW_INCLUDE_DIR})
else()
    find_path(INPUT_GLFW_INCLUDE_DIR NAMES GLFW/glfw3.h)
endif()

if(INPUT_GLFW_INCLUDE_DIR)
    message(STATUS "InputCore: using GLFW headers from ${INPUT_GLFW_INCLUDE_DIR}")
else()
    message(WARNING "InputCore: GLFW/glfw3.h not found -- InputCore is skipped. Pass -DGLFW_INCLUDE_DIR=/path/to/include to enable it.")
endif()

if(INPUT_GLFW_INCLUDE_DIR)

# ---------------------------------------------------------------------------
# Input (Phantom::Input)
# ---------------------------------------------------------------------------

add_library(InputCore STATIC
    ${INPUT_ROOT}/InputManager.cpp
)
target_include_directories(InputCore PRIVATE
    ${CGLIB_ROOT}/ThirdParty/glm-0.9.9.8
    ${INPUT_GLFW_INCLUDE_DIR}
)
target_compile_options(InputCore PRIVATE ${INPUT_WARN_FLAGS})
target_compile_definitions(InputCore PRIVATE GLFW_INCLUDE_NONE)

else()
    message(WARNING "InputCore: GLFW headers not found -- InputCore is skipped. Pass -DGLFW_INCLUDE_DIR=... to enable it.")
endif()
