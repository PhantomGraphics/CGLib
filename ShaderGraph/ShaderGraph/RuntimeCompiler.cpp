#define _CRT_SECURE_NO_WARNINGS
#include "RuntimeCompiler.h"
#include <memory>

#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace Phantom::ShaderGraph {

namespace {

// Subset of the shaderc C API (shaderc/shaderc.h). Enum values are ABI-stable:
// shaderc_vertex_shader = 0, shaderc_fragment_shader = 1, shaderc_target_env_vulkan = 0,
// shaderc_env_version_vulkan_1_0 = 1 << 22, shaderc_optimization_level_performance = 2,
// shaderc_compilation_status_success = 0.
struct Api {
    void* (*compilerInit)() = nullptr;
    void (*compilerRelease)(void*) = nullptr;
    void* (*optionsInit)() = nullptr;
    void (*optionsRelease)(void*) = nullptr;
    void (*setTargetEnv)(void*, int, uint32_t) = nullptr;
    void (*setOptLevel)(void*, int) = nullptr;
    void* (*compile)(void*, const char*, size_t, int, const char*, const char*, const void*) = nullptr;
    size_t (*resultLength)(const void*) = nullptr;
    const char* (*resultBytes)(const void*) = nullptr;
    int (*resultStatus)(const void*) = nullptr;
    const char* (*resultError)(const void*) = nullptr;
    void (*resultRelease)(void*) = nullptr;
};

constexpr int kVertex = 0, kFragment = 1;
constexpr uint32_t kVulkan10 = 1u << 22;

#if defined(_WIN32)
const char* kLibName = "shaderc_shared.dll";
void* openLib(const std::string& p) { return reinterpret_cast<void*>(LoadLibraryA(p.c_str())); }
void* sym(void* l, const char* n) { return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(l), n)); }
void closeLib(void* l) { FreeLibrary(static_cast<HMODULE>(l)); }
#else
#if defined(__APPLE__)
const char* kLibName = "libshaderc_shared.dylib";
#else
const char* kLibName = "libshaderc_shared.so";
#endif
void* openLib(const std::string& p) { return dlopen(p.c_str(), RTLD_NOW); }
void* sym(void* l, const char* n) { return dlsym(l, n); }
void closeLib(void* l) { dlclose(l); }
#endif

template <class F>
bool bind(void* lib, const char* name, F& f) {
    f = reinterpret_cast<F>(sym(lib, name));
    return f != nullptr;
}

}  // namespace

RuntimeCompiler::~RuntimeCompiler() {
    Api* a = static_cast<Api*>(api_);
    if (a && compiler_) a->compilerRelease(compiler_);
    delete a;
    if (lib_) closeLib(lib_);
}

bool RuntimeCompiler::load(const std::string& explicitPath) {
    if (compiler_) return true;
    std::vector<std::string> candidates;
    if (!explicitPath.empty()) candidates.push_back(explicitPath);
#if defined(_WIN32)
    candidates.push_back(kLibName);  // exe directory / PATH (LoadLibrary default search)
#else
    candidates.push_back(kLibName);
#endif
    if (const char* sdk = std::getenv("VULKAN_SDK")) {
        std::string s = sdk;
#if defined(_WIN32)
        candidates.push_back(s + "\\Bin\\" + kLibName);
#else
        candidates.push_back(s + "/lib/" + kLibName);
#endif
    }
    void* lib = nullptr;
    std::string path;
    for (const std::string& c : candidates) {
        lib = openLib(c);
        if (lib) { path = c; break; }
    }
    if (!lib) {
        loadError_ = std::string("could not load ") + kLibName + " (searched exe dir/PATH and $VULKAN_SDK)";
        return false;
    }
    auto api = std::make_unique<Api>();
    const bool ok = bind(lib, "shaderc_compiler_initialize", api->compilerInit) &&
                    bind(lib, "shaderc_compiler_release", api->compilerRelease) &&
                    bind(lib, "shaderc_compile_options_initialize", api->optionsInit) &&
                    bind(lib, "shaderc_compile_options_release", api->optionsRelease) &&
                    bind(lib, "shaderc_compile_options_set_target_env", api->setTargetEnv) &&
                    bind(lib, "shaderc_compile_options_set_optimization_level", api->setOptLevel) &&
                    bind(lib, "shaderc_compile_into_spv", api->compile) &&
                    bind(lib, "shaderc_result_get_length", api->resultLength) &&
                    bind(lib, "shaderc_result_get_bytes", api->resultBytes) &&
                    bind(lib, "shaderc_result_get_compilation_status", api->resultStatus) &&
                    bind(lib, "shaderc_result_get_error_message", api->resultError) &&
                    bind(lib, "shaderc_result_release", api->resultRelease);
    if (!ok) {
        loadError_ = path + " does not export the expected shaderc C API";
        closeLib(lib);
        return false;
    }
    void* c = api->compilerInit();
    if (!c) {
        loadError_ = "shaderc_compiler_initialize failed";
        closeLib(lib);
        return false;
    }
    lib_ = lib;
    compiler_ = c;
    api_ = api.release();
    loadedPath_ = path;
    loadError_.clear();
    return true;
}

SpirvResult RuntimeCompiler::compile(int kind, const std::string& source, const std::string& name) const {
    SpirvResult r;
    const Api* a = static_cast<const Api*>(api_);
    if (!a || !compiler_) {
        r.error = loadError_.empty() ? "shader compiler not loaded" : loadError_;
        return r;
    }
    void* opt = a->optionsInit();
    if (opt) {
        a->setTargetEnv(opt, 0, kVulkan10);
        a->setOptLevel(opt, 2);
    }
    void* res = a->compile(compiler_, source.c_str(), source.size(), kind, name.c_str(), "main", opt);
    if (opt) a->optionsRelease(opt);
    if (!res) {
        r.error = "shaderc_compile_into_spv returned null";
        return r;
    }
    if (a->resultStatus(res) == 0) {
        const size_t bytes = a->resultLength(res);
        r.spirv.resize(bytes / 4);
        std::memcpy(r.spirv.data(), a->resultBytes(res), r.spirv.size() * 4);
        r.ok = !r.spirv.empty();
    } else {
        const char* e = a->resultError(res);
        r.error = e ? e : "compilation failed";
        // lines look like "<name>:<line>: error: ..."
        const std::string prefix = name + ":";
        size_t pos = 0;
        while ((pos = r.error.find(prefix, pos)) != std::string::npos) {
            pos += prefix.size();
            const int line = std::atoi(r.error.c_str() + pos);
            if (line > 0) r.errorLines.push_back(line);
        }
    }
    a->resultRelease(res);
    return r;
}

SpirvResult RuntimeCompiler::compileFragment(const std::string& s, const std::string& n) const {
    return compile(kFragment, s, n);
}
SpirvResult RuntimeCompiler::compileVertex(const std::string& s, const std::string& n) const {
    return compile(kVertex, s, n);
}

}  // namespace Phantom::ShaderGraph
