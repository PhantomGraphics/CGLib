#include "GltfReader.h"
#include "GltfAccessorBuilder.h"
#include "../../File/File/GLTFFileReader.h"

#include "../../../CGLib/ThirdParty/stb/stb_image.h"

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <cstdio>
#include <cstring>

using namespace Phantom::Gltf;

namespace {

std::optional<std::vector<uint8_t>> readFile(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) {
        std::fprintf(stderr, "[GltfReader] Cannot open: %s\n", p.string().c_str());
        return std::nullopt;
    }
    auto sz = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    return buf;
}

std::vector<uint8_t> decodeBase64(const std::string& s) {
    static const std::string table =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<uint8_t> out;
    int val = 0, bits = -8;
    for (unsigned char c : s) {
        auto pos = table.find(c);
        if (pos == std::string::npos) continue;
        val = (val << 6) + static_cast<int>(pos);
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

bool startsWith(const std::string& text, const char* prefix) {
    return text.rfind(prefix, 0) == 0;
}

bool decodeImage(GltfImage& img, const uint8_t* data, int len) {
    int w, h, ch;
    stbi_uc* px = stbi_load_from_memory(data, len, &w, &h, &ch, STBI_rgb_alpha);
    if (!px) {
        std::fprintf(stderr, "[GltfReader] stb_image failed to decode image\n");
        return false;
    }
    img.width = w;
    img.height = h;
    img.channels = 4;
    img.pixels.assign(px, px + w * h * 4);
    stbi_image_free(px);
    return true;
}

void tryDecodeImageFromUri(GltfImage& img, const std::filesystem::path& baseDir) {
    if (img.uri.empty()) return;

    if (startsWith(img.uri, "data:")) {
        const auto comma = img.uri.find(',');
        if (comma == std::string::npos) return;
        auto raw = decodeBase64(img.uri.substr(comma + 1));
        if (!raw.empty()) decodeImage(img, raw.data(), static_cast<int>(raw.size()));
        return;
    }

    auto raw = readFile(baseDir / img.uri);
    if (raw && !raw->empty()) decodeImage(img, raw->data(), static_cast<int>(raw->size()));
}

} // namespace

std::optional<GltfDocument> GltfReader::load(const std::filesystem::path& path) {
    Phantom::File::GLTFFileReader reader;
    if (!reader.read(path)) {
        std::fprintf(stderr, "[GltfReader] Failed to load glTF via Phantom::File: %s\n", path.string().c_str());
        return std::nullopt;
    }

    return load(reader.getGLTF(), path.parent_path());
}

GltfDocument GltfReader::load(const Phantom::File::GLTFFile& src, const std::filesystem::path& baseDir) {
    GltfDocument doc;

    if (!src.textures.empty()) {
        doc.samplers.push_back(GltfSampler{});
    }

    for (const auto& t : src.textures) {
        GltfTexture tex;
        tex.samplerIndex = doc.samplers.empty() ? -1 : 0;
        tex.imageIndex   = t.imageIndex;
        doc.textures.push_back(tex);
    }

    for (const auto& m : src.materials) {
        GltfMaterial mat;
        mat.name        = m.name;
        mat.doubleSided = m.doubleSided;
        mat.pbrMetallicRoughness.baseColorFactor = glm::vec4(
            m.pbrMetallicRoughness.baseColorFactor[0],
            m.pbrMetallicRoughness.baseColorFactor[1],
            m.pbrMetallicRoughness.baseColorFactor[2],
            m.pbrMetallicRoughness.baseColorFactor[3]);
        mat.pbrMetallicRoughness.metallicFactor  = m.pbrMetallicRoughness.metallicFactor;
        mat.pbrMetallicRoughness.roughnessFactor = m.pbrMetallicRoughness.roughnessFactor;
        mat.pbrMetallicRoughness.baseColorTexture.index          = m.pbrMetallicRoughness.baseColorTextureIndex;
        mat.pbrMetallicRoughness.metallicRoughnessTexture.index  = m.pbrMetallicRoughness.metallicRoughnessTextureIndex;
        mat.normalTexture.index   = m.normalTextureIndex;
        mat.emissiveTexture.index = m.emissiveTextureIndex;
        mat.emissiveFactor = glm::vec3(m.emissiveFactor[0], m.emissiveFactor[1], m.emissiveFactor[2]);
        doc.materials.push_back(std::move(mat));
    }

    for (const auto& s : src.images) {
        GltfImage img;
        img.uri      = s.uri;
        img.mimeType = s.mimeType;
        if (!s.data.empty()) {
            decodeImage(img, s.data.data(), static_cast<int>(s.data.size()));
        } else {
            tryDecodeImageFromUri(img, baseDir);
        }
        doc.images.push_back(std::move(img));
    }

    for (const auto& sm : src.meshes) {
        GltfMesh mesh;
        mesh.name = sm.name;
        for (const auto& sp : sm.primitives) {
            GltfPrimitive prim;
            prim.materialIndex    = sp.materialIndex;
            prim.positionAccessor = appendAccessor(doc, sp.positions, GltfComponentType::Float, GltfAccessorType::Vec3);
            prim.normalAccessor   = appendAccessor(doc, sp.normals,   GltfComponentType::Float, GltfAccessorType::Vec3);
            prim.texCoord0Accessor = appendAccessor(doc, sp.texCoords, GltfComponentType::Float, GltfAccessorType::Vec2);
            prim.tangentAccessor  = appendAccessor(doc, sp.tangents,  GltfComponentType::Float, GltfAccessorType::Vec4);
            prim.jointsAccessor   = appendAccessor(doc, sp.joints,    GltfComponentType::UnsignedInt, GltfAccessorType::Vec4);
            prim.weightsAccessor  = appendAccessor(doc, sp.weights,  GltfComponentType::Float, GltfAccessorType::Vec4);
            prim.indicesAccessor  = appendAccessor(doc, sp.indices,   GltfComponentType::UnsignedInt, GltfAccessorType::Scalar);
            for (const auto& targetDeltas : sp.targets) {
                GltfMorphTarget target;
                target.positionAccessor = appendAccessor(doc, targetDeltas, GltfComponentType::Float, GltfAccessorType::Vec3);
                prim.targets.push_back(target);
            }
            mesh.primitives.push_back(prim);
        }
        mesh.weights = sm.morphWeights;
        doc.meshes.push_back(std::move(mesh));
    }

    for (const auto& sn : src.nodes) {
        GltfNode node;
        node.name      = sn.name;
        node.meshIndex = sn.meshIndex;
        node.skin      = sn.skin;
        node.children  = sn.children;
        node.hasMatrix = sn.hasMatrix;
        if (sn.hasMatrix) {
            node.matrix = glm::make_mat4(sn.matrix.data());
        }
        node.translation = glm::vec3(sn.translation[0], sn.translation[1], sn.translation[2]);
        node.rotation    = glm::vec4(sn.rotation[0],    sn.rotation[1],    sn.rotation[2],    sn.rotation[3]);
        node.scale       = glm::vec3(sn.scale[0],       sn.scale[1],       sn.scale[2]);
        node.weights     = sn.morphWeights;
        doc.nodes.push_back(std::move(node));
    }

    for (const auto& sk : src.skins) {
        GltfSkin skin;
        skin.name         = sk.name;
        skin.joints       = sk.joints;
        skin.skeletonRoot = sk.skeletonRoot;
        skin.inverseBindMatrices.reserve(sk.inverseBindMatrices.size());
        for (const auto& m : sk.inverseBindMatrices) {
            skin.inverseBindMatrices.push_back(glm::make_mat4(m.data()));
        }
        doc.skins.push_back(std::move(skin));
    }

    for (const auto& ss : src.scenes) {
        GltfScene scene;
        scene.name  = ss.name;
        scene.nodes = ss.nodes;
        doc.scenes.push_back(std::move(scene));
    }

    doc.defaultScene = src.defaultScene;
    return doc;
}
