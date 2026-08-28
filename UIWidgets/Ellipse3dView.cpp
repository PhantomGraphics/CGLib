#include "Ellipse3dView.h"

using namespace Phantom::Math;
using namespace Phantom::UI;

Ellipse3dView::Ellipse3dView(const std::string& name) :
	IView(name),
	centerView("Center", Vector3df(0, 0, 0)),
	uvecView("UVec", Vector3df(1,0,0)),
	vvecView("VVec", Vector3df(0,1,0))
{
	add(&centerView);
	add(&uvecView);
	add(&vvecView);
}

Ellipse3dView::Ellipse3dView(const std::string& name, const Ellipse3df& value) :
	Ellipse3dView(name)
{
	setValue(value);
}

Ellipse3df Ellipse3dView::getValue() const
{
	return Math::Ellipse3df(centerView.getValue(), uvecView.getValue(), vvecView.getValue());
}

void Ellipse3dView::setValue(const Ellipse3df& value)
{
	this->centerView.setValue(value.getCenter());
	this->uvecView.setValue(value.getUVec());
	this->vvecView.setValue(value.getVVec());
}