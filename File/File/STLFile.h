#pragma once

#include <string>
#include <vector>

#include "CGLib/Math/Triangle3d.h"

namespace Phantom {
	namespace File {

/// @brief Represents a single face in an STL file.
struct STLFace
{
	/// @brief Default constructor.
	STLFace()
	{};

	/// @brief Constructs from a triangle, computing the normal automatically.
	/// @param triangle The triangle data.
	explicit STLFace(const Math::Triangle3df& triangle) :
		STLFace(triangle, triangle.getNormal())
	{}

	/// @brief Constructs from a triangle and an explicit normal vector.
	/// @param triangle The triangle data.
	/// @param normal The face normal vector.
	STLFace(const Math::Triangle3df& triangle, const Math::Vector3df& normal) :
		triangle(triangle),
		normal(normal)
	{}

	Math::Triangle3df triangle; ///< Triangle geometry.
	Math::Vector3df normal;     ///< Face normal vector.
};

/// @brief Holds the entire contents of an STL file.
struct STLFile
{
	std::string header;           ///< STL header string.
	size_t faceCount = 0;         ///< Number of faces.
	std::vector<STLFace> faces;   ///< List of face data.
};

	}
}
