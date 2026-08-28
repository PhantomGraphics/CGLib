#include "Box3dView.h"

using namespace Phantom::Math;
using namespace Phantom::UI;

Box3dView::Box3dView(const std::string& name) :
	IView(name),
	minView("Min", Vector3dd(0, 0, 0)),
	maxView("Max", Vector3dd(1, 1, 1))
{
	add(&minView);
	add(&maxView);
}

Box3dView::Box3dView(const std::string& name, const Box3df& value) :
	IView(name),
	minView("Min", value.getMin()),
	maxView("Max", value.getMax())
{
	add(&minView);
	add(&maxView);
}

Box3df Box3dView::getValue() const
{
	return Box3df(minView.getValue(), maxView.getValue());
}

void Box3dView::setValue(const Box3df& value)
{
	minView.setValue(value.getMin());
	maxView.setValue(value.getMax());
}