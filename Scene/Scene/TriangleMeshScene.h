#pragma once

#include "CGLib/Scene/Scene/SceneBase.h"
#include "TriangleMesh.h"

#include <vector>
#include <memory>

namespace Phantom {
	namespace Scene {

/// @brief Scene node that holds a TriangleMesh shape.
class TriangleMeshScene : public SceneBase
{
public:
	/// @brief Assign a triangle mesh to this scene node.
	/// @param shape Owning pointer to the triangle mesh.
	void setShape(std::unique_ptr<Shape::TriangleMesh> shape) { this->shape = std::move(shape); }

	/// @brief Get a non-owning pointer to the contained triangle mesh.
	/// @return Raw pointer to the triangle mesh, or nullptr if not set.
	Shape::TriangleMesh* getShape() { return shape.get(); }

	/// @brief Compute the bounding box of the contained triangle mesh.
	/// @return Axis-aligned bounding box of the mesh.
	Math::Box3df getBoundingBox() const override { return shape->getBoundingBox(); }

private:
	std::unique_ptr<Shape::TriangleMesh> shape;
};

	}
}
