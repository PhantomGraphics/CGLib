#include "IndexedParticle.h"
#include "SpaceValidation.h"

//#include "SPHParticle.h"

using namespace Phantom::Math;
using namespace Phantom::Space;

void IndexedParticle::setGridId(const float effectLength)
{
	this->gridId = toGridId(this->position, effectLength);
}

IndexedParticle::GridId IndexedParticle::toIdX(const std::array<int, 3>& index)
{
	return static_cast<GridId>(index[2]) * gridStrideZ + static_cast<GridId>(index[1]) * gridStrideY + index[0];
}

bool IndexedParticle::tryToGridId(const Vector3df& pos, const float effectLength, GridId& out)
{
	if (!Validation::isValidCellSize(effectLength)) {
		return false;
	}
	std::array<int, 3> index;
	if (!Validation::tryToCellIndex(pos, effectLength, index)) {
		return false;
	}
	out = toIdX(index);
	return true;
}

bool IndexedParticle::trySetGridId(const float effectLength)
{
	return tryToGridId(this->position, effectLength, this->gridId);
}

IndexedParticle::GridId IndexedParticle::toGridId(const Vector3df& pos, const float effectLength)
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