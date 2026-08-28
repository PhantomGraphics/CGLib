#pragma once

#include "SparseVolume.h"

namespace Phantom {
namespace Volume {

// キャッシュアクセサ。直近アクセスした LeafNode と InternalNode をキャッシュし、
// 空間的局所性のあるアクセスでツリー探索をスキップして O(1) に近い速度を実現する。
// スレッドセーフではない。複数スレッドから使う場合はスレッドごとに生成すること。
template<typename T>
class ValueAccessor {
public:
    using VolumeType   = SparseVolume<T>;
    using InternalType = InternalNode<T, LeafNode<T, 3>, 4>;
    using LeafType     = LeafNode<T, 3>;

    explicit ValueAccessor(VolumeType& volume)
        : mVolume(&volume) {}

    // キャッシュを活用した高速書き込み
    void setValue(const Coord& index, const T& value) {
        if (!isCached(index)) {
            InternalType* internal = mVolume->mTree.touchChild(index);
            mCachedInternal   = internal;
            mCachedLeaf       = internal->touchChild(index, mVolume->getBackground());
            mCachedLeafOrigin = LeafType::getOrigin(index);
        }
        mCachedLeaf->setValue(LeafType::coordToOffset(index), value);
    }

    // キャッシュを活用した高速読み込み
    const T& getValue(const Coord& index) const {
        if (!isCached(index)) {
            InternalType* internal = mVolume->mTree.probeChild(index);
            if (!internal) {
                mCachedInternal = nullptr;
                mCachedLeaf     = nullptr;
                return mVolume->getBackground();
            }
            mCachedInternal = internal;
            mCachedLeaf     = internal->probeChild(index);
            if (mCachedLeaf) {
                mCachedLeafOrigin = LeafType::getOrigin(index);
            }
        }
        if (!mCachedLeaf) return mVolume->getBackground();
        return mCachedLeaf->getValue(LeafType::coordToOffset(index));
    }

    void clearCache() {
        mCachedInternal = nullptr;
        mCachedLeaf     = nullptr;
    }

private:
    // 直近アクセスと同じ LeafNode 内かどうか確認する
    bool isCached(const Coord& index) const {
        return mCachedLeaf != nullptr
            && LeafType::getOrigin(index) == mCachedLeafOrigin;
    }

    VolumeType*           mVolume;
    mutable InternalType* mCachedInternal   = nullptr;
    mutable LeafType*     mCachedLeaf       = nullptr;
    mutable Coord         mCachedLeafOrigin = Coord(0, 0, 0);
};

} // namespace Math
} // namespace Phantom
