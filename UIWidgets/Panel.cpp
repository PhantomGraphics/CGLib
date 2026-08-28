#include "Panel.h"

#include "imgui.h"

using namespace Phantom::UI;

void Panel::onShow()
{
	if (!name.empty()) {
		ImGui::Begin(name.c_str());
	}

	for (auto c : children) {
		c->show();
	}

	if (!name.empty()) {
		ImGui::End();
	}
}