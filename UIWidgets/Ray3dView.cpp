#include "Ray3dView.h"

using namespace Phantom::Math;
using namespace Phantom::UI;

Ray3dView::Ray3dView(const std::string& name) :
	IView(name),
	originView("Origin", Math::Vector3dd(0, 0, 0)),
	directionView("Direction", Math::Vector3dd(0, 0, 1))
{
	add(&originView);
	add(&directionView);
}

Ray3dView::Ray3dView(const std::string& name, const Math::Ray3df& value) :
	IView(name),
	originView("Origin", Math::Vector3dd(0, 0, 0)),
	directionView("Direction", Math::Vector3dd(0, 0, 1))
{
	add(&originView);
	add(&directionView);
}

Ray3df Ray3dView::getValue() const
{
	return Math::Ray3df(originView.getValue(), directionView.getValue());
}

void Ray3dView::setValue(const Ray3df& value)
{
	this->originView.setValue(value.getOrigin());
	this->directionView.setValue(value.getDirection());
}
