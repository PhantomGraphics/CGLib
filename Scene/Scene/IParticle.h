#pragma once

#include "CGLib/Math/Vector3d.h"

namespace Phantom {
	namespace Shape {

/// @brief Interface for a particle with a 3D position.
class IParticle
{
public:
	virtual ~IParticle() = default;

	/// @brief Get the position of the particle.
	/// @return 3D position as a float vector.
	virtual Math::Vector3df getPosition() const = 0;
};

/// @brief Concrete particle that stores a single 3D position.
struct Particle : public IParticle
{
public:
	/// @brief Construct a particle at the given position.
	/// @param p Initial position.
	explicit Particle(const Math::Vector3df& p) {
		this->pos = p;
	}

	/// @brief Get the position of the particle.
	/// @return 3D position as a float vector.
	Math::Vector3df getPosition() const override {
		return pos;
	}

private:
	Math::Vector3df pos;
};


	}
}
