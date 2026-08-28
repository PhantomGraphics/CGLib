#include "Vector3dView.h"

#include "imgui.h"

using namespace Phantom::Math;
using namespace Phantom::UI;

void Vector3dView::onShow()
{
	ImGui::InputScalarN(name.c_str(), ImGuiDataType_Float, &value[0], 3);
	//ImGui::InputFloat3(name.c_str(), &value[0]);
}