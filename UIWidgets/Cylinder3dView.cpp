#include "Cylinder3dView.h"

using namespace Phantom::Math;
using namespace Phantom::UI;

Cylinder3dView::Cylinder3dView(const std::string& name) :
	IView(name),
	bottomView("Bottom", Vector3df(0, 0, 0)),
	uvecView("UVec", Vector3df(1, 0, 0)),
	vvecView("VVec", Vector3df(0, 1, 0)),
	wvecView("WVec", Vector3df(0, 0, 1))
{
	add(&bottomView);
	add(&uvecView);
	add(&vvecView);
	add(&wvecView);
}

Cylinder3dView::Cylinder3dView(const std::string& name, const Cylinder3df& value) :
	Cylinder3dView(name)
{
	setValue(value);
}

Cylinder3df Cylinder3dView::getValue() const
{
	return Cylinder3df(this->bottomView.getValue(), this->uvecView.getValue(), this->vvecView.getValue(), this->wvecView.getValue());
}

void Cylinder3dView::setValue(const Cylinder3df& value)
{
	this->bottomView.setValue(value.getBottom());
	this->uvecView.setValue(value.getUVec());
	this->vvecView.setValue(value.getVVec());
	this->wvecView.setValue(value.getWVec());
}