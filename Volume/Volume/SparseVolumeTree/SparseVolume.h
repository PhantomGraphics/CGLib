#pragma once

#include "RootNode.h"
#include "InternalNode.h"
#include "LeafNode.h"
#include "Coord.h"

#include "../../../../CGLib/Math/Vector3d.h"
#include "../../../../CGLib/Math/Box3d.h"
#include "../../../../CGLib/Util/UnCopyable.h"

#include <cmath>

namespace Phantom {
namespace Volume {

// Root -> InternalNode(16^3) -> LeafNode(8^3) の 2 段ツリー型
template<typename T>
using TreeType = RootNode<T, InternalNode<T, LeafNode<T, 3>, 4>>;

// ValueAccessor の前方宣言 (friend 宣言に必要)
template<typename T> class ValueAccessor;

// OpenVDB の VDB ツリー構造を参考にしたスパースボリューム。
// 大規模ボリューム (数億ボクセル規模) を少ないメモリで扱える疎なデータ構造。
template<typename T>
class SparseVolume : private Phantom::UnCopyable {
public:
    explicit SparseVolume(const T& background = T{})
        : mTree(background) {}

    // ---- 値の読み書き -----------------------------------------------

    void setValue(const Coord& index, const T& value) {
        mTree.setValue(index, value);
    }

    const T& getValue(const Coord& index) const {
        return mTree.getValue(index);
    }

    const T& getBackground() const { return mTree.getBackground(); }

    bool isActive(const Coord& index) const {
        return getValue(index) != getBackground();
    }

    // ---- アクティブボクセル反復 -------------------------------------

    // callback: void(const Coord& index, const Vector3d<T>& worldPos, const T& value)
    template<typename Func>
    void forEachActive(Func&& callback) const {
        mTree.forEachActive([&](const Coord& index, const T& value) {
            callback(index, indexToWorld(index), value);
        });
    }

    // ---- 統計 -------------------------------------------------------

    int getActiveVoxelCount() const {
        return mTree.getActiveVoxelCount();
    }

    Math::Box3d<float> getBoundingBox() const {
        Math::Box3d<float> box = Math::Box3d<float>::createDegeneratedBox();
        bool first = true;
        mTree.forEachActive([&](const Coord& index, const T&) {
            Math::Vector3d<float> pos(
                static_cast<float>(index.x) * mVoxelSize,
                static_cast<float>(index.y) * mVoxelSize,
                static_cast<float>(index.z) * mVoxelSize);
            if (first) { box = Math::Box3d<float>(pos); first = false; }
            else        { box.add(pos); }
        });
        return box;
    }

    // ---- ワールド変換 -----------------------------------------------

    void  setVoxelSize(float voxelSize) { mVoxelSize = voxelSize; }
    float getVoxelSize()          const { return mVoxelSize; }

    // インデックス座標 -> ワールド座標
    Math::Vector3d<T> indexToWorld(const Coord& index) const {
        return Math::Vector3d<T>(
            static_cast<T>(index.x) * static_cast<T>(mVoxelSize),
            static_cast<T>(index.y) * static_cast<T>(mVoxelSize),
            static_cast<T>(index.z) * static_cast<T>(mVoxelSize));
    }

    // ワールド座標 -> 最近傍インデックス座標
    Coord worldToIndex(const Math::Vector3d<T>& worldPos) const {
        return Coord(
            static_cast<int32_t>(std::round(worldPos.x / static_cast<T>(mVoxelSize))),
            static_cast<int32_t>(std::round(worldPos.y / static_cast<T>(mVoxelSize))),
            static_cast<int32_t>(std::round(worldPos.z / static_cast<T>(mVoxelSize))));
    }

    void clear() { mTree.clear(); }

private:
    friend class ValueAccessor<T>;

    TreeType<T> mTree;
    float       mVoxelSize = 1.0f;
};

using SparseVolumef = SparseVolume<float>;
using SparseVolumed = SparseVolume<double>;

} // namespace Math
} // namespace Phantom
