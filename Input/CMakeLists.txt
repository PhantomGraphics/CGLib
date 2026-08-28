# Linux (clang) + Windows (VS2026) build for InputCore (Crystal::Input --
# per-frame GLFW keyboard/gamepad polling into GameAction states).
#
# This is a standalone CMake build, separate from the MSBuild/.vcxproj-based
# Windows build (Input.vcxproj, driven by Phantom2026.sln, untouched by this
# file). Part of docs/todo/PLAN_crossplatform_non_cgapp_build.md Phase 3:
# Input is only consumed by CGApp (Universe/CGStudio), which is out of this
# plan's scope, so unlike Phase 2's apps there is no runtime verification
# available here -- successful compilation is the completion criterion.
#
# Usage (Linux, from WSL -- avoid /mnt/c/... for speed on a Linux-native checkout):
#   cmake -S CGLib/Input -B CGLib/Input/build_linux \
#       -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
#       -DCMAKE_BUILD_TYPE=Debug \
#       -DGLFW_INCLUDE_DIR=/path/to/include
#   cmake --build CGLib/Input/build_linux -j
#
# InputCore only needs GLFW *headers* (GLFW/glfw3.h is a declarations-only
# header; InputManager.cpp never calls into the GLFW loader itself -- it just
# reads the GLFWwindow*/joystick state passed in by the caller) -- it links
# against nothing here, so unlike the Phase 2 app CMakeLists there is no
# loader discovery/link step.

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
set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(CGLIB_ROOT ${REPO_ROOT}/CGLib)

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
# Input (Crystal::Input)
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
