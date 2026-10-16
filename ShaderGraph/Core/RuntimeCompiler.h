#pragma once

// Runtime GLSL -> SPIR-V compilation and the "mesh surface" fragment ABI
// (docs/todo/PLAN_phantomstudio_shader_graph.md Phase 0).
//
// The compiler is the shaderc C API loaded at run time from shaderc_shared.dll
// (.so / .dylib elsewhere): no link-time dependency, no Vulkan SDK needed on the
// target machine as long as the DLL ships beside the executable, and it works for
// Debug builds (the SDK's static shaderc_combined.lib is Release-CRT only).
// Library search order: explicit path, directory of the executable / working
// directory, then $VULKAN_SDK/Bin. Exception-free.

#include <cstdint>
#include <string>
#include <vector>

#include "ShaderGraph.h"

namespace Phantom::ShaderGraph {

struct SpirvResult {
    bool ok = false;
    std::vector<uint32_t> spirv;
    std::string error;          // compiler message(s), empty when ok
    std::vector<int> errorLines;  // 1-based source lines parsed from `error`
};

class RuntimeCompiler {
public:
    RuntimeCompiler() = default;
    ~RuntimeCompiler();
    RuntimeCompiler(const RuntimeCompiler&) = delete;
    RuntimeCompiler& operator=(const RuntimeCompiler&) = delete;

    // Loads the library once; returns false (with loadError()) if it cannot be found/used.
    bool load(const std::string& explicitPath = "");
    bool available() const { return compiler_ != nullptr; }
    const std::string& loadError() const { return loadError_; }
    const std::string& loadedPath() const { return loadedPath_; }

    SpirvResult compileFragment(const std::string& source, const std::string& name = "graph.frag") const;
    SpirvResult compileVertex(const std::string& source, const std::string& name = "graph.vert") const;

private:
    SpirvResult compile(int kind, const std::string& source, const std::string& name) const;

    void* lib_ = nullptr;
    void* compiler_ = nullptr;
    void* api_ = nullptr;  // resolved function table (private to the .cpp)
    std::string loadError_, loadedPath_;
};

// ---- mesh surface fragment ABI v1 -------------------------------------------
//
// The graph fragment consumes the same vertex interface as
// CGApp/PhantomStudio/shaders/cgstudio.vert (locations 0..4), the same push
// constant block (model, baseColor, flags), the camera UBO at set 0 / binding 0,
// and owns set 1 (SGParams UBO + sg_tex<N> samplers, see ShaderGraph.h).
// Colour pipeline: graph colours and textures are LINEAR; the template applies
// the same pow(1/2.2) output encode as cgstudio.frag (the swapchain is UNORM).
// Colour textures must therefore be sampled through an sRGB view; the existing
// TextureManager is UNORM only and gains an sRGB variant in Phase 3.

struct SurfaceFragment {
    std::string source;
    int graphLineOffset = 0;  // add to CompileResult::lineMap lines to get lines in `source`
};

SurfaceFragment buildSurfaceFragment(const CompileResult& graph);
// Maps an error line of SurfaceFragment::source back to a graph node (0 = template / outside).
NodeId nodeForFragmentLine(const SurfaceFragment& f, const CompileResult& graph, int line);

}  // namespace Phantom::ShaderGraph
