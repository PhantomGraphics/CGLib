#include "IndexedParticle.h"

//#include "SPHParticle.h"

using namespace Phantom::Math;
using namespace Phantom::Space;

void IndexedParticle::setGridId(const float effectLength)
{
	this->gridId = toGridId(this->position, effectLength);
}

int IndexedParticle::toIdX(std::array<int, 3> index)
{
	return index[2] * gridStrideZ + index[1] * gridStrideY + index[0];
}

int IndexedParticle::toGridId(const Vector3df& pos, const float effectLength)
{
	return toIdX(toIndex(pos, effectLength));
}

std::array<int, 3> IndexedParticle::toIndex(const Vector3df& pos, const float effectLength)
{
	const auto ix = static_cast<int>(pos.x / effectLength);
	const auto iy = static_cast<int>(pos.y / effectLength);
	const auto iz = static_cast<int>(pos.z / effectLength);
	return std::array<int, 3>{ix, iy, iz};
}