#pragma once

#include "CGLib/Renderer/Renderer/TriangleRenderer.h"
#include "CGLib/Shader/VertexBuffer.h"
#include "CGLib/Scene/Scene/IPresenter.h"

namespace Phantom {
	namespace Scene {
		class TriangleMeshScene;

/// @brief Presenter that renders a TriangleMeshScene as shaded triangles.
class TriangleMeshPresenter : public IPresenter
{
public:
	/// @brief Construct the presenter with its model and renderer.
	/// @param scene    The triangle mesh scene to present.
	/// @param renderer The triangle renderer used for drawing.
	TriangleMeshPresenter(Scene::TriangleMeshScene* scene, Phantom::Renderer::TriangleRenderer* renderer) :
		model(scene),
		view(renderer)
	{
	}

	/// @brief Build CPU-side vertex and index data from the mesh.
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

	Scene::TriangleMeshScene* model;
	Phantom::Renderer::TriangleRenderer* view;
};

	}
}
