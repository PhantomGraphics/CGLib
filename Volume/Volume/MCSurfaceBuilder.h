#pragma once

#include "CGLib/Math/Vector3d.h"
#include "CGLib/Math/Triangle3d.h"
#include "CGLib/Math/Box3d.h"
#include "Volume.h"
#include "SparseVolumeTree/SparseVolume.h"

#include "MCCell.h"

namespace Phantom {
	namespace Volume {

class MCSurfaceBuilder
{
public:
	void build(const Volume<float>& volume, const float isoLevel);

	void build(const SparseVolumef& sparse, float isoLevel);

	int march(const MCCell& cell, const float isoLevel);

	std::vector<Math::Triangle3df> getTriangles() const { return triangles; }

private:
	Math::Vector3df getInterpolatedPosition(const float isolevel, const MCCell::Vertex& v1, const MCCell::Vertex& v2);

	std::vector<Math::Triangle3df> triangles;
};

	}
}