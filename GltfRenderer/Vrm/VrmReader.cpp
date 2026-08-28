#include "VrmReader.h"
#include "VrmExtensionParser.h"
#include "VrmMToonFallback.h"
#include "../Gltf/GltfReader.h"
#include "../../File/File/GLTFFileReader.h"

#include <cstdio>

using namespace Phantom::Gltf;
using Json = nlohmann::json;

namespace {

const std::string* findExtension(const std::vector<std::pair<std::string, std::string>>& exts,
                                  const char* name) {
    for (const auto& e : exts)
        if (e.first == name) return &e.second;
    return nullptr;
}

} // namespace

std::optional<VrmDocument> VrmReader::load(const std::filesystem::path& path) {
    Phantom::File::GLTFFileReader reader;
    if (!reader.read(path)) {
        std::fprintf(stderr, "[VrmReader] Failed to load glTF via Phantom::File: %s\n", path.string().c_str());
        return std::nullopt;
    }
    const Phantom::File::GLTFFile src = reader.getGLTF();

    VrmDocument out;
    out.gltf = GltfReader::load(src, path.parent_path());

    const std::string* v1Json = findExtension(src.rootExtensionsJson, "VRMC_vrm");
    const std::string* v0Json = findExtension(src.rootExtensionsJson, "VRM");
    const std::string* raw = v1Json ? v1Json : v0Json;
    out.specVersion = v1Json ? VrmSpecVersion::V1 : (v0Json ? VrmSpecVersion::V0 : VrmSpecVersion::Unknown);

    if (!raw) {
        std::fprintf(stderr, "[VrmReader] No VRM extension found, importing as plain glTF: %s\n", path.string().c_str());
        return out;
    }

    const Json root = Json::parse(*raw, nullptr, /*allow_exceptions=*/false);
    if (root.is_discarded()) {
        std::fprintf(stderr, "[VrmReader] Malformed VRM extension JSON, importing as plain glTF: %s\n", path.string().c_str());
        out.specVersion = VrmSpecVersion::Unknown;
        return out;
    }

    out.humanoid    = VrmExtensionParser::parseHumanoid(root, out.specVersion);
    out.expressions = VrmExtensionParser::parseExpressions(root, out.specVersion, out.gltf);
    out.meta        = VrmExtensionParser::parseMeta(root, out.specVersion);
    VrmMToonFallback::apply(out.gltf, root, out.specVersion, src);

    return out;
}
