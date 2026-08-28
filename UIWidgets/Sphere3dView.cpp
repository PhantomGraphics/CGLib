#include "Sphere3dView.h"

using namespace Phantom::Math;
using namespace Phantom::UI;

Sphere3dView::Sphere3dView(const std::string& name) :
	IView(name),
	center("Center"),
	radius("Radius", 1.0f)
{
	add(&center);
	add(&radius);
}

Sphere3dView::Sphere3dView(const std::string& name, const Sphere3df& value) :
	Sphere3dView(name)
{
	setValue(value);
}

Sphere3df Sphere3dView::getValue() const
{
	return Math::Sphere3df(center.getValue(), radius.getValue());
}

void Sphere3dView::setValue(const Sphere3df& value)
{
	this->center.setValue(value.getCenter());
	this->radius.setValue(value.getRadius());
}