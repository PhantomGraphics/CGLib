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
    file(GLOB _math_sources ${CGLIB_ROOT}/Math/*.cpp)
    list(FILTER _math_sources EXCLUDE REGEX "pch\\.cpp$")
    add_library(MathCore STATIC ${_math_sources})
    target_include_directories(MathCore PUBLIC ${CGLIB_ROOT} ${REPO_ROOT})
    target_compile_options(MathCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(MathCore PRIVATE cxx_std_20)
endfunction()

function(phantom_add_graphics_core)
    if(TARGET GraphicsCore)
        return()
    endif()
    phantom_add_math_core()
    file(GLOB _graphics_sources ${CGLIB_ROOT}/Graphics/*.cpp)
    list(FILTER _graphics_sources EXCLUDE REGEX "pch\\.cpp$")
    add_library(GraphicsCore STATIC ${_graphics_sources})
    target_include_directories(GraphicsCore PUBLIC ${CGLIB_ROOT} ${REPO_ROOT})
    target_link_libraries(GraphicsCore PUBLIC MathCore)
    target_compile_options(GraphicsCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(GraphicsCore PRIVATE cxx_std_20)
endfunction()

function(phantom_add_numerics_core)
    if(TARGET NumericsCore)
        return()
    endif()
    add_library(NumericsCore STATIC
        ${CGLIB_ROOT}/Numerics/Numerics/Converter.cpp
        ${CGLIB_ROOT}/Numerics/Numerics/SVD2d.cpp
        ${CGLIB_ROOT}/Numerics/Numerics/SVD3d.cpp
    )
    target_include_directories(NumericsCore PUBLIC ${REPO_ROOT})
    target_compile_options(NumericsCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(NumericsCore PRIVATE cxx_std_20)
endfunction()

function(phantom_add_space_core)
    if(TARGET SpaceCore)
        return()
    endif()
    phantom_add_math_core()
    file(GLOB _space_sources ${CGLIB_ROOT}/Space/Space/*.cpp)
    list(FILTER _space_sources EXCLUDE REGEX "pch\\.cpp$")
    add_library(SpaceCore STATIC ${_space_sources})
    target_include_directories(SpaceCore PUBLIC ${REPO_ROOT})
    target_link_libraries(SpaceCore PUBLIC MathCore)
    target_compile_options(SpaceCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(SpaceCore PRIVATE cxx_std_20)

    # IndexedSortBasedSearcher.cpp's #pragma omp parallel for is silently a
    # no-op without this -- unlike Physics/PointCloud's own CMakeLists.txt
    # (which each find_package(OpenMP) themselves), SpaceCore previously had
    # no OpenMP link of its own, so the CMake build's neighbor search ran
    # fully serial even though the same source compiles multithreaded in the
    # MSBuild/Blender-addon build (docs/issue/wcsph_parallel_scaling_profile.md
    # section 7).
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(SpaceCore PUBLIC OpenMP::OpenMP_CXX)
    endif()
endfunction()

function(phantom_add_scene_core)
    if(TARGET SceneCore)
        return()
    endif()
    phantom_add_math_core()
    # NOTE on Scene's on-disk source list: Scene/Scene/ also contains
    # ParticleSystemIdPresenter.cpp/.h, ParticleSystemPresenter.cpp/.h,
    # TriangleMeshPresenter.cpp/.h and WireFramePresenter.cpp/.h -- these are
    # deliberately NOT part of Scene.vcxproj's <ClCompile> list (Scene.vcxproj.
    # filters is stale and still lists them) because they pull in
    # CGLib/Renderer/Renderer/PointRenderer.h, a GUI/Vulkan-facing header outside
    # this Vulkan-free module's real dependency set. Do NOT file(GLOB) this
    # directory -- the explicit list below matches Scene.vcxproj exactly.
    add_library(SceneCore STATIC
        ${CGLIB_ROOT}/Scene/Scene/ParticleSystem.cpp
        ${CGLIB_ROOT}/Scene/Scene/ParticleSystemBuilder.cpp
        ${CGLIB_ROOT}/Scene/Scene/ParticleSystemScene.cpp
        ${CGLIB_ROOT}/Scene/Scene/SceneBase.cpp
        ${CGLIB_ROOT}/Scene/Scene/SceneGroup.cpp
        ${CGLIB_ROOT}/Scene/Scene/TriangleMesh.cpp
        ${CGLIB_ROOT}/Scene/Scene/TriangleMeshBuilder.cpp
        ${CGLIB_ROOT}/Scene/Scene/WireFrame.cpp
        ${CGLIB_ROOT}/Scene/Scene/WireFrameBuilder.cpp
        ${CGLIB_ROOT}/Scene/Scene/WireFrameScene.cpp
    )
    target_include_directories(SceneCore PUBLIC ${REPO_ROOT})
    target_link_libraries(SceneCore PUBLIC MathCore)
    target_compile_options(SceneCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(SceneCore PRIVATE cxx_std_20)
endfunction()

function(phantom_add_volume_core)
    if(TARGET VolumeCore)
        return()
    endif()
    phantom_add_math_core()
    phantom_add_space_core()
    # NOTE on OpenVDB: SparseVolumeTree/{VdbReader,VdbWriter}.h read/write the
    # raw .vdb file format using only the C++ standard library -- no OpenVDB
    # library dependency despite the file names (see those headers' own
    # comments). No find_package/system package needed for it.
    add_library(VolumeCore STATIC
        ${CGLIB_ROOT}/Volume/Volume/LevelSet.cpp
        ${CGLIB_ROOT}/Volume/Volume/MCSurfaceBuilder.cpp
        ${CGLIB_ROOT}/Volume/Volume/SurfaceVoxelizer.cpp
        ${CGLIB_ROOT}/Volume/Volume/Volume.cpp
        ${CGLIB_ROOT}/Volume/Volume/VolumeNode.cpp
    )
    target_include_directories(VolumeCore PUBLIC ${REPO_ROOT})
    target_link_libraries(VolumeCore PUBLIC MathCore SpaceCore)
    target_compile_options(VolumeCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(VolumeCore PRIVATE cxx_std_20)
endfunction()

function(phantom_add_file_core)
    if(TARGET FileCore)
        return()
    endif()
    phantom_add_math_core()
    phantom_add_graphics_core()
    # NOTE on File's on-disk source list: File/File/ also contains File.cpp, a
    # never-wired-up leftover from the original VS "static library" project
    # template (a stub fnFile() nobody calls) -- confirmed NOT part of File.
    # vcxproj's <ClCompile> list. Do NOT file(GLOB) this directory -- the
    # explicit list below matches File.vcxproj exactly. cgltf (GLTFFileReader/
    # Writer) is a single-header library under File/ThirdParty/cgltf, included
    # via the repo-root-relative path "CGLib/File/ThirdParty/cgltf/cgltf.h"
    # -- no extra include dir needed beyond REPO_ROOT (already PUBLIC below).
    add_library(FileCore STATIC
        ${CGLIB_ROOT}/File/File/BVHFileReader.cpp
        ${CGLIB_ROOT}/File/File/GLTFFileReader.cpp
        ${CGLIB_ROOT}/File/File/GLTFFileWriter.cpp
        ${CGLIB_ROOT}/File/File/MTLFileReader.cpp
        ${CGLIB_ROOT}/File/File/MTLFileWriter.cpp
        ${CGLIB_ROOT}/File/File/OBJFileReader.cpp
        ${CGLIB_ROOT}/File/File/OBJFileWriter.cpp
        ${CGLIB_ROOT}/File/File/OBJSyntaxParser.cpp
        ${CGLIB_ROOT}/File/File/PLYFileReader.cpp
        ${CGLIB_ROOT}/File/File/PLYFileWriter.cpp
        ${CGLIB_ROOT}/File/File/PMDFileReader.cpp
        ${CGLIB_ROOT}/File/File/PMXFileReader.cpp
        ${CGLIB_ROOT}/File/File/STLFileReader.cpp
        ${CGLIB_ROOT}/File/File/STLFileWriter.cpp
        ${CGLIB_ROOT}/File/File/VMDFileReader.cpp
    )
    target_include_directories(FileCore PUBLIC ${REPO_ROOT})
    target_link_libraries(FileCore PUBLIC MathCore GraphicsCore)
    target_compile_options(FileCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(FileCore PRIVATE cxx_std_20)
endfunction()

function(phantom_add_terrain_core)
    if(TARGET TerrainCore)
        return()
    endif()
    # CPU-only height-field terrain generation (Phantom::Terrain). Deliberately
    # standalone -- no MathCore/GLM/Vulkan/JSON dependency (plan section 3.1);
    # the seeded gradient noise is implemented in-module.
    add_library(TerrainCore STATIC
        ${CGLIB_ROOT}/Terrain/Terrain/TerrainGenerator.cpp
    )
    target_include_directories(TerrainCore PUBLIC ${REPO_ROOT})
    target_compile_options(TerrainCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(TerrainCore PRIVATE cxx_std_20)
endfunction()

function(phantom_add_animation_core)
    if(TARGET AnimationCore)
        return()
    endif()
    phantom_add_math_core()
    # NOTE on VMDConverter.cpp: VMD's Shift-JIS(CP932)->UTF-8 text decoding is
    # #ifdef _WIN32-split -- Windows keeps <windows.h>'s MultiByteToWideChar/
    # WideCharToMultiByte(932, ...), Linux uses glibc's
    # iconv("UTF-8", "CP932") (always available, no extra link dependency).
    add_library(AnimationCore STATIC
        ${CGLIB_ROOT}/Animation/Animation/Animator.cpp
        ${CGLIB_ROOT}/Animation/Animation/BVHConverter.cpp
        ${CGLIB_ROOT}/Animation/Animation/IKSolver.cpp
        ${CGLIB_ROOT}/Animation/Animation/MorphAnimator.cpp
        ${CGLIB_ROOT}/Animation/Animation/PMXConverter.cpp
        ${CGLIB_ROOT}/Animation/Animation/Skeleton.cpp
        ${CGLIB_ROOT}/Animation/Animation/VMDConverter.cpp
    )
    target_include_directories(AnimationCore PUBLIC ${REPO_ROOT} ${CGLIB_ROOT}/ThirdParty/glm-0.9.9.8)
    target_link_libraries(AnimationCore PUBLIC MathCore)
    target_compile_options(AnimationCore PRIVATE ${PHANTOM_WARN_FLAGS})
    target_compile_features(AnimationCore PRIVATE cxx_std_20)
endfunction()
