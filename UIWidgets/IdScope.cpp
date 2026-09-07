#include "IdScope.h"

#include "imgui.h"

using namespace Phantom::UI;

void IdScope::onShow()
{
	if (idProvider_) name = idProvider_();

	ImGui::PushID(name.c_str());
	for (auto c : children) {
		c->show();
	}
	ImGui::PopID();
}
