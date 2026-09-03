# CGLib -- top-level CMake build.
#
# This is the single product build entry point for CGLib as a standalone
# repository (docs/standalone-repository-plan.md). It configures, builds and
# tests every module without referencing anything outside this directory.
#
#   # CPU-only (no Vulkan SDK required)
#   cmake --preset windows-debug -DCGLIB_ENABLE_VULKAN=OFF
#   cmake --build --preset windows-debug
#   ctest --preset windows-debug
#
#   # With the Vulkan/GLFW modules (auto-enabled when a Vulkan SDK is found)
#   cmake --preset linux-debug
#
# Options (see the table in docs/standalone-repository-plan.md section 3):
#   CGLIB_BUILD_TESTING   ON when top-level   CTest/GoogleTest suites
#   CGLIB_BUILD_EXAMPLES  OFF                 examples/ consumer samples
#   CGLIB_BUILD_VIEWERS   OFF                 Vulkan/GLFW viewer executables
#   CGLIB_ENABLE_VULKAN   auto-detected       Vulkan/GLFW-dependent modules
#   CGLIB_FETCH_GTEST     ON                  fetch pinned GTest if none found

cmake_minimum_required(VERSION 3.20)

project(CGLib VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CGLIB_IS_TOP_LEVEL ON)
else()
    set(CGLIB_IS_TOP_LEVEL OFF)
endif()

# C++20 is the whole library's minimum standard (see docs/standalone-repository-plan.md
# section 7 -- resolved to "yes"). Individual *Core targets also request it via
# target_compile_features so they stay correct if consumed from a lower project.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# --- Options -----------------------------------------------------------------

option(CGLIB_BUILD_TESTING "Build the GoogleTest suites" ${CGLIB_IS_TOP_LEVEL})
option(CGLIB_BUILD_EXAMPLES "Build the examples/ consumer samples" OFF)
option(CGLIB_BUILD_VIEWERS "Build the Vulkan/GLFW viewer executables" OFF)

# CGLIB_ENABLE_VULKAN defaults to whether a Vulkan SDK is discoverable. Forcing
# it ON without the SDK present is a hard error (a plan requirement: an enabled
# feature must not be silently dropped).
find_package(Vulkan QUIET)
option(CGLIB_ENABLE_VULKAN "Build the Vulkan/GLFW-dependent modules" ${Vulkan_FOUND})

set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
include(PhantomGTest)
include(PhantomCoreLibs)
include(PhantomVulkanApp)

# --- Testing ---------------------------------------------------------------

if(CGLIB_BUILD_TESTING)
    enable_testing()
    phantom_find_gtest()
    include(GoogleTest)
endif()

# gtest_discover_tests() runs the test binary at build time to enumerate cases;
# a wrapper keeps the call sites short and consistent.
function(cglib_add_test target)
    if(NOT CGLIB_BUILD_TESTING OR NOT PHANTOM_GTEST_FOUND)
        return()
    endif()
    cmake_parse_arguments(A "" "WORKING_DIRECTORY" "SOURCES;LINK;INCLUDES" ${ARGN})
    add_executable(${target} ${A_SOURCES})
    target_include_directories(${target} PRIVATE ${REPO_ROOT} ${A_INCLUDES})
    target_link_libraries(${target} PRIVATE ${A_LINK} GTest::gtest GTest::gtest_main)
    target_compile_options(${target} PRIVATE ${PHANTOM_WARN_FLAGS})
    if(A_WORKING_DIRECTORY)
        gtest_discover_tests(${target} WORKING_DIRECTORY ${A_WORKING_DIRECTORY})
    else()
        gtest_discover_tests(${target})
    endif()
endfunction()

# ---------------------------------------------------------------------------
# CPU-only modules (always built)
# ---------------------------------------------------------------------------

# Math (Phantom::Math)
phantom_add_math_core()
file(GLOB MATHTEST_SOURCES ${CGLIB_ROOT}/MathTest/*.cpp)
list(FILTER MATHTEST_SOURCES EXCLUDE REGEX "pch\\.cpp$")
cglib_add_test(MathTest SOURCES ${MATHTEST_SOURCES} INCLUDES ${CGLIB_ROOT}/MathTest LINK MathCore)

# Graphics (Phantom::Graphics)
phantom_add_graphics_core()
file(GLOB GRAPHICSTEST_SOURCES ${CGLIB_ROOT}/GraphicsTest/*.cpp)
list(FILTER GRAPHICSTEST_SOURCES EXCLUDE REGEX "pch\\.cpp$")
cglib_add_test(GraphicsTest SOURCES ${GRAPHICSTEST_SOURCES} INCLUDES ${CGLIB_ROOT}/GraphicsTest LINK GraphicsCore MathCore)

# Pugixml (ThirdParty/pugixml) -- XML parser, no external deps.
add_library(PugixmlCore STATIC ${CGLIB_ROOT}/ThirdParty/pugixml/pugixml.cpp)
target_include_directories(PugixmlCore PUBLIC ${CGLIB_ROOT}/ThirdParty/pugixml)
target_compile_options(PugixmlCore PRIVATE ${PHANTOM_WARN_FLAGS})

# Numerics / Scene / File / Animation / Volume / Space each own a CMakeLists.txt
# (also usable standalone). Their *Core builders are idempotent, their *Test
# targets register with CTest.
add_subdirectory(Numerics)
add_subdirectory(Scene)
add_subdirectory(File)
add_subdirectory(Animation)
add_subdirectory(Volume)
add_subdirectory(Space)

# ---------------------------------------------------------------------------
# Vulkan / GLFW modules
# ---------------------------------------------------------------------------

phantom_find_vulkan_headers()

if(CGLIB_ENABLE_VULKAN AND NOT PHANTOM_VULKAN_INCLUDE_DIR)
    message(FATAL_ERROR
        "CGLIB_ENABLE_VULKAN=ON but vulkan/vulkan.h was not found. "
        "Install a Vulkan SDK (set VULKAN_SDK), pass -DVULKAN_INCLUDE_DIR=/path/to/include, "
        "or configure with -DCGLIB_ENABLE_VULKAN=OFF for a CPU-only build.")
endif()

if(PHANTOM_VULKAN_INCLUDE_DIR)
    phantom_add_vulkangraphics_core()
    phantom_add_uiwidgets_core()

    phantom_find_vulkan_loader()
    phantom_find_glfw_loader()

    if(CGLIB_BUILD_TESTING AND PHANTOM_GTEST_FOUND AND PHANTOM_VULKAN_LIBRARY AND PHANTOM_GLFW_LIBRARY)
        file(GLOB VULKANGRAPHICSTEST_SOURCES ${CGLIB_ROOT}/VulkanGraphics/VulkanGraphicsTest/*.cpp)
        add_executable(VulkanGraphicsTest ${VULKANGRAPHICSTEST_SOURCES})
        target_include_directories(VulkanGraphicsTest PRIVATE
            ${CGLIB_ROOT}/ThirdParty/glfw-3.3.8/include
            ${CGLIB_ROOT}/VulkanGraphics/VulkanGraphicsTest)
        target_link_libraries(VulkanGraphicsTest PRIVATE
            VulkanGraphicsCore ${PHANTOM_VULKAN_LIBRARY} ${PHANTOM_GLFW_LIBRARY}
            GTest::gtest GTest::gtest_main)
        target_compile_options(VulkanGraphicsTest PRIVATE ${PHANTOM_WARN_FLAGS})
        add_custom_command(TARGET VulkanGraphicsTest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CGLIB_ROOT}/VulkanGraphics/VulkanGraphicsTest/shaders
                $<TARGET_FILE_DIR:VulkanGraphicsTest>/shaders)
        # Enumerating cases needs a real Vulkan device; register as one ctest
        # entry so `cmake --build` always succeeds and running it is opt-in.
        add_test(NAME VulkanGraphicsTest COMMAND VulkanGraphicsTest)
    elseif(CGLIB_BUILD_TESTING AND PHANTOM_GTEST_FOUND)
        message(STATUS "CGLib: Vulkan/GLFW loader not found -- VulkanGraphicsTest skipped (VulkanGraphicsCore still builds).")
    endif()

    # Peripheral Vulkan modules -- each owns a CMakeLists.txt.
    add_subdirectory(Renderer)
    add_subdirectory(GltfRenderer)
    add_subdirectory(Gizmo)
    add_subdirectory(Input)
    add_subdirectory(Particles)
    add_subdirectory(PostProcess)
endif()

# ---------------------------------------------------------------------------
# Namespaced public aliases -- consumers link CGLib::<Component>
# ---------------------------------------------------------------------------

set(CGLIB_ALIAS_MAP
    Math=MathCore  Graphics=GraphicsCore  Numerics=NumericsCore  Space=SpaceCore
    Scene=SceneCore  Volume=VolumeCore  File=FileCore  Animation=AnimationCore
    Pugixml=PugixmlCore
    VulkanGraphics=VulkanGraphicsCore  UI=UIWidgetsCore  VkAppBase=VkAppBaseCore
    VkRenderer=VkRendererCore  GltfRenderer=GltfRendererCore  VolumeRenderer=VolumeRenderer
    Gizmo=GizmoCore  Input=InputCore  Particles=ParticlesCore  PostProcess=PostProcessCore)
foreach(_pair IN LISTS CGLIB_ALIAS_MAP)
    string(REPLACE "=" ";" _pair "${_pair}")
    list(GET _pair 0 _name)
    list(GET _pair 1 _target)
    if(TARGET ${_target} AND NOT TARGET CGLib::${_name})
        add_library(CGLib::${_name} ALIAS ${_target})
    endif()
endforeach()

if(CGLIB_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ---------------------------------------------------------------------------
# Configure summary
# ---------------------------------------------------------------------------

message(STATUS "")
message(STATUS "CGLib ${PROJECT_VERSION} configuration:")
message(STATUS "  C++ standard          : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type            : ${CMAKE_BUILD_TYPE}${CMAKE_CONFIGURATION_TYPES}")
message(STATUS "  Tests (CGLIB_BUILD_TESTING) : ${CGLIB_BUILD_TESTING} (GoogleTest found: ${PHANTOM_GTEST_FOUND})")
message(STATUS "  Vulkan (CGLIB_ENABLE_VULKAN): ${CGLIB_ENABLE_VULKAN} (headers: ${PHANTOM_VULKAN_INCLUDE_DIR})")
message(STATUS "  Examples / Viewers    : ${CGLIB_BUILD_EXAMPLES} / ${CGLIB_BUILD_VIEWERS}")
message(STATUS "")
