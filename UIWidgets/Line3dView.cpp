#include "Line3dView.h"

using namespace Phantom::Math;
using namespace Phantom::UI;

Line3dView::Line3dView(const std::string& name) :
	IView(name),
	startView("Start", Vector3df(0,0,0)),
	endView("End", Vector3df(1,0,0))
{
	add(&startView);
	add(&endView);
}

Line3dView::Line3dView(const std::string& name, const Line3df& value) :
	IView(name),
	startView("Start", value.getStart()),
	endView("End", value.getEnd())
{
	add(&startView);
	add(&endView);
}

Line3df Line3dView::getValue() const
{
	return Line3df(startView.getValue(), endView.getValue());
}

void Line3dView::setValue(const Line3df& value)
{
	this->startView.setValue(value.getStart());
	this->endView.setValue(value.getEnd());
}
