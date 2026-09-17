#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Phantom::Gltf::Phmat
{

// Phase 4C item 4 ("descriptor/push constantをreflectionで検証する"): a `.phshader` node's GLSL
// is arbitrary, user-authored text spliced into the generated shader (PhmatCompiler.h). Unlike
// the graph's own built-in node types -- which only ever touch the fixed set=0 globals and the 5
// fixed set=1 texture slots kGltfPbrHeader declares -- a `.phshader` body could in principle
// declare its own `layout(set=..., binding=...)` resource or a push-constant block, either of
// which would silently mismatch GltfSceneRenderer's fixed material pipeline layout (built once
// from materialSetLayout_/globalSetLayout_, independent of any one material's shader). Reflecting
// the actually-*compiled* SPIR-V (not the GLSL source text) after compileGlslToSpirv() succeeds
// catches this before setMaterialShaderOverride() ever attempts to build a VkPipeline from it.
//
// No third-party SPIR-V reflection library (SPIRV-Cross/SPIRV-Reflect) is used -- SPIR-V's binary
// layout is simple enough (a fixed 5-word header, then a stream of instructions each starting
// with a (wordCount<<16 | opcode) word) that walking the two opcodes this needs (OpDecorate,
// OpVariable) is a few dozen lines, and this codebase already prefers a small self-contained
// parser over a new dependency for a narrowly-scoped need (see PhmatCompiler.cpp's own FNV-1a
// cache-key hash instead of pulling in a sha256 library).

struct PhmatDescriptorBinding {
    uint32_t set = 0;
    uint32_t binding = 0;
};

struct PhmatReflectionResult {
    std::vector<PhmatDescriptorBinding> descriptors; // deduplicated, sorted by (set, then binding)
    bool usesPushConstants = false;
};

// Parses a compiled SPIR-V module's word stream (as produced by compileGlslToSpirv()) and reports
// every (set, binding) pair it statically declares (via an OpVariable in a resource storage class,
// cross-referenced with that variable's DescriptorSet/Binding OpDecorate pair), plus whether it
// declares any PushConstant-storage-class variable. Returns false (outResult left default) only if
// spirv does not start with the SPIR-V magic number -- callers should treat that as "reflection
// unavailable" rather than as evidence the shader itself is broken (it already compiled).
bool reflectSpirv(const std::vector<uint32_t>& spirv, PhmatReflectionResult& outResult);

// The fixed set of (set, binding) pairs a phmat-generated fragment shader may legally reference --
// exactly what GltfSceneRenderer's material pipeline layout provides (kGltfPbrHeader in
// PhmatCompiler.cpp): set=0 binding 0-4 (GlobalUBO + irradiance/prefiltered/BRDF-LUT/shadow map),
// set=1 binding 0-5 (MaterialUBO -- never referenced by generated code, but still valid to
// redeclare -- + the 5 fixed texture slots).
bool isAllowedDescriptorBinding(const PhmatDescriptorBinding& b);

// Checks a reflection result against isAllowedDescriptorBinding() and rejects push constants
// (GltfSceneRenderer's material pipeline layout defines none). Appends one human-readable message
// per violation to outMessages (which is not cleared first, so callers can accumulate across
// multiple checks); returns true iff no message was appended.
bool validateReflection(const PhmatReflectionResult& result, std::vector<std::string>& outMessages);

}
