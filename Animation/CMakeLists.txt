# Cross-platform CMake build for the Animation core library (Phantom::Animation),
# its GoogleTest suite (AnimationTest), and AnimationRenderer/AnimationView
# (Vulkan app).
#
# This is a standalone CMake build, separate from the MSBuild/.vcxproj-based
# Windows build (Animation/Animation/Animation.vcxproj + Animation/AnimationTest/
# AnimationTest.vcxproj, driven by Phantom2026.sln). AnimationCore/
# GltfRendererCore/VkAppBaseCore/etc. are now defined once in
# ../../cmake/PhantomCoreLibs.cmake and ../../cmake/PhantomVulkanApp.cmake
# (Phase 4 §5.4 item 1 dedup, internal design notes)
# -- this file just calls them and adds AnimationTest/AnimationRenderer/
# AnimationView on top, so it also works standalone
# (`cmake -S CGLib/Animation -B ...`) exactly as before, and identically
# when add_subdirectory()'d from the root CMakeLists.txt.
#
# Usage (from WSL, in a Linux-native checkout -- avoid /mnt/c/... for speed):
#   cmake -S CGLib/Animation -B CGLib/Animation/build_linux \
#       -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
#       -DCMAKE_BUILD_TYPE=Debug
#   cmake --build CGLib/Animation/build_linux -j
#   ctest --test-dir CGLib/Animation/build_linux --output-on-failure
#
# Prerequisite: libgtest-dev (provides libgtest.a/libgtest_main.a + the GTest
# CMake package config under /usr/lib/x86_64-linux-gnu/cmake/GTest).
#
# Dependency notes (confirmed via each .vcxproj's real <ProjectReference>/
# #include lists, not just the plan doc's simplified diagram):
#   - AnimationView.vcxproj does NOT reference AnimationRenderer.vcxproj at
#     all -- AnimationView renders MMD by converting PMX/VMD to glTF
#     (MmdToGltfConverter/GltfAnimationEvaluator, both in GltfRenderer) and
#     drawing that through GltfRendererCore's GltfSceneRenderer, not through
#     AnimationRenderer's MMD-specific SkinnedMeshRenderer/BoneWireRenderer
#     path. AnimationRenderer is still built below as its own target (in
#     scope per the plan's "all CGApp-external projects" mandate) but
#     AnimationView does not link it.
#   - AnimationView.vcxproj also lists a ProjectReference on VkRenderer.vcxproj,
#     but nothing under AnimationView/*.cpp actually calls into Phantom::VKR
#     (confirmed by grep) -- a stale/vestigial reference, not linked here.

cmake_minimum_required(VERSION 3.20)

project(AnimationCoreLinux LANGUAGES CXX)

# Animation.vcxproj explicitly sets <LanguageStandard>stdcpp17</LanguageStandard>
# (unlike File/GltfRenderer/the Vulkan app chain, which need C++20) -- matches
# the repo-wide BuildConfig.props default, kept explicit here per plan section
# 3 item 6's "no ambiguous unspecified standard" policy.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(ANIMATION_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(CGLIB_ROOT ${REPO_ROOT}/CGLib)

if(MSVC)
    add_compile_options(/utf-8)
    set(PHANTOM_WARN_FLAGS /W3)
else()
    set(PHANTOM_WARN_FLAGS -Wall)
endif()

list(APPEND CMAKE_MODULE_PATH ${REPO_ROOT}/cmake)
include(PhantomGTest)
include(PhantomCoreLibs)
include(PhantomVulkanApp)

phantom_add_animation_core()

# --- AnimationTest (GoogleTest) ---

phantom_find_gtest()

if(PHANTOM_GTEST_FOUND)
    enable_testing()

    add_executable(AnimationTest
        ${ANIMATION_ROOT}/AnimationTest/AnimatorTest.cpp
        ${ANIMATION_ROOT}/AnimationTest/BVHConverterTest.cpp
        ${ANIMATION_ROOT}/AnimationTest/IKSolverTest.cpp
        ${ANIMATION_ROOT}/AnimationTest/MorphAnimatorTest.cpp
        ${ANIMATION_ROOT}/AnimationTest/VMDConverterTest.cpp
    )
    target_include_directories(AnimationTest PRIVATE
        ${ANIMATION_ROOT}/AnimationTest
    )
    target_link_libraries(AnimationTest PRIVATE
        AnimationCore
        GTest::gtest
        GTest::gtest_main
    )
    target_compile_options(AnimationTest PRIVATE ${PHANTOM_WARN_FLAGS})

    include(GoogleTest)
    gtest_discover_tests(AnimationTest)
else()
    message(WARNING "GTest not found -- skipping AnimationTest target")
endif()

# ---------------------------------------------------------------------------
# AnimationRenderer + AnimationView -- Phase 2 item 4.
# ---------------------------------------------------------------------------

phantom_find_vulkan_headers()

if(PHANTOM_VULKAN_INCLUDE_DIR)

phantom_find_vulkan_loader()
phantom_find_glfw_loader()

if(PHANTOM_VULKAN_LIBRARY AND PHANTOM_GLFW_LIBRARY)

phantom_add_vkappbase_core()
phantom_add_vkrenderer_core()
phantom_add_gltfrenderer_core()

set(ANIMATIONVIEW_ROOT ${ANIMATION_ROOT}/AnimationView)
set(ANIMATIONRENDERER_ROOT ${ANIMATION_ROOT}/AnimationRenderer)

# --- AnimationRenderer (Phantom::Animation MMD skinned-mesh/bone-wire
# renderer) -- built per plan scope, but NOT linked into AnimationView below
# (see file-header NOTE: AnimationView.vcxproj doesn't reference it).

add_library(AnimationRenderer STATIC
    ${ANIMATIONRENDERER_ROOT}/BoneWireRenderer.cpp
    ${ANIMATIONRENDERER_ROOT}/GpuSkinnedRenderer.cpp
    ${ANIMATIONRENDERER_ROOT}/MmdMeshRenderer.cpp
    ${ANIMATIONRENDERER_ROOT}/SkinnedMeshRenderer.cpp
    ${ANIMATIONRENDERER_ROOT}/VulkanTextureHelper.cpp
)
target_include_directories(AnimationRenderer PRIVATE
    ${CGLIB_ROOT}/ThirdParty/stb
)
target_link_libraries(AnimationRenderer PRIVATE AnimationCore VulkanGraphicsCore VkRendererCore)
target_compile_options(AnimationRenderer PRIVATE ${PHANTOM_WARN_FLAGS})
target_compile_features(AnimationRenderer PRIVATE cxx_std_20)

# --- AnimationView (standalone ImGui + Vulkan MMD animation viewer) ---

add_executable(AnimationView
    ${ANIMATIONVIEW_ROOT}/CommandDispatcher.cpp
    ${ANIMATIONVIEW_ROOT}/AnimationPanel.cpp
    ${ANIMATIONVIEW_ROOT}/AnimationViewApp.cpp
    ${ANIMATIONVIEW_ROOT}/main.cpp
)
target_include_directories(AnimationView PRIVATE
    ${CGLIB_ROOT}/UIWidgets
)
target_link_libraries(AnimationView PRIVATE
    GltfRendererCore
    FileCore
    AnimationCore
    VkAppBaseCore
    ${PHANTOM_VULKAN_LIBRARY}
    ${PHANTOM_GLFW_LIBRARY}
)
target_compile_options(AnimationView PRIVATE ${PHANTOM_WARN_FLAGS})
target_compile_features(AnimationView PRIVATE cxx_std_20)

add_custom_command(TARGET AnimationView POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${ANIMATIONVIEW_ROOT}/shaders
        $<TARGET_FILE_DIR:AnimationView>/shaders
)

else()
    message(WARNING "Animation: Vulkan and/or GLFW loader not found -- AnimationRenderer/AnimationView are skipped.")
endif()

else()
    message(WARNING "Animation: vulkan/vulkan.h not found -- AnimationRenderer/AnimationView are skipped.")
endif()
