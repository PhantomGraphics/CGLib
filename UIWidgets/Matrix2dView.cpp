#include "Matrix2dView.h"

#include "imgui.h"

using namespace Phantom::Math;
using namespace Phantom::UI;

void Matrix2dView::onShow()
{
	ImGui::InputFloat("00", &value[0][0]);
	//ImGui::SameLine();
	ImGui::InputFloat("01", &value[0][1]);
	//ImGui::SameLine();

	//ImGui::SameLine();
	ImGui::InputFloat("10", &value[1][0]);
	//ImGui::SameLine();
	ImGui::InputFloat("11", &value[1][1]);
}