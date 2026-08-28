#include "TriangleMesh.h"

using namespace Phantom::Math;
using namespace Phantom::Shape;

TriangleFace::TriangleFace(const Triangle3df& triangle) :
	triangle(triangle),
	normal(triangle.getNormal())
{
}

TriangleMesh::TriangleMesh()
{
}

TriangleMesh::~TriangleMesh()
{
}

void TriangleMesh::clear()
{
	faces.clear();
}

Box3df TriangleMesh::getBoundingBox() const
{
	Box3df bb = Box3df::createDegeneratedBox();
	for (auto& f : faces) {
		const auto vs = f.triangle.getVertices();
		bb.add(vs[0]);
		bb.add(vs[1]);
		bb.add(vs[2]);
	}
	return bb;
}

void TriangleMesh::addFace(const TriangleFace& face)
{
	this->faces.push_back(face);
}