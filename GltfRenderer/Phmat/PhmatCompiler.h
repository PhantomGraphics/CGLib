#pragma once

#include "PhmatGraph.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Phantom::Gltf::Phmat
{

// A ".phshader" file: a small, separately-versioned JSON document (Phase 4C item 4, "custom GLSL
// は`.phshader`参照として別管理する") holding one GLSL function a `.phmat` graph's "custom" node
// type calls. Kept as JSON (not raw GLSL) so the function's call signature -- exactly what a
// PhmatGraph.h "custom" node already redundantly declares for itself, since parsing/validating a
// `.phmat` graph stays filesystem-free -- is data the compiler controls and generates itself,
// rather than something it would need to parse out of arbitrary GLSL text.
struct PhshaderInput {
    std::string name; // used verbatim as the generated GLSL function's parameter name
    ValueType   type;
};

struct PhshaderSource {
    int                        version = 0;
    std::string                functionName; // generated GLSL function identifier
    std::vector<PhshaderInput> inputs;
    ValueType                  outputType = ValueType::Float;
    std::string                helpers; // raw GLSL emitted once before the function (may be empty)
    std::string                body;    // raw GLSL statements forming the function's body
};

// Parses a ".phshader" JSON document's text (does not touch the filesystem -- see
// loadPhshaderFile() below for that). Returns false with outError explaining why on malformed
// JSON, an unsupported version, or a missing/invalid required field.
bool parsePhshader(const std::string& jsonText, PhshaderSource& out, std::string& outError);

// Reads phshaderPath from disk and calls parsePhshader() on its contents.
bool loadPhshaderFile(const std::string& phshaderPath, PhshaderSource& out, std::string& outError);

// Tracks where one Custom node's ".phshader" function body ended up in the GLSL text
// compileGraphToGlsl() generates, so a glslc compile error reported against the generated file's
// line number can be rewritten to point at the original .phshader file and a line number relative
// to its own "body" field instead (see loadPhmatMaterial()'s error-log rewriting -- plan item 4
// "error位置を表示する"). When several Custom nodes reference the same .phshader (same
// functionName), the function is only emitted once and every such node gets its own entry here,
// all pointing at that one shared line range.
struct PhshaderSplice {
    std::string nodeId;
    std::string phshaderPath;
    int firstBodyLineInGenerated = 0; // 1-based line, in outGlsl, where the .phshader body's own line 1 begins
    int lastBodyLineInGenerated = 0;  // 1-based, inclusive -- the body's last physical line
};

// Generates a full gltf.frag-compatible GLSL fragment shader source from an already-validated
// graph (call PhmatGraph.h's validateAndSort() first and pass its topoOrder here). The emitted
// shader declares the exact same set=0/set=1 bindings and vertex-stage input locations as
// CGLib/GltfViewer/shaders/gltf.frag (see that file's comment header for why this is a
// duplication rather than a #include -- CGLib's shaders already accept per-app copies, see
// CGApp/CLAUDE.md), so it links against GltfSceneRenderer's existing pipeline layout / gltf.vert
// unchanged; only the block that used to read MaterialUBO + the 5 fixed textures is replaced by
// the graph's generated node declarations feeding the same Cook-Torrance lighting tail.
//
// phshaders must have one entry per Custom-type node in `graph`, keyed by that node's id --
// loadPhmatMaterial() below is the one place that loads/signature-checks these from disk before
// calling this function; a graph with no Custom nodes can pass an empty map. Returns false
// (outGlsl left empty) if the graph references a node type this compiler has no codegen for
// (which validateAndSort() having already run should make unreachable in practice), or if any
// Custom node's declared signature does not match its phshaders[] entry's own signature.
bool compileGraphToGlsl(const PhmatGraph& graph, const std::vector<std::string>& topoOrder,
                         const std::unordered_map<std::string, PhshaderSource>& phshaders,
                         std::string& outGlsl, std::vector<PhshaderSplice>& outSplices,
                         std::vector<PhmatDiagnostic>& outDiagnostics);

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
// parses, validates, resolves+loads every Custom node's ".phshader" file (relative to phmatPath's
// own directory), generates GLSL, compiles/caches SPIR-V under cacheDir, and reflects the compiled
// SPIR-V (PhmatReflection.h) to reject a .phshader that declares a descriptor binding or push
// constant outside what GltfSceneRenderer's fixed material pipeline layout provides -- all in one
// call. success=false at any stage leaves fragSpirv empty and diagnostics explains why, with a
// glslc compile error inside a Custom node's body rewritten to name that node's .phshader path and
// a line number relative to its own "body" field (PhshaderSplice) rather than the generated file's
// line number. The caller (GltfSceneRenderer::setMaterialShaderOverride(), see its own comment) is
// expected to keep whatever pipeline it already had rather than switch to a broken one (plan item
// 4: "compile失敗時は旧pipelineを維持し、fallback materialとerror位置を表示する").
PhmatLoadResult loadPhmatMaterial(const std::string& phmatPath, const std::string& cacheDir);

}
