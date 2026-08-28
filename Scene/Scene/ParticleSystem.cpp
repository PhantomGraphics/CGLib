#include "ParticleSystem.h"

using namespace Phantom::Math;
using namespace Phantom::Shape;

void ParticleSystem::add(std::unique_ptr<IParticle> particle)
{
	this->particles.push_back(std::move(particle));
}

Box3df ParticleSystem::getBoundingBox() const
{
	if (particles.empty()) {
		return Box3df::createDegeneratedBox();
	}
	Math::Box3df bb(particles.front()->getPosition());
	for (auto& p : particles) {
		bb.add(p->getPosition());
	}
	return bb;
}
