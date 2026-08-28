#pragma once

#include <cstdint>
#include <cstddef>

namespace Phantom {
namespace Volume {

// 整数ボクセル座標。std::unordered_map のキーとして使用するため Hash 関子をネストする。
struct Coord {
    int32_t x, y, z;

    Coord() : x(0), y(0), z(0) {}
    Coord(int32_t x, int32_t y, int32_t z) : x(x), y(y), z(z) {}

    bool operator==(const Coord& rhs) const {
        return x == rhs.x && y == rhs.y && z == rhs.z;
    }
    bool operator!=(const Coord& rhs) const { return !(*this == rhs); }

    // std::map 用
    bool operator<(const Coord& rhs) const {
        if (x != rhs.x) return x < rhs.x;
        if (y != rhs.y) return y < rhs.y;
        return z < rhs.z;
    }

    // ノード探索用ビットシフト
    Coord operator>>(int shift) const {
        return Coord(x >> shift, y >> shift, z >> shift);
    }
    // ローカルオフセット取得用マスク
    Coord operator&(int32_t mask) const {
        return Coord(x & mask, y & mask, z & mask);
    }
    Coord operator+(const Coord& rhs) const {
        return Coord(x + rhs.x, y + rhs.y, z + rhs.z);
    }

    int32_t& operator[](int i)       { return (&x)[i]; }
    int32_t  operator[](int i) const { return (&x)[i]; }

    struct Hash {
        size_t operator()(const Coord& c) const {
            size_t h = static_cast<size_t>(c.x) * 73856093u;
            h ^= static_cast<size_t>(c.y) * 19349663u;
            h ^= static_cast<size_t>(c.z) * 83492791u;
            return h;
        }
    };
};

} // namespace Math
} // namespace Phantom
