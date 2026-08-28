#include "SceneGroup.h"

using namespace Phantom::Math;
using namespace Phantom::Scene;

Box3df SceneGroup::getBoundingBox() const
{
	Box3df bb = Box3df::createDegeneratedBox();
	for (auto c : children) {
		bb.add( c->getBoundingBox() );
	}
	return bb;
}