#include "TriangleMeshPresenter.h"

#include "TriangleMeshScene.h"

using namespace Phantom::Graphics;
using namespace Phantom::Shape;
using namespace Phantom::Scene;

void TriangleMeshPresenter::build()
{
	vbo.position.create();
	vbo.color.create();
}

void TriangleMeshPresenter::send()
{
	Shader::VertexBuffer<float> position;
	Shader::VertexBuffer<float> color;

	auto shape = model->getShape();

	const auto faces = shape->getFaces();

	for (const auto& f : faces) {
		const auto t = f.triangle;
		const auto vs = t.getVertices();
		position.add(vs[0]);
		position.add(vs[1]);
		position.add(vs[2]);
		color.add(ColorRGBAf(1, 0, 0, 0));
		color.add(ColorRGBAf(1, 0, 0, 0));
		color.add(ColorRGBAf(1, 0, 0, 0));
	}

	vbo.position.send(position);
	vbo.color.send(color);

	//const auto faces = shape->getFaces();
	int index = 0;
	for (const auto& f : faces) {
		indices.push_back(index);
		indices.push_back(index+1);
		indices.push_back(index+2);
		index += 3;
	}
}

void TriangleMeshPresenter::render(const Camera& camera)
{
	view->buffer.position = &vbo.position;
	view->buffer.color = &vbo.color;
	view->buffer.indices = indices;
	view->buffer.modelViewMatrix = camera.getModelViewMatrix();
	view->buffer.projectionMatrix = camera.getProjectionMatrix();
	//view->buffer.lineWidth = 1.0f;

	view->render();
}
