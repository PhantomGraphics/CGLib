#pragma once

#include "Coord.h"
#include <array>
#include <bitset>

namespace Phantom {
namespace Volume {

// 8^3 = 512 ボクセルを密配列で保持するリーフノード。
// mValueMask でアクティブ状態を管理し、値とアクティブ状態を独立して設定できる。
template<typename T, int Log2Dim = 3>
class LeafNode {
public:
    static constexpr int DIM      = 1 << Log2Dim;       // 8
    static constexpr int SIZE     = 1 << (3 * Log2Dim); // 512
    static constexpr int DIM_MASK = DIM - 1;
    // RootNode がツリー深さを計算するために参照する
    static constexpr int TOTAL_BITS = Log2Dim;           // 3

    explicit LeafNode(const T& background) : mBackground(background) {
        mBuffer.fill(background);
    }

    // offset は coordToOffset で計算したローカルオフセット (0 〜 SIZE-1)
    void setValue(int offset, const T& value) {
        mBuffer[offset] = value;
        mValueMask.set(offset);
    }

    const T& getValue(int offset) const {
        return mValueMask.test(offset) ? mBuffer[offset] : mBackground;
    }

    void setActive(int offset, bool active) {
        if (active) mValueMask.set(offset);
        else        mValueMask.reset(offset);
    }

    bool isActive(int offset)  const { return mValueMask.test(offset); }
    int  getActiveCount()      const { return static_cast<int>(mValueMask.count()); }
    bool isEmpty()             const { return mValueMask.none(); }

    // アクティブなボクセルを列挙する。callback: void(const Coord& worldCoord, const T& value)
    template<typename Func>
    void forEachActive(const Coord& origin, Func&& callback) const {
        if (mValueMask.none()) return;
        for (int i = 0; i < SIZE; ++i) {
            if (!mValueMask.test(i)) continue;
            int lx = i / (DIM * DIM);
            int ly = (i / DIM) % DIM;
            int lz =  i % DIM;
            callback(Coord(origin.x + lx, origin.y + ly, origin.z + lz), mBuffer[i]);
        }
    }

    // ワールド座標の低 Log2Dim ビットを使ってリーフ内オフセットを計算する
    static int coordToOffset(const Coord& coord) {
        return (coord.x & DIM_MASK) * DIM * DIM
             + (coord.y & DIM_MASK) * DIM
             + (coord.z & DIM_MASK);
    }

    // このリーフがカバーする起点座標（低 Log2Dim ビットをゼロクリア）
    static Coord getOrigin(const Coord& coord) {
        return Coord(coord.x & ~DIM_MASK, coord.y & ~DIM_MASK, coord.z & ~DIM_MASK);
    }

private:
    std::array<T, SIZE> mBuffer;
    std::bitset<SIZE>   mValueMask;
    T                   mBackground;
};

} // namespace Math
} // namespace Phantom
