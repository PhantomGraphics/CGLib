# MeshEdit core library (Phantom::MeshEdit -- CPU-only polygon mesh editing) and its GoogleTest suite.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/MeshEdit -B build/MeshEdit -DCMAKE_BUILD_TYPE=Debug && cmake --build build/MeshEdit
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake; this file adds the module's test target on top.

cmake_minimum_required(VERSION 3.20)

project(MeshEditStandalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(MESHEDIT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
include(PhantomGTest)
include(PhantomCoreLibs)
include(PhantomVulkanApp)

phantom_add_meshedit_core()

# MeshEditDemoCore: the CPU side of MeshEditView (the world -- mesh, selection, history, draw data -- and the command router).
# No ImGui and no Vulkan, so MeshEditTest can check what the viewer does without a window and MeshEditView shares it.
if(NOT TARGET MeshEditDemoCore)
    add_library(MeshEditDemoCore STATIC
        ${MESHEDIT_ROOT}/MeshEditView/MeshWorld.cpp
        ${MESHEDIT_ROOT}/MeshEditView/CommandDispatcher.cpp
    )
    # CommandDispatcher.h uses VkAppBase's header-only scenario interfaces (no Vulkan symbols).
    target_include_directories(MeshEditDemoCore PUBLIC ${REPO_ROOT} ${CGLIB_ROOT}/ThirdParty/glm)
    target_link_libraries(MeshEditDemoCore PUBLIC MeshEditCore)
    target_compile_options(MeshEditDemoCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(MeshEditDemoCore PUBLIC cxx_std_20)
endif()

# --- MeshEditTest (GoogleTest) ---

phantom_find_gtest()

if(PHANTOM_GTEST_FOUND)
    enable_testing()

    cglib_add_test(MeshEditTest
        SOURCES
            ${MESHEDIT_ROOT}/MeshEditTest/MeshEditPickingTest.cpp
            ${MESHEDIT_ROOT}/MeshEditTest/MeshEditStateTest.cpp
            ${MESHEDIT_ROOT}/MeshEditTest/MeshBooleanTest.cpp
            ${MESHEDIT_ROOT}/MeshEditTest/EditableMeshTest.cpp
            ${MESHEDIT_ROOT}/MeshEditTest/EditableMeshConvertTest.cpp
            ${MESHEDIT_ROOT}/MeshEditTest/MeshEditOperationsTest.cpp
            ${MESHEDIT_ROOT}/MeshEditTest/MeshEditViewTest.cpp
        INCLUDES ${MESHEDIT_ROOT}/MeshEditTest
        LINK MeshEditCore MeshEditDemoCore
    )
else()
    message(STATUS "GTest unavailable or tests disabled -- skipping MeshEditTest target")
endif()

# ---------------------------------------------------------------------------
# MeshEditView (MeshEditApp : VkAppBase) -- standalone ImGui + Vulkan viewer
# ---------------------------------------------------------------------------

phantom_find_vulkan_headers()

if(PHANTOM_VULKAN_INCLUDE_DIR)

phantom_find_vulkan_loader()
phantom_find_glfw_loader()

if(PHANTOM_VULKAN_LIBRARY AND PHANTOM_GLFW_LIBRARY)

phantom_add_vkappbase_core()
phantom_add_vkrenderer_core()

# Viewer executables are opt-out only for the CGLib top-level (CGLIB_BUILD_VIEWERS); a module
# configured on its own leaves the variable undefined and keeps building its viewer.
if(NOT DEFINED CGLIB_BUILD_VIEWERS OR CGLIB_BUILD_VIEWERS)
add_executable(MeshEditView
    ${MESHEDIT_ROOT}/MeshEditView/main.cpp
    ${MESHEDIT_ROOT}/MeshEditView/MeshEditApp.cpp
    ${MESHEDIT_ROOT}/MeshEditView/MeshMenuPanel.cpp
    ${MESHEDIT_ROOT}/MeshEditView/Renderer.cpp
)
target_include_directories(MeshEditView PRIVATE
    ${REPO_ROOT}
    ${CGLIB_ROOT}/UIWidgets
    ${CGLIB_ROOT}/Renderer/VkRenderer
)
target_link_libraries(MeshEditView PRIVATE
    MeshEditDemoCore
    MeshEditCore
    VkAppBaseCore
    VkRendererCore
    ${PHANTOM_VULKAN_LIBRARY}
    ${PHANTOM_GLFW_LIBRARY}
)
target_compile_options(MeshEditView PRIVATE ${PHANTOM_WARN_FLAGS})

# loadSPVRepo() resolves "shaders/*.spv" relative to the running binary's directory on Windows and to the current working
# directory elsewhere -- copy the shaders next to the binary (the same as the other viewers).
add_custom_command(TARGET MeshEditView POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${MESHEDIT_ROOT}/MeshEditView/shaders
        $<TARGET_FILE_DIR:MeshEditView>/shaders
)
endif()

else()
    phantom_gpu_target_skipped("MeshEdit: Vulkan and/or GLFW loader not found -- MeshEditView is skipped (MeshEdit/MeshEditTest still build). Pass -DVULKAN_LIBRARY=... -DGLFW_LIBRARY=... to enable it.")
endif()

endif()
