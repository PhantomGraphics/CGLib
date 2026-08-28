#include "ComboBox.h"
#include "imgui.h"

using namespace Phantom::UI;

void ComboBox::onShow()
{
	auto str = name.c_str();
	if (ImGui::BeginCombo(str, s_currentItem)) {
		for (int i = 0; i < items.size(); ++i) {
			const bool is_selected = (s_currentItem == items[i].c_str());
			if (ImGui::Selectable(items[i].c_str(), is_selected))
				s_currentItem = items[i].c_str();
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
}