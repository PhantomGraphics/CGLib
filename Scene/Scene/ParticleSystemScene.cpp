#include "ParticleSystemScene.h"

#include <cassert>

using namespace Phantom::Math;
using namespace Phantom::Shape;
using namespace Phantom::Scene;


ParticleSystemScene::ParticleSystemScene()
{
	//assert(false);
}

Box3df ParticleSystemScene::getBoundingBox() const
{
	return shape->getBoundingBox();
}