# Animation core library (Phantom::Animation -- skeleton / IK / morph / MMD animation), its GoogleTest suite, and the AnimationRenderer + AnimationView Vulkan app.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/Animation -B build/Animation -DCMAKE_BUILD_TYPE=Debug && cmake --build build/Animation
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake / cmake/PhantomVulkanApp.cmake;
# this file adds the module's test / app targets on top. See docs/standalone-repository-plan.md.

cmake_minimum_required(VERSION 3.20)

project(AnimationCoreLinux LANGUAGES CXX)

# Animation.vcxproj explicitly sets <LanguageStandard>stdcpp17</LanguageStandard>
# (unlike File/GltfRenderer/the Vulkan app chain, which need C++20) -- matches
# the repo-wide BuildConfig.props default, kept explicit here per plan section
# 3 item 6's "no ambiguous unspecified standard" policy.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ANIMATION_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
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

elseif(NOT DEFINED CGLIB_ENABLE_VULKAN OR CGLIB_ENABLE_VULKAN)
    message(WARNING "Animation: vulkan/vulkan.h not found -- AnimationRenderer/AnimationView are skipped.")
endif()
