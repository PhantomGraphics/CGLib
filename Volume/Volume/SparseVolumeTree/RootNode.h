#pragma once

#include "Coord.h"
#include <unordered_map>

namespace Phantom {
namespace Volume {

// 疎なハッシュマップでルートエントリを管理するルートノード。
// 各エントリは InternalNode への子ポインタか、均一タイル値を保持する。
template<typename T, typename ChildType>
class RootNode {
public:
    // ChildType (InternalNode) が担当する総ビット幅
    static constexpr int CHILD_TOTAL_BITS = ChildType::TOTAL_BITS; // 7

    explicit RootNode(const T& background) : mBackground(background) {}

    ~RootNode() {
        clear();
    }

    void setValue(const Coord& coord, const T& value) {
        auto& entry = mTable[toRootKey(coord)];
        if (!entry.child) {
            entry.child = new ChildType(mBackground);
        }
        entry.child->setValue(coord, value, mBackground);
    }

    const T& getValue(const Coord& coord) const {
        auto it = mTable.find(toRootKey(coord));
        if (it == mTable.end()) return mBackground;
        const auto& entry = it->second;
        if (entry.child) return entry.child->getValue(coord);
        return entry.active ? entry.value : mBackground;
    }

    bool isActive(const Coord& coord) const {
        auto it = mTable.find(toRootKey(coord));
        if (it == mTable.end()) return false;
        const auto& entry = it->second;
        if (entry.child) return entry.child->isActive(coord);
        return entry.active;
    }

    int getActiveVoxelCount() const {
        int count = 0;
        for (const auto& kv : mTable) {
            const auto& entry = kv.second;
            if (entry.child) {
                count += entry.child->getActiveCount();
            } else if (entry.active) {
                // ルートタイルは ChildType のサブツリー全体がアクティブ
                // カバーするボクセル数 = 1 << (3 * CHILD_TOTAL_BITS)
                count += 1 << (3 * CHILD_TOTAL_BITS);
            }
        }
        return count;
    }

    // ValueAccessor 用: InternalNode を探す (なければ nullptr)
    const ChildType* probeChild(const Coord& coord) const {
        auto it = mTable.find(toRootKey(coord));
        if (it == mTable.end()) return nullptr;
        return it->second.child;
    }
    ChildType* probeChild(const Coord& coord) {
        auto it = mTable.find(toRootKey(coord));
        if (it == mTable.end()) return nullptr;
        return it->second.child;
    }

    // ValueAccessor 用: InternalNode を取得する (なければ生成)
    ChildType* touchChild(const Coord& coord) {
        auto& entry = mTable[toRootKey(coord)];
        if (!entry.child) {
            entry.child = new ChildType(mBackground);
        }
        return entry.child;
    }

    // callback: void(const Coord& worldCoord, const T& value)
    template<typename Func>
    void forEachActive(Func&& callback) const {
        int childDim = 1 << CHILD_TOTAL_BITS;
        for (const auto& kv : mTable) {
            const Coord& key   = kv.first;
            const auto&  entry = kv.second;
            Coord origin(
                key.x * childDim,
                key.y * childDim,
                key.z * childDim);

            if (entry.child) {
                entry.child->forEachActive(origin, callback);
            } else if (entry.active) {
                // ルートタイル: カバー領域の全ボクセルを展開する
                for (int x = 0; x < childDim; ++x)
                    for (int y = 0; y < childDim; ++y)
                        for (int z = 0; z < childDim; ++z)
                            callback(Coord(origin.x + x, origin.y + y, origin.z + z),
                                     entry.value);
            }
        }
    }

    void clear() {
        for (auto& kv : mTable) {
            delete kv.second.child;
            kv.second.child = nullptr;
        }
        mTable.clear();
    }

    const T& getBackground() const { return mBackground; }

private:
    struct RootData {
        ChildType* child  = nullptr;
        T          value  = T{};
        bool       active = false;
    };

    Coord toRootKey(const Coord& coord) const {
        return coord >> CHILD_TOTAL_BITS;
    }

    std::unordered_map<Coord, RootData, Coord::Hash> mTable;
    T mBackground;
};

} // namespace Math
} // namespace Phantom
