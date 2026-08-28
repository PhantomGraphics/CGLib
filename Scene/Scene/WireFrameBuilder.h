#pragma once

#include <vector>
#include <memory>

#include "CGLib/Math/Vector3d.h"

#include "../Scene/WireFrame.h"

namespace Phantom {
	namespace Math {
		template<typename T>
		class ICurve3d;
		template<typename T>
		class ISurface3d;
		template<typename T>
		class IVolume3d;
	}
	namespace Shape {

		/// @brief Builder that samples geometric primitives into a WireFrame.
		class WireFrameBuilder
		{
		public:
			/// @brief Sample points along a 3D curve and connect them as a polyline.
			/// @param curve The curve to sample.
			/// @param unum  Number of sample points along the curve.
			void add(const Math::ICurve3d<float>& curve, int unum);

			/// @brief Sample a grid on a 3D surface and add the resulting edges.
			/// @param surface The surface to sample.
			/// @param unum    Number of sample points in the u direction.
			/// @param vnum    Number of sample points in the v direction.
			void add(const Math::ISurface3d<float>& surface, const int unum, const int vnum);

			/// @brief Sample a grid inside a 3D volume and add the resulting edges.
			/// @param volume The volume to sample.
			/// @param unum   Number of sample points in the u direction.
			/// @param vnun   Number of sample points in the v direction.
			/// @param wnum   Number of sample points in the w direction.
			void add(const Math::IVolume3d<float>& volume, const int unum, const int vnun, const int wnum);

			/// @brief Build and return the resulting WireFrame.
			/// @return Owning pointer to the constructed WireFrame.
			std::unique_ptr<WireFrame> toWireFrame();

		private:
			/// @brief Add a vertex position and return its index.
			/// @param v The position to add.
			/// @return Index of the newly added vertex.
			int createPosition(const Math::Vector3df& v);

			void addEdge(const WireFrame::Edge& e) { this->edges.push_back(e); }

			std::vector<Math::Vector3df> positions;
			std::vector<WireFrame::Edge> edges;
		};

	}
}
