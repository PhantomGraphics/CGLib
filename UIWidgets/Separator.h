#pragma once

#include "IWindow.h"

namespace Phantom {
	namespace UI {

/**
 * @brief 水平区切り線（宣言的構成用）．
 *
 * ImGui::Separator を描画するだけのウィジェット．メニュー内でも使える．
 */
class Separator : public IWindow
{
public:
	Separator() :
		IWindow("Separator")
	{}

	void onShow() override;
};

	}
}
