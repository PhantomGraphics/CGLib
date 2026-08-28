#pragma once

#include "../../../CGLib/Math/Vector3d.h"
#include <array>

namespace Phantom {
	namespace Volume {

		template<typename T>
		class VolumeNode
		{
		public:
			VolumeNode(const Math::Vector3df& position, const std::array<int, 3>& index) :
				position(position),
				value(0.0),
				index(index)
			{
			}

			Math::Vector3df getPosition() const { return position; }

			T getValue() const { return value; }

			void setValue(const T v) { this->value = v; }

			std::array<int, 3> getIndex() const { return index; }

			void setGradient(const Math::Vector3df& g) { this->gradient = g; }

			Math::Vector3df getGradient() const { return gradient; }

		private:
			Math::Vector3df position;
			T value;
			const std::array<int, 3> index;
			Math::Vector3df gradient;
		};
	}
}