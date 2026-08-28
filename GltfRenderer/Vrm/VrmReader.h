#pragma once

#include "VrmTypes.h"
#include <filesystem>
#include <optional>

namespace Phantom::Gltf {

class VrmReader {
public:
    // Load a .vrm (or a plain .gltf/.glb that happens to carry a VRM extension) file into a
    // VrmDocument. Only fails (returns std::nullopt) if the underlying glTF/glb itself can't be
    // parsed (Phantom::File::GLTFFileReader failure) -- a file with no "VRM"/"VRMC_vrm" extension
    // at all still succeeds, with specVersion left at VrmSpecVersion::Unknown and empty
    // humanoid/expressions, so callers can use this as a generic "load as VRM-if-possible, else
    // plain glTF" entry point without a separate fallback path.
    static std::optional<VrmDocument> load(const std::filesystem::path& path);
};

}
