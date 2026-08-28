#pragma once

#include "GltfTypes.h"
#include <vector>
#include <string>

namespace Phantom::Gltf
{

// Owns all glTF resources loaded from a single file
struct GltfDocument {
    std::vector<GltfBuffer>     buffers;
    std::vector<GltfBufferView> bufferViews;
    std::vector<GltfAccessor>   accessors;
    std::vector<GltfImage>      images;
    std::vector<GltfSampler>    samplers;
    std::vector<GltfTexture>    textures;
    std::vector<GltfMaterial>   materials;
    std::vector<GltfMesh>       meshes;
    std::vector<GltfNode>       nodes;
    std::vector<GltfScene>      scenes;
    std::vector<GltfSkin>       skins;
    std::vector<GltfAnimation>  animations;
    int defaultScene = 0;
};

}