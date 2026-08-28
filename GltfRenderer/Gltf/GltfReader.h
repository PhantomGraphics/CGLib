#pragma once

#include "GltfDocument.h"
#include "../../File/File/GLTFFile.h"
#include <filesystem>
#include <optional>

namespace Phantom::Gltf {

    class GltfReader {
    public:
        // Load a .gltf or .glb file and return a fully populated GltfDocument,
        // or std::nullopt on failure (logged to stderr).
        static std::optional<GltfDocument> load(const std::filesystem::path& path);

        // Maps an already-parsed Phantom::File::GLTFFile into a GltfDocument. `baseDir` resolves
        // relative image URIs (see GltfImage). Exposed separately (not just an implementation
        // detail of load(path) above) so callers that already hold a parsed GLTFFile -- e.g.
        // VrmReader, which needs the same GLTFFile to also read rootExtensionsJson/
        // GLTFMaterial::extensionsJson -- can reuse this mapping without re-parsing the file.
        static GltfDocument load(const Phantom::File::GLTFFile& src, const std::filesystem::path& baseDir);
    };
}
