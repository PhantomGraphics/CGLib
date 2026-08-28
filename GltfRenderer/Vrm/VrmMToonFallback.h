#pragma once

#include "VrmTypes.h"
#include "../Gltf/GltfDocument.h"
#include "../../File/File/GLTFFile.h"
#include "json.hpp"

namespace Phantom::Gltf {

// Best-effort MToon -> flat PBR flattening, applied to an already-built GltfDocument's materials
// in place. No real toon shading (rim light, shading step, outline) is reproduced -- this mirrors
// SkeletonGltfConverter's precedent of dropping MMD toon/sphere/edge shading for the same flat-PBR
// renderer (GltfSceneRenderer). See docs/todo/PLAN_mmd_gltf_unification.md's Context section for
// the same design call made there.
class VrmMToonFallback {
public:
    // VRM 1.0's MToon ("VRMC_materials_mtoon") reuses the material's own standard glTF
    // pbrMetallicRoughness.baseColorFactor/baseColorTexture as its "lit color" (no duplicate
    // color fields in the extension), so GltfReader already got that part right; only
    // metallic/roughness need forcing to a matte look. VRM 0.x's legacy "VRM/MToon" Unity shader
    // predates that convention and stores its real base color/texture inside
    // extensions.VRM.materialProperties[i] instead, so those get read and copied into
    // pbrMetallicRoughness explicitly. `src` supplies the per-material extensionsJson needed to
    // detect VRM 1.0's MToon materials (doc.materials[i] <-> src.materials[i] by index, since
    // GltfReader maps them 1:1 in file order).
    static void apply(GltfDocument& doc, const nlohmann::json& vrmRoot, VrmSpecVersion ver,
                       const Phantom::File::GLTFFile& src);
};

}
