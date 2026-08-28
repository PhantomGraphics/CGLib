#pragma once

#include <vector>
#include "../../../CGLib/Math/Vector3d.h"

namespace Phantom {
    namespace Space {

        /**
         * @brief Describes a triangular face by indices into a vertex array.
         */
        struct Face {
            /// Index of the first vertex in the parent vertex array.
            int v1;
            /// Index of the second vertex in the parent vertex array.
            int v2;
            /// Index of the third vertex in the parent vertex array.
            int v3;
        };

        /**
         * @brief Samples points inside a closed triangular polygon mesh.
         *
         * Given a mesh described by vertices and triangular faces, generates 3D sample
         * points that lie inside the mesh volume using a uniform grid approach.
         *
         * Inside/outside determination is done via ray casting: for each candidate grid
         * point a ray is cast in a fixed direction and the number of triangle intersections
         * is counted (even = outside, odd = inside).
         *
         * Typical usage:
         * @code
         *   PolygonSampler sampler(vertices, faces);
         *   auto points = sampler.generateUniformGrid(0.1);
         * @endcode
         *
         * @note The sampler holds const references to the vertex and face arrays.
         *       The arrays must remain valid for the lifetime of the sampler.
         */
        class PolygonSampler {
        private:
            const std::vector<Math::Vector3df>& vertices;
            const std::vector<Face>& faces;

            bool rayIntersectsTriangle(const Math::Vector3df& p, const Face& face) const;

            bool isInside(const Math::Vector3df& p) const;

        public:
            /**
             * @brief Constructs a PolygonSampler over the given mesh.
             * @param v Const reference to the vertex array.
             * @param f Const reference to the face (triangle index) array.
             */
            PolygonSampler(const std::vector<Math::Vector3df>& v, const std::vector<Face>& f)
                : vertices(v), faces(f) {
            }

            /**
             * @brief Generates a uniform grid of sample points inside the mesh.
             *
             * Iterates over the mesh's axis-aligned bounding box with the given spacing,
             * tests each grid point against the mesh interior using ray casting,
             * and returns those that are inside.
             *
             * @param spacing Distance between adjacent grid points along each axis.
             * @return Vector of 3D positions lying inside the mesh.
             */
            std::vector<Math::Vector3df> generateUniformGrid(double spacing) const;

        };

    }
}
