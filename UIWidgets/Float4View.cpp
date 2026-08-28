#include "Float4View.h"

#include "imgui.h"

using namespace Phantom::Math;
using namespace Phantom::UI;

Float4View::Float4View(const std::string& name) :
	IWindow(name)
{
}

Float4View::Float4View(const std::string& name, const Vector4df& value) :
	IWindow(name),
	value(value)
{
}

void Float4View::onShow()
{
	auto str = name.c_str();
	ImGui::InputScalarN(str, ImGuiDataType_Double, &value[0], 4);
}