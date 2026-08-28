#pragma once

#include "TriangleMesh.h"
#include "CGLib/Util/UnCopyable.h"
#include <memory>

namespace Phantom {
	namespace Math {
		template<typename T>
		class ISurface3d;
		template<typename T>
		class IVolume3d;
	}
	namespace Shape {

/// @brief Builder that tessellates geometric primitives into a TriangleMesh.
class TriangleMeshBuilder : private UnCopyable
{
public:
	TriangleMeshBuilder();

	/// @brief Tessellate a 3D surface and add the resulting triangles.
	/// @param sphere The surface to tessellate.
	/// @param unum   Number of divisions in the u direction.
	/// @param vnum   Number of divisions in the v direction.
	void add(const Math::ISurface3d<float>& sphere, const int unum, const int vnum);

	/// @brief Tessellate the isosurface of a 3D volume and add the resulting triangles.
	/// @param volume The volume to tessellate.
	/// @param unum   Number of divisions in the u direction.
	/// @param vnum   Number of divisions in the v direction.
	/// @param wnum   Number of divisions in the w direction.
	void add(const Math::IVolume3d<float>& volume, const int unum, const int vnum, const int wnum);

	/// @brief Build and return the resulting TriangleMesh.
	/// @return Owning pointer to the constructed TriangleMesh.
	std::unique_ptr<TriangleMesh> build();

private:
	std::vector<TriangleFace> faces;
};

	}
}
