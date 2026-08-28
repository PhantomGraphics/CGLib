#pragma once

#include "GltfDocument.h"

#include <cstring>
#include <vector>

namespace Phantom::Gltf {

// Appends `values` as a brand-new buffer+bufferView+accessor triplet to `doc` and returns the
// new accessor's index (-1 if `values` is empty). Used to synthesize accessors for data that
// didn't come from an actual glTF file's binary chunk (e.g. GltfReader converting from
// Phantom::File::GLTFFile's already-flattened per-vertex arrays, or SkeletonGltfConverter
// building a document directly from a Skeleton/SkinnedMesh).
template<typename T>
int appendAccessor(GltfDocument& doc,
                    const std::vector<T>& values,
                    GltfComponentType componentType,
                    GltfAccessorType accessorType)
{
    if (values.empty()) return -1;

    GltfBuffer buffer;
    buffer.data.resize(values.size() * sizeof(T));
    std::memcpy(buffer.data.data(), values.data(), buffer.data.size());
    const int bufferIndex = static_cast<int>(doc.buffers.size());
    doc.buffers.push_back(std::move(buffer));

    GltfBufferView view;
    view.bufferIndex = bufferIndex;
    view.byteOffset  = 0;
    view.byteLength  = doc.buffers[bufferIndex].data.size();
    view.byteStride  = 0;
    const int viewIndex = static_cast<int>(doc.bufferViews.size());
    doc.bufferViews.push_back(view);

    GltfAccessor accessor;
    accessor.bufferViewIndex = viewIndex;
    accessor.byteOffset      = 0;
    accessor.componentType   = componentType;
    accessor.type            = accessorType;
    accessor.count           = values.size();
    const int accessorIndex  = static_cast<int>(doc.accessors.size());
    doc.accessors.push_back(accessor);

    return accessorIndex;
}

} // namespace Phantom::Gltf
