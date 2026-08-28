#pragma once

#include "CGLib/Math/Box3d.h"
#include "IParticle.h"
#include <vector>
#include <memory>

namespace Phantom {
	namespace Shape {

/// @brief Collection of particles forming a particle system.
class ParticleSystem
{
public:
	/// @brief Add a particle to the system.
	/// @param particle Owning pointer to the particle to add.
	void add(std::unique_ptr<Shape::IParticle> particle);

	/// @brief Get all particles in the system.
	/// @return Const reference to the internal particle vector.
	const std::vector<std::unique_ptr<IParticle>>& getParticles() const { return particles; }

	/// @brief Compute the axis-aligned bounding box of all particles.
	/// @return Bounding box enclosing every particle position.
	Math::Box3df getBoundingBox() const;

private:
	std::vector<std::unique_ptr<IParticle>> particles;
};

	}
}
