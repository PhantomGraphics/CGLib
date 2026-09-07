#include "Section.h"

#include "imgui.h"

using namespace Phantom::UI;

void Section::onShow()
{
	const ImGuiTreeNodeFlags flags =
		defaultOpen_ ? ImGuiTreeNodeFlags_DefaultOpen : 0;
	if (ImGui::CollapsingHeader(name.c_str(), flags)) {
		for (auto c : children) {
			c->show();
		}
	}
}
