#pragma once

#include "CGLib/Renderer/Renderer/LineRenderer.h"
#include "CGLib/Shader/VertexBuffer.h"
#include "CGLib/Scene/Scene/IPresenter.h"

namespace Phantom {
	namespace Scene {
		class WireFrameScene;

		/// @brief Presenter that renders a WireFrameScene as lines.
		class WireFramePresenter : public IPresenter
		{
		public:
			/// @brief Construct the presenter with its model and renderer.
			/// @param psScene  The wire-frame scene to present.
			/// @param renderer The line renderer used for drawing.
			WireFramePresenter(Scene::WireFrameScene* psScene, Phantom::Renderer::LineRenderer* renderer) :
				model(psScene),
				view(renderer)
			{
			}

			/// @brief Build CPU-side vertex and index data from the wire frame.
			void build() override;

			/// @brief Upload the built data to GPU vertex buffer objects.
			void send() override;

			/// @brief Issue the draw call using the given camera.
			/// @param camera The camera used for view/projection transforms.
			void render(const Graphics::Camera& camera) override;

		private:
			struct VBO {
				Shader::VertexBufferObject position;
				Shader::VertexBufferObject color;
			};
			VBO vbo;
			std::vector<unsigned int> indices;

			Scene::WireFrameScene* model;
			Phantom::Renderer::LineRenderer* view;
		};

	}
}
