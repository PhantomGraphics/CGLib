#include "ParticleSystemPresenter.h"
#include "ParticleSystemScene.h"

using namespace Phantom::Graphics;
using namespace Phantom::Scene;

void ParticleSystemPresenter::build()
{
	vbo.position.create();
	vbo.size.create();
	vbo.color.create();
}

void ParticleSystemPresenter::send()
{
	Shader::VertexBuffer<float> position;
	Shader::VertexBuffer<float> color;
	Shader::VertexBuffer<float> size;

	const auto& particles = model->getShape()->getParticles();
	for (auto& p : particles) {
		position.add(p->getPosition());
		color.add(ColorRGBAf(1, 0, 0, 0));
		size.add(1);
	}

	vbo.position.send(position);
	vbo.color.send(color);
	vbo.size.send(size);

	count = static_cast<int>( particles.size() );
}

void ParticleSystemPresenter::render(const Graphics::Camera& camera)
{
	view->buffer.position = &vbo.position;
	view->buffer.size = &vbo.size;
	view->buffer.color = &vbo.color;
	view->buffer.count = count;
	view->buffer.modelViewMatrix = camera.getModelViewMatrix();
	view->buffer.projectionMatrix = camera.getProjectionMatrix();

	view->render();
}
