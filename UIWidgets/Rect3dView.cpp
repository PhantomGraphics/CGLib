#include "Rect3dView.h"

using namespace Phantom::Math;
using namespace Phantom::UI;

Rect3dView::Rect3dView(const std::string& name) :
	IView(name),
	originView("Origin", Vector3dd(0, 0, 0)),
	uvecView("UVec", Vector3dd(1, 0, 0)),
	vvecView("VVec", Vector3dd(0, 1, 0))
{
	add(&originView);
	add(&uvecView);
	add(&vvecView);
}

Rect3dView::Rect3dView(const std::string& name, const Rectangle3df& value) :
	IView(name),
	originView("Origin", value.getPosition(0,0)),
	uvecView("UVec", value.getUVec()),
	vvecView("VVec", value.getVVec())
{
	add(&originView);
	add(&uvecView);
	add(&vvecView);
}

Rectangle3df Rect3dView::getValue() const
{
	const auto o = originView.getValue();
	const auto u = uvecView.getValue();
	const auto v = vvecView.getValue();
	return Rectangle3df(o, u, v);
}

void Rect3dView::setValue(const Rectangle3df& value)
{
	originView.setValue(value.getPosition(0, 0));
	uvecView.setValue(value.getUVec());
	vvecView.setValue(value.getVVec());
}