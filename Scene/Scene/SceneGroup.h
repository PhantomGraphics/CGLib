#pragma once

#include "SceneBase.h"

namespace Phantom {
	namespace Scene {

/// @brief A scene node that groups multiple child scenes.
///
/// SceneGroup acts as an interior node in the scene graph.
/// Its bounding box is the union of all its children's bounding boxes.
class SceneGroup : public SceneBase
{
public:
	/// @brief Get the bounding box that encloses all child scenes.
	/// @return Axis-aligned bounding box covering all children.
	Math::Box3df getBoundingBox() const override;

	/*
	void step() const {
		for (auto c : children) {
			c->step();
		}
	}
	*/
};
	}
}
