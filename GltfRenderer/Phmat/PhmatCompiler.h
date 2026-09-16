#pragma once

#include "PhmatGraph.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Phantom::Gltf::Phmat
{

// Generates a full gltf.frag-compatible GLSL fragment shader source from an already-validated
// graph (call PhmatGraph.h's validateAndSort() first and pass its topoOrder here). The emitted
// shader declares the exact same set=0/set=1 bindings and vertex-stage input locations as
// CGLib/GltfViewer/shaders/gltf.frag (see that file's comment header for why this is a
// duplication rather than a #include -- CGLib's shaders already accept per-app copies, see
// CGApp/CLAUDE.md), so it links against GltfSceneRenderer's existing pipeline layout / gltf.vert
// unchanged; only the block that used to read MaterialUBO + the 5 fixed textures is replaced by
// the graph's generated node declarations feeding the same Cook-Torrance lighting tail. Returns
// false (outGlsl left empty) only if the graph references a node type this compiler has no
// codegen for -- which validateAndSort() having already run should make unreachable in practice.
bool compileGraphToGlsl(const PhmatGraph& graph, const std::vector<std::string>& topoOrder,
                         std::string& outGlsl, std::vector<PhmatDiagnostic>& outDiagnostics);

// Locates glslc.exe/glslc via the VULKAN_SDK environment variable, mirroring every existing
// shaders/compile_shaders.bat in this repo (`%VULKAN_SDK%\Bin\glslc.exe` on Windows,
// `$VULKAN_SDK/bin/glslc` on Linux). Returns false if VULKAN_SDK is unset or the binary is not
// where expected -- callers should treat this the same as "Vulkan SDK not found" elsewhere in
// this codebase (skip/degrade, do not treat as a hard failure of the calling app).
bool findGlslcPath(std::string& outPath);

// Compiles GLSL fragment source to SPIR-V by shelling out to glslc (no in-process GLSL compiler
// exists in this codebase). glslSource is content-hashed (FNV-1a 64 -- a cache key, not a
// security boundary, so a non-cryptographic hash is fine) to name the cached
// "<hash>.frag"/"<hash>.spv" pair under cacheDir; a cache hit (matching .spv already present)
// skips invoking glslc entirely. This is the sense in which SPIR-V is a *cache* per the plan
// (item 1: "生成SPIR-Vをcacheとする。SPIR-Vだけを真実のソースにしない") -- deleting cacheDir
// only costs one glslc invocation per distinct graph, never data loss, since glslSource (in turn
// generated from the .phmat graph) is always sufficient to regenerate it deterministically.
bool compileGlslToSpirv(const std::string& glslSource, const std::string& cacheDir,
                         std::vector<uint32_t>& outSpirv, std::string& outErrorLog);

struct PhmatLoadResult {
    bool                         success = false;
    std::vector<uint32_t>        fragSpirv;   // valid only if success
    std::vector<PhmatDiagnostic> diagnostics; // parse/validate/compile errors, whichever stage failed
};

// Top-level entry point a consumer (e.g. GltfViewer::App) calls: reads phmatPath from disk,
// parses, validates, generates GLSL, and compiles/caches SPIR-V under cacheDir, in one call.
// success=false at any stage leaves fragSpirv empty and diagnostics explains why -- the caller
// (GltfSceneRenderer::setMaterialShaderOverride(), see its own comment) is expected to keep
// whatever pipeline it already had rather than switch to a broken one (plan item 4:
// "compile失敗時は旧pipelineを維持し、fallback materialとerror位置を表示する").
PhmatLoadResult loadPhmatMaterial(const std::string& phmatPath, const std::string& cacheDir);

}
