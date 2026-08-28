#include "Circle3dView.h"

using namespace Phantom::Math;
using namespace Phantom::UI;

Circle3dView::Circle3dView(const std::string& name) :
	Circle3dView(name, Circle3df())
{
}

Circle3dView::Circle3dView(const std::string& name, const Math::Circle3df& value) :
	IView(name),
	center("Center", Math::Vector3df(0, 0, 0)),
	normal("Normal", Math::Vector3df(0, 0, 1)),
	radius("Radius", 1.0)
{
	add(&center);
	add(&normal);
	add(&radius);
}

Circle3df Circle3dView::getValue() const
{
	return Circle3df(radius.getValue(), center.getValue(), normal.getValue());
}

void Circle3dView::setValue(const Circle3df& value)
{
	this->center.setValue(value.getCenter());
	this->radius.setValue(value.getRadius());
	this->normal.setValue(value.getNormal());
}