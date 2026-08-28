#pragma once

#include "CGLib/Scene/Scene/SceneBase.h"
#include "../Scene/WireFrame.h"
#include <vector>

namespace Phantom {
	namespace Scene {

		/// @brief Scene node that holds a WireFrame shape.
		class WireFrameScene : public SceneBase
		{
		public:
			//WireFrameScene()

			/// @brief Assign a wire frame to this scene node.
			/// @param shape Owning pointer to the wire frame.
			void setShape(std::unique_ptr<Shape::WireFrame> shape) { this->shape = std::move(shape); }

			/// @brief Get a non-owning pointer to the contained wire frame.
			/// @return Raw pointer to the wire frame, or nullptr if not set.
			Shape::WireFrame* getShape() { return shape.get(); }

			/// @brief Compute the bounding box of the contained wire frame.
			/// @return Axis-aligned bounding box of all vertices.
			Math::Box3df getBoundingBox() const override { return shape->getBoundingBox(); }

		private:
			std::unique_ptr<Shape::WireFrame> shape;
		};

	}
}
