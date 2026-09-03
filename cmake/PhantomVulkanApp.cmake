# Idempotent Vulkan/GLFW discovery + Vulkan-dependent core-library builders for
# CGLib's GPU/UI modules, shared by the top-level CGLib/CMakeLists.txt and each
# module's own CMakeLists.txt. Include cmake/PhantomCoreLibs.cmake first --
# phantom_add_uiwidgets_core()/phantom_add_gltfrenderer_core()/
# phantom_add_volumerenderer_core() depend on its Math/Graphics/Space/Volume/
# File/Animation builders.
#
# Callers must set CGLIB_ROOT/REPO_ROOT/PHANTOM_WARN_FLAGS (include(CGLibCommon)
# does) before calling anything here. When CGLIB_ENABLE_VULKAN is defined and
# false, phantom_find_vulkan_headers() resolves to "not found" without
# searching, so every `if(PHANTOM_VULKAN_INCLUDE_DIR)` gate downstream skips
# cleanly -- that is how the CPU-only build drops the whole GPU/UI subtree.
#
# phantom_find_vulkan_headers()/phantom_find_vulkan_loader()/
# phantom_find_glfw_loader() each resolve once per configure (guarded by a
# CACHE INTERNAL entry) and publish:
#   PHANTOM_VULKAN_INCLUDE_DIR  -- directory containing vulkan/vulkan.h, or "" if not found
#   PHANTOM_VULKAN_LIBRARY      -- path to the Vulkan loader, or "" if not found
#   PHANTOM_GLFW_LIBRARY        -- path to a GLFW loader, or "" if not found
# Every add_subdirectory()'d module that needs Vulkan/GLFW should call these
# and gate its own targets on the result being non-empty, exactly like every
# pre-Phase-4 CMakeLists.txt already did with its own locally-prefixed copy of
# this same discovery logic -- must be a standalone/system Vulkan+GLFW
# install, never an app-bundled copy (e.g. Blender's). Only
# phantom_find_vulkan_headers() is required to build the *Core libraries
# below (compile-only); the loader/GLFW discovery is only needed to link an
# actual executable.

function(phantom_find_vulkan_headers)
    if(DEFINED CACHE{PHANTOM_VULKAN_INCLUDE_DIR})
        return()
    endif()
    if(DEFINED CGLIB_ENABLE_VULKAN AND NOT CGLIB_ENABLE_VULKAN)
        set(PHANTOM_VULKAN_INCLUDE_DIR "" CACHE INTERNAL "Resolved Vulkan headers directory, empty if not found")
        return()
    endif()
    set(_default "")
    if(WIN32 AND DEFINED ENV{VULKAN_SDK})
        set(_default "$ENV{VULKAN_SDK}/Include")
    endif()
    set(VULKAN_INCLUDE_DIR "${_default}" CACHE PATH "Directory containing vulkan/vulkan.h (defaults to $ENV{VULKAN_SDK}/Include on Windows; on Linux, e.g. a Vulkan-Headers checkout's include/, or a system libvulkan-dev's /usr/include). Must NOT be an app-bundled copy (e.g. Blender's).")

    set(_resolved "")
    if(VULKAN_INCLUDE_DIR)
        if(EXISTS "${VULKAN_INCLUDE_DIR}/vulkan/vulkan.h")
            set(_resolved ${VULKAN_INCLUDE_DIR})
        endif()
    else()
        find_path(_found_vulkan_headers NAMES vulkan/vulkan.h)
        set(_resolved ${_found_vulkan_headers})
    endif()

    if(_resolved)
        message(STATUS "Phantom: using Vulkan headers from ${_resolved}")
    else()
        message(WARNING "Phantom: vulkan/vulkan.h not found -- Vulkan-dependent targets are skipped. Pass -DVULKAN_INCLUDE_DIR=/path/to/include to enable them (on Windows, check $ENV{VULKAN_SDK} is set).")
    endif()
    set(PHANTOM_VULKAN_INCLUDE_DIR "${_resolved}" CACHE INTERNAL "Resolved Vulkan headers directory, empty if not found")
endfunction()

function(phantom_find_vulkan_loader)
    if(DEFINED CACHE{PHANTOM_VULKAN_LIBRARY})
        return()
    endif()
    set(_default "")
    if(WIN32 AND DEFINED ENV{VULKAN_SDK})
        set(_default "$ENV{VULKAN_SDK}/Lib/vulkan-1.lib")
    endif()
    set(VULKAN_LIBRARY "${_default}" CACHE FILEPATH "Path to the Vulkan loader (defaults to $ENV{VULKAN_SDK}/Lib/vulkan-1.lib on Windows). Must be the system/SDK loader, not an app-bundled copy.")

    if(NOT VULKAN_LIBRARY)
        find_library(_found_vulkan_lib NAMES vulkan libvulkan.so.1 PATHS /usr/lib/x86_64-linux-gnu /usr/lib)
        set(_found ${_found_vulkan_lib})
    else()
        set(_found ${VULKAN_LIBRARY})
    endif()
    set(PHANTOM_VULKAN_LIBRARY "${_found}" CACHE INTERNAL "Resolved Vulkan loader path, empty if not found")
endfunction()

function(phantom_find_glfw_loader)
    if(DEFINED CACHE{PHANTOM_GLFW_LIBRARY})
        return()
    endif()
    set(_default "")
    if(WIN32)
        set(_default "${CGLIB_ROOT}/ThirdParty/glfw-3.3.8/lib-vc2019/glfw3.lib")
    endif()
    set(GLFW_LIBRARY "${_default}" CACHE FILEPATH "Path to a GLFW loader (defaults to the repo-vendored CGLib/ThirdParty/glfw-3.3.8/lib-vc2019/glfw3.lib on Windows; on Linux there is no vendored .so -- e.g. `apt-get download libglfw3` + dpkg-deb -x).")

    if(NOT GLFW_LIBRARY)
        find_library(_found_glfw_lib NAMES glfw glfw3 libglfw.so.3 PATHS /usr/lib/x86_64-linux-gnu /usr/lib)
        set(_found ${_found_glfw_lib})
    else()
        set(_found ${GLFW_LIBRARY})
    endif()
    set(PHANTOM_GLFW_LIBRARY "${_found}" CACHE INTERNAL "Resolved GLFW loader path, empty if not found")
endfunction()

# ---------------------------------------------------------------------------
# VulkanGraphics (Phantom::VKG) -- depends on Vulkan headers + vendored glm/VMA
# ---------------------------------------------------------------------------

function(phantom_add_vulkangraphics_core)
    if(TARGET VulkanGraphicsCore)
        return()
    endif()
    file(GLOB _sources ${CGLIB_ROOT}/VulkanGraphics/*.cpp)
    add_library(VulkanGraphicsCore STATIC ${_sources})
    target_include_directories(VulkanGraphicsCore PUBLIC
        ${CGLIB_ROOT}
        ${PHANTOM_VULKAN_INCLUDE_DIR}
        ${CGLIB_ROOT}/ThirdParty/glm-0.9.9.8
        ${CGLIB_ROOT}/ThirdParty/VulkanMemoryAllocator
    )
    target_compile_options(VulkanGraphicsCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(VulkanGraphicsCore PRIVATE cxx_std_20)
endfunction()

# ---------------------------------------------------------------------------
# UIWidgets (Phantom::UI) -- ImGui core+backends, depends on Graphics + Vulkan/GLFW headers
# ---------------------------------------------------------------------------

function(phantom_add_uiwidgets_core)
    if(TARGET UIWidgetsCore)
        return()
    endif()
    phantom_add_graphics_core()
    set(_imgui_dir ${CGLIB_ROOT}/ThirdParty/imgui)
    add_library(UIWidgetsCore STATIC
        ${CGLIB_ROOT}/UIWidgets/BoolView.cpp
        ${CGLIB_ROOT}/UIWidgets/Box3dView.cpp
        ${CGLIB_ROOT}/UIWidgets/Button.cpp
        ${CGLIB_ROOT}/UIWidgets/Circle3dView.cpp
        ${CGLIB_ROOT}/UIWidgets/ColorMapView.cpp
        ${CGLIB_ROOT}/UIWidgets/ComboBox.cpp
        ${CGLIB_ROOT}/UIWidgets/Cylinder3dView.cpp
        ${CGLIB_ROOT}/UIWidgets/DirectoryView.cpp
        ${CGLIB_ROOT}/UIWidgets/Ellipse3dView.cpp
        ${CGLIB_ROOT}/UIWidgets/Ellipsoid3dView.cpp
        ${CGLIB_ROOT}/UIWidgets/FileOpenDialog.cpp
        ${CGLIB_ROOT}/UIWidgets/FileOpenView.cpp
        ${CGLIB_ROOT}/UIWidgets/FileSaveDialog.cpp
        ${CGLIB_ROOT}/UIWidgets/FileSaveView.cpp
        ${CGLIB_ROOT}/UIWidgets/Float4View.cpp
        ${CGLIB_ROOT}/UIWidgets/FloatView.cpp
        ${CGLIB_ROOT}/UIWidgets/IMenu.cpp
        ${CGLIB_ROOT}/UIWidgets/IMenuItem.cpp
        ${CGLIB_ROOT}/UIWidgets/IntView.cpp
        ${CGLIB_ROOT}/UIWidgets/Line3dView.cpp
        ${CGLIB_ROOT}/UIWidgets/Matrix2dView.cpp
        ${CGLIB_ROOT}/UIWidgets/Matrix3dView.cpp
        ${CGLIB_ROOT}/UIWidgets/Matrix4dView.cpp
        ${CGLIB_ROOT}/UIWidgets/Panel.cpp
        ${CGLIB_ROOT}/UIWidgets/Ray3dView.cpp
        ${CGLIB_ROOT}/UIWidgets/Rect3dView.cpp
        ${CGLIB_ROOT}/UIWidgets/Sphere3dView.cpp
        ${CGLIB_ROOT}/UIWidgets/StringView.cpp
        ${CGLIB_ROOT}/UIWidgets/tinyfiledialogs.cpp
        ${CGLIB_ROOT}/UIWidgets/Vector3dView.cpp
        ${_imgui_dir}/imgui.cpp
        ${_imgui_dir}/imgui_draw.cpp
        ${_imgui_dir}/imgui_widgets.cpp
        ${_imgui_dir}/imgui_tables.cpp
        ${_imgui_dir}/imgui_stdlib.cpp
        ${_imgui_dir}/backends/imgui_impl_glfw.cpp
        ${_imgui_dir}/backends/imgui_impl_vulkan.cpp
    )
    target_include_directories(UIWidgetsCore PUBLIC
        ${CGLIB_ROOT}
        ${CGLIB_ROOT}/Graphics
        ${_imgui_dir}
        ${CGLIB_ROOT}/ThirdParty/glfw-3.3.8/include
        ${PHANTOM_VULKAN_INCLUDE_DIR}
    )
    target_link_libraries(UIWidgetsCore PUBLIC GraphicsCore MathCore)
    target_compile_options(UIWidgetsCore PRIVATE ${PHANTOM_WARN_FLAGS})
    # Linux has no GL/gl.h without mesa dev packages; imgui_impl_glfw.cpp
    # doesn't request a client API itself, so glfw3.h defaults to pulling it
    # in unless told not to. Every app in this repo is Vulkan-only -- kept
    # PUBLIC (not PRIVATE) so consuming executables that #include <GLFW/
    # glfw3.h> directly inherit the same definition, instead of each one
    # re-declaring it themselves as every pre-Phase-4 copy of this block did.
    target_compile_definitions(UIWidgetsCore PUBLIC GLFW_INCLUDE_NONE)
    target_compile_features(UIWidgetsCore PRIVATE cxx_std_20)
endfunction()

# ---------------------------------------------------------------------------
# VkAppBase (Phantom::VKG app scaffold: window/swapchain/ImGui glue + ScenarioRunner)
# ---------------------------------------------------------------------------

function(phantom_add_vkappbase_core)
    if(TARGET VkAppBaseCore)
        return()
    endif()
    phantom_add_vulkangraphics_core()
    phantom_add_uiwidgets_core()
    add_library(VkAppBaseCore STATIC
        ${CGLIB_ROOT}/VkAppBase/VkAppBase.cpp
        ${CGLIB_ROOT}/VkAppBase/VkRendererBase.cpp
        ${CGLIB_ROOT}/VkAppBase/VulkanWindow.cpp
        ${CGLIB_ROOT}/VkAppBase/ScenarioRunner/ScenarioRunner.cpp
        ${CGLIB_ROOT}/VkAppBase/ScenarioRunner/ScenarioBrowserPanel.cpp
    )
    target_include_directories(VkAppBaseCore PUBLIC
        ${REPO_ROOT}
        ${PHANTOM_VULKAN_INCLUDE_DIR}
        ${CGLIB_ROOT}/ThirdParty/glm-0.9.9.8
        ${CGLIB_ROOT}/ThirdParty/glfw-3.3.8/include
        ${CGLIB_ROOT}/VulkanGraphics
        ${CGLIB_ROOT}/ThirdParty/imgui
        ${CGLIB_ROOT}/ThirdParty
        ${CGLIB_ROOT}/VkAppBase
    )
    target_link_libraries(VkAppBaseCore PUBLIC VulkanGraphicsCore UIWidgetsCore)
    target_compile_options(VkAppBaseCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(VkAppBaseCore PRIVATE cxx_std_20)
endfunction()

# ---------------------------------------------------------------------------
# VkRenderer (Phantom::VKR line/point/triangle/tex/skybox renderers)
# ---------------------------------------------------------------------------

function(phantom_add_vkrenderer_core)
    if(TARGET VkRendererCore)
        return()
    endif()
    phantom_add_vulkangraphics_core()
    add_library(VkRendererCore STATIC
        ${CGLIB_ROOT}/Renderer/VkRenderer/VkPointRenderer.cpp
        ${CGLIB_ROOT}/Renderer/VkRenderer/VkLineRenderer.cpp
        ${CGLIB_ROOT}/Renderer/VkRenderer/VkTriangleRenderer.cpp
        ${CGLIB_ROOT}/Renderer/VkRenderer/VkTexRenderer.cpp
        ${CGLIB_ROOT}/Renderer/VkRenderer/VkSkyBoxRenderer.cpp
    )
    target_include_directories(VkRendererCore PUBLIC
        ${REPO_ROOT}
        ${CGLIB_ROOT}/ThirdParty/imgui
        ${PHANTOM_VULKAN_INCLUDE_DIR}
        ${CGLIB_ROOT}/ThirdParty/glm-0.9.9.8
        ${CGLIB_ROOT}/ThirdParty/glfw-3.3.8/include
        ${CGLIB_ROOT}/VkAppBase
        ${CGLIB_ROOT}/VulkanGraphics
        ${CGLIB_ROOT}/UIWidgets
        ${CGLIB_ROOT}/Renderer/VkRenderer
    )
    target_link_libraries(VkRendererCore PUBLIC VulkanGraphicsCore)
    target_compile_options(VkRendererCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(VkRendererCore PRIVATE cxx_std_20)
endfunction()

# ---------------------------------------------------------------------------
# GltfRendererCore (Phantom::Gltf / Phantom::Vrm) -- needs File+Animation+the
# Vulkan/VkAppBase chain (see the CGLib/GltfRenderer/CMakeLists.txt file
# header this was extracted from for why GltfRendererTest can't be built
# Vulkan-independently: GltfSceneRenderer.cpp/ShadowMapPass.cpp/
# LightManager.cpp use Phantom::VKG types directly).
# ---------------------------------------------------------------------------

function(phantom_add_gltfrenderer_core)
    if(TARGET GltfRendererCore)
        return()
    endif()
    phantom_add_file_core()
    phantom_add_animation_core()
    phantom_add_vulkangraphics_core()
    phantom_add_vkappbase_core()
    set(_gltfr_root ${CGLIB_ROOT}/GltfRenderer)
    add_library(GltfRendererCore STATIC
        ${_gltfr_root}/Gltf/GltfReader.cpp
        ${_gltfr_root}/Gltf/GltfAccessorView.cpp
        ${_gltfr_root}/Gltf/GltfBounds.cpp
        ${_gltfr_root}/Gltf/SkeletonGltfConverter.cpp
        ${_gltfr_root}/Gltf/MmdAnimationBaker.cpp
        ${_gltfr_root}/Gltf/GltfAnimationEvaluator.cpp
        ${_gltfr_root}/Gltf/MmdToGltfConverter.cpp
        ${_gltfr_root}/Gltf/GltfMorphApply.cpp
        ${_gltfr_root}/Gltf/ObjToGltfConverter.cpp
        ${_gltfr_root}/Gltf/StlToGltfConverter.cpp
        ${_gltfr_root}/Vrm/VrmExtensionParser.cpp
        ${_gltfr_root}/Vrm/VrmMToonFallback.cpp
        ${_gltfr_root}/Vrm/VrmReader.cpp
        ${_gltfr_root}/IBL/GltfIBLPrecomputer.cpp
        ${_gltfr_root}/Renderer/GltfMesh.cpp
        ${_gltfr_root}/Renderer/GltfMaterial.cpp
        ${_gltfr_root}/Renderer/GltfSceneRenderer.cpp
        ${_gltfr_root}/Renderer/ShadowMapPass.cpp
        ${_gltfr_root}/Renderer/LightManager.cpp
    )
    target_include_directories(GltfRendererCore PUBLIC
        ${REPO_ROOT}
        ${CGLIB_ROOT}/ThirdParty/glm-0.9.9.8
        ${CGLIB_ROOT}/ThirdParty/nlohmann
        ${CGLIB_ROOT}/ThirdParty/stb
        ${CGLIB_ROOT}/ThirdParty/glfw-3.3.8/include
        ${CGLIB_ROOT}/VkAppBase
        ${CGLIB_ROOT}/VulkanGraphics
        ${PHANTOM_VULKAN_INCLUDE_DIR}
    )
    target_link_libraries(GltfRendererCore PUBLIC
        FileCore
        AnimationCore
        VulkanGraphicsCore
        VkAppBaseCore
        MathCore
        GraphicsCore
    )
    target_compile_options(GltfRendererCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(GltfRendererCore PRIVATE cxx_std_20)
endfunction()

# ---------------------------------------------------------------------------
# VolumeRenderer (Phantom::VKR PBVR particle/opacity-shadow-map pipeline) --
# shared between CGLib/Volume's own VolumeView and PointCloud/GSView (which
# used to duplicate this as GSViewVolumeCore/GSViewVolumeRenderer).
# ---------------------------------------------------------------------------

function(phantom_add_volumerenderer_core)
    if(TARGET VolumeRenderer)
        return()
    endif()
    phantom_add_volume_core()
    phantom_add_vulkangraphics_core()
    add_library(VolumeRenderer STATIC
        ${CGLIB_ROOT}/Volume/VolumeRenderer/ParticleGenerator.cpp
        ${CGLIB_ROOT}/Volume/VolumeRenderer/TransferFunction.cpp
        ${CGLIB_ROOT}/Volume/VolumeRenderer/PBVRPipeline.cpp
        ${CGLIB_ROOT}/Volume/VolumeRenderer/PBVRRenderer.cpp
        ${CGLIB_ROOT}/Volume/VolumeRenderer/VolumeComputePBVR.cpp
        ${CGLIB_ROOT}/Volume/VolumeRenderer/OpacityShadowMapPass.cpp
    )
    target_include_directories(VolumeRenderer PUBLIC
        ${REPO_ROOT}
        ${CGLIB_ROOT}/ThirdParty/glm-0.9.9.8
        ${PHANTOM_VULKAN_INCLUDE_DIR}
    )
    target_link_libraries(VolumeRenderer PUBLIC VulkanGraphicsCore VolumeCore)
    target_compile_options(VolumeRenderer PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(VolumeRenderer PRIVATE cxx_std_20)
endfunction()
