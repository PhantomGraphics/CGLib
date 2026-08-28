#pragma once

#include <vector>
#include <memory>

#include "../Scene/IVertex.h"
#include "CGLib/Math/Box3d.h"

namespace Phantom {
	namespace Shape {

		/// @brief A wire-frame shape composed of vertices and edges.
		class WireFrame
		{
		public:
			/// @brief An edge defined by the indices of its two endpoint vertices.
			struct Edge
			{
				Edge() = default;

				/// @brief Construct an edge from start and end vertex indices.
				/// @param startIndex Index of the start vertex.
				/// @param endIndex   Index of the end vertex.
				Edge(const unsigned int startIndex, const unsigned int endIndex) :
					startIndex(startIndex),
					endIndex(endIndex)
				{}

				unsigned int startIndex; ///< Index of the start vertex.
				unsigned int endIndex;   ///< Index of the end vertex.
			};

			WireFrame() = default;

			/// @brief Add a vertex to the wire frame.
			/// @param v Owning pointer to the vertex to add.
			void add(std::unique_ptr<IVertex> v);

			/// @brief Get all vertices in the wire frame.
			/// @return Const reference to the internal vertex vector.
			const std::vector<std::unique_ptr<IVertex>>& getVertices() const { return vertices; }

			/// @brief Add an edge to the wire frame.
			/// @param edge The edge to add (references vertex indices).
			void addEdge(const Edge& edge);

			/// @brief Get a copy of all edges in the wire frame.
			/// @return Vector of edges.
			std::vector<Edge> getEdges() const { return edges; }

			/// @brief Compute the axis-aligned bounding box of all vertices.
			/// @return Bounding box enclosing every vertex position.
			Math::Box3df getBoundingBox() const;

		private:
			std::vector<Edge> edges;
			std::vector<std::unique_ptr<Shape::IVertex>> vertices;

		};
	}
}
