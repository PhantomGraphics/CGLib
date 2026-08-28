#pragma once

#include <array>
#include "CGLib/Math/Vector3d.h"

namespace Phantom {
	namespace Volume {

struct MCCell
{
	struct Vertex
	{
		Vertex()
		{}

		Vertex(const Math::Vector3df& position, const float value) :
			position(position),
			value(value)
		{}

		Math::Vector3df position;
		float value;
	};

	MCCell()
	{}

	explicit MCCell(const std::array<Vertex, 8>& vertices) :
		vertices(vertices)
	{
	}

	std::array<Vertex, 8> vertices;
};

	}
}