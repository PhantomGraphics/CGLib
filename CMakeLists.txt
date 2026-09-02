# Cross-platform CMake build for CGLib's four modules (Phantom::Math / Phantom::Graphics /
# Phantom::VKG (VulkanGraphics) / Phantom::UI (UIWidgets)) and their GoogleTest suites.
#
# This is a standalone CMake build, separate from the MSBuild/.vcxproj-based Windows build
# (Phantom2026.sln, which remains the build used by the rest of the repo's ProjectReference
# graph -- see CGLib/.vcxproj files, still hand-maintained and unaffected by this file). It
# exists to (a) validate that CGLib compiles cleanly with clang on Linux, and (b) let CGLib be
# opened/built standalone in Visual Studio 2026's native "Open Folder" CMake mode without
# touching the .vcxproj-based solution. Math/Graphics/VulkanGraphics/UIWidgets are now defined
# once in ../cmake/PhantomCoreLibs.cmake and ../cmake/PhantomVulkanApp.cmake (Phase 4 §5.4
# item 1 dedup, internal design notes) -- this file (the original
# home of all four) just calls them and adds the *Test targets on top, so this file alone
# still builds Math/Graphics/VulkanGraphics/UIWidgets standalone exactly as before, without
# needing any of the modules formerly under a separate internal submodule (now merged in
# flat under this same CGLib/ directory) --
# those are add_subdirectory()'d individually from the root CMakeLists.txt instead.
#
# --- Linux (WSL/clang) usage -- avoid /mnt/c/... for speed on a Linux-native checkout ---
#   cmake -S CGLib -B CGLib/build_linux \
#       -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
#       -DCMAKE_BUILD_TYPE=Debug \
#       -DVULKAN_INCLUDE_DIR=/path/to/Vulkan-Headers/include
#   cmake --build CGLib/build_linux -j
#   ctest --test-dir CGLib/build_linux --output-on-failure
#
# --- Windows (Visual Studio 2026) usage ---
#   Open the CGLib/ folder directly in VS2026 (File > Open > Folder) -- CMakePresets.json
#   in this directory is picked up automatically and drives configure/build/test from the
#   VS toolbar, no .vcxproj involved. Equivalent from a "x64 Native Tools Command Prompt for
#   VS 2026": `cmake --preset windows-debug && cmake --build --preset windows-debug`.
#
# Prerequisites:
#   - GoogleTest: Linux uses libgtest-dev (find_package(GTest), system package). Windows reuses
#     the same prebuilt NuGet package (`Microsoft.googletest.v140.windesktop.msvcstl.static.rt-
#     dyn`) already restored under the repo-root `packages/` for the .vcxproj-based Test
#     projects -- no new dependency introduced. If that package isn't present yet, restore it
#     once via any .vcxproj-based build/NuGet restore (e.g. opening Phantom2026.sln) first.
#   - Vulkan headers (VulkanGraphics/UIWidgets): Windows defaults to `$ENV{VULKAN_SDK}/Include`
#     (same LunarG SDK the .vcxproj build uses). Linux has no repo-vendored copy, so point
#     VULKAN_INCLUDE_DIR at any vulkan/vulkan.h provider (KhronosGroup/Vulkan-Headers checkout,
#     or a distro libvulkan-dev's /usr/include). Do NOT point this at a Blender-bundled copy or
#     any other app-private Vulkan install -- must be a standalone/system Vulkan header+loader,
#     not borrowed from an unrelated application. If not found, VulkanGraphics/UIWidgets/their
#     tests are skipped and only Math/Graphics build.
#   - A Vulkan loader is only needed to LINK VulkanGraphicsTest, not to build the
#     VulkanGraphicsCore/UIWidgetsCore static libraries (compile-only is enough for those --
#     they never call into the loader at CMake configure/build time). Windows defaults to
#     `$ENV{VULKAN_SDK}/Lib/vulkan-1.lib`; Linux points VULKAN_LIBRARY at the system loader
#     (e.g. /usr/lib/x86_64-linux-gnu/libvulkan.so.1) explicitly if CMake's own find can't
#     locate an unversioned libvulkan.so.
#   - GLFW (VulkanGraphicsTest's fixture opens a real hidden window + VkSurfaceKHR): Windows
#     defaults to the repo-vendored static `ThirdParty/glfw-3.3.8/lib-vc2019/glfw3.lib` (same
#     one the .vcxproj build links). Linux has no vendored .so (only the Windows static libs),
#     point GLFW_LIBRARY at any libglfw.so(.3) -- e.g. `apt-get download libglfw3` (no root
#     needed) + dpkg-deb -x, or a system libglfw3-dev install.

cmake_minimum_required(VERSION 3.20)

project(CGLibStandalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Single-config generators (Ninja, Makefiles) need an explicit build type; multi-config
# generators (Visual Studio) manage this themselves via CMAKE_CONFIGURATION_TYPES instead.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    set(PHANTOM_WARN_FLAGS /W3)
    # Matches BuildConfig.props' `/utf-8 %(AdditionalOptions)` applied repo-wide to the
    # .vcxproj build -- several sources have Japanese comments that aren't valid in the
    # legacy MSVC source code page, so this is required, not cosmetic.
    add_compile_options(/utf-8)
else()
    set(PHANTOM_WARN_FLAGS -Wall)
endif()

set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

list(APPEND CMAKE_MODULE_PATH ${REPO_ROOT}/cmake)
include(PhantomGTest)
include(PhantomCoreLibs)
include(PhantomVulkanApp)

phantom_find_gtest()

# ---------------------------------------------------------------------------
# Math (Phantom::Math)
# ---------------------------------------------------------------------------

phantom_add_math_core()

if(PHANTOM_GTEST_FOUND)
    enable_testing()

    file(GLOB MATHTEST_SOURCES ${CGLIB_ROOT}/MathTest/*.cpp)
    list(FILTER MATHTEST_SOURCES EXCLUDE REGEX "pch\\.cpp$")

    add_executable(MathTest ${MATHTEST_SOURCES})
    target_include_directories(MathTest PRIVATE ${CGLIB_ROOT} ${CGLIB_ROOT}/MathTest)
    target_link_libraries(MathTest PRIVATE MathCore GTest::gtest GTest::gtest_main)
    target_compile_options(MathTest PRIVATE ${PHANTOM_WARN_FLAGS})

    include(GoogleTest)
    gtest_discover_tests(MathTest)
endif()

# ---------------------------------------------------------------------------
# Graphics (Phantom::Graphics) -- depends on Math
# ---------------------------------------------------------------------------

phantom_add_graphics_core()

if(PHANTOM_GTEST_FOUND)
    file(GLOB GRAPHICSTEST_SOURCES ${CGLIB_ROOT}/GraphicsTest/*.cpp)
    list(FILTER GRAPHICSTEST_SOURCES EXCLUDE REGEX "pch\\.cpp$")

    add_executable(GraphicsTest ${GRAPHICSTEST_SOURCES})
    target_include_directories(GraphicsTest PRIVATE ${CGLIB_ROOT} ${CGLIB_ROOT}/GraphicsTest)
    target_link_libraries(GraphicsTest PRIVATE GraphicsCore MathCore GTest::gtest GTest::gtest_main)
    target_compile_options(GraphicsTest PRIVATE ${PHANTOM_WARN_FLAGS})

    include(GoogleTest)
    gtest_discover_tests(GraphicsTest)
endif()

# ---------------------------------------------------------------------------
# Pugixml (ThirdParty/pugixml) -- header-ish XML parser, no external deps.
# CGLib/ThirdParty/pugixml/Pugixml.vcxproj is its own vcxproj on the Windows
# side (listed in Phantom2026.sln), absorbed here per
# internal design notes Phase 3 §5.3 instead of
# getting its own top-level CMakeLists.txt, since it has no dependents within
# this repo's non-CGApp scope to build/test it against -- compiling cleanly
# is the completion criterion, same as the Phase 3 peripheral libs.
# ---------------------------------------------------------------------------

add_library(PugixmlCore STATIC ${CGLIB_ROOT}/ThirdParty/pugixml/pugixml.cpp)
target_include_directories(PugixmlCore PRIVATE ${CGLIB_ROOT}/ThirdParty/pugixml)
target_compile_options(PugixmlCore PRIVATE ${PHANTOM_WARN_FLAGS})

# ---------------------------------------------------------------------------
# Vulkan headers/loader discovery (shared by VulkanGraphics + UIWidgets)
# ---------------------------------------------------------------------------

phantom_find_vulkan_headers()

if(PHANTOM_VULKAN_INCLUDE_DIR)

# ---------------------------------------------------------------------------
# VulkanGraphics (Phantom::VKG) -- depends on Vulkan headers + vendored glm/VMA
# ---------------------------------------------------------------------------

phantom_add_vulkangraphics_core()

# The system/SDK Vulkan loader (NOT any app-bundled copy such as Blender's) -- only needed to
# link VulkanGraphicsTest; VulkanGraphicsCore itself only needs headers to compile.
phantom_find_vulkan_loader()
phantom_find_glfw_loader()

if(PHANTOM_GTEST_FOUND AND PHANTOM_VULKAN_LIBRARY AND PHANTOM_GLFW_LIBRARY)
    file(GLOB VULKANGRAPHICSTEST_SOURCES ${CGLIB_ROOT}/VulkanGraphics/VulkanGraphicsTest/*.cpp)

    add_executable(VulkanGraphicsTest ${VULKANGRAPHICSTEST_SOURCES})
    target_include_directories(VulkanGraphicsTest PRIVATE
        ${CGLIB_ROOT}/ThirdParty/glfw-3.3.8/include
        ${CGLIB_ROOT}/VulkanGraphics/VulkanGraphicsTest
    )
    target_link_libraries(VulkanGraphicsTest PRIVATE
        VulkanGraphicsCore
        ${PHANTOM_VULKAN_LIBRARY}
        ${PHANTOM_GLFW_LIBRARY}
        GTest::gtest
        GTest::gtest_main
    )
    target_compile_options(VulkanGraphicsTest PRIVATE ${PHANTOM_WARN_FLAGS})

    # Windows' PostBuildEvent copies shaders/*.spv next to the .exe; mirror that here so
    # loadSPV("shaders/test.comp.spv") (a cwd-relative lookup) resolves when run from the
    # build directory, same as ctest/`cmake --build` would run it.
    add_custom_command(TARGET VulkanGraphicsTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CGLIB_ROOT}/VulkanGraphics/VulkanGraphicsTest/shaders
            $<TARGET_FILE_DIR:VulkanGraphicsTest>/shaders
    )

    include(GoogleTest)
    # discover_tests would run the binary at build time to enumerate cases, which needs an
    # actual Vulkan device (ICD) -- not guaranteed in a headless/WSL environment. Register it
    # as a single ctest entry instead so `cmake --build` always succeeds; running it is opt-in.
    add_test(NAME VulkanGraphicsTest COMMAND VulkanGraphicsTest)
else()
    if(PHANTOM_GTEST_FOUND)
        message(WARNING "CGLib: Vulkan and/or GLFW loader not found -- VulkanGraphicsTest is skipped (VulkanGraphicsCore itself still builds). Pass -DVULKAN_LIBRARY=... -DGLFW_LIBRARY=... to enable it.")
    endif()
endif()

# ---------------------------------------------------------------------------
# UIWidgets (Phantom::UI) -- ImGui core+backends, depends on Graphics + Vulkan/GLFW headers
# ---------------------------------------------------------------------------

phantom_add_uiwidgets_core()

endif() # PHANTOM_VULKAN_INCLUDE_DIR
