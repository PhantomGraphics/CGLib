#pragma once

// VulkanSPVResolver.h
// Exe-directory-relative SPIR-V loader. Include this header in place of (or
// alongside) VulkanSPVLoader.h and call ::VKG::loadSPVRepo() with a path
// relative to the module (exe or DLL) directory, e.g. "shaders/my.vert.spv".
//
// PostBuildEvent in each app's .vcxproj copies compiled .spv files to
// $(OutDir)shaders\ so that the loader finds them next to the binary.

#include "VulkanSPVLoader.h"

#include <cstdio>
#include <filesystem>
#include <vector>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace Phantom::VKG {

namespace detail {

// Returns the directory that contains the module (exe or DLL) which was
// linked against this inline function. When compiled into a DLL (e.g.
// CsPointCloud.dll), GetModuleHandleEx with FROM_ADDRESS gives us the DLL's
// own path rather than the host exe's path.
inline std::filesystem::path detectModuleDir()
{
#ifdef _WIN32
    HMODULE self = nullptr;
    if (::GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&detectModuleDir),
            &self) && self)
    {
        wchar_t buf[MAX_PATH]{};
        if (::GetModuleFileNameW(self, buf, MAX_PATH) > 0)
            return std::filesystem::path(buf).parent_path();
    }
    // Fallback: host exe directory
    wchar_t buf[MAX_PATH]{};
    if (::GetModuleFileNameW(nullptr, buf, MAX_PATH) > 0)
        return std::filesystem::path(buf).parent_path();
#endif
    std::error_code ec;
    return std::filesystem::current_path(ec);
}

} // namespace detail

// Load a SPIR-V file given a path relative to the module directory.
//
//   cfg.vertSpv = ::VKG::loadSPVRepo("shaders/gltf.vert.spv");
//
// The module directory is detected once per module and cached. On failure an
// empty vector is returned and a diagnostic is printed to stderr.
inline std::vector<uint32_t> loadSPVRepo(const std::string& moduleRelativePath)
{
    static const std::filesystem::path kBase = detail::detectModuleDir();

    if (kBase.empty()) {
        std::fprintf(stderr,
            "[VKG] loadSPVRepo: Cannot determine module directory. "
            "path=%s\n", moduleRelativePath.c_str());
        return {};
    }

    return loadSPV((kBase / moduleRelativePath).string());
}

} // namespace VKG

namespace VKG {
using namespace Phantom::VKG;
}
