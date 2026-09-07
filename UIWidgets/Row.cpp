#include "Row.h"

#include "imgui.h"

using namespace Phantom::UI;

void Row::onShow()
{
	bool first = true;
	for (auto c : children) {
		if (!first) ImGui::SameLine();
		c->show();
		first = false;
	}
}
