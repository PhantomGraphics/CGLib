#include "LinearOctreeIndex.h"
#include <cmath>

using namespace Phantom::Space;

LinearOctreeIndex::LinearOctreeIndex(const unsigned int level, const unsigned int number)
{
    const auto start = static_cast<unsigned int>((std::pow(8, level) - 1) / 7);
    this->index1d = start + number;
}

std::pair<unsigned int, unsigned int> LinearOctreeIndex::getLevelAndNumber() const
{
    unsigned int number = index1d;
    int level = 0;
    // ハッシュ値から所属する最小空間のモートンオーダーに変換
    while (number >= std::pow(8, level)) {
        number -= static_cast<unsigned int>(std::pow(8, level));
        level++;
    }
    return std::make_pair(level, number);
}

LinearOctreeIndex LinearOctreeIndex::getParentIndex() const
{
    const auto levelNumber = getLevelAndNumber();
    const auto parentLevel = levelNumber.first - 1;
    const auto number = levelNumber.second;
    const auto parentNumber = number >> 3;
    return LinearOctreeIndex(parentLevel, parentNumber);
}

unsigned int LinearOctreeIndex::getIndex1d() const
{
    return index1d;
}