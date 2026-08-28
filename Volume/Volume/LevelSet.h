#pragma once

#include <vector>
#include "CGLib/Math/Triangle3d.h"
#include "CGLib/Math/Rectangle3d.h"
#include "../Volume/SparseVolumeTree/SparseVolume.h"

namespace Phantom
{
	namespace Volume {

		class LevelSet
		{
		public:
			LevelSet() = default;
			~LevelSet() = default;

			void setSignedDistance(const std::vector<Math::Triangle3df>& triangles, SparseVolumef& volume);

			void setSignedDistance(const Math::Box3df& box, SparseVolumef& volume, double thickness);

			void setSignedDistance(const Math::Triangle3df& triangle, SparseVolumef& volume);

			void setSignedDistance(const Math::Rectangle3df& rect, SparseVolumef& volume);

//			void setSignedDistance(const std::vector<Math::Triangle3df>& triangles, Volume<float>& volume, const float tolerance = 1e-6f);
		};
	}
}