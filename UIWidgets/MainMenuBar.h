#pragma once

#include "IView.h"

namespace Phantom {
	namespace UI {

/**
 * @brief アプリ上端のメインメニューバー（宣言的構成用）．
 *
 * ImGui::BeginMainMenuBar / EndMainMenuBar を内部で管理し，子（IMenu 等）を
 * 順に show() する．構成は add() で一度だけ登録する．
 */
class MainMenuBar : public IView
{
public:
	MainMenuBar() :
		IView("MainMenuBar")
	{}

	void onShow() override;
};

	}
}
