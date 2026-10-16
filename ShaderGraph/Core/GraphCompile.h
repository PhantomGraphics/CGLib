#pragma once

// Compiling one shader graph end to end: graph -> GLSL -> (when the runtime compiler is available) SPIR-V of the mesh
// surface fragment, with the diagnostics of every stage mapped back to graph nodes. Moved from PhantomStudio's
// ShaderGraphAdapter (docs/todo/PLAN_phantomstudio_library_extraction.md Phase 4). CPU-only; the runtime compiler is
// shaderc_shared loaded at run time, so builds and machines without it still get GLSL and diagnostics.

#include <cstdint>
#include <string>
#include <vector>

#include "RuntimeCompiler.h"
#include "ShaderGraph.h"

namespace Phantom::ShaderGraph {

// Result of compiling one graph: GLSL (always when the graph is valid) and, when the runtime compiler (shaderc_shared)
// is available, SPIR-V of the full mesh-surface fragment.
struct ShaderCompileOutcome {
    enum class Spirv { NotRequested, Unavailable, Ok, Failed };
    bool success = false;  // the graph is valid and GLSL was generated
    CompileResult graph;
    SurfaceFragment fragment;
    Spirv spirvStatus = Spirv::NotRequested;
    std::vector<uint32_t> spirv;
    std::string spirvError;
    std::vector<Diagnostic> diagnostics;  // graph diagnostics + missing textures + GLSL errors mapped to nodes
};

// Compiles `graph`. `wantSpirv` also runs the runtime compiler (serialised process-wide: the compiler instance is shared and
// loaded on first use). A compiler that cannot be loaded gives Spirv::Unavailable plus a "compiler.unavailable" error
// diagnostic (GLSL is still produced); a GLSL error gives Spirv::Failed plus a "glsl.error" diagnostic on the offending node.
// A texture file that does not exist is only a "texture.missing" warning (a white texture is used).
ShaderCompileOutcome compileShaderGraph(const Graph& graph, bool wantSpirv);

// True if the diagnostics contain a problem that makes the graph structurally invalid (duplicate or non-positive ids, links
// to missing nodes/sockets, type mismatch, several links into one input, cycles). Such an edit is rejected before it is
// committed; everything else is kept as diagnostics.
bool hasStructuralShaderError(const std::vector<Diagnostic>& diagnostics);

// Signature of everything that needs a recompile: node ids/types/versions, string parameters (Type / Path / ColorSpace) and
// links. Numeric values and layout are excluded.
std::string shaderStructureSignature(const Graph& graph);

}  // namespace Phantom::ShaderGraph
