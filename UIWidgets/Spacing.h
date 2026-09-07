#pragma once

#include "IWindow.h"

namespace Phantom {
	namespace UI {

/**
 * @brief 縦方向の小さな余白（宣言的構成用）．ImGui::Spacing を描画するだけ．
 */
class Spacing : public IWindow
{
public:
	Spacing() :
		IWindow("Spacing")
	{}

	void onShow() override;
};

	}
}
