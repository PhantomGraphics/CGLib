# GltfRenderer core library (Phantom::Gltf / Phantom::Vrm -- glTF/VRM loading and rendering), its GoogleTest suite, and the GltfViewer app.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/GltfRenderer -B build/GltfRenderer -DCMAKE_BUILD_TYPE=Debug && cmake --build build/GltfRenderer
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake / cmake/PhantomVulkanApp.cmake;
# this file adds the module's test / app targets on top. See docs/standalone-repository-plan.md.

cmake_minimum_required(VERSION 3.20)

project(GltfRendererStandalone LANGUAGES CXX)

# Matches GltfRenderer.vcxproj/GltfViewer.vcxproj (<LanguageStandard>stdcpp20
# -- VkAppBase.cpp needs std::string_view::starts_with).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(GLTFR_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()
set(GLTFVIEWER_ROOT ${CGLIB_ROOT}/GltfViewer)
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
include(PhantomGTest)
include(PhantomCoreLibs)
include(PhantomVulkanApp)

phantom_find_gtest()

phantom_find_vulkan_headers()

if(PHANTOM_VULKAN_INCLUDE_DIR)

phantom_find_vulkan_loader()
phantom_find_glfw_loader()

if(PHANTOM_VULKAN_LIBRARY AND PHANTOM_GLFW_LIBRARY)

phantom_add_vkappbase_core()
phantom_add_gltfrenderer_core()

# --- GltfRendererTest (GoogleTest) ---

if(PHANTOM_GTEST_FOUND)
    enable_testing()

    add_executable(GltfRendererTest
        ${GLTFR_ROOT}/GltfRendererTest/ObjToGltfConverterTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/StlToGltfConverterTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/MmdAnimationBakerTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/GltfAnimationEvaluatorTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/SkeletonGltfConverterTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/MmdToGltfConverterTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/GLTFFileReaderMorphTargetTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/GLTFFileReaderNodeMatrixTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/VrmExtensionParserTest.cpp
        ${GLTFR_ROOT}/GltfRendererTest/VrmReaderTest.cpp
    )
    target_include_directories(GltfRendererTest PRIVATE
        ${GLTFR_ROOT}/GltfRendererTest
    )
    target_link_libraries(GltfRendererTest PRIVATE
        GltfRendererCore
        GTest::gtest
        GTest::gtest_main
        ${PHANTOM_VULKAN_LIBRARY}
    )
    target_compile_options(GltfRendererTest PRIVATE ${PHANTOM_WARN_FLAGS})

    include(GoogleTest)
    gtest_discover_tests(GltfRendererTest)
else()
    message(WARNING "GTest not found -- skipping GltfRendererTest target")
endif()

# ---------------------------------------------------------------------------
# GltfViewer (Phantom::Gltf app) -- standalone ImGui + Vulkan glTF/VRM viewer
# ---------------------------------------------------------------------------

add_executable(GltfViewer
    ${GLTFVIEWER_ROOT}/main.cpp
    ${GLTFVIEWER_ROOT}/CommandDispatcher.cpp
    ${GLTFVIEWER_ROOT}/App.cpp
    ${GLTFVIEWER_ROOT}/ControlPanel.cpp
)
target_include_directories(GltfViewer PRIVATE
    ${CGLIB_ROOT}/UIWidgets
)
target_link_libraries(GltfViewer PRIVATE
    GltfRendererCore
    VkAppBaseCore
    ${PHANTOM_VULKAN_LIBRARY}
    ${PHANTOM_GLFW_LIBRARY}
)
target_compile_options(GltfViewer PRIVATE ${PHANTOM_WARN_FLAGS})

# loadSPVRepo() resolves "shaders/*.spv" relative to the binary's own
# directory on Windows, and to CWD on Linux -- mirror Windows' PostBuildEvent
# xcopy so the binary finds its shaders either way.
add_custom_command(TARGET GltfViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${GLTFVIEWER_ROOT}/shaders
        $<TARGET_FILE_DIR:GltfViewer>/shaders
)

else()
    message(WARNING "GltfRenderer: Vulkan and/or GLFW loader not found -- GltfRendererCore/GltfRendererTest/GltfViewer are all skipped.")
endif()

else()
    message(WARNING "GltfRenderer: vulkan/vulkan.h not found -- GltfRendererCore/GltfRendererTest/GltfViewer are all skipped (they all need Vulkan, see file header). Pass -DVULKAN_INCLUDE_DIR=/path/to/include to enable them.")
endif()
