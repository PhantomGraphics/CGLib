# Idempotent, GUI/Vulkan-independent core-library builders for CGLib's CPU-only
# modules (Math/Graphics/Numerics/Space/Scene/Volume/File/Animation), shared by
# the top-level CGLib/CMakeLists.txt and by each module's own CMakeLists.txt
# (which is also usable standalone as `cmake -S CGLib/<Module> -B ...`).
#
# Each phantom_add_*_core() function is a no-op if its target already exists
# (`if(TARGET ...) return()` guard), so calling the same function from
# multiple add_subdirectory()'d modules in one configure is safe -- whichever
# module is processed first actually creates the target; every later caller
# just reuses it via target_link_libraries. A module that needs another
# module's core lib (e.g. VolumeCore needs SpaceCore) just calls that module's
# phantom_add_*_core() directly instead of duplicating its source list.
#
# Callers must set these before calling anything here (include(CGLibCommon)
# sets all three -- see cmake/CGLibCommon.cmake):
#   CGLIB_ROOT          -- absolute path to the CGLib source root
#   REPO_ROOT           -- include root under which "CGLib/..." resolves
#   PHANTOM_WARN_FLAGS  -- /W3 (MSVC) or -Wall (else)
#
# Every target here is promoted to C++20 via target_compile_features. The
# standalone build's minimum standard is C++20 (VkAppBase.cpp uses
# std::string_view::starts_with, and the portable geometry/IO/animation code
# compiles identically under C++20); the per-target feature is kept so these
# libraries stay correct even if included from a project that defaults lower.
#
# Include directories are PUBLIC so a consumer only needs
# target_link_libraries(... X) to inherit X's include paths transitively.

function(phantom_add_math_core)
    if(TARGET MathCore)
        return()
    endif()
    file(GLOB _math_sources CONFIGURE_DEPENDS ${CGLIB_ROOT}/Math/Core/*.cpp)
    list(FILTER _math_sources EXCLUDE REGEX "pch\\.cpp$")
    add_library(MathCore STATIC ${_math_sources})
    target_include_directories(MathCore PUBLIC ${CGLIB_ROOT} ${REPO_ROOT})
    target_compile_options(MathCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(MathCore PUBLIC cxx_std_20)
endfunction()

function(phantom_add_graphics_core)
    if(TARGET GraphicsCore)
        return()
    endif()
    phantom_add_math_core()
    file(GLOB _graphics_sources CONFIGURE_DEPENDS ${CGLIB_ROOT}/Graphics/Core/*.cpp)
    list(FILTER _graphics_sources EXCLUDE REGEX "pch\\.cpp$")
    add_library(GraphicsCore STATIC ${_graphics_sources})
    target_include_directories(GraphicsCore PUBLIC ${CGLIB_ROOT} ${REPO_ROOT})
    target_link_libraries(GraphicsCore PUBLIC MathCore)
    target_compile_options(GraphicsCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(GraphicsCore PUBLIC cxx_std_20)
endfunction()

function(phantom_add_numerics_core)
    if(TARGET NumericsCore)
        return()
    endif()
    phantom_add_math_core()
    add_library(NumericsCore STATIC
        ${CGLIB_ROOT}/Numerics/Core/Converter.cpp
        ${CGLIB_ROOT}/Numerics/Core/SVD2d.cpp
        ${CGLIB_ROOT}/Numerics/Core/SVD3d.cpp
    )
    target_include_directories(NumericsCore PUBLIC ${REPO_ROOT})
    # Converter.h/SVD*.h take and return Math's Vector*/Matrix* types.
    target_link_libraries(NumericsCore PUBLIC MathCore)
    target_compile_options(NumericsCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(NumericsCore PUBLIC cxx_std_20)
endfunction()

function(phantom_add_space_core)
    if(TARGET SpaceCore)
        return()
    endif()
    phantom_add_math_core()
    file(GLOB _space_sources CONFIGURE_DEPENDS ${CGLIB_ROOT}/Space/Core/*.cpp)
    list(FILTER _space_sources EXCLUDE REGEX "pch\\.cpp$")
    add_library(SpaceCore STATIC ${_space_sources})
    target_include_directories(SpaceCore PUBLIC ${REPO_ROOT})
    target_link_libraries(SpaceCore PUBLIC MathCore)
    target_compile_options(SpaceCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(SpaceCore PUBLIC cxx_std_20)

    # IndexedSortBasedSearcher.cpp's #pragma omp parallel for is silently a
    # no-op without this -- unlike Physics/PointCloud's own CMakeLists.txt
    # (which each find_package(OpenMP) themselves), SpaceCore previously had
    # no OpenMP link of its own, so the CMake build's neighbor search ran
    # fully serial even though the same source compiles multithreaded in the
    # MSBuild/Blender-addon build (docs/issue/wcsph_parallel_scaling_profile.md
    # section 7).
    find_package(OpenMP)
    # PRIVATE: no SpaceCore header uses OpenMP (only the .cpp pragma). For a static
    # library the link dependency still reaches consumers as $<LINK_ONLY:...>.
    if(OpenMP_CXX_FOUND)
        target_link_libraries(SpaceCore PRIVATE OpenMP::OpenMP_CXX)
    endif()
endfunction()


function(phantom_add_volume_core)
    if(TARGET VolumeCore)
        return()
    endif()
    phantom_add_math_core()
    phantom_add_space_core()
    add_library(VolumeCore STATIC
        ${CGLIB_ROOT}/Volume/Core/LevelSet.cpp
        ${CGLIB_ROOT}/Volume/Core/MCSurfaceBuilder.cpp
        ${CGLIB_ROOT}/Volume/Core/SurfaceVoxelizer.cpp
        ${CGLIB_ROOT}/Volume/Core/Volume.cpp
        ${CGLIB_ROOT}/Volume/Core/VolumeNode.cpp
        ${CGLIB_ROOT}/Volume/Core/VolumeScattering.cpp
    )
    target_include_directories(VolumeCore PUBLIC ${REPO_ROOT})
    target_link_libraries(VolumeCore PUBLIC MathCore SpaceCore)
    target_compile_options(VolumeCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(VolumeCore PUBLIC cxx_std_20)
endfunction()

function(phantom_add_file_core)
    if(TARGET FileCore)
        return()
    endif()
    phantom_add_math_core()
    phantom_add_graphics_core()
    # NOTE on File's on-disk source list: File/Core/ also contains File.cpp, a
    # never-wired-up leftover from the original VS "static library" project
    # template (a stub fnFile() nobody calls) -- confirmed NOT part of File.
    # vcxproj's <ClCompile> list. Do NOT file(GLOB) this directory -- the
    # explicit list below matches File.vcxproj exactly. cgltf (GltfFileReader/
    # Writer) is a single-header library under File/ThirdParty/cgltf, included
    # via the repo-root-relative path "CGLib/File/ThirdParty/cgltf/cgltf.h"
    # -- no extra include dir needed beyond REPO_ROOT (already PUBLIC below).
    add_library(FileCore STATIC
        ${CGLIB_ROOT}/File/Core/BVHFileReader.cpp
        ${CGLIB_ROOT}/File/Core/GltfFileReader.cpp
        ${CGLIB_ROOT}/File/Core/GltfFileWriter.cpp
        ${CGLIB_ROOT}/File/Core/MTLFileReader.cpp
        ${CGLIB_ROOT}/File/Core/MTLFileWriter.cpp
        ${CGLIB_ROOT}/File/Core/OBJFileReader.cpp
        ${CGLIB_ROOT}/File/Core/OBJFileWriter.cpp
        ${CGLIB_ROOT}/File/Core/OBJSyntaxParser.cpp
        ${CGLIB_ROOT}/File/Core/PLYFileReader.cpp
        ${CGLIB_ROOT}/File/Core/PLYFileWriter.cpp
        ${CGLIB_ROOT}/File/Core/PMDFileReader.cpp
        ${CGLIB_ROOT}/File/Core/PMXFileReader.cpp
        ${CGLIB_ROOT}/File/Core/STLFileReader.cpp
        ${CGLIB_ROOT}/File/Core/STLFileWriter.cpp
        ${CGLIB_ROOT}/File/Core/VMDFileReader.cpp
    )
    target_include_directories(FileCore PUBLIC ${REPO_ROOT})
    target_link_libraries(FileCore PUBLIC MathCore GraphicsCore)
    target_compile_options(FileCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(FileCore PUBLIC cxx_std_20)
endfunction()

function(phantom_add_geometrynode_core)
    if(TARGET GeometryNodeCore)
        return()
    endif()
    # Node-graph geometry generation (Phantom::GeometryNode -- CPU-only DAG
    # evaluation of mesh nodes, docs/todo/PLAN_geometry_node.md). No Math/Vulkan/
    # Studio dependency; the graph JSON uses the vendored header-only nlohmann/json
    # (reached by a relative include from GraphTypes.h).
    add_library(GeometryNodeCore STATIC
        ${CGLIB_ROOT}/GeometryNode/Core/GeometryOps.cpp
        ${CGLIB_ROOT}/GeometryNode/Core/TerrainOps.cpp
        ${CGLIB_ROOT}/GeometryNode/Core/GraphAsset.cpp
        ${CGLIB_ROOT}/GeometryNode/Core/GraphGroup.cpp
        ${CGLIB_ROOT}/GeometryNode/Core/GraphTypes.cpp
        ${CGLIB_ROOT}/GeometryNode/Core/NodeRegistry.cpp
        ${CGLIB_ROOT}/GeometryNode/Core/GraphEvaluator.cpp
        ${CGLIB_ROOT}/GeometryNode/Core/Field.cpp
        ${CGLIB_ROOT}/GeometryNode/Core/FieldNodes.cpp
        ${CGLIB_ROOT}/GeometryNode/Core/GraphJson.cpp
    )
    target_include_directories(GeometryNodeCore PUBLIC ${REPO_ROOT})
    target_compile_options(GeometryNodeCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(GeometryNodeCore PUBLIC cxx_std_20)
endfunction()

function(phantom_add_meshedit_core)
    if(TARGET MeshEditCore)
        return()
    endif()
    # Polygon mesh editing (Phantom::MeshEdit -- CPU-only): EditableMesh (logical topology with persistent ids),
    # shape operations (extrude/inset/bevel/loop cut/merge/delete/shading), the modifier stack (mirror, subdivision),
    # screen-space picking, and the edit-session selection state with its argument parsing.
    # No Vulkan / scene dependency; the render-mesh conversions use Graphics' CPU MeshData; picking uses the vendored header-only GLM.
    phantom_add_graphics_core()
    add_library(MeshEditCore STATIC
        ${CGLIB_ROOT}/MeshEdit/Core/EditableMesh.cpp
        ${CGLIB_ROOT}/MeshEdit/Core/EditableMeshConvert.cpp
        ${CGLIB_ROOT}/MeshEdit/Core/MeshBoolean.cpp
        ${CGLIB_ROOT}/MeshEdit/Core/MeshEditOperations.cpp
        ${CGLIB_ROOT}/MeshEdit/Core/MeshEditPicking.cpp
        ${CGLIB_ROOT}/MeshEdit/Core/MeshEditState.cpp
    )
    target_include_directories(MeshEditCore PUBLIC ${REPO_ROOT} ${CGLIB_ROOT}/ThirdParty/glm)
    target_link_libraries(MeshEditCore PUBLIC GraphicsCore)
    target_compile_options(MeshEditCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(MeshEditCore PUBLIC cxx_std_20)
endfunction()

function(phantom_add_gltf_core)
    if(TARGET GltfCore)
        return()
    endif()
    # CPU-only glTF evaluation (Phantom::Gltf): accessor reading, node transforms, animation / morph evaluation, the character
    # pose (clip placement + blend + authored bone / morph data) and its CPU bake. Depends only on GltfDocument, Animation's
    # track types and Graphics' CPU MeshData -- no Vulkan, so the pose can be used (and tested) without a GPU. GltfRendererCore
    # links this and keeps the renderer / reader / converters.
    phantom_add_math_core()
    phantom_add_graphics_core()
    phantom_add_animation_core()
    set(_gltf_root ${CGLIB_ROOT}/GltfRenderer/Gltf)
    add_library(GltfCore STATIC
        ${_gltf_root}/GltfAccessorView.cpp
        ${_gltf_root}/GltfNodeTransform.cpp
        ${_gltf_root}/GltfAnimationEvaluator.cpp
        ${_gltf_root}/GltfMorphApply.cpp
        ${_gltf_root}/CharacterPose.cpp
        ${_gltf_root}/CharacterPoseBake.cpp
    )
    target_include_directories(GltfCore PUBLIC ${REPO_ROOT} ${CGLIB_ROOT}/ThirdParty/glm)
    target_link_libraries(GltfCore PUBLIC MathCore GraphicsCore AnimationCore)
    target_compile_options(GltfCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(GltfCore PUBLIC cxx_std_20)
endfunction()

function(phantom_add_pbr_core)
    if(TARGET PBRCore)
        return()
    endif()
    # Physically based shading parts shared by the renderers (Phantom::PBR -- CPU-only): the light description and the
    # viewport's lighting uniform block. No Vulkan / scene dependency; uses the vendored header-only GLM.
    add_library(PBRCore STATIC
        ${CGLIB_ROOT}/PBR/Core/Light.cpp
        ${CGLIB_ROOT}/PBR/Core/LightBlock.cpp
        ${CGLIB_ROOT}/PBR/Core/Shadow.cpp
    )
    # Shared GLSL (PBR/Shaders/*.glsl) is the single source for the viewport shaders: glslc `#include`s the files directly
    # (PhantomStudio cgstudio.frag), and the runtime-assembled shader-graph fragment embeds the same text through this generated
    # header (Phantom::PBR::kLightBlockMembersGlsl / kLightEvalGlsl). Regenerated at configure time when a .glsl file changes.
    set(_pbr_glsl_dir ${CGLIB_ROOT}/PBR/Shaders)
    set(_pbr_gen_dir ${CMAKE_BINARY_DIR}/_pbr_generated)
    set(_pbr_gen_header "// Generated by CMake from CGLib/PBR/Shaders/*.glsl -- do not edit.\n#pragma once\nnamespace Phantom::PBR {\n")
    foreach(_pair light_block_members=kLightBlockMembersGlsl light_eval=kLightEvalGlsl)
        string(REPLACE "=" ";" _pair "${_pair}")
        list(GET _pair 0 _file)
        list(GET _pair 1 _var)
        file(READ ${_pbr_glsl_dir}/${_file}.glsl _glsl)
        string(APPEND _pbr_gen_header "inline constexpr const char* ${_var} = R\"PBRGLSL(${_glsl})PBRGLSL\";\n")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_pbr_glsl_dir}/${_file}.glsl)
    endforeach()
    string(APPEND _pbr_gen_header "}\n")
    set(_pbr_gen_file ${_pbr_gen_dir}/PBR/LightGlslSource.h)
    set(_pbr_gen_old "")
    if(EXISTS ${_pbr_gen_file})
        file(READ ${_pbr_gen_file} _pbr_gen_old)
    endif()
    if(NOT "${_pbr_gen_old}" STREQUAL "${_pbr_gen_header}")
        file(WRITE ${_pbr_gen_file} "${_pbr_gen_header}")
    endif()
    target_include_directories(PBRCore PUBLIC ${REPO_ROOT} ${CGLIB_ROOT}/ThirdParty/glm ${_pbr_gen_dir})
    target_compile_options(PBRCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(PBRCore PUBLIC cxx_std_20)
endfunction()

function(phantom_add_shadergraph_core)
    if(TARGET ShaderGraphCore)
        return()
    endif()
    # Shader graph foundation (Phantom::ShaderGraph -- CPU-only graph model, validation,
    # GLSL generation and JSON, docs/todo/PLAN_phantomstudio_shader_graph.md Phase 1).
    # No Math/Vulkan/Studio dependency; JSON uses the vendored header-only nlohmann/json
    # (reached by a relative include from ShaderGraph.h).
    add_library(ShaderGraphCore STATIC
        ${CGLIB_ROOT}/ShaderGraph/Core/NodeRegistry.cpp
        ${CGLIB_ROOT}/ShaderGraph/Core/Validate.cpp
        ${CGLIB_ROOT}/ShaderGraph/Core/GlslGen.cpp
        ${CGLIB_ROOT}/ShaderGraph/Core/GraphJson.cpp
        ${CGLIB_ROOT}/ShaderGraph/Core/GraphAsset.cpp
        ${CGLIB_ROOT}/ShaderGraph/Core/RuntimeCompiler.cpp
        ${CGLIB_ROOT}/ShaderGraph/Core/SurfaceFragment.cpp
    )
    # SurfaceFragment embeds the shared viewport light GLSL generated by PBRCore (the header is all it needs).
    phantom_add_pbr_core()
    target_include_directories(ShaderGraphCore PRIVATE $<TARGET_PROPERTY:PBRCore,INTERFACE_INCLUDE_DIRECTORIES>)
    # RuntimeCompiler loads shaderc_shared at run time (no link-time dependency); only dlopen on POSIX.
    target_link_libraries(ShaderGraphCore PUBLIC ${CMAKE_DL_LIBS})
    target_include_directories(ShaderGraphCore PUBLIC ${REPO_ROOT})
    target_compile_options(ShaderGraphCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(ShaderGraphCore PUBLIC cxx_std_20)
endfunction()

function(phantom_add_assetcore)
    if(TARGET AssetCore)
        return()
    endif()
    # Project-relative asset identity/manifest (Phantom::Asset -- Blender->Universe
    # authoring loop Phase 2 item 1, docs/spec/phantom_asset_manifest.md). No Math/
    # Vulkan/ImGui dependency -- only nlohmann/json for AssetManifest (de)serialization,
    # the same header-only dependency GltfRendererCore/UniverseSceneIO already use.
    add_library(AssetCore STATIC
        ${CGLIB_ROOT}/AssetCore/Core/AssetId.cpp
        ${CGLIB_ROOT}/AssetCore/Core/AssetUri.cpp
        ${CGLIB_ROOT}/AssetCore/Core/AssetManifest.cpp
        ${CGLIB_ROOT}/AssetCore/Core/ContentHash.cpp
        ${CGLIB_ROOT}/AssetCore/Core/FileWatcher.cpp
    )
    target_include_directories(AssetCore PUBLIC
        ${REPO_ROOT}
        ${CGLIB_ROOT}/ThirdParty/nlohmann
    )
    target_compile_options(AssetCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(AssetCore PUBLIC cxx_std_20)
endfunction()

function(phantom_add_scene_runtime_core)
    if(TARGET SceneRuntimeCore)
        return()
    endif()
    # Vulkan/ImGui-independent scene node hierarchy (Phantom::SceneRuntime -- Blender->
    # Universe authoring loop Phase 2 item 2, docs/spec/phantom_scene_runtime.md). Needs
    # MathCore (TRS transforms) and AssetCore (reuses Phantom::Asset::AssetId as its node
    # id type -- see SceneRuntime/NodeId.h) plus nlohmann/json for serialization, same as
    # AssetCore above.
    phantom_add_math_core()
    phantom_add_assetcore()
    add_library(SceneRuntimeCore STATIC
        ${CGLIB_ROOT}/SceneRuntime/Core/Transform.cpp
        ${CGLIB_ROOT}/SceneRuntime/Core/SceneNode.cpp
        ${CGLIB_ROOT}/SceneRuntime/Core/SceneGraph.cpp
        ${CGLIB_ROOT}/SceneRuntime/Core/ComponentSchema.cpp
        ${CGLIB_ROOT}/SceneRuntime/Core/ComponentSchemaRegistry.cpp
        ${CGLIB_ROOT}/SceneRuntime/Core/SceneV2.cpp
        ${CGLIB_ROOT}/SceneRuntime/Core/SceneV2Editor.cpp
        ${CGLIB_ROOT}/SceneRuntime/Core/ColliderDesc.cpp
        ${CGLIB_ROOT}/SceneRuntime/Core/AssetSidecar.cpp
        ${CGLIB_ROOT}/SceneRuntime/Core/SceneV2Merge.cpp
        ${CGLIB_ROOT}/SceneRuntime/Core/PlaySession.cpp
        ${CGLIB_ROOT}/SceneRuntime/Core/ClipPlayback.cpp
    )
    target_include_directories(SceneRuntimeCore PUBLIC
        ${REPO_ROOT}
        ${CGLIB_ROOT}/ThirdParty/nlohmann
        ${CGLIB_ROOT}/ThirdParty/glm
    )
    target_link_libraries(SceneRuntimeCore PUBLIC MathCore AssetCore)
    target_compile_options(SceneRuntimeCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(SceneRuntimeCore PUBLIC cxx_std_20)
endfunction()

function(phantom_add_animation_core)
    if(TARGET AnimationCore)
        return()
    endif()
    phantom_add_math_core()
    phantom_add_file_core()
    # NOTE on VMDConverter.cpp: VMD's Shift-JIS(CP932)->UTF-8 text decoding is
    # #ifdef _WIN32-split -- Windows keeps <windows.h>'s MultiByteToWideChar/
    # WideCharToMultiByte(932, ...), Linux uses glibc's
    # iconv("UTF-8", "CP932") (always available, no extra link dependency).
    add_library(AnimationCore STATIC
        ${CGLIB_ROOT}/Animation/Core/AnimationTrack.cpp
        ${CGLIB_ROOT}/Animation/Core/Animator.cpp
        ${CGLIB_ROOT}/Animation/Core/BVHConverter.cpp
        ${CGLIB_ROOT}/Animation/Core/IKSolver.cpp
        ${CGLIB_ROOT}/Animation/Core/MorphAnimator.cpp
        ${CGLIB_ROOT}/Animation/Core/PMXConverter.cpp
        ${CGLIB_ROOT}/Animation/Core/Skeleton.cpp
        ${CGLIB_ROOT}/Animation/Core/VMDConverter.cpp
    )
    target_include_directories(AnimationCore PUBLIC ${REPO_ROOT} ${CGLIB_ROOT}/ThirdParty/glm)
    # BVHConverter.h/PMXConverter.h/VMDConverter.h include File's BVHFile/PMXFile/VMDFile.
    target_link_libraries(AnimationCore PUBLIC MathCore FileCore)
    target_compile_options(AnimationCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(AnimationCore PUBLIC cxx_std_20)
endfunction()
