#include "Ellipsoid3dView.h"

using namespace Phantom::Math;
using namespace Phantom::UI;

Ellipsoid3dView::Ellipsoid3dView(const std::string& name) :
	IView(name),
	centerView("Center", Vector3df(0,0,0)),
	uvecView("UVec", Vector3df(1,0,0)),
	vvecView("VVec", Vector3df(0,1,0)),
	wvecView("WVec", Vector3df(0,0,1))
{
	add(&centerView);
	add(&uvecView);
	add(&vvecView);
	add(&wvecView);
}

Ellipsoid3dView::Ellipsoid3dView(const std::string& name, const Ellipsoid3df& value) :
	Ellipsoid3dView(name)
{
	setValue(value);
}

Ellipsoid3df Ellipsoid3dView::getValue() const
{
	return Ellipsoid3df(this->centerView.getValue(), this->uvecView.getValue(), this->vvecView.getValue(), this->wvecView.getValue());
}

void Ellipsoid3dView::setValue(const Ellipsoid3df& value)
{
	this->centerView.setValue(value.getCenter());
	this->uvecView.setValue(value.getUVec());
	this->vvecView.setValue(value.getVVec());
	this->wvecView.setValue(value.getWVec());
}