#pragma once

#include "VolumeNode.h"

namespace Phantom {
	namespace Volume {

		template<typename T>
		class VolumeCell
		{
		public:
			VolumeCell() = default;

			VolumeNode<T>* v000 = nullptr;
			VolumeNode<T>* v100 = nullptr;
			VolumeNode<T>* v010 = nullptr;
			VolumeNode<T>* v110 = nullptr;
			VolumeNode<T>* v001 = nullptr;
			VolumeNode<T>* v101 = nullptr;
			VolumeNode<T>* v011 = nullptr;
			VolumeNode<T>* v111 = nullptr;

			T getValueAt(const double fx, const double fy, const double fz) const;

			Math::Vector3df getGradientAt(const double fx, const double fy, const double fz) const;

		private:
			Math::Vector3df position;
		};

	}
}