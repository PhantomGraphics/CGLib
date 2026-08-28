#pragma once

#include "CGLib/Renderer/Renderer/PointRenderer.h"
#include "CGLib/Shader/VertexBuffer.h"
#include "CGLib/Scene/Scene/IPresenter.h"

namespace Phantom {
	namespace Scene {
		class ParticleSystemScene;

/// @brief Presenter that renders a ParticleSystemScene as colored points.
class ParticleSystemPresenter : public IPresenter
{
public:
	/// @brief Construct the presenter with its model and renderer.
	/// @param psScene  The particle system scene to present.
	/// @param renderer The point renderer used for drawing.
	ParticleSystemPresenter(Scene::ParticleSystemScene* psScene, Phantom::Renderer::PointRenderer* renderer) :
		model(psScene),
		view(renderer)
	{
	}

	/// @brief Build CPU-side vertex data (positions, colors, sizes).
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
		Shader::VertexBufferObject size;
	};
	VBO vbo;
	int count;

	Scene::ParticleSystemScene* model;
	Phantom::Renderer::PointRenderer* view;
};
	}
}
