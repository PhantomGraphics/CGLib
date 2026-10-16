#include "GltfAccessorView.h"
#include <cstdio>
#include <array>

using namespace Phantom::Gltf;

GltfAccessorView::GltfAccessorView(const GltfDocument& doc, int accessorIndex) {
    if (accessorIndex < 0 || accessorIndex >= static_cast<int>(doc.accessors.size())) {
        std::fprintf(stderr, "[GltfAccessorView] invalid accessor index: %d\n", accessorIndex);
        return; // leaves ptr_/stride_/count_ at their zero-initialized defaults
    }

    const auto& acc = doc.accessors[accessorIndex];
    count_ = acc.count;
    type_  = acc.type;

    int compCount = componentCount(acc.type);
    int compSize  = componentByteSize(acc.componentType);
    size_t elementSize = static_cast<size_t>(compCount * compSize);

    if (acc.bufferViewIndex < 0) {
        // sparse accessor - not supported yet, use zero data
        ptr_    = nullptr;
        stride_ = elementSize;
        return;
    }

    const auto& bv  = doc.bufferViews[acc.bufferViewIndex];
    const auto& buf = doc.buffers[bv.bufferIndex];

    ptr_    = buf.data.data() + bv.byteOffset + acc.byteOffset;
    stride_ = (bv.byteStride > 0) ? static_cast<size_t>(bv.byteStride) : elementSize;
}

uint32_t Phantom::Gltf::readIndexValue(const GltfAccessorView& view, const GltfAccessor& accessor, size_t i) {
    switch (accessor.componentType) {
        case GltfComponentType::UnsignedByte:  return view.get<uint8_t>(i);
        case GltfComponentType::UnsignedShort: return view.get<uint16_t>(i);
        default:                               return view.get<uint32_t>(i);
    }
}

glm::uvec4 Phantom::Gltf::readJointIndices(const GltfAccessorView& view, const GltfAccessor& accessor, size_t i) {
    switch (accessor.componentType) {
        case GltfComponentType::UnsignedByte: {
            const auto j = view.get<std::array<uint8_t, 4>>(i);
            return {j[0], j[1], j[2], j[3]};
        }
        case GltfComponentType::UnsignedShort: {
            const auto j = view.get<std::array<uint16_t, 4>>(i);
            return {j[0], j[1], j[2], j[3]};
        }
        case GltfComponentType::UnsignedInt: return view.get<glm::uvec4>(i);
        default: return glm::uvec4(0);
    }
}
