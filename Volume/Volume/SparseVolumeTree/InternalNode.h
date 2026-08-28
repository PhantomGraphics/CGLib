#pragma once

#include "Coord.h"
#include <array>
#include <bitset>

namespace Phantom {
namespace Volume {

// 16^3 = 4096 スロットを持つ中間ノード。
// 各スロットは子ノードポインタかタイル値のいずれかを保持する (mChildMask で判定)。
template<typename T, typename ChildType, int Log2Dim = 4>
class InternalNode {
public:
    static constexpr int DIM      = 1 << Log2Dim;       // 16
    static constexpr int SIZE     = 1 << (3 * Log2Dim); // 4096
    static constexpr int DIM_MASK = DIM - 1;
    // このノード + 子ノードが合計でカバーするビット幅 (RootNode が参照する)
    static constexpr int TOTAL_BITS = Log2Dim + ChildType::TOTAL_BITS; // 7

    explicit InternalNode(const T& background) : mBackground(background) {
        for (int i = 0; i < SIZE; ++i) {
            mNodes[i].value = background;
        }
    }

    ~InternalNode() {
        for (int i = 0; i < SIZE; ++i) {
            if (mChildMask.test(i)) {
                delete mNodes[i].child;
            }
        }
    }

    // 値を設定する。対応するスロットがタイルなら子ノードを生成してから委譲する。
    void setValue(const Coord& coord, const T& value, const T& background) {
        int offset = coordToOffset(coord);
        if (!mChildMask.test(offset)) {
            expandTile(offset);
        }
        mNodes[offset].child->setValue(ChildType::coordToOffset(coord), value);
    }

    const T& getValue(const Coord& coord) const {
        int offset = coordToOffset(coord);
        if (mChildMask.test(offset)) {
            return mNodes[offset].child->getValue(ChildType::coordToOffset(coord));
        }
        return mValueMask.test(offset) ? mNodes[offset].value : mBackground;
    }

    bool isActive(const Coord& coord) const {
        int offset = coordToOffset(coord);
        if (mChildMask.test(offset)) {
            return mNodes[offset].child->isActive(ChildType::coordToOffset(coord));
        }
        return mValueMask.test(offset);
    }

    int getActiveCount() const {
        int count = 0;
        for (int i = 0; i < SIZE; ++i) {
            if (mChildMask.test(i)) {
                count += mNodes[i].child->getActiveCount();
            } else if (mValueMask.test(i)) {
                // タイルはサブツリー全体がアクティブ
                count += ChildType::SIZE;
            }
        }
        return count;
    }

    // スロット全体を同一値で埋め、子ノードを削除する
    void setTile(int offset, const T& value, bool active) {
        if (mChildMask.test(offset)) {
            delete mNodes[offset].child;
            mChildMask.reset(offset);
        }
        mNodes[offset].value = value;
        if (active) mValueMask.set(offset);
        else        mValueMask.reset(offset);
    }

    bool isChildMaskOn(int offset) const { return mChildMask.test(offset); }
    bool isValueMaskOn(int offset) const { return mValueMask.test(offset); }

    ChildType*       getChild(int offset)       { return mNodes[offset].child; }
    const ChildType* getChild(int offset) const { return mNodes[offset].child; }

    // ValueAccessor 用: 子ノードを探す (なければ nullptr)
    const ChildType* probeChild(const Coord& coord) const {
        int offset = coordToOffset(coord);
        if (!mChildMask.test(offset)) return nullptr;
        return mNodes[offset].child;
    }
    ChildType* probeChild(const Coord& coord) {
        int offset = coordToOffset(coord);
        if (!mChildMask.test(offset)) return nullptr;
        return mNodes[offset].child;
    }

    // ValueAccessor 用: 子ノードを取得する (なければ生成)
    ChildType* touchChild(const Coord& coord, const T& /*background*/) {
        int offset = coordToOffset(coord);
        if (!mChildMask.test(offset)) {
            expandTile(offset);
        }
        return mNodes[offset].child;
    }

    // callback: void(const Coord& worldCoord, const T& value)
    template<typename Func>
    void forEachActive(const Coord& origin, Func&& callback) const {
        int childDim = 1 << ChildType::TOTAL_BITS;
        for (int i = 0; i < SIZE; ++i) {
            if (!mChildMask.test(i) && !mValueMask.test(i)) continue;

            int ix = i / (DIM * DIM);
            int iy = (i / DIM) % DIM;
            int iz =  i % DIM;
            Coord childOrigin(
                origin.x + ix * childDim,
                origin.y + iy * childDim,
                origin.z + iz * childDim
            );

            if (mChildMask.test(i)) {
                mNodes[i].child->forEachActive(childOrigin, callback);
            } else {
                // アクティブタイル: 子ノード領域の全ボクセルを展開して列挙する
                for (int lx = 0; lx < ChildType::DIM; ++lx) {
                    for (int ly = 0; ly < ChildType::DIM; ++ly) {
                        for (int lz = 0; lz < ChildType::DIM; ++lz) {
                            callback(
                                Coord(childOrigin.x + lx,
                                      childOrigin.y + ly,
                                      childOrigin.z + lz),
                                mNodes[i].value);
                        }
                    }
                }
            }
        }
    }

    // 子ノードが担当するビット数分だけシフトし、このノードのローカルオフセットを返す
    int coordToOffset(const Coord& coord) const {
        int x = (coord.x >> ChildType::TOTAL_BITS) & DIM_MASK;
        int y = (coord.y >> ChildType::TOTAL_BITS) & DIM_MASK;
        int z = (coord.z >> ChildType::TOTAL_BITS) & DIM_MASK;
        return x * DIM * DIM + y * DIM + z;
    }

private:
    // タイルスロットを子ノードに展開する
    void expandTile(int offset) {
        T tileVal      = mNodes[offset].value;
        bool wasActive = mValueMask.test(offset);
        ChildType* child = new ChildType(tileVal);
        if (wasActive) {
            for (int i = 0; i < ChildType::SIZE; ++i) {
                child->setValue(i, tileVal);
            }
        }
        mChildMask.set(offset);
        mValueMask.reset(offset);
        mNodes[offset].child = child;
    }

    union NodeUnion {
        ChildType* child;
        T          value;
    };

    std::array<NodeUnion, SIZE> mNodes;
    std::bitset<SIZE>           mChildMask; // 1 = 子ノード、0 = タイル
    std::bitset<SIZE>           mValueMask; // タイルのアクティブ状態
    T                           mBackground;
};

} // namespace Math
} // namespace Phantom
