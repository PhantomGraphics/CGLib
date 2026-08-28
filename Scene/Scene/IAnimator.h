#pragma once

namespace Phantom {
	namespace Scene {

/// @brief Interface for scene animators.
///
/// Implement this interface to define per-step animation logic
/// that advances the simulation or animation state by one tick.
class IAnimator
{
public:
	~IAnimator() {};

	/// @brief Advance the animation by one step.
	virtual void step() = 0;
};

	}
}
