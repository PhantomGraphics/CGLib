#include "ComboBox.h"
#include "imgui.h"

using namespace Phantom::UI;

void ComboBox::onShow()
{
	const char* preview = selectedIndex >= 0 && selectedIndex < static_cast<int>(items.size())
		? items[static_cast<std::size_t>(selectedIndex)].c_str() : nullptr;
	if (ImGui::BeginCombo(name.c_str(), preview)) {
		for (int i = 0; i < static_cast<int>(items.size()); ++i) {
			const bool is_selected = selectedIndex == i;
			if (ImGui::Selectable(items[i].c_str(), is_selected))
				selectedIndex = i;
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
}
