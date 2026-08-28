#pragma once

#include "GltfDocument.h"
#include <cstring>

namespace Phantom::Gltf {

    // CPU-side helper for reading typed elements out of a glTF accessor
    class GltfAccessorView {
    public:
        GltfAccessorView(const GltfDocument& doc, int accessorIndex);

        size_t count() const { return count_; }
        GltfAccessorType type() const { return type_; }

        template<typename T>
        T get(size_t i) const {
            T val{};
            const uint8_t* src = ptr_ + i * stride_;
            std::memcpy(&val, src, sizeof(T));
            return val;
        }

    private:
        const uint8_t* ptr_ = nullptr;
        size_t           stride_ = 0;
        size_t           count_ = 0;
        GltfAccessorType type_ = GltfAccessorType::Scalar;
    };

}