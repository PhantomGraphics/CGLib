#pragma once

#include "CGLib/Math/Vector3d.h"

namespace Phantom {
	namespace Shape {

		/// @brief Interface for a vertex with a 3D position.
		class IVertex
		{
		public:
			virtual ~IVertex() = default;

			/// @brief Get the position of the vertex.
			/// @return 3D position as a float vector.
			virtual Math::Vector3df getPosition() const = 0;

		};

		/// @brief Concrete vertex that stores a single 3D position.
		struct Vertex : public IVertex
		{
		public:
			/// @brief Construct a vertex at the given position.
			/// @param p Initial position.
			explicit Vertex(const Math::Vector3df& p) : pos(p) {
			}

			/// @brief Get the position of the vertex.
			/// @return 3D position as a float vector.
			Math::Vector3df getPosition() const override {
				return pos;
			}

		private:
			Math::Vector3df pos;
		};


	}
}
