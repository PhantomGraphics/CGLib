#include "BoolView.h"
#include "imgui.h"

using namespace Phantom::UI;

BoolView::BoolView(const std::string& name) :
	BoolView(name, false)
{}

BoolView::BoolView(const std::string& name, const bool value) :
	IWindow(name),
	value(value)
{}

void BoolView::onShow()
{
	if (getter_) value = getter_();
	if (ImGui::Checkbox(name.c_str(), &value)) {
		if (setter_) setter_(value);
	}
}
