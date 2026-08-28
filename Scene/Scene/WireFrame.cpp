#include "WireFrame.h"

using namespace Phantom::Math;
using namespace Phantom::Shape;

void WireFrame::add(std::unique_ptr<IVertex> v)
{
	this->vertices.push_back(std::move(v));
}

void WireFrame::addEdge(const Edge& edge)
{
	this->edges.push_back(edge);
}

Box3df WireFrame::getBoundingBox() const
{
	if (vertices.empty()) {
		return Box3df::createDegeneratedBox();
	}
	Math::Box3df bb(vertices.front()->getPosition());
	for (auto& p : vertices) {
		bb.add(p->getPosition());
	}
	return bb;
}

