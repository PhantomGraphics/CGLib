#pragma once

#include "CGLib/Scene/Scene/SceneBase.h"
#include "ParticleSystem.h"
#include <vector>

namespace Phantom {
	namespace Scene {

/// @brief Scene node that holds a ParticleSystem shape.
class ParticleSystemScene : public SceneBase
{
public:
	/// @brief Default constructor.
	ParticleSystemScene();

	/// @brief Assign a particle system to this scene node.
	/// @param shape Owning pointer to the particle system.
	void setShape(std::unique_ptr<Shape::ParticleSystem> shape) { this->shape = std::move(shape); }

	/// @brief Get a non-owning pointer to the contained particle system.
	/// @return Raw pointer to the particle system, or nullptr if not set.
	Shape::ParticleSystem* getShape() { return shape.get(); }

	/// @brief Compute the bounding box of the contained particle system.
	/// @return Axis-aligned bounding box of all particles.
	Math::Box3df getBoundingBox() const override;

private:
	std::unique_ptr<Shape::ParticleSystem> shape;
};
	}
}
