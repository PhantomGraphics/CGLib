#pragma once

#include "CGLib/Math/Triangle3d.h"
#include "CGLib/Math/Box3d.h"

#include <vector>

namespace Phantom {
	namespace Shape {

/// @brief A triangle face storing geometry and a surface normal.
class TriangleFace
{
public:
	TriangleFace() = default;

	/// @brief Construct a face from a triangle; normal is computed from the triangle.
	/// @param triangle The triangle geometry.
	explicit TriangleFace(const Math::Triangle3df& triangle);

	/// @brief Construct a face from a triangle and an explicit normal.
	/// @param triangle The triangle geometry.
	/// @param normal   Surface normal (double precision).
	TriangleFace(const Math::Triangle3df& triangle, const Math::Vector3dd& normal) :
		triangle(triangle),
		normal(normal)
	{}

	Math::Triangle3df triangle; ///< The triangle geometry.
	Math::Vector3dd normal;     ///< Surface normal of the face.
};

/// @brief A mesh composed of triangle faces.
class TriangleMesh
{
public:
	TriangleMesh();

	~TriangleMesh();

	/// @brief Remove all faces from the mesh.
	void clear();

	/// @brief Compute the axis-aligned bounding box of the mesh.
	/// @return Bounding box enclosing all triangle vertices.
	Math::Box3df getBoundingBox() const;

	/// @brief Get a copy of all faces in the mesh.
	/// @return Vector of triangle faces.
	std::vector<TriangleFace> getFaces() const { return faces; }

	/// @brief Add a face to the mesh.
	/// @param face The triangle face to add.
	void addFace(const TriangleFace& face);

public:
	std::vector<TriangleFace> faces; ///< All triangle faces in the mesh.
};

	}
}
