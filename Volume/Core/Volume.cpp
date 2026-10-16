#include "Volume.h"

using namespace Phantom::Math;
using namespace Phantom::Volume;

template<typename T>
Volume<T>::Volume() :
	box(Box3df())
{}

template<typename T>
Volume<T>::Volume(const Box3df& box, const std::array<size_t, 3>& res) :
	box(box),
	resolutions(res)
{
	nodes.resize(res[0]);
	for (int i = 0; i < res[0]; ++i) {
		nodes[i].resize(res[1]);
		for (int j = 0; j < res[1]; ++j) {
			nodes[i][j].resize(res[2]);
		}
	}
}

template<typename T>
Vector3df Volume<T>::getCellPosition(const size_t i, const size_t j, const size_t k) const
{
	const auto& length = getCellLength();
	const auto cx = length.x * i;
	const auto cy = length.y * j;
	const auto cz = length.z * k;
	return box.getMin() + length * 0.5f + Math::Vector3df(cx, cy, cz);
}

template<typename T>
std::array<size_t, 3> Volume<T>::getIndexFromPosition(const Vector3df& position)
{
	const auto p = position - box.getMin();
	const auto length = getCellLength();
	const auto ix = static_cast<size_t>(p.x / length.x);
	const auto iy = static_cast<size_t>(p.y / length.y);
	const auto iz = static_cast<size_t>(p.z / length.z);
	return { ix, iy, iz };
}

template<typename T>
Vector3df Volume<T>::getCellLength() const
{
	const auto& length = box.getLength();
	//const auto& resolutions = nodes.getResolutions();
	const auto dx = length.x / static_cast<double>(resolutions[0]);
	const auto dy = length.y / static_cast<double>(resolutions[1]);
	const auto dz = length.z / static_cast<double>(resolutions[2]);
	return Math::Vector3df(dx, dy, dz);
}

template<typename T>
T Volume<T>::getValue(const Index3d& index) const
{
	return nodes[index[0]][index[1]][index[2]];
}

template<typename T>
void Volume<T>::setValue(const Index3d& index, const T value)
{
	nodes[index[0]][index[1]][index[2]] = value;
}

//template class Volume<bool>;
template class Phantom::Volume::Volume<float>;
template class Phantom::Volume::Volume<double>;